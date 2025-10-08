/*
    Copyright (C) 2015-2025, Navaro, All Rights Reserved
    SPDX-License-Identifier: MIT

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.
 */

#include "qoraal/config.h"
#if CFG_OS_ZEPHYR

#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>

#include "qoraal/qoraal.h"

#define MAX_TLS_ID          4

typedef struct os_zephyr_thread os_zephyr_thread_t;

/* -------------------------------------------------------------------------- */
/* Helpers                                                                    */
/* -------------------------------------------------------------------------- */

static struct k_spinlock   _tls_lock;
static uint32_t            _tls_alloc_bitmap;
static os_zephyr_thread_t  _main_thread;
static bool                _main_thread_ready;

static inline k_timeout_t
os_zephyr_timeout_from_ticks(uint32_t ticks)
{
    if (ticks == OS_TIME_INFINITE) {
        return K_FOREVER;
    }

    if (ticks == OS_TIME_IMMEDIATE) {
        return K_NO_WAIT;
    }

    return K_TICKS(ticks);
}

static os_zephyr_thread_t *
os_zephyr_thread_layout(void *workspace, size_t workspace_size, size_t stack_size, bool write_header)
{
    if (workspace_size < sizeof(uint32_t)) {
        return NULL;
    }

    size_t total_size = workspace_size;
    uint8_t *base_ptr = (uint8_t *)workspace;
    if (stack_size == 0U) {
        stack_size = ((uint32_t *)base_ptr)[0];
    } else if (write_header) {
        ((uint32_t *)base_ptr)[0] = (uint32_t)stack_size;
    }

    if (stack_size == 0U) {
        return NULL;
    }

    base_ptr += sizeof(uint32_t);
    workspace_size -= sizeof(uint32_t);

    uintptr_t workspace_start = (uintptr_t)base_ptr;
    uintptr_t workspace_end = workspace_start + workspace_size;

    uintptr_t thread_addr = ROUND_UP(workspace_start, __alignof__(os_zephyr_thread_t));
    uintptr_t stack_addr = ROUND_UP(thread_addr + sizeof(os_zephyr_thread_t), Z_KERNEL_STACK_OBJ_ALIGN);
    uintptr_t required_end = stack_addr + K_THREAD_STACK_LEN(stack_size);

    if (required_end > workspace_end) {
        return NULL;
    }

    size_t clear_bytes = workspace_end - thread_addr;
    memset((void *)thread_addr, 0, clear_bytes);

    os_zephyr_thread_t *thread = (os_zephyr_thread_t *)thread_addr;
    thread->stack_mem = (k_thread_stack_t *)stack_addr;
    thread->stack_size = stack_size;
    thread->workspace_base = workspace;
    thread->workspace_size = total_size;
    thread->pthread_sem = (p_sem_t)&thread->thread_sem;

    return thread;
}

static os_zephyr_thread_t *
os_zephyr_thread_alloc(size_t stack_size)
{
    size_t workspace_size = OS_THREAD_WA_SIZE(stack_size);
    void *buffer = qoraal_malloc(QORAAL_HeapOperatingSystem, workspace_size);
    if (!buffer) {
        return NULL;
    }

    os_zephyr_thread_t *thread = os_zephyr_thread_layout(buffer, workspace_size, stack_size, true);
    if (!thread) {
        qoraal_free(QORAAL_HeapOperatingSystem, buffer);
        return NULL;
    }

    thread->heap = 1;
    return thread;
}

static void
os_zephyr_thread_common_init(os_zephyr_thread_t *thread)
{
    k_sem_init(&thread->join_sem, 0, 1);
    k_sem_init(&thread->thread_sem, 0, K_SEM_MAX_LIMIT);
    k_sem_init(&thread->notify_sem, 0, K_SEM_MAX_LIMIT);
    thread->pthread_sem = (p_sem_t)&thread->thread_sem;
    thread->errno_val = 0;
    thread->tls_bitmap = 0;
    thread->notify_value = 0;
    atomic_set(&thread->terminated, 0);
}

static void
os_zephyr_thread_set_current(os_zephyr_thread_t *thread)
{
    k_thread_custom_data_set(thread);
}

static os_zephyr_thread_t *
os_zephyr_thread_get_current(void)
{
    os_zephyr_thread_t *thread = k_thread_custom_data_get();

    if (thread) {
        return thread;
    }

    if (!_main_thread_ready) {
        memset(&_main_thread, 0, sizeof(_main_thread));
        os_zephyr_thread_common_init(&_main_thread);
        _main_thread.heap = 0;
        _main_thread.workspace_base = NULL;
        _main_thread.workspace_size = 0;
        _main_thread.stack_mem = NULL;
        _main_thread.stack_size = 0;
        _main_thread.name = k_thread_name_get(k_current_get());
        _main_thread_ready = true;
    }

    os_zephyr_thread_set_current(&_main_thread);
    return &_main_thread;
}

static void
os_zephyr_thread_entry(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    os_zephyr_thread_t *thread = (os_zephyr_thread_t *)p1;
    os_zephyr_thread_set_current(thread);

    if (thread->entry) {
        thread->entry(thread->arg);
    }

    atomic_set(&thread->terminated, 1);
    k_sem_give(&thread->join_sem);
}

static inline os_zephyr_thread_t *
os_zephyr_thread_from_handle(p_thread_t *handle)
{
    if (!handle || !(*handle)) {
        return NULL;
    }
    return (os_zephyr_thread_t *)(*handle);
}

static inline k_timeout_t
os_zephyr_timeout_from_ms(uint32_t ms)
{
    if (ms == OS_TIME_INFINITE) {
        return K_FOREVER;
    }
    return K_MSEC(ms);
}

/* -------------------------------------------------------------------------- */
/* System control                                                             */
/* -------------------------------------------------------------------------- */

void
os_sys_start(void)
{
    /* Zephyr kernel is already running when main executes. */
}

int
os_sys_started(void)
{
    return 1;
}

void
os_sys_lock(void)
{
    k_sched_lock();
}

void
os_sys_unlock(void)
{
    k_sched_unlock();
}

uint32_t
os_sys_is_irq(void)
{
    return k_is_in_isr();
}

uint32_t
os_sys_ticks(void)
{
    return (uint32_t)k_uptime_ticks();
}

uint32_t
os_sys_tick_freq(void)
{
    return CONFIG_SYS_CLOCK_TICKS_PER_SEC;
}

uint32_t
os_sys_timestamp(void)
{
    return k_cycle_get_32();
}

uint32_t
os_sys_us_timestamp(void)
{
    return k_cyc_to_us_near32(k_cycle_get_32());
}

void
os_sys_stop(void)
{
    k_fatal_halt(0);
}

void
os_sys_halt(const char *msg)
{
    ARG_UNUSED(msg);
    k_panic();
}

/* -------------------------------------------------------------------------- */
/* Thread management                                                          */
/* -------------------------------------------------------------------------- */

int32_t
os_thread_create(uint16_t stack_size, uint32_t prio, p_thread_function_t pf,
                 void *arg, p_thread_t *thread_handle, const char *name)
{
    if (stack_size == 0U) {
        if (thread_handle) {
            *thread_handle = NULL;
        }
        return E_PARM;
    }

    os_zephyr_thread_t *thread = os_zephyr_thread_alloc(stack_size);
    if (!thread) {
        if (thread_handle) {
            *thread_handle = NULL;
        }
        return E_NOMEM;
    }

    os_zephyr_thread_common_init(thread);
    thread->entry = pf;
    thread->arg = arg;
    thread->name = name;

    k_tid_t tid = k_thread_create(&thread->thread,
                                  thread->stack_mem,
                                  thread->stack_size,
                                  os_zephyr_thread_entry,
                                  thread, NULL, NULL,
                                  OS_THREAD_PRIO(prio),
                                  0,
                                  K_NO_WAIT);

    if (!tid) {
        qoraal_free(QORAAL_HeapOperatingSystem, thread->workspace_base);
        if (thread_handle) {
            *thread_handle = NULL;
        }
        return EFAIL;
    }

    if (name) {
        k_thread_name_set(tid, name);
    }

    if (thread_handle) {
        *thread_handle = (p_thread_t)thread;
    }

    return EOK;
}

int32_t
os_thread_create_static(void *wsp, uint16_t size, uint32_t prio,
                        p_thread_function_t pf, void *arg,
                        p_thread_t *thread_handle, const char *name)
{
    os_zephyr_thread_t *thread = os_zephyr_thread_layout(wsp, size, 0, false);
    if (!thread) {
        if (thread_handle) {
            *thread_handle = NULL;
        }
        return E_PARM;
    }

    thread->heap = 0;
    thread->workspace_base = wsp;
    thread->workspace_size = size;
    os_zephyr_thread_common_init(thread);
    thread->entry = pf;
    thread->arg = arg;
    thread->name = name;

    k_tid_t tid = k_thread_create(&thread->thread,
                                  thread->stack_mem,
                                  thread->stack_size,
                                  os_zephyr_thread_entry,
                                  thread, NULL, NULL,
                                  OS_THREAD_PRIO(prio),
                                  0,
                                  K_NO_WAIT);
    if (!tid) {
        if (thread_handle) {
            *thread_handle = NULL;
        }
        return EFAIL;
    }

    if (name) {
        k_thread_name_set(tid, name);
    }

    if (thread_handle) {
        *thread_handle = (p_thread_t)thread;
    }

    return EOK;
}

const char *
os_thread_get_name(p_thread_t *thread_handle)
{
    os_zephyr_thread_t *thread = os_zephyr_thread_from_handle(thread_handle);
    if (!thread) {
        return NULL;
    }

    const char *name = k_thread_name_get(&thread->thread);
    if (name) {
        return name;
    }
    return thread->name;
}

p_thread_t
os_thread_current(void)
{
    return (p_thread_t)os_zephyr_thread_get_current();
}

void
os_thread_sleep(uint32_t msec)
{
    k_sleep(os_zephyr_timeout_from_ms(msec));
}

void
os_thread_sleep_ticks(uint32_t ticks)
{
    k_sleep(os_zephyr_timeout_from_ticks(ticks));
}

void
os_thread_join(p_thread_t *thread_handle)
{
    (void)os_thread_join_timeout(thread_handle, OS_TIME_INFINITE);
}

int32_t
os_thread_join_timeout(p_thread_t *thread_handle, uint32_t ticks)
{
    os_zephyr_thread_t *thread = os_zephyr_thread_from_handle(thread_handle);
    if (!thread) {
        return E_PARM;
    }

    int rc = k_sem_take(&thread->join_sem, os_zephyr_timeout_from_ticks(ticks));
    if (rc == 0) {
        return EOK;
    }
    return E_TIMEOUT;
}

void
os_thread_release(p_thread_t *thread_handle)
{
    os_zephyr_thread_t *thread = os_zephyr_thread_from_handle(thread_handle);
    if (!thread) {
        return;
    }

    if (!atomic_get(&thread->terminated)) {
        k_thread_abort(&thread->thread);
        atomic_set(&thread->terminated, 1);
        k_sem_give(&thread->join_sem);
    }

    os_thread_join(thread_handle);

    if (thread->heap && thread->workspace_base) {
        qoraal_free(QORAAL_HeapOperatingSystem, thread->workspace_base);
    }

    if (thread_handle) {
        *thread_handle = NULL;
    }
}

uint32_t
os_thread_get_prio(void)
{
    int prio = k_thread_priority_get(k_current_get());
    return OS_THREAD_GET_PRIO(prio);
}

uint32_t
os_thread_set_prio(p_thread_t *thread_handle, uint32_t prio)
{
    k_tid_t target;

    if (!thread_handle || !(*thread_handle)) {
        target = k_current_get();
    } else {
        os_zephyr_thread_t *thread = (os_zephyr_thread_t *)(*thread_handle);
        target = &thread->thread;
    }

    int old = k_thread_priority_get(target);
    k_thread_priority_set(target, OS_THREAD_PRIO(prio));
    return OS_THREAD_GET_PRIO(old);
}

int32_t
os_thread_tls_alloc(int32_t *index)
{
    if (!index) {
        return E_PARM;
    }

    struct k_spinlock_key key = k_spin_lock(&_tls_lock);
    for (int i = 0; i < MAX_TLS_ID; ++i) {
        if ((~_tls_alloc_bitmap) & (1U << i)) {
            _tls_alloc_bitmap |= (1U << i);
            k_spin_unlock(&_tls_lock, key);
            *index = i;
            return EOK;
        }
    }
    k_spin_unlock(&_tls_lock, key);
    *index = -1;
    return E_BUSY;
}

void
os_thread_tls_free(int32_t index)
{
    if (index < 0 || index >= MAX_TLS_ID) {
        return;
    }

    struct k_spinlock_key key = k_spin_lock(&_tls_lock);
    _tls_alloc_bitmap &= ~(1U << index);
    k_spin_unlock(&_tls_lock, key);
}

int32_t
os_thread_tls_set(int32_t idx, uint32_t value)
{
    os_zephyr_thread_t *thread = os_zephyr_thread_get_current();
    if (!thread || idx < 0 || idx >= MAX_TLS_ID) {
        return E_PARM;
    }

    thread->tls_values[idx] = value;
    thread->tls_bitmap |= (1U << idx);
    return EOK;
}

uint32_t
os_thread_tls_get(int32_t idx)
{
    os_zephyr_thread_t *thread = os_zephyr_thread_get_current();
    if (!thread || idx < 0 || idx >= MAX_TLS_ID) {
        return 0;
    }

    if (!(thread->tls_bitmap & (1U << idx))) {
        return 0;
    }

    return thread->tls_values[idx];
}

p_sem_t *
os_thread_thdsem_get(void)
{
    os_zephyr_thread_t *thread = os_zephyr_thread_get_current();
    return &thread->pthread_sem;
}

int32_t *
os_thread_errno(void)
{
    os_zephyr_thread_t *thread = os_zephyr_thread_get_current();
    return &thread->errno_val;
}

int32_t
os_thread_wait(uint32_t ticks)
{
    os_zephyr_thread_t *thread = os_zephyr_thread_get_current();
    int rc = k_sem_take(&thread->notify_sem, os_zephyr_timeout_from_ticks(ticks));
    if (rc == 0) {
        return thread->notify_value;
    }
    if (rc == -EAGAIN || rc == -EBUSY) {
        return E_TIMEOUT;
    }
    return EFAIL;
}

int32_t
os_thread_notify(p_thread_t *thread_handle, int32_t msg)
{
    os_zephyr_thread_t *thread = os_zephyr_thread_from_handle(thread_handle);
    if (!thread) {
        return E_PARM;
    }

    thread->notify_value = msg;
    k_sem_give(&thread->notify_sem);
    return EOK;
}

int32_t
os_thread_notify_isr(p_thread_t *thread_handle, int32_t msg)
{
    return os_thread_notify(thread_handle, msg);
}

/* -------------------------------------------------------------------------- */
/* Mutexes                                                                    */
/* -------------------------------------------------------------------------- */

int32_t
os_mutex_create(p_mutex_t *mutex)
{
    struct k_mutex *kmutex = qoraal_malloc(QORAAL_HeapOperatingSystem, sizeof(struct k_mutex));
    if (!kmutex) {
        return E_NOMEM;
    }
    k_mutex_init(kmutex);
    *mutex = (p_mutex_t)kmutex;
    return EOK;
}

void
os_mutex_delete(p_mutex_t *mutex)
{
    if (!mutex || !(*mutex)) {
        return;
    }
    struct k_mutex *kmutex = (struct k_mutex *)(*mutex);
    qoraal_free(QORAAL_HeapOperatingSystem, kmutex);
    *mutex = NULL;
}

int32_t
os_mutex_init(p_mutex_t *mutex)
{
    struct k_mutex *kmutex = (struct k_mutex *)(*mutex);
    k_mutex_init(kmutex);
    return EOK;
}

void
os_mutex_deinit(p_mutex_t *mutex)
{
    ARG_UNUSED(mutex);
}

int32_t
os_mutex_lock(p_mutex_t *mutex)
{
    struct k_mutex *kmutex = (struct k_mutex *)(*mutex);
    return k_mutex_lock(kmutex, K_FOREVER) == 0 ? EOK : EFAIL;
}

void
os_mutex_unlock(p_mutex_t *mutex)
{
    struct k_mutex *kmutex = (struct k_mutex *)(*mutex);
    k_mutex_unlock(kmutex);
}

int32_t
os_mutex_trylock(p_mutex_t *mutex)
{
    struct k_mutex *kmutex = (struct k_mutex *)(*mutex);
    int rc = k_mutex_lock(kmutex, K_NO_WAIT);
    if (rc == 0) {
        return EOK;
    }
    if (rc == -EBUSY) {
        return E_BUSY;
    }
    return EFAIL;
}

/* -------------------------------------------------------------------------- */
/* Counting semaphores                                                         */
/* -------------------------------------------------------------------------- */

static int32_t
os_sem_configure(struct k_sem *ksem, int32_t count, uint32_t limit)
{
    if (count < 0) {
        count = 0;
    }
    if ((uint32_t)count > limit) {
        count = (int32_t)limit;
    }
    int rc = k_sem_init(ksem, (unsigned int)count, limit);
    return rc == 0 ? EOK : EFAIL;
}

int32_t
os_sem_create(p_sem_t *sem, int32_t cnt)
{
    struct k_sem *ksem = qoraal_malloc(QORAAL_HeapOperatingSystem, sizeof(struct k_sem));
    if (!ksem) {
        return E_NOMEM;
    }
    if (os_sem_configure(ksem, cnt, K_SEM_MAX_LIMIT) != EOK) {
        qoraal_free(QORAAL_HeapOperatingSystem, ksem);
        return EFAIL;
    }
    *sem = (p_sem_t)ksem;
    return EOK;
}

void
os_sem_delete(p_sem_t *sem)
{
    if (!sem || !(*sem)) {
        return;
    }
    struct k_sem *ksem = (struct k_sem *)(*sem);
    qoraal_free(QORAAL_HeapOperatingSystem, ksem);
    *sem = NULL;
}

int32_t
os_sem_init(p_sem_t *sem, int32_t cnt)
{
    if (!sem || !(*sem)) {
        return E_PARM;
    }
    struct k_sem *ksem = (struct k_sem *)(*sem);
    return os_sem_configure(ksem, cnt, K_SEM_MAX_LIMIT);
}

void
os_sem_deinit(p_sem_t *sem)
{
    ARG_UNUSED(sem);
}

int32_t
os_sem_reset(p_sem_t *sem, int32_t cnt)
{
    if (!sem || !(*sem)) {
        return E_PARM;
    }
    struct k_sem *ksem = (struct k_sem *)(*sem);
    return os_sem_configure(ksem, cnt, K_SEM_MAX_LIMIT);
}

int32_t
os_sem_wait(p_sem_t *sem)
{
    if (!sem || !(*sem)) {
        return E_PARM;
    }
    struct k_sem *ksem = (struct k_sem *)(*sem);
    return k_sem_take(ksem, K_FOREVER) == 0 ? EOK : EFAIL;
}

int32_t
os_sem_wait_timeout(p_sem_t *sem, uint32_t ticks)
{
    if (!sem || !(*sem)) {
        return E_PARM;
    }
    struct k_sem *ksem = (struct k_sem *)(*sem);
    int rc = k_sem_take(ksem, os_zephyr_timeout_from_ticks(ticks));
    if (rc == 0) {
        return EOK;
    }
    if (rc == -EAGAIN || rc == -EBUSY) {
        return E_TIMEOUT;
    }
    return EFAIL;
}

void
os_sem_signal(p_sem_t *sem)
{
    if (!sem || !(*sem)) {
        return;
    }
    struct k_sem *ksem = (struct k_sem *)(*sem);
    k_sem_give(ksem);
}

void
os_sem_signal_isr(p_sem_t *sem)
{
    os_sem_signal(sem);
}

int32_t
os_sem_count(p_sem_t *sem)
{
    if (!sem || !(*sem)) {
        return 0;
    }
    struct k_sem *ksem = (struct k_sem *)(*sem);
    return k_sem_count_get(ksem);
}

/* -------------------------------------------------------------------------- */
/* Binary semaphores                                                          */
/* -------------------------------------------------------------------------- */

static int32_t
os_bsem_configure(struct k_sem *ksem, int32_t taken)
{
    unsigned int initial = taken ? 0U : 1U;
    int rc = k_sem_init(ksem, initial, 1U);
    return rc == 0 ? EOK : EFAIL;
}

int32_t
os_bsem_create(p_sem_t *sem, int32_t taken)
{
    struct k_sem *ksem = qoraal_malloc(QORAAL_HeapOperatingSystem, sizeof(struct k_sem));
    if (!ksem) {
        return E_NOMEM;
    }
    if (os_bsem_configure(ksem, taken) != EOK) {
        qoraal_free(QORAAL_HeapOperatingSystem, ksem);
        return EFAIL;
    }
    *sem = (p_sem_t)ksem;
    return EOK;
}

void
os_bsem_delete(p_sem_t *sem)
{
    os_sem_delete(sem);
}

int32_t
os_bsem_init(p_sem_t *sem, int32_t taken)
{
    if (!sem || !(*sem)) {
        return E_PARM;
    }
    struct k_sem *ksem = (struct k_sem *)(*sem);
    return os_bsem_configure(ksem, taken);
}

void
os_bsem_deinit(p_sem_t *sem)
{
    ARG_UNUSED(sem);
}

int32_t
os_bsem_reset(p_sem_t *sem, int32_t taken)
{
    if (!sem || !(*sem)) {
        return E_PARM;
    }
    struct k_sem *ksem = (struct k_sem *)(*sem);
    return os_bsem_configure(ksem, taken);
}

int32_t
os_bsem_wait(p_sem_t *sem)
{
    return os_sem_wait(sem);
}

int32_t
os_bsem_wait_timeout(p_sem_t *sem, uint32_t ticks)
{
    return os_sem_wait_timeout(sem, ticks);
}

void
os_bsem_signal(p_sem_t *sem)
{
    os_sem_signal(sem);
}

void
os_bsem_signal_isr(p_sem_t *sem)
{
    os_sem_signal_isr(sem);
}

/* -------------------------------------------------------------------------- */
/* Events                                                                     */
/* -------------------------------------------------------------------------- */

int32_t
os_event_create(p_event_t *event)
{
    struct k_event *kevent = qoraal_malloc(QORAAL_HeapOperatingSystem, sizeof(struct k_event));
    if (!kevent) {
        if (event) {
            *event = NULL;
        }
        return E_NOMEM;
    }
    k_event_init(kevent);
    *event = (p_event_t)kevent;
    return EOK;
}

void
os_event_delete(p_event_t *event)
{
    if (!event || !(*event)) {
        return;
    }
    struct k_event *kevent = (struct k_event *)(*event);
    qoraal_free(QORAAL_HeapOperatingSystem, kevent);
    *event = NULL;
}

int32_t
os_event_init(p_event_t *event)
{
    if (!event || !(*event)) {
        return E_PARM;
    }
    struct k_event *kevent = (struct k_event *)(*event);
    k_event_init(kevent);
    return EOK;
}

void
os_event_deinit(p_event_t *event)
{
    ARG_UNUSED(event);
}

static uint32_t
os_event_wait_common(struct k_event *kevent, uint32_t clear_on_exit, uint32_t mask,
                     uint32_t all, k_timeout_t timeout)
{
    uint32_t events = k_event_wait(kevent, mask, all != 0U, timeout);
    if (events && clear_on_exit) {
        k_event_clear(kevent, clear_on_exit);
    }
    return events;
}

uint32_t
os_event_wait(p_event_t *event, uint32_t clear_on_exit, uint32_t mask, uint32_t all)
{
    if (!event || !(*event)) {
        return 0;
    }
    struct k_event *kevent = (struct k_event *)(*event);
    return os_event_wait_common(kevent, clear_on_exit, mask, all, K_FOREVER);
}

uint32_t
os_event_wait_timeout(p_event_t *event, uint32_t clear_on_exit, uint32_t mask,
                      uint32_t all, uint32_t ticks)
{
    if (!event || !(*event)) {
        return 0;
    }
    struct k_event *kevent = (struct k_event *)(*event);
    return os_event_wait_common(kevent, clear_on_exit, mask, all,
                                os_zephyr_timeout_from_ticks(ticks));
}

void
os_event_clear(p_event_t *event, uint32_t mask)
{
    if (!event || !(*event)) {
        return;
    }
    struct k_event *kevent = (struct k_event *)(*event);
    k_event_clear(kevent, mask);
}

void
os_event_signal(p_event_t *event, uint32_t mask)
{
    if (!event || !(*event)) {
        return;
    }
    struct k_event *kevent = (struct k_event *)(*event);
    k_event_post(kevent, mask);
}

void
os_event_signal_isr(p_event_t *event, uint32_t mask)
{
    os_event_signal(event, mask);
}

/* -------------------------------------------------------------------------- */
/* Timers                                                                     */
/* -------------------------------------------------------------------------- */

typedef struct os_zephyr_timer_wrapper os_zephyr_timer_t;

static void
os_timer_expiry_handler(struct k_timer *ktimer)
{
    os_zephyr_timer_t *wrapper = CONTAINER_OF(ktimer, os_zephyr_timer_t, timer);
    if (wrapper->callback) {
        wrapper->callback(wrapper->callback_param);
    }
}

static os_zephyr_timer_t *
os_timer_from_handle(p_timer_t *timer)
{
    if (!timer || !(*timer)) {
        return NULL;
    }
    return (os_zephyr_timer_t *)(*timer);
}

int32_t
os_timer_create(p_timer_t *timer, p_timer_function_t fp, void *parm)
{
    os_zephyr_timer_t *wrapper = qoraal_malloc(QORAAL_HeapOperatingSystem, sizeof(os_zephyr_timer_t));
    if (!wrapper) {
        if (timer) {
            *timer = NULL;
        }
        return E_NOMEM;
    }

    k_timer_init(&wrapper->timer, os_timer_expiry_handler, NULL);
    wrapper->callback = fp;
    wrapper->callback_param = parm;
    *timer = (p_timer_t)wrapper;
    return EOK;
}

void
os_timer_delete(p_timer_t *timer)
{
    os_zephyr_timer_t *wrapper = os_timer_from_handle(timer);
    if (!wrapper) {
        return;
    }
    k_timer_stop(&wrapper->timer);
    qoraal_free(QORAAL_HeapOperatingSystem, wrapper);
    *timer = NULL;
}

int32_t
os_timer_init(p_timer_t *timer, p_timer_function_t fp, void *parm)
{
    os_zephyr_timer_t *wrapper = os_timer_from_handle(timer);
    if (!wrapper) {
        return E_PARM;
    }
    k_timer_init(&wrapper->timer, os_timer_expiry_handler, NULL);
    wrapper->callback = fp;
    wrapper->callback_param = parm;
    return EOK;
}

void
os_timer_deinit(p_timer_t *timer)
{
    os_zephyr_timer_t *wrapper = os_timer_from_handle(timer);
    if (!wrapper) {
        return;
    }
    k_timer_stop(&wrapper->timer);
}

static inline void
os_timer_start_common(p_timer_t *timer, uint32_t ticks)
{
    os_zephyr_timer_t *wrapper = os_timer_from_handle(timer);
    if (!wrapper) {
        return;
    }
    if (ticks == 0U) {
        ticks = 1U;
    }
    k_timer_start(&wrapper->timer, K_TICKS(ticks), K_NO_WAIT);
}

void
os_timer_set(p_timer_t *timer, uint32_t ticks)
{
    os_timer_start_common(timer, ticks);
}

void
os_timer_set_i(p_timer_t *timer, uint32_t ticks)
{
    os_timer_start_common(timer, ticks);
}

int32_t
os_timer_is_set(p_timer_t *timer)
{
    os_zephyr_timer_t *wrapper = os_timer_from_handle(timer);
    if (!wrapper) {
        return 0;
    }
    return k_timer_remaining_ticks(&wrapper->timer) > 0 ? 1 : 0;
}

void
os_timer_reset(p_timer_t *timer)
{
    os_zephyr_timer_t *wrapper = os_timer_from_handle(timer);
    if (!wrapper) {
        return;
    }
    k_timer_stop(&wrapper->timer);
}

/* -------------------------------------------------------------------------- */
/* MLock helpers rely on common implementation                                */
/* -------------------------------------------------------------------------- */

#endif /* CFG_OS_ZEPHYR */

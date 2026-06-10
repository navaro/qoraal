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
#include "qoraal/qoraal.h"
#include "qoraal/svc/svc_wdt.h"
#include "qoraal/svc/svc_tasks.h"
#include "qoraal/common/lists.h"


#define SVC_WDT_FLAGS_ACTIVE                (1<<0)
#define SVC_WDT_FLAGS_KICKED                (1<<1)
#define SVC_WDT_FLAGS_FLAGGED               (1<<2)
#define SVC_WDT_FLAGS_REPORTED              (1<<3)

#define SVC_WDT_SCAN_INTERVAL_MS            5000U
#define SVC_WDT_SEC2MS(sec)                 ((sec) * 1000U)

static uint32_t svc_wdt_timeout_sec (SVC_WDT_TIMEOUTS_T id) ;

#ifndef CFG_SVC_WDT_DISABLE_PLATFORM

static void     svc_wdt_task_cb (SVC_TASKS_T *task, uintptr_t parm, uint32_t reason) ;
static uint8_t  svc_wdt_process (SVC_WDT_TIMEOUTS_T id) ;
static uint32_t svc_wdt_check_interval_ms (SVC_WDT_TIMEOUTS_T id) ;
static uint32_t svc_wdt_hw_kick_interval_ms (uint32_t hw_timeout_sec) ;
static uint32_t svc_wdt_task_interval_ms (void) ;
static void     svc_wdt_set_hw_timeout (uint32_t hw_timeout_sec) ;
static void     svc_wdt_reset_state (void) ;
static int32_t  svc_wdt_schedule_next (void) ;
static uint32_t svc_wdt_elapsed_ms (void) ;
static uint8_t  svc_wdt_bucket_reported (SVC_WDT_TIMEOUTS_T id) ;
static uint8_t  svc_wdt_process_bucket (SVC_WDT_TIMEOUTS_T id, uint32_t *elapsed_ms) ;

/*
 * svc_wdt has two timing domains:
 *
 * - Software watchdog buckets are scanned at fixed intervals derived from
 *   their bucket timeout. This keeps flagging/reporting behavior predictable.
 *
 * - The physical watchdog is kicked at half of the timeout returned by the
 *   platform callback. This allows a longer hardware reset window without
 *   changing software watchdog bucket semantics.
 *
 * Elapsed time is measured with os_sys_timestamp(). Unsigned subtraction keeps
 * the delta correct across the 32-bit millisecond timestamp rollover.
 */
static uint32_t             _svc_wdt_hw_timeout_sec = 0 ;
static uint32_t             _svc_wdt_hw_kick_interval_ms = 0 ;
static uint32_t             _svc_wdt_task_interval_ms = SVC_WDT_SCAN_INTERVAL_MS ;
static uint32_t             _svc_wdt_last_ms = 0 ;
static uint32_t             _svc_wdt_hw_elapsed_ms = 0 ;
static uint32_t             _svc_wdt_10_elapsed_ms = 0 ;
static uint32_t             _svc_wdt_30_elapsed_ms = 0 ;
static uint32_t             _svc_wdt_60_elapsed_ms = 0 ;
static OS_MUTEX_DECL        (_svc_wdt_mutex) ;
static stack_t              _svc_wdt_handler_stack[TIMEOUT_LAST] ;
static SVC_TASKS_DECL		(_svc_wdt_task)  ;
#endif


/**
 * @brief   Initializes events.
 *
 * @svc
 */
int32_t
svc_wdt_init (void)
{
#ifndef CFG_SVC_WDT_DISABLE_PLATFORM
    int i ;
    DBG_MESSAGE_SVC_WDT (DBG_MESSAGE_SEVERITY_INFO, " -->> svc_wdt_init") ;
    os_mutex_init (&_svc_wdt_mutex) ;

    for (i=0; i<TIMEOUT_LAST; i++) {
        stack_init (&_svc_wdt_handler_stack[i]) ;
    }
#endif
    return EOK ;
}

/**
 * @brief   Start the wdt.
 *
 *
 * @svc
 */
int32_t
svc_wdt_start (void)
{
#ifndef CFG_SVC_WDT_DISABLE_PLATFORM
    svc_wdt_reset_state () ;
    svc_wdt_set_hw_timeout (qoraal_wdt_kick ()) ;

    if (_svc_wdt_hw_timeout_sec) {
        svc_tasks_cancel (&_svc_wdt_task) ;
        return svc_wdt_schedule_next () ;
    }

    return EOK ;
#else
    return EOK ;
#endif
}

/**
 * @brief   Stop the wdt.
 *
 *
 * @svc
 */
void
svc_wdt_stop (void)
{
#ifndef CFG_SVC_WDT_DISABLE_PLATFORM
    svc_tasks_cancel (&_svc_wdt_task) ;

#endif
    return ;
}

/**
 * @brief   Register a watchdog handler listener.
 *
 * @param[in] handler       Caller allocated handle structure
 * @param[in] id            TImeout group
 *
 * @return              Error.
 *
 * @svc
 */
void
svc_wdt_register (SVC_WDT_HANDLE_T * handler, SVC_WDT_TIMEOUTS_T id)
{
    memset (handler, 0, sizeof(SVC_WDT_HANDLE_T)) ;
    handler->thread = os_thread_current () ;
    handler->timeout = id ;
#ifndef CFG_SVC_WDT_DISABLE_PLATFORM
    if (id < TIMEOUT_LAST) {
        os_mutex_lock (&_svc_wdt_mutex) ;

        stack_remove (&_svc_wdt_handler_stack[id], (plists_t)handler, OFFSETOF(SVC_WDT_HANDLE_T, next)) ;
        stack_add_head (&_svc_wdt_handler_stack[id], handler, OFFSETOF(SVC_WDT_HANDLE_T, next)) ;

        os_mutex_unlock (&_svc_wdt_mutex) ;
    }
#endif
    return  ;
}

/**
 * @brief   Remove a watchdog handler listener.
 *
 * @param[in] handler       Caller allocated handle structure
 * @param[in] id            TImeout group
 *
 * @svc
 */
void 
svc_wdt_unregister (SVC_WDT_HANDLE_T * handler, SVC_WDT_TIMEOUTS_T id)
{
#ifndef CFG_SVC_WDT_DISABLE_PLATFORM
    if (id < TIMEOUT_LAST) {
        os_mutex_lock (&_svc_wdt_mutex) ;
        stack_remove (&_svc_wdt_handler_stack[id], (plists_t)handler, OFFSETOF(SVC_WDT_HANDLE_T, next)) ;
        os_mutex_unlock (&_svc_wdt_mutex) ;
    }
#endif
}

/**
 * @brief   Get the timeout value for a watchdog handler.
 *
 * @param[in] handler       Caller allocated handle structure
 *
 * @return              Timeout value in seconds, or -1 if invalid.
 *
 * @svc
 */
uint32_t
svc_wdt_timeout (SVC_WDT_HANDLE_T * handler)
{
    return svc_wdt_timeout_sec (handler->timeout) ;
}

static uint32_t
svc_wdt_timeout_sec (SVC_WDT_TIMEOUTS_T id)
{
    if (id == TIMEOUT_10_SEC) return 10 ;
    else if (id == TIMEOUT_30_SEC) return 30 ;
    else if (id == TIMEOUT_60_SEC) return 60 ;
    return 10 ;
}

void
svc_wdt_activate (SVC_WDT_HANDLE_T * handler)
{
#ifndef CFG_SVC_WDT_DISABLE_PLATFORM
    p_thread_t thread = os_thread_current () ;
    os_mutex_lock (&_svc_wdt_mutex) ;
    handler->thread = thread ;
    handler->flags = (SVC_WDT_FLAGS_ACTIVE|SVC_WDT_FLAGS_KICKED) ;
    os_mutex_unlock (&_svc_wdt_mutex) ;
#endif
}

void
svc_wdt_set_id (SVC_WDT_HANDLE_T * handler, uintptr_t id)
{
    handler->id = id ;
}


void
svc_wdt_deactivate (SVC_WDT_HANDLE_T * handler)
{
#ifndef CFG_SVC_WDT_DISABLE_PLATFORM
    if (handler->flags & SVC_WDT_FLAGS_REPORTED) {
        DBG_MESSAGE_SVC_WDT (DBG_MESSAGE_SEVERITY_REPORT,
                "WDT   : : deactivate '%s' (0x%x) as reported",
                os_thread_get_name(&handler->thread), handler->id) ;

    }

    os_mutex_lock (&_svc_wdt_mutex) ;
    handler->flags &= ~(SVC_WDT_FLAGS_ACTIVE|SVC_WDT_FLAGS_KICKED|SVC_WDT_FLAGS_FLAGGED|SVC_WDT_FLAGS_REPORTED) ;
    os_mutex_unlock (&_svc_wdt_mutex) ;
#endif
}

void
svc_wdt_handler_kick (SVC_WDT_HANDLE_T * handler)
{
#ifndef CFG_SVC_WDT_DISABLE_PLATFORM
    if (handler->flags & SVC_WDT_FLAGS_REPORTED) {
        DBG_MESSAGE_SVC_WDT (DBG_MESSAGE_SEVERITY_REPORT,
                "WDT   : : kick '%s' (0x%x) as reported",
                os_thread_get_name(&handler->thread), handler->id) ;

    }

    os_mutex_lock (&_svc_wdt_mutex) ;
    handler->flags |= SVC_WDT_FLAGS_KICKED ;
    handler->flags &= ~(SVC_WDT_FLAGS_REPORTED|SVC_WDT_FLAGS_FLAGGED) ;
    os_mutex_unlock (&_svc_wdt_mutex) ;
#endif
}

void
svc_wdt_kick (void)
{
#ifndef CFG_SVC_WDT_DISABLE_PLATFORM
    uint32_t hw_timeout_sec ;

    DBG_MESSAGE_SVC_WDT (DBG_MESSAGE_SEVERITY_INFO,
            "WDT   : : kick");
    hw_timeout_sec = qoraal_wdt_kick () ;

    os_mutex_lock (&_svc_wdt_mutex) ;
    svc_wdt_set_hw_timeout (hw_timeout_sec) ;
    os_mutex_unlock (&_svc_wdt_mutex) ;
#endif
}

#ifndef CFG_SVC_WDT_DISABLE_PLATFORM
static uint32_t
svc_wdt_check_interval_ms (SVC_WDT_TIMEOUTS_T id)
{
    uint32_t interval = SVC_WDT_SEC2MS(svc_wdt_timeout_sec (id)) / 2U ;
    return interval ? interval : 1U ;
}

static uint32_t
svc_wdt_hw_kick_interval_ms (uint32_t hw_timeout_sec)
{
    uint32_t interval = SVC_WDT_SEC2MS(hw_timeout_sec) / 2U ;
    return interval ? interval : 1U ;
}

static uint32_t
svc_wdt_task_interval_ms (void)
{
    if (_svc_wdt_hw_kick_interval_ms &&
            (_svc_wdt_hw_kick_interval_ms < SVC_WDT_SCAN_INTERVAL_MS)) {
        return _svc_wdt_hw_kick_interval_ms ;
    }

    return SVC_WDT_SCAN_INTERVAL_MS ;
}

static void
svc_wdt_set_hw_timeout (uint32_t hw_timeout_sec)
{
    _svc_wdt_hw_timeout_sec = hw_timeout_sec ;
    _svc_wdt_hw_kick_interval_ms = svc_wdt_hw_kick_interval_ms (hw_timeout_sec) ;
    _svc_wdt_hw_elapsed_ms = 0 ;
    _svc_wdt_last_ms = os_sys_timestamp () ;
}

static void
svc_wdt_reset_state (void)
{
    _svc_wdt_10_elapsed_ms = 0 ;
    _svc_wdt_30_elapsed_ms = 0 ;
    _svc_wdt_60_elapsed_ms = 0 ;
    _svc_wdt_last_ms = os_sys_timestamp () ;
}

static int32_t
svc_wdt_schedule_next (void)
{
    if (!_svc_wdt_hw_timeout_sec) {
        return EOK ;
    }

    _svc_wdt_task_interval_ms = svc_wdt_task_interval_ms () ;
    return svc_tasks_schedule (&_svc_wdt_task, svc_wdt_task_cb, 0,
            SERVICE_PRIO_QUEUE0, SVC_TASK_MS2TICKS(_svc_wdt_task_interval_ms)) ;
}

static uint32_t
svc_wdt_elapsed_ms (void)
{
    uint32_t now_ms = os_sys_timestamp () ;
    uint32_t elapsed_ms = now_ms - _svc_wdt_last_ms ;

    _svc_wdt_last_ms = now_ms ;
    return elapsed_ms ;
}

uint8_t
svc_wdt_process (SVC_WDT_TIMEOUTS_T id)
{
    int kick = 1 ;
    SVC_WDT_HANDLE_T* start ;

    for ( start = (SVC_WDT_HANDLE_T*)stack_head (&_svc_wdt_handler_stack[id]) ;
        (start!=NULL_LLO)
            ; ) {

        if (start->flags & SVC_WDT_FLAGS_ACTIVE) {

            if (start->flags & SVC_WDT_FLAGS_KICKED) {
                start->flags &= ~(SVC_WDT_FLAGS_KICKED|SVC_WDT_FLAGS_FLAGGED|SVC_WDT_FLAGS_REPORTED) ;

            } else {
                if (start->flags & SVC_WDT_FLAGS_FLAGGED) {

                    if (!(start->flags & SVC_WDT_FLAGS_REPORTED)) {
                        start->flags |= SVC_WDT_FLAGS_REPORTED ;
                        DBG_MESSAGE_SVC_WDT (DBG_MESSAGE_SEVERITY_WARNING,
                            "WDT   : : reported '%s' (0x%x)",
                            os_thread_get_name(&start->thread), start->id) ;
                    }
                    kick = 0 ;
#if 0
                    if (id == TIMEOUT_10_SEC) STATS_COUNTER_INC(wdt_10) ;
                    else if (id == TIMEOUT_30_SEC) STATS_COUNTER_INC(wdt_30) ;
                    else if (id == TIMEOUT_60_SEC) STATS_COUNTER_INC(wdt_60) ;
#endif
                } else {

                    start->flags |= SVC_WDT_FLAGS_FLAGGED ;
                    DBG_MESSAGE_SVC_WDT (DBG_MESSAGE_SEVERITY_LOG,
                            "WDT   : : flagging '%s' (0x%x)",
                            os_thread_get_name(&start->thread), start->id) ;

                }

            }

        }

        start = (SVC_WDT_HANDLE_T*)stack_next ((plists_t)start, OFFSETOF(SVC_WDT_HANDLE_T, next));

    }

    return kick ;
}

static uint8_t
svc_wdt_bucket_reported (SVC_WDT_TIMEOUTS_T id)
{
    SVC_WDT_HANDLE_T* start ;

    for ( start = (SVC_WDT_HANDLE_T*)stack_head (&_svc_wdt_handler_stack[id]) ;
        (start!=NULL_LLO)
            ; ) {

        if ((start->flags & SVC_WDT_FLAGS_ACTIVE) &&
                (start->flags & SVC_WDT_FLAGS_REPORTED)) {
            return 1 ;
        }

        start = (SVC_WDT_HANDLE_T*)stack_next ((plists_t)start, OFFSETOF(SVC_WDT_HANDLE_T, next));
    }

    return 0 ;
}

static uint8_t
svc_wdt_process_bucket (SVC_WDT_TIMEOUTS_T id, uint32_t *elapsed_ms)
{
    uint32_t interval = svc_wdt_check_interval_ms (id) ;
    uint8_t due = (*elapsed_ms >= interval) ;

    if (due) {
        *elapsed_ms %= interval ;
        return svc_wdt_process (id) ;
    }

    return !svc_wdt_bucket_reported (id) ;
}

static void
svc_wdt_task_cb (SVC_TASKS_T *task, uintptr_t parm, uint32_t reason)
{
    if (reason == SERVICE_CALLBACK_REASON_RUN) {
        uint32_t elapsed_ms ;
        uint8_t kick ;
        
        os_mutex_lock (&_svc_wdt_mutex) ;
        elapsed_ms = svc_wdt_elapsed_ms () ;
        _svc_wdt_hw_elapsed_ms += elapsed_ms ;
        _svc_wdt_10_elapsed_ms += elapsed_ms ;
        _svc_wdt_30_elapsed_ms += elapsed_ms ;
        _svc_wdt_60_elapsed_ms += elapsed_ms ;

        kick = svc_wdt_process_bucket (TIMEOUT_10_SEC, &_svc_wdt_10_elapsed_ms) ;
        kick &= svc_wdt_process_bucket (TIMEOUT_30_SEC, &_svc_wdt_30_elapsed_ms) ;
        kick &= svc_wdt_process_bucket (TIMEOUT_60_SEC, &_svc_wdt_60_elapsed_ms) ;

        if (_svc_wdt_hw_elapsed_ms >= _svc_wdt_hw_kick_interval_ms) {
            if (kick) {
                svc_wdt_set_hw_timeout (qoraal_wdt_kick ()) ;
                DBG_MESSAGE_SVC_WDT (DBG_MESSAGE_SEVERITY_INFO,
                        "WDT   : : task kick");

            } else {
                DBG_MESSAGE_SVC_WDT (DBG_MESSAGE_SEVERITY_INFO,
                        "WDT   : : task NOT kicked");

            }
        }
        os_mutex_unlock (&_svc_wdt_mutex) ;

        (void)svc_wdt_schedule_next () ;

    }

    svc_tasks_complete (&_svc_wdt_task) ;

}
#endif

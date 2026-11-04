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
#include "qoraal/svc/svc_events.h"
#include "qoraal/svc/svc_wdt.h"
#include "qoraal/common/lists.h"

#if CFG_OS_ZEPHYR && CFG_SVC_EVENTS_SYSWORKQ
#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#endif

static stack_t                          _svc_events_handler_stack[SVC_EVENTS_LAST] ;

#if !CFG_OS_OS_LESS

#if !defined CFG_OS_MUTEX_DISABLE
static OS_MUTEX_DECL            (_svc_events_mutex) ;
#endif
static OS_EVENT_DECL            (_svc_events_event) ;

#if CFG_OS_ZEPHYR && CFG_SVC_EVENTS_SYSWORKQ
static atomic_t                 _events_stop = ATOMIC_INIT(1) ;
#else
#define WORKING_THREAD_SIZE     (3072)
static p_thread_t               _svc_events_thread = 0 ;
static OS_THREAD_WORKING_AREA   (wa_svc_events_thread, WORKING_THREAD_SIZE);
static bool                     _events_stop = false ;
#endif

static bool
svc_events_stopped (void)
{
#if CFG_OS_ZEPHYR && CFG_SVC_EVENTS_SYSWORKQ
    return atomic_get (&_events_stop) != 0 ;
#else
    return _events_stop ;
#endif
}

static void
svc_events_dispatch (uint32_t events)
{
    uint32_t eid = 0 ;

#if !defined CFG_OS_MUTEX_DISABLE
    os_mutex_lock (&_svc_events_mutex) ;
#endif

    while (events && !svc_events_stopped ()) {
        if (events & (1U<<eid)) {
            events &= ~(1U<<eid) ;

            SVC_EVENTS_HANDLER_T* start ;
            for (start = (SVC_EVENTS_HANDLER_T*)stack_head (&_svc_events_handler_stack[eid]) ;
                    start != NULL_LLO ; ) {
                SVC_EVENTS_CALLBACK_T fp = start->fp ;
                void *ctx = start->ctx ;
                start = (SVC_EVENTS_HANDLER_T*)stack_next ((plists_t)start, OFFSETOF(SVC_EVENTS_HANDLER_T, next)) ;

#if !defined CFG_OS_MUTEX_DISABLE
                os_mutex_unlock (&_svc_events_mutex) ;
#endif
                fp (eid, ctx) ;
#if !defined CFG_OS_MUTEX_DISABLE
                os_mutex_lock (&_svc_events_mutex) ;
#endif
            }
        }
        eid++ ;
    }

#if !defined CFG_OS_MUTEX_DISABLE
    os_mutex_unlock (&_svc_events_mutex) ;
#endif
}

#if CFG_OS_ZEPHYR && CFG_SVC_EVENTS_SYSWORKQ
static void
svc_events_work (struct k_work *work)
{
    (void)work ;
    if (svc_events_stopped ()) return ;

    /* Consume one snapshot; signals during dispatch queue another invocation. */
    uint32_t events = os_event_wait_timeout (&_svc_events_event, 1, ~0U, 0, OS_TIME_IMMEDIATE) ;
    if (!events) return ;

    /* The watchdog can retain this handle while reporting a timeout. */
    static SVC_WDT_HANDLE_T hwdt ;
    svc_wdt_register (&hwdt, TIMEOUT_10_SEC) ;
    svc_wdt_activate (&hwdt) ;
    svc_events_dispatch (events) ;
    svc_wdt_unregister (&hwdt, TIMEOUT_10_SEC) ;
}

static K_WORK_DEFINE (_svc_events_work, svc_events_work) ;

static int32_t
svc_events_submit (void)
{
    if (!svc_events_stopped ()) {
        int res = k_work_submit (&_svc_events_work) ;
        __ASSERT (res >= 0, "svc-events work submission failed: %d", res) ;
        if (res < 0) return E_UNEXP ;
    }
    return EOK ;
}
#else
static void
svc_events_thread (void *arg)
{
    SVC_WDT_HANDLE_T hwdt ;
    svc_wdt_register (&hwdt, TIMEOUT_10_SEC) ;
    while (!_events_stop) {
        svc_wdt_deactivate (&hwdt) ;
        uint32_t events = os_event_wait (&_svc_events_event, 1, ~0, 0) ;
        svc_wdt_activate (&hwdt) ;
        svc_events_dispatch (events) ;
    }
    svc_wdt_unregister (&hwdt, TIMEOUT_10_SEC) ;
}
#endif

#else

static uint32_t                         _svc_events_event ;

void
svc_events_handle (void)
{
    uint32_t events  ;

    osalSysLock () ;
    events = _svc_events_event ;
    _svc_events_event = 0 ;
    osalSysUnlock () ;
    
    uint32_t eid = 0 ;

    while (events) {
    if (events & (1<<eid)) {

        events &= ~(1<<eid);

        SVC_EVENTS_HANDLER_T* start ;

        for ( start = (SVC_EVENTS_HANDLER_T*)stack_head (&_svc_events_handler_stack[eid]) ;
            (start!=NULL_LLO)
                ; ) {

            start->fp (eid, start->ctx) ;
            start = (SVC_EVENTS_HANDLER_T*)stack_next ((plists_t)start, OFFSETOF(SVC_EVENTS_HANDLER_T, next));

        }



        STATS_COUNTER_INC(event_cnt) ;
    }
    eid++;

    }

}
#endif /* CFG_OS_OS_LESS */

/**
 * @brief   Initializes events.
 *
 * @svc
 */
int32_t
svc_events_init (void)
{
    int i ;
#if !defined CFG_OS_MUTEX_DISABLE
    os_mutex_init (&_svc_events_mutex) ;
#endif

    for (i=0; i<SVC_EVENTS_LAST; i++) {
        stack_init (&_svc_events_handler_stack[i]) ;
    }

    os_event_init (&_svc_events_event) ;
#if CFG_OS_ZEPHYR && CFG_SVC_EVENTS_SYSWORKQ
    atomic_set (&_events_stop, 1) ;
#endif

    return EOK ;
}

/**
 * @brief   Start the events.
 *
 *
 * @svc
 */
int32_t
svc_events_start (void)
{
#if CFG_OS_ZEPHYR && CFG_SVC_EVENTS_SYSWORKQ
    atomic_clear (&_events_stop) ;
    return svc_events_submit () ;
#else
    _events_stop = false ;
#if !CFG_OS_OS_LESS
    return os_thread_create_static (wa_svc_events_thread, sizeof(wa_svc_events_thread),
            CFG_SVC_EVENTS_THREAD_PRIO, svc_events_thread,
            0, &_svc_events_thread, "svc-events") ;
#endif
    return EOK ;
#endif
}

/**
 * @brief   Stop the events asynchronously.
 *
 *
 * @svc
 */
void
svc_events_stop (void)
{
#if CFG_OS_ZEPHYR && CFG_SVC_EVENTS_SYSWORKQ
    /* Queued work leaves pending bits buffered until the next start. */
    atomic_set (&_events_stop, 1) ;
#else
    if (!_events_stop) {
        _events_stop = true ;
        svc_events_signal (0) ;
    }
#endif
}

/**
 * @brief   Register an event listener.
 *
 * @param[in] id            Event identifier
 * @param[in] handler       User allocated entry for the event list
 * @param[in] fp            Callback handler
 *
 * @return              Error.
 *
 * @svc
 */
void
svc_events_register (SVC_EVENTS_T id, SVC_EVENTS_HANDLER_T * handler, SVC_EVENTS_CALLBACK_T fp, void * ctx)
{
#if !defined CFG_OS_MUTEX_DISABLE
    os_mutex_lock (&_svc_events_mutex) ;
#else
    osalSysLock () ;
#endif

    stack_remove (&_svc_events_handler_stack[id], (plists_t)handler, OFFSETOF(SVC_EVENTS_HANDLER_T, next)) ;
    handler->fp = fp ;
    handler->ctx = ctx ;
    stack_add_head (&_svc_events_handler_stack[id], handler, OFFSETOF(SVC_EVENTS_HANDLER_T, next)) ;

#if !defined CFG_OS_MUTEX_DISABLE
    os_mutex_unlock (&_svc_events_mutex) ;
#else
    osalSysUnlock () ;
#endif
    return  ;
}

/**
 * @brief   Remove an event listener.
 *
 * @param[in] id            Event identifier
 * @param[in] handler       registered event handler
 *
 * @svc
 */
void 
svc_events_unregister (SVC_EVENTS_T id,  SVC_EVENTS_HANDLER_T * handler)
{
#if !defined CFG_OS_MUTEX_DISABLE
    os_mutex_lock (&_svc_events_mutex) ;
#else
    osalSysLock () ;
#endif

    stack_remove (&_svc_events_handler_stack[id], (plists_t)handler, OFFSETOF(SVC_EVENTS_HANDLER_T, next)) ;

#if !defined CFG_OS_MUTEX_DISABLE
    os_mutex_unlock (&_svc_events_mutex) ;
#else
    osalSysUnlock () ;
#endif
}

/**
 * @brief   Signal an event from an interrupt context.
 *
 * @param[in] id            Event identifier
 *
 * @svc
 */
void
svc_events_signal_isr (SVC_EVENTS_T id)
{
#if !CFG_OS_OS_LESS
    os_event_signal_isr (&_svc_events_event, (1<<id)) ;
#if CFG_OS_ZEPHYR && CFG_SVC_EVENTS_SYSWORKQ
    (void)svc_events_submit () ;
#endif
#else
    osalSysLockFromISR () ;
    _svc_events_event |= (1<<id);
    osalSysUnlockFromISR () ;
#endif
}

/**
 * @brief   Signal an event.
 *
 * @param[in] id            Event identifier
 *
 * @svc
 */
void
svc_events_signal (SVC_EVENTS_T id)
{
#if !CFG_OS_OS_LESS    
    os_event_signal (&_svc_events_event, (1<<id)) ;
#if CFG_OS_ZEPHYR && CFG_SVC_EVENTS_SYSWORKQ
    (void)svc_events_submit () ;
#endif
#else
    osalSysLock () ;
    _svc_events_event |= (1<<id);
    osalSysUnlock () ;
#endif
}




/*
 *  Copyright (C) 2015-2025, Navaro, All Rights Reserved
 *  SPDX-License-Identifier: MIT
 *
 *  This file is part of CORAL Connect (https://navaro.nl)
 */

/*
 * Host test for svc_tasks cancel, including tasks that re-arm themselves
 * from their own callback, the pattern periodic tasks use. It runs on the
 * POSIX port and exits non-zero when a check fails.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include "qoraal/qoraal.h"
#include "qoraal/svc/svc_tasks.h"

static int _checks ;
static int _failures ;

#define CHECK(cond, ...)  do {                                          \
        _checks++ ;                                                     \
        if (!(cond)) {                                                  \
            _failures++ ;                                               \
            printf ("FAIL %s:%d: ", __FILE__, __LINE__) ;               \
            printf (__VA_ARGS__) ;                                      \
            printf ("\n") ;                                             \
        }                                                               \
    } while (0)

static void *   test_malloc (QORAAL_HEAP heap, size_t size) { (void)heap ; return malloc (size) ; }
static void     test_free (QORAAL_HEAP heap, void *mem) { (void)heap ; free (mem) ; }
static void     test_print (const char *s) { (void)s ; }
static int32_t  test_getch (uint32_t timeout) { os_thread_sleep (timeout) ; return EOF ; }
static void     test_assert (const char *s) { printf ("ASSERT: %s\n", s) ; _failures++ ; }
static uint32_t test_time (void) { return 0 ; }
static uint32_t test_rand (void) { return (uint32_t)rand () ; }
static uint32_t test_wdt_kick (void) { return 0 ; }

static const QORAAL_CFG_T _cfg = {
    .malloc = test_malloc, .free = test_free, .print = test_print,
    .getch = test_getch, .debug_assert = test_assert,
    .current_time = test_time, .rand = test_rand, .wdt_kick = test_wdt_kick
} ;

#define LATER       SVC_TASK_MS2TICKS(10000)

/*
 * A task whose RUN callback can re-arm the task before or after its work.
 * The task is the first member, so the callback can find the rest.
 */
typedef struct {
    SVC_TASKS_T         task ;
    uint32_t            rearm_ticks ;   /* 0 re-arms on the ready list */
    int                 rearm_first ;   /* re-arm before the work, not after */
    int                 rearms ;        /* how many runs re-arm */
    int                 cancel_self ;   /* cancel the re-arm from RUN */
    uint32_t            work_ms ;
    volatile int        runs ;
    volatile int        in_run ;
    volatile int        canceled ;
    volatile int        canceled_in_run ;
    volatile int        active_in_run ;
    volatile int32_t    self_cancel_res ;
    volatile uintptr_t  cancel_parm ;
} TEST_TASK_T ;

static void test_task_cb (SVC_TASKS_T * task, uintptr_t parm, uint32_t reason) ;

static void
rearm (TEST_TASK_T * t, uintptr_t parm)
{
    if (t->rearms-- > 0) {
        svc_tasks_schedule (&t->task, test_task_cb, parm, SERVICE_PRIO_QUEUE1, t->rearm_ticks) ;
    }
}

static void
wait_idle (TEST_TASK_T * t)
{
    for (int i = 0 ; i < 200 && svc_tasks_is_active (&t->task) ; i++) os_thread_sleep (5) ;
}

static void
test_task_cb (SVC_TASKS_T * task, uintptr_t parm, uint32_t reason)
{
    TEST_TASK_T * t = (TEST_TASK_T *)task ;

    if (reason != SERVICE_CALLBACK_REASON_RUN) {
        if (t->in_run) t->canceled_in_run++ ;
        t->canceled++ ;
        t->cancel_parm = parm ;
        svc_tasks_complete (task) ;
        return ;
    }

    t->runs++ ;
    t->in_run = 1 ;
    if (t->rearm_first) rearm (t, parm) ;
    t->active_in_run = (int)svc_tasks_is_active (task) ;
    if (t->cancel_self) t->self_cancel_res = svc_tasks_cancel (task) ;
    os_thread_sleep (t->work_ms) ;
    if (!t->rearm_first) rearm (t, parm) ;
    t->in_run = 0 ;
    svc_tasks_complete (task) ;
}

static void
test_task_init (TEST_TASK_T * t, uint32_t rearm_ticks, int rearm_first, uint32_t work_ms)
{
    memset (t, 0, sizeof (*t)) ;
    svc_tasks_init_task (&t->task) ;
    t->rearm_ticks = rearm_ticks ;
    t->rearm_first = rearm_first ;
    t->rearms = 1 ;
    t->work_ms = work_ms ;
}

static void
start_run (TEST_TASK_T * t, uintptr_t parm)
{
    svc_tasks_schedule (&t->task, test_task_cb, parm, SERVICE_PRIO_QUEUE1, 0) ;
    for (int i = 0 ; i < 200 && !t->in_run ; i++) os_thread_sleep (5) ;
}

static uintptr_t
test_parm (void)
{
    return sizeof (uintptr_t) > 4 ? (uintptr_t)0x123456789abcdef0ULL : (uintptr_t)0x9abcdef0UL ;
}

/* A queued task that is not running is cancelled at once. */
static void
test_cancel_queued (void)
{
    static TEST_TASK_T t ;
    test_task_init (&t, 0, 0, 0) ;

    svc_tasks_schedule (&t.task, test_task_cb, test_parm (), SERVICE_PRIO_QUEUE1, LATER) ;
    int32_t res = svc_tasks_cancel (&t.task) ;

    CHECK (res == EOK, "queued: cancel returned %d", (int)res) ;
    CHECK (t.canceled == 1, "queued: CANCELED ran %d times", t.canceled) ;
    CHECK (t.cancel_parm == test_parm (), "queued: CANCELED got parm 0x%llx",
            (unsigned long long)t.cancel_parm) ;
    CHECK (t.runs == 0, "queued: RUN ran %d times", t.runs) ;
}

/*
 * The task re-arms itself and keeps working. cancel_wait from another thread
 * returns only once RUN is done. The re-armed instance never runs, and no
 * CANCELED callback runs while RUN does.
 */
static void
test_cancel_wait_rearmed (TEST_TASK_T * t, const char * name, uint32_t rearm_ticks, int rearm_first)
{
    test_task_init (t, rearm_ticks, rearm_first, 300) ;

    start_run (t, test_parm ()) ;
    CHECK (t->in_run, "%s: RUN did not start", name) ;

    int32_t res = svc_tasks_cancel_wait (&t->task, SVC_TASK_MS2TICKS(2000)) ;
    int in_run = t->in_run ;

    CHECK (res == EOK, "%s: cancel_wait returned %d", name, (int)res) ;
    CHECK (!in_run, "%s: cancel_wait returned while RUN was still running", name) ;
    CHECK (t->canceled_in_run == 0, "%s: CANCELED ran while RUN was running", name) ;
    CHECK (svc_tasks_status (&t->task) == SERVICE_STATUS_COMPLETE, "%s: task still queued", name) ;
    CHECK (!svc_tasks_is_active (&t->task), "%s: task still active", name) ;
    if (rearm_first) {
        CHECK (t->active_in_run, "%s: is_active was 0 in RUN after re-arming", name) ;
    }

    os_thread_sleep (100) ;
    CHECK (t->runs == 1, "%s: RUN ran %d times", name, t->runs) ;
}

/* A cancel of a re-armed task during RUN, with no time to wait, removes the
 * re-arm and reports busy. */
static void
test_cancel_rearmed_busy (void)
{
    static TEST_TASK_T t ;
    test_task_init (&t, LATER, 1, 200) ;

    start_run (&t, 0) ;
    int32_t res = svc_tasks_cancel_wait (&t.task, 0) ;

    CHECK (res == E_BUSY, "busy: cancel_wait with no timeout returned %d", (int)res) ;
    CHECK (svc_tasks_status (&t.task) == SERVICE_STATUS_COMPLETE, "busy: task still queued") ;
    wait_idle (&t) ;
    CHECK (svc_tasks_cancel (&t.task) == EOK, "busy: cancel after RUN not EOK") ;
    CHECK (t.canceled == 0 && t.runs == 1, "busy: CANCELED %d, RUN %d", t.canceled, t.runs) ;
}

/* The callback cancels its own re-arm, as a callback that reschedules does. */
static void
test_cancel_self (void)
{
    static TEST_TASK_T t ;
    test_task_init (&t, LATER, 1, 0) ;
    t.cancel_self = 1 ;

    start_run (&t, 0) ;
    wait_idle (&t) ;
    os_thread_sleep (50) ;

    CHECK (t.self_cancel_res == E_BUSY, "self: cancel in RUN returned %d", (int)t.self_cancel_res) ;
    CHECK (svc_tasks_status (&t.task) == SERVICE_STATUS_COMPLETE, "self: task still queued") ;
    CHECK (t.canceled == 0 && t.runs == 1, "self: CANCELED %d, RUN %d", t.canceled, t.runs) ;
}

/* cancel_wait on a long RUN that does not re-arm still times out. */
static void
test_cancel_wait_timeout (void)
{
    static TEST_TASK_T t ;
    test_task_init (&t, 0, 0, 400) ;
    t.rearms = 0 ;

    start_run (&t, 0) ;
    int32_t res = svc_tasks_cancel_wait (&t.task, SVC_TASK_MS2TICKS(100)) ;
    CHECK (res == E_TIMEOUT, "timeout: cancel_wait returned %d", (int)res) ;

    res = svc_tasks_cancel_wait (&t.task, SVC_TASK_MS2TICKS(2000)) ;
    CHECK (res == EOK, "timeout: second cancel_wait returned %d", (int)res) ;
    CHECK (!t.in_run && !t.canceled, "timeout: in RUN %d, CANCELED %d", t.in_run, t.canceled) ;
}

int
main (void)
{
    setvbuf (stdout, 0, _IONBF, 0) ;

    if (qoraal_init_default (&_cfg, 0) != EOK) {
        printf ("FAIL qoraal_init_default\n") ;
        return 1 ;
    }
    os_sys_start () ;
    qoraal_start_default () ;
    os_thread_sleep (100) ;

    /* Each case has its own task, so a RUN left over from a failing case
     * cannot change the next one. */
    static TEST_TASK_T timer, ready, late ;

    test_cancel_queued () ;
    test_cancel_wait_rearmed (&timer, "timer", LATER, 1) ;
    test_cancel_wait_rearmed (&ready, "ready", 0, 1) ;
    test_cancel_wait_rearmed (&late, "late", LATER, 0) ;
    test_cancel_rearmed_busy () ;
    test_cancel_self () ;
    test_cancel_wait_timeout () ;

    printf ("%d checks, %d failed\n", _checks, _failures) ;
    fflush (stdout) ;
    _exit (_failures ? 1 : 0) ;
}

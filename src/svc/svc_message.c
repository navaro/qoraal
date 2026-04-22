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

#include <string.h>
#include "qoraal/config.h"
#include "qoraal/qoraal.h"
#include "qoraal/common/lists.h"
#include "qoraal/common/rtclib.h"
#include "qoraal/os.h"
#include "qoraal/svc/svc_message.h"

static SVC_MESSAGE_FILTER_T _message_filter = {0} ;
static SVC_TASK_PRIO_T      _message_task_prio ;
static uint32_t             _message_id = 0 ;
static int32_t              _message_sending = 0 ;

static LISTS_LINKED_DECL    (_message_channels) ;
static OS_MUTEX_DECL        (_message_mutex) ;

static void
message_channel_available (void)
{
    SVC_MESSAGE_MASK_T mask = 0 ;
    SVC_MESSAGE_CHANNEL_T * start ;

    for ( start = (SVC_MESSAGE_CHANNEL_T*)linked_head (&_message_channels) ;
            (start != NULL_LLO) ;
            start = (SVC_MESSAGE_CHANNEL_T*)linked_next ((plists_t)start, OFFSETOF(SVC_MESSAGE_CHANNEL_T, next)) ) {
        int i ;

        for (i = 0 ; i < SVC_MESSAGE_FILTER_CNT ; i++) {
            mask |= start->filter[i].mask ;
        }
    }

    _message_filter.mask = mask ;
}

static uint32_t
message_channel_matches (const SVC_MESSAGE_CHANNEL_T * channel, int32_t module)
{
    SVC_MESSAGE_MASK_T mask = SVC_MESSAGE_MODULE_MASK(module) ;
    int i ;

    if (!channel || !channel->fp || !mask) {
        return 0 ;
    }

    for (i = 0 ; i < SVC_MESSAGE_FILTER_CNT ; i++) {
        if (channel->filter[i].mask & mask) {
            return 1 ;
        }
    }

    return 0 ;
}

static void
message_task_callback (SVC_TASKS_T * task, uintptr_t parm, uint32_t reason)
{
    SVC_MESSAGE_T * message = (SVC_MESSAGE_T*) task ;

    (void)parm ;

    if (reason == SERVICE_CALLBACK_REASON_RUN) {
        SVC_MESSAGE_CHANNEL_T * start ;

        os_mutex_lock (&_message_mutex) ;
        for ( start = (SVC_MESSAGE_CHANNEL_T*)linked_head (&_message_channels) ;
                (start != NULL_LLO) ;
                start = (SVC_MESSAGE_CHANNEL_T*)linked_next ((plists_t)start, OFFSETOF(SVC_MESSAGE_CHANNEL_T, next)) ) {
            if (message_channel_matches (start, message->module)) {
                start->fp (start, message) ;
            }
        }
        os_mutex_unlock (&_message_mutex) ;
    }

    os_sys_lock() ;
    _message_sending-- ;
    os_sys_unlock() ;

    svc_tasks_complete (task) ;
    qoraal_free (QORAAL_HeapAuxiliary, message) ;
}

int32_t
svc_message_init (SVC_TASK_PRIO_T prio)
{
    os_mutex_init (&_message_mutex) ;
    linked_init (&_message_channels) ;
    _message_filter.mask = 0 ;
    _message_task_prio = prio ;
    _message_id = 0 ;
    _message_sending = 0 ;

    return EOK ;
}

int32_t
svc_message_start (void)
{
    return EOK ;
}

uint32_t
svc_message_would_post (int32_t module)
{
    SVC_MESSAGE_MASK_T mask = SVC_MESSAGE_MODULE_MASK(module) ;

    return (mask != 0) && ((_message_filter.mask & mask) != 0) ;
}

SVC_MESSAGE_T *
svc_message_create (uint32_t size)
{
    SVC_MESSAGE_T * message ;
    uint32_t now = qoraal_current_time() ;

    message = (SVC_MESSAGE_T*)qoraal_malloc (QORAAL_HeapAuxiliary, sizeof(SVC_MESSAGE_T) + size) ;
    if (!message) {
        return 0 ;
    }

    memset (message, 0, sizeof(SVC_MESSAGE_T) + size) ;
    svc_tasks_init_task (&message->task) ;

    message->id = _message_id++ ;
    message->module = SVC_SERVICES_INVALID ;
    message->size = size ;
    message->timestamp_ms = os_sys_timestamp() ;
    rtc_localtime (now, &message->date, &message->time) ;

    return message ;
}

int32_t
svc_message_post (SVC_MESSAGE_T * message)
{
    int32_t status ;

    if (!message) {
        return E_PARM ;
    }

    if (!svc_message_would_post (message->module)) {
        qoraal_free (QORAAL_HeapAuxiliary, message) ;
        return EOK ;
    }

    if (_message_sending >= SVC_MESSAGE_MAX_QUEUE_SIZE) {
        qoraal_free (QORAAL_HeapAuxiliary, message) ;
        return E_TIMEOUT ;
    }

    status = svc_tasks_schedule (&message->task, message_task_callback, 0, _message_task_prio, 0) ;
    if (status != EOK) {
        qoraal_free (QORAAL_HeapAuxiliary, message) ;
        return status ;
    }

    os_sys_lock() ;
    _message_sending++ ;
    os_sys_unlock() ;

    return EOK ;
}

void
svc_message_channel_add (SVC_MESSAGE_CHANNEL_T * channel)
{
    os_mutex_lock (&_message_mutex) ;
    linked_add_tail (&_message_channels, channel, OFFSETOF(SVC_MESSAGE_CHANNEL_T, next)) ;
    message_channel_available () ;
    os_mutex_unlock (&_message_mutex) ;
}

void
svc_message_channel_remove (SVC_MESSAGE_CHANNEL_T * channel)
{
    os_mutex_lock (&_message_mutex) ;
    linked_remove (&_message_channels, channel, OFFSETOF(SVC_MESSAGE_CHANNEL_T, next)) ;
    message_channel_available () ;
    os_mutex_unlock (&_message_mutex) ;
}

int32_t
svc_message_wait (uint32_t timeout)
{
    int32_t res = EOK ;

    while (_message_sending >= (SVC_MESSAGE_MAX_QUEUE_SIZE - 1)) {
        if ((res = svc_tasks_wait_queue (_message_task_prio, timeout)) == E_TIMEOUT) {
            break ;
        }
    }

    return res ;
}

int32_t
svc_message_wait_all (uint32_t timeout)
{
    while (_message_sending > 0) {
        if (timeout <= SVC_TASK_MS2TICKS(10)) {
            break ;
        }

        os_thread_sleep (10) ;
        timeout -= SVC_TASK_MS2TICKS(10) ;
    }

    return _message_sending ? EFAIL : EOK ;
}

SVC_MESSAGE_FILTER_T
svc_message_get_filter (void)
{
    return _message_filter ;
}

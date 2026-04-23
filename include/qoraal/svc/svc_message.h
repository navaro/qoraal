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

#ifndef __SVC_MESSAGE_H__
#define __SVC_MESSAGE_H__

#include <stdint.h>
#include "qoraal/common/rtclib.h"
#include "qoraal/svc/svc_services.h"
#include "qoraal/svc/svc_tasks.h"

#define DBG_MESSAGE_SVC_MESSAGE(severity, fmt_str, ...)    DBG_MESSAGE_T_REPORT (SVC_LOGGER_TYPE(severity,0), 0, fmt_str, ##__VA_ARGS__)
#define DBG_ASSERT_SVC_MESSAGE                             DBG_ASSERT_T

#ifndef SVC_MESSAGE_MAX_QUEUE_SIZE
#define SVC_MESSAGE_MAX_QUEUE_SIZE                    32
#endif

#define SVC_MESSAGE_FILTER_CNT                        2

typedef uint64_t    SVC_MESSAGE_MASK_T ;

#define SVC_MESSAGE_MASK                              ((SVC_MESSAGE_MASK_T)-1)
#define SVC_MESSAGE_MODULE_MASK(module_id)            \
                        ((((int32_t)(module_id)) >= 0 && ((int32_t)(module_id)) < 64) ?   \
                        ((SVC_MESSAGE_MASK_T)1ULL << (uint32_t)(module_id)) : 0ULL)

struct SVC_MESSAGE_S;
typedef struct SVC_MESSAGE_S SVC_MESSAGE_T ;

typedef void (*SVC_MESSAGE_CHANNEL_FP)(void * channel, const SVC_MESSAGE_T * message) ;

typedef struct SVC_MESSAGE_FILTER_S {
    SVC_MESSAGE_MASK_T        mask ;
} SVC_MESSAGE_FILTER_T ;

typedef struct SVC_MESSAGE_CHANNEL_S {
    struct SVC_MESSAGE_CHANNEL_S * next ;
    SVC_MESSAGE_CHANNEL_FP         fp ;
    SVC_MESSAGE_FILTER_T           filter[SVC_MESSAGE_FILTER_CNT] ;
    void *                       user ;
} SVC_MESSAGE_CHANNEL_T ;

struct SVC_MESSAGE_S {
    SVC_TASKS_T              task ;
    uint32_t                 id ;
    uint32_t                 type ;
    int32_t                  module ;
    uint32_t                 size ;
    uint64_t                 timestamp_ms ;
    RTCLIB_DATE_T            date ;
    RTCLIB_TIME_T            time ;
    uint8_t                  payload[] ;
} ;

#define SVC_MESSAGE_DATA(message)                       ((void *)((message)->payload))
#define SVC_MESSAGE_CONST_DATA(message)                 ((const void *)((message)->payload))

#ifdef __cplusplus
extern "C" {
#endif

    extern int32_t          svc_message_init (SVC_TASK_PRIO_T prio) ;
    extern int32_t          svc_message_start (void) ;

    extern uint32_t         svc_message_would_post (int32_t module) ;
    extern SVC_MESSAGE_T *  svc_message_create (uint32_t size, uint32_t type, int32_t module) ;
    extern int32_t          svc_message_post (SVC_MESSAGE_T * message) ;

    extern void             svc_message_channel_add (SVC_MESSAGE_CHANNEL_T * channel) ;
    extern void             svc_message_channel_remove (SVC_MESSAGE_CHANNEL_T * channel) ;

    extern int32_t          svc_message_wait (uint32_t timeout) ;
    extern int32_t          svc_message_wait_all (uint32_t timeout) ;

    extern SVC_MESSAGE_FILTER_T svc_message_get_filter (void) ;

#ifdef __cplusplus
}
#endif

#endif /* __SVC_MESSAGE_H__ */

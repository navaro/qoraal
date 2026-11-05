#include <string.h>
#include "qoraal/config.h"
#include "qoraal/qoraal.h"
#include "qoraal/common/lists.h"
#include "qoraal/common/rtclib.h"
#include "qoraal/os.h"
#include "qoraal/svc/svc_message.h"

static SVC_TASK_PRIO_T      _message_task_prio;
static int32_t              _message_sending = 0;

static LISTS_LINKED_DECL    (_message_channels);
static OS_MUTEX_DECL        (_message_mutex);

/*
 * _message_mutex guards the channel list only. It is not held while a
 * channel callback runs, so a slow channel (a socket write, a flash erase)
 * does not hold up posters or channel add/remove. All messages are
 * delivered by the one svc_tasks queue set in svc_message_init(), so at
 * most one callback runs at a time. Two pointers, read and written under
 * the mutex, keep the list walk safe while it is unlocked:
 *   _message_current  the channel whose callback is running. Removing it
 *                     waits for the callback to return, so a channel that
 *                     lives on its owner's stack is never used after the
 *                     owner removed it.
 *   _message_next     where the walk continues. Removing that channel moves
 *                     it on to the channel after it.
 */
static SVC_MESSAGE_CHANNEL_T *  _message_current = 0;
static SVC_MESSAGE_CHANNEL_T *  _message_next = 0;
static p_thread_t               _message_dispatcher = 0;

#define MESSAGE_REMOVE_POLL_MS      10

static uint32_t
message_channel_matches(const SVC_MESSAGE_CHANNEL_T *channel, SVC_MESSAGE_MASK_T route)
{
    if (!channel || !channel->fp || !route) {
        return 0;
    }

    return svc_message_filter_match(&channel->filter, route);
}

static void
message_task_callback(SVC_TASKS_T *task, uintptr_t parm, uint32_t reason)
{
    SVC_MESSAGE_T *message = (SVC_MESSAGE_T *)task;
    SVC_MESSAGE_MASK_T route;

    (void)parm;

    if (reason == SERVICE_CALLBACK_REASON_RUN) {
        SVC_MESSAGE_CHANNEL_T *start;

        route = svc_message_route(message);

        os_mutex_lock(&_message_mutex);
        _message_dispatcher = os_thread_current();

        start = (SVC_MESSAGE_CHANNEL_T *)linked_head(&_message_channels);
        while (start != NULL_LLO) {
            _message_next = (SVC_MESSAGE_CHANNEL_T *)linked_next((plists_t)start,
                                OFFSETOF(SVC_MESSAGE_CHANNEL_T, next));

            if (message_channel_matches(start, route)) {
                _message_current = start;
                os_mutex_unlock(&_message_mutex);

                start->fp(start->user, message);

                os_mutex_lock(&_message_mutex);
                _message_current = 0;
            }

            /* svc_message_channel_remove() keeps _message_next valid. */
            start = _message_next;
        }

        _message_next = 0;
        _message_dispatcher = 0;
        os_mutex_unlock(&_message_mutex);
    }

    os_sys_lock();
    _message_sending--;
    os_sys_unlock();

    svc_tasks_complete(task);
    qoraal_free(QORAAL_HeapAuxiliary, message);
}

int32_t
svc_message_init(SVC_TASK_PRIO_T prio)
{
    os_mutex_init(&_message_mutex);
    linked_init(&_message_channels);

    _message_task_prio = prio;
    _message_sending = 0;

    return EOK;
}

int32_t
svc_message_start(void)
{
    return EOK;
}

uint32_t
svc_message_would_post_route(SVC_MESSAGE_MASK_T route)
{
    SVC_MESSAGE_CHANNEL_T *start;
    uint32_t would_post = 0;

    if (!route) {
        return 0;
    }

    os_mutex_lock(&_message_mutex);
    for (start = (SVC_MESSAGE_CHANNEL_T *)linked_head(&_message_channels);
         start != NULL_LLO;
         start = (SVC_MESSAGE_CHANNEL_T *)linked_next((plists_t)start, OFFSETOF(SVC_MESSAGE_CHANNEL_T, next))) {

        if (message_channel_matches(start, route)) {
            would_post = 1;
            break;
        }
    }
    os_mutex_unlock(&_message_mutex);

    return would_post;
}

SVC_MESSAGE_T *
svc_message_create(uint16_t size, int16_t module, uint32_t flags)
{
    SVC_MESSAGE_T *message;

    message = (SVC_MESSAGE_T *)qoraal_malloc(
        QORAAL_HeapAuxiliary,
        sizeof(SVC_MESSAGE_T) + size
    );

    if (!message) {
        return 0;
    }

    memset(message, 0, sizeof(SVC_MESSAGE_T) + size);
    svc_tasks_init_task(&message->task);

    message->module = module;
    message->flags = flags ? flags : SVC_MESSAGE_DEFAULT_FLAGS;
    message->size = size;

    return message;
}

int32_t
svc_message_post(SVC_MESSAGE_T *message)
{
    int32_t status;
    SVC_MESSAGE_MASK_T route;

    if (!message) {
        return E_PARM;
    }

    route = svc_message_route(message);

    if (!svc_message_would_post_route(route)) {
        qoraal_free(QORAAL_HeapAuxiliary, message);
        return EOK;
    }

    if (_message_sending >= SVC_MESSAGE_MAX_QUEUE_SIZE) {
        qoraal_free(QORAAL_HeapAuxiliary, message);
        return E_TIMEOUT;
    }

    status = svc_tasks_schedule(
        &message->task,
        message_task_callback,
        0,
        _message_task_prio,
        0
    );

    if (status != EOK) {
        qoraal_free(QORAAL_HeapAuxiliary, message);
        return status;
    }

    os_sys_lock();
    _message_sending++;
    os_sys_unlock();

    return EOK;
}

void
svc_message_channel_add(SVC_MESSAGE_CHANNEL_T *channel)
{
    os_mutex_lock(&_message_mutex);
    linked_add_tail(&_message_channels, channel, OFFSETOF(SVC_MESSAGE_CHANNEL_T, next));
    os_mutex_unlock(&_message_mutex);
}

/**
 * @brief   Remove a channel.
 * @details If the channel's callback is running, this waits for it to
 *          return, so the caller may free the channel afterwards. A callback
 *          may remove its own channel; that does not wait.
 */
void
svc_message_channel_remove(SVC_MESSAGE_CHANNEL_T *channel)
{
    os_mutex_lock(&_message_mutex);

    while ((channel == _message_current) &&
           (_message_dispatcher != os_thread_current())) {
        os_mutex_unlock(&_message_mutex);
        os_thread_sleep(MESSAGE_REMOVE_POLL_MS);
        os_mutex_lock(&_message_mutex);
    }

    if (channel == _message_next) {
        _message_next = (SVC_MESSAGE_CHANNEL_T *)linked_next((plists_t)channel,
                            OFFSETOF(SVC_MESSAGE_CHANNEL_T, next));
    }

    linked_remove(&_message_channels, channel, OFFSETOF(SVC_MESSAGE_CHANNEL_T, next));
    os_mutex_unlock(&_message_mutex);
}

int32_t
svc_message_wait(uint32_t timeout)
{
    int32_t res = EOK;

    while (_message_sending >= (SVC_MESSAGE_MAX_QUEUE_SIZE - 1)) {
        if ((res = svc_tasks_wait_queue(_message_task_prio, timeout)) == E_TIMEOUT) {
            break;
        }
    }

    return res;
}

int32_t
svc_message_wait_all(uint32_t timeout)
{
    while (_message_sending > 0) {
        if (timeout <= SVC_TASK_MS2TICKS(10)) {
            break;
        }

        os_thread_sleep(10);
        timeout -= SVC_TASK_MS2TICKS(10);
    }

    return _message_sending ? EFAIL : EOK;
}
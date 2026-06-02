#include <string.h>
#include "qoraal/config.h"
#include "qoraal/qoraal.h"
#include "qoraal/common/lists.h"
#include "qoraal/common/rtclib.h"
#include "qoraal/os.h"
#include "qoraal/svc/svc_message.h"

static SVC_MESSAGE_FILTER_T _message_filter = {0};
static SVC_TASK_PRIO_T      _message_task_prio;
static int32_t              _message_sending = 0;

static LISTS_LINKED_DECL    (_message_channels);
static OS_MUTEX_DECL        (_message_mutex);

static void
message_channel_available(void)
{
    SVC_MESSAGE_CHANNEL_T *start;

    memset(&_message_filter, 0, sizeof(_message_filter));

    for (start = (SVC_MESSAGE_CHANNEL_T *)linked_head(&_message_channels);
         start != NULL_LLO;
         start = (SVC_MESSAGE_CHANNEL_T *)linked_next((plists_t)start, OFFSETOF(SVC_MESSAGE_CHANNEL_T, next))) {

        _message_filter.require |= start->filter.require;
        _message_filter.accept  |= start->filter.accept;
        _message_filter.reject  |= start->filter.reject;
    }
}

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

        for (start = (SVC_MESSAGE_CHANNEL_T *)linked_head(&_message_channels);
             start != NULL_LLO;
             start = (SVC_MESSAGE_CHANNEL_T *)linked_next((plists_t)start, OFFSETOF(SVC_MESSAGE_CHANNEL_T, next))) {

            if (message_channel_matches(start, route)) {
                start->fp(start->user, message);
            }
        }

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

    memset(&_message_filter, 0, sizeof(_message_filter));

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
    if (!route) {
        return 0;
    }

    return svc_message_filter_match(&_message_filter, route);
}

uint32_t
svc_message_would_post(int16_t module)
{
    return svc_message_would_post_route(
        SVC_MESSAGE_ROUTE(module, SVC_MESSAGE_DEFAULT_FLAGS)
    );
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
    message_channel_available();
    os_mutex_unlock(&_message_mutex);
}

void
svc_message_channel_remove(SVC_MESSAGE_CHANNEL_T *channel)
{
    os_mutex_lock(&_message_mutex);
    linked_remove(&_message_channels, channel, OFFSETOF(SVC_MESSAGE_CHANNEL_T, next));
    message_channel_available();
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

SVC_MESSAGE_FILTER_T
svc_message_get_filter(void)
{
    return _message_filter;
}
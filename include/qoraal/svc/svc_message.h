#ifndef __SVC_MESSAGE_H__
#define __SVC_MESSAGE_H__

#include <stdint.h>
#include "qoraal/common/rtclib.h"
#include "qoraal/svc/svc_services.h"
#include "qoraal/svc/svc_tasks.h"

#define DBG_MESSAGE_SVC_MESSAGE(severity, fmt_str, ...) \
    DBG_MESSAGE_T_REPORT(SVC_LOGGER_TYPE(severity, 0), 0, fmt_str, ##__VA_ARGS__)

#define DBG_ASSERT_SVC_MESSAGE DBG_ASSERT_T

#ifndef SVC_MESSAGE_MAX_QUEUE_SIZE
#define SVC_MESSAGE_MAX_QUEUE_SIZE 32
#endif

typedef uint64_t SVC_MESSAGE_MASK_T;

#define SVC_MESSAGE_MASK ((SVC_MESSAGE_MASK_T)-1)

/*
 * Route layout (SVC_MESSAGE_MASK_T, 64 bits):
 *
 * bits  0..15   message flags/class/severity/qos
 * bits 16..47   module/source bits
 * bits 48..63   reserved for future routing
 *
 * Message flags layout (SVC_MESSAGE_T::flags, 32 bits):
 *
 * bits  0..3    user/local flags       ┐
 * bits  4..7    class                  │ used for routing (SVC_MESSAGE_FLAGS_MASK)
 * bits  8..11   severity               │
 * bits 12..15   qos                    ┘
 * bits 16..31   application-defined    — stored in message, ignored by routing
 */

#define SVC_MESSAGE_FLAGS_BITS         16
#define SVC_MESSAGE_FLAGS_MASK         ((SVC_MESSAGE_MASK_T)((1ULL << SVC_MESSAGE_FLAGS_BITS) - 1ULL))

#define SVC_MESSAGE_APP_FLAGS_MASK     ((uint32_t)0xFFFF0000U)

#define SVC_MESSAGE_MODULE_BITS        32
#define SVC_MESSAGE_MODULE_SHIFT       SVC_MESSAGE_FLAGS_BITS

#define SVC_MESSAGE_MODULE_MASK(module_id)                                      \
    ((((int32_t)(module_id)) >= 0 &&                                            \
      ((int32_t)(module_id)) < SVC_MESSAGE_MODULE_BITS) ?                       \
        ((SVC_MESSAGE_MASK_T)1ULL <<                                            \
            ((uint32_t)(module_id) + SVC_MESSAGE_MODULE_SHIFT)) :               \
        0ULL)

/* User/local flag bits */
#define SVC_MESSAGE_FLAG_SHIFT         0
#define SVC_MESSAGE_FLAG_0             ((uint32_t)(1U << (SVC_MESSAGE_FLAG_SHIFT + 0)))
#define SVC_MESSAGE_FLAG_1             ((uint32_t)(1U << (SVC_MESSAGE_FLAG_SHIFT + 1)))
#define SVC_MESSAGE_FLAG_2             ((uint32_t)(1U << (SVC_MESSAGE_FLAG_SHIFT + 2)))
#define SVC_MESSAGE_FLAG_3             ((uint32_t)(1U << (SVC_MESSAGE_FLAG_SHIFT + 3)))

#define SVC_MESSAGE_FLAG_MASK          ((uint32_t)(SVC_MESSAGE_FLAG_0 |         \
                                                   SVC_MESSAGE_FLAG_1 |         \
                                                   SVC_MESSAGE_FLAG_2 |         \
                                                   SVC_MESSAGE_FLAG_3))

/* Message class bits */
#define SVC_MESSAGE_CLASS_SHIFT        4
#define SVC_MESSAGE_CLASS_EVENT        ((uint32_t)(1U << (SVC_MESSAGE_CLASS_SHIFT + 0)))
#define SVC_MESSAGE_CLASS_COMMAND      ((uint32_t)(1U << (SVC_MESSAGE_CLASS_SHIFT + 1)))
#define SVC_MESSAGE_CLASS_RESPONSE     ((uint32_t)(1U << (SVC_MESSAGE_CLASS_SHIFT + 2)))
#define SVC_MESSAGE_CLASS_LOG          ((uint32_t)(1U << (SVC_MESSAGE_CLASS_SHIFT + 3)))

#define SVC_MESSAGE_CLASS_MASK         ((uint32_t)(SVC_MESSAGE_CLASS_EVENT    | \
                                                   SVC_MESSAGE_CLASS_COMMAND  | \
                                                   SVC_MESSAGE_CLASS_RESPONSE | \
                                                   SVC_MESSAGE_CLASS_LOG))

/* Severity bits */
#define SVC_MESSAGE_SEVERITY_SHIFT     8
#define SVC_MESSAGE_SEVERITY_DEBUG     ((uint32_t)(1U << (SVC_MESSAGE_SEVERITY_SHIFT + 0)))
#define SVC_MESSAGE_SEVERITY_INFO      ((uint32_t)(1U << (SVC_MESSAGE_SEVERITY_SHIFT + 1)))
#define SVC_MESSAGE_SEVERITY_WARN      ((uint32_t)(1U << (SVC_MESSAGE_SEVERITY_SHIFT + 2)))
#define SVC_MESSAGE_SEVERITY_ERROR     ((uint32_t)(1U << (SVC_MESSAGE_SEVERITY_SHIFT + 3)))

#define SVC_MESSAGE_SEVERITY_MASK      ((uint32_t)(SVC_MESSAGE_SEVERITY_DEBUG | \
                                                   SVC_MESSAGE_SEVERITY_INFO  | \
                                                   SVC_MESSAGE_SEVERITY_WARN  | \
                                                   SVC_MESSAGE_SEVERITY_ERROR))

/* QoS bits */
#define SVC_MESSAGE_QOS_SHIFT          12
#define SVC_MESSAGE_QOS_LOW            ((uint32_t)(1U << (SVC_MESSAGE_QOS_SHIFT + 0)))
#define SVC_MESSAGE_QOS_NORMAL         ((uint32_t)(1U << (SVC_MESSAGE_QOS_SHIFT + 1)))
#define SVC_MESSAGE_QOS_HIGH           ((uint32_t)(1U << (SVC_MESSAGE_QOS_SHIFT + 2)))
#define SVC_MESSAGE_QOS_CRITICAL       ((uint32_t)(1U << (SVC_MESSAGE_QOS_SHIFT + 3)))

#define SVC_MESSAGE_QOS_MASK           ((uint32_t)(SVC_MESSAGE_QOS_LOW      |   \
                                                   SVC_MESSAGE_QOS_NORMAL   |   \
                                                   SVC_MESSAGE_QOS_HIGH     |   \
                                                   SVC_MESSAGE_QOS_CRITICAL))

#define SVC_MESSAGE_DEFAULT_FLAGS      ((uint32_t)(SVC_MESSAGE_CLASS_EVENT |     \
                                                   SVC_MESSAGE_QOS_NORMAL))

#define SVC_MESSAGE_ROUTE(module, flags)                                        \
    (SVC_MESSAGE_MODULE_MASK(module) |                                          \
     ((SVC_MESSAGE_MASK_T)(flags) & SVC_MESSAGE_FLAGS_MASK))

#define SVC_MESSAGE_SET_CLASS(message, class_bit)                               \
    do {                                                                        \
        (message)->flags &= (uint32_t)~SVC_MESSAGE_CLASS_MASK;                  \
        (message)->flags |= (uint32_t)(class_bit);                              \
    } while (0)

#define SVC_MESSAGE_SET_SEVERITY(message, severity_bit)                         \
    do {                                                                        \
        (message)->flags &= (uint32_t)~SVC_MESSAGE_SEVERITY_MASK;               \
        (message)->flags |= (uint32_t)(severity_bit);                           \
    } while (0)

#define SVC_MESSAGE_SET_QOS(message, qos_bit)                                   \
    do {                                                                        \
        (message)->flags &= (uint32_t)~SVC_MESSAGE_QOS_MASK;                    \
        (message)->flags |= (uint32_t)(qos_bit);                                \
    } while (0)

#define SVC_MESSAGE_ADD_FLAGS(message, bits)                                    \
    do {                                                                        \
        (message)->flags |= (uint32_t)(bits);                                   \
    } while (0)

#define SVC_MESSAGE_CLEAR_FLAGS(message, bits)                                  \
    do {                                                                        \
        (message)->flags &= (uint32_t)~(bits);                                  \
    } while (0)

struct SVC_MESSAGE_S;
typedef struct SVC_MESSAGE_S SVC_MESSAGE_T;

typedef void (*SVC_MESSAGE_CHANNEL_FP)(
    void *user,
    const SVC_MESSAGE_T *message
);

typedef struct SVC_MESSAGE_FILTER_S {
    SVC_MESSAGE_MASK_T require;
    SVC_MESSAGE_MASK_T accept;
    SVC_MESSAGE_MASK_T reject;
} SVC_MESSAGE_FILTER_T;

typedef struct SVC_MESSAGE_CHANNEL_S {
    struct SVC_MESSAGE_CHANNEL_S *next;
    SVC_MESSAGE_CHANNEL_FP        fp;
    SVC_MESSAGE_FILTER_T          filter;
    void                         *user;
} SVC_MESSAGE_CHANNEL_T;

struct SVC_MESSAGE_S {
    SVC_TASKS_T task;

    uint32_t    flags;
    int16_t     module;

    uint16_t    size;

    uint8_t     payload[];
};

#define SVC_MESSAGE_DATA(message) \
    ((void *)((message)->payload))

#define SVC_MESSAGE_CONST_DATA(message) \
    ((const void *)((message)->payload))

static inline SVC_MESSAGE_MASK_T
svc_message_route(const SVC_MESSAGE_T *message)
{
    return SVC_MESSAGE_ROUTE(message->module, message->flags);
}

static inline int
svc_message_filter_match(
    const SVC_MESSAGE_FILTER_T *filter,
    SVC_MESSAGE_MASK_T route
)
{
    if ((route & filter->require) != filter->require) {
        return 0;
    }

    if (filter->accept && ((route & filter->accept) == 0)) {
        return 0;
    }

    if (route & filter->reject) {
        return 0;
    }

    return 1;
}

#ifdef __cplusplus
extern "C" {
#endif

extern int32_t svc_message_init(SVC_TASK_PRIO_T prio);
extern int32_t svc_message_start(void);

extern uint32_t svc_message_would_post(int16_t module);
extern uint32_t svc_message_would_post_route(SVC_MESSAGE_MASK_T route);

extern SVC_MESSAGE_T *svc_message_create(
    uint16_t size,
    int16_t module,
    uint32_t flags
);

extern int32_t svc_message_post(SVC_MESSAGE_T *message);

extern void svc_message_channel_add(SVC_MESSAGE_CHANNEL_T *channel);
extern void svc_message_channel_remove(SVC_MESSAGE_CHANNEL_T *channel);

extern int32_t svc_message_wait(uint32_t timeout);
extern int32_t svc_message_wait_all(uint32_t timeout);

extern SVC_MESSAGE_FILTER_T svc_message_get_filter(void);

#ifdef __cplusplus
}
#endif

#endif /* __SVC_MESSAGE_H__ */
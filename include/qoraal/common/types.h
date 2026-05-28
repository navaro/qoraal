/*
 *  Copyright (C) 2015-2025, Navaro, All Rights Reserved
 *  SPDX-License-Identifier: Apache-2.0
 */

#ifndef __QORAAL_TYPES_H__
#define __QORAAL_TYPES_H__

#include <stddef.h>
#include <stdint.h>

/*===========================================================================*/
/* Enum descriptor types.                                                    */
/*===========================================================================*/

typedef struct QORAAL_ENUM_VALUE_S {
    const char *name;
    int32_t value;
} QORAAL_ENUM_VALUE_T;

typedef struct QORAAL_ENUM_TYPE_S {
    const char *type_name;
    const QORAAL_ENUM_VALUE_T *values;
    uint16_t count;
    uint16_t id;
    struct QORAAL_ENUM_TYPE_S * next ;
} QORAAL_ENUM_TYPE_T;

/*===========================================================================*/
/* Property types.                                                           */
/*===========================================================================*/

#define QORAAL_PROP_VALUE_BUFFER_MAX 128

typedef enum {
    QORAAL_PROP_STRING,
    QORAAL_PROP_INTEGER,
    QORAAL_PROP_BOOLEAN,
    QORAAL_PROP_ACTION,
    QORAAL_PROP_ENUM,
} QORAAL_PROP_TYPE_T;

/* Resolver callback: given an enum type name, returns the type descriptor. */
typedef const QORAAL_ENUM_TYPE_T * (*qoraal_enum_resolver_t)(const char *name);

typedef struct QORAAL_PROP_S QORAAL_PROP_T;

typedef int32_t (*qoraal_prop_callback_t)(void *val, QORAAL_PROP_T *prop);

struct QORAAL_PROP_S {
    const char * name;              /* Property name, e.g. "state". */
    QORAAL_PROP_TYPE_T type;        /* Property value type. */
    const char * description;       /* Human-readable description. */
    const char * enum_name;         /* Enum type name resolved at runtime. */
    qoraal_prop_callback_t get_callback;
    qoraal_prop_callback_t set_callback;
    uintptr_t arg;
};

/* Resource capability flags for QORAAL_PROP_RESOURCE_T.flags */
#define QORAAL_PROP_RESOURCE_FLAG_EVENTS    (1U << 0)  /* resource publishes streaming events */

typedef struct QORAAL_PROP_RESOURCE_S {
    struct QORAAL_PROP_RESOURCE_S *next; /* Intrusive registration list. */

    const char * title;
    const char * version;
    const char * ep;

    const char * tag;
    const char * description;
    const char * get_summary;
    const char * set_summary;

    QORAAL_PROP_T *props;
    size_t props_count;

    uint32_t flags;  /* QORAAL_PROP_RESOURCE_FLAG_* capability bits */
    int32_t  module; /* Owning service/module id; -1 = unassigned */
} QORAAL_PROP_RESOURCE_T;


/*===========================================================================*/
/* Property initializer macros.                                              */
/*===========================================================================*/

#define QORAAL_PROP_INIT(prop_, type_, description_, get_, set_, arg_) \
    {                                                                  \
        prop_,                                                         \
        type_,                                                         \
        description_,                                                  \
        0,                                                             \
        get_,                                                          \
        set_,                                                          \
        arg_                                                           \
    }

#define QORAAL_PROP_ENUM_INIT(prop_, description_, enum_name_, get_, set_, arg_) \
    {                                                                  \
        prop_,                                                         \
        QORAAL_PROP_ENUM,                                              \
        description_,                                                  \
        enum_name_,                                                    \
        get_,                                                          \
        set_,                                                          \
        arg_                                                           \
    }

#define QORAAL_PROP_DECL(name, prop_, type_, description_, get_, set_, arg_) \
    QORAAL_PROP_T name = QORAAL_PROP_INIT(prop_, type_, description_, get_, set_, arg_)

#define QORAAL_PROP_ENUM_DECL(name, prop_, description_, enum_name_, get_, set_, arg_) \
    QORAAL_PROP_T name = QORAAL_PROP_ENUM_INIT(prop_, description_, enum_name_, get_, set_, arg_)

#define QORAAL_PROP_RESOURCE_INIT(title_, version_, ep_, tag_, desc_, get_sum_, set_sum_, props_) \
    {                                                                  \
        0,                                                             \
        title_,                                                        \
        version_,                                                      \
        ep_,                                                           \
        tag_,                                                          \
        desc_,                                                         \
        get_sum_,                                                      \
        set_sum_,                                                      \
        props_,                                                        \
        QORAAL_ARRAY_SIZE(props_),                                     \
        0,                                                             \
        -1                                                             \
    }

#define QORAAL_PROP_RESOURCE_INIT_EX(title_, version_, ep_, tag_, desc_, get_sum_, set_sum_, props_, flags_, module_) \
    {                                                                  \
        0,                                                             \
        title_,                                                        \
        version_,                                                      \
        ep_,                                                           \
        tag_,                                                          \
        desc_,                                                         \
        get_sum_,                                                      \
        set_sum_,                                                      \
        props_,                                                        \
        QORAAL_ARRAY_SIZE(props_),                                     \
        flags_,                                                        \
        module_                                                        \
    }

/* Convenience macro for resources that publish streaming events.
 * CMM assigns the owning service/module id during resource registration. */
#define QORAAL_PROP_RESOURCE_INIT_EVENTS(title_, version_, ep_, tag_, desc_, get_sum_, set_sum_, props_) \
    QORAAL_PROP_RESOURCE_INIT_EX(title_, version_, ep_, tag_, desc_, get_sum_, set_sum_, props_, \
                                 QORAAL_PROP_RESOURCE_FLAG_EVENTS, -1)

#define QORAAL_PROP_RESOURCE_DECL_EX(name, title_, version_, ep_, tag_, desc_, get_sum_, set_sum_, props_) \
    QORAAL_PROP_RESOURCE_T name = QORAAL_PROP_RESOURCE_INIT(title_, version_, ep_, tag_, desc_, get_sum_, set_sum_, props_)

/*===========================================================================*/
/* Helper macros.                                                            */
/*===========================================================================*/

#define QORAAL_ARRAY_SIZE(array_) \
    (sizeof(array_) / sizeof((array_)[0]))

#endif /* __QORAAL_TYPES_H__ */

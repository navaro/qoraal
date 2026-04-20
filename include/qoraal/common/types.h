/*
 *  Copyright (C) 2015-2025, Navaro, All Rights Reserved
 *  SPDX-License-Identifier: Apache-2.0
 */

#ifndef __QORAAL_TYPES_H__
#define __QORAAL_TYPES_H__

#include <stddef.h>
#include <stdint.h>

typedef struct QORAAL_ENUM_VALUE_S {
    const char *name;
    int32_t value;
} QORAAL_ENUM_VALUE_T;

typedef struct QORAAL_ENUM_TYPE_S {
    const char *type_name;
    const QORAAL_ENUM_VALUE_T *values;
    size_t count;
} QORAAL_ENUM_TYPE_T;

#endif /* __QORAAL_TYPES_H__ */

/*
 *  Copyright (C) 2015-2025, Navaro, All Rights Reserved
 *  SPDX-License-Identifier: Apache-2.0
 *
 *  Licensed under the Apache License, Version 2.0 (the "License"); you may
 *  not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 *  WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *  This file is part of CORAL Connect (https://navaro.nl)
 */


#if defined CFG_OS_ZEPHYR

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/console/console.h>
#include "qoraal/platform.h"
#include "qoraal/errordef.h"
#include <stdlib.h>


static uint32_t _platform_flash_size;

int32_t
platform_init(uint32_t flash_size)
{
    _platform_flash_size = flash_size;
    console_init();
    return 0;
}

int32_t
platform_start(void)
{
    ARG_UNUSED(_platform_flash_size);
    return 0;
}

int32_t
platform_stop(void)
{
    return 0;
}

void *
platform_malloc(QORAAL_HEAP heap, size_t size)
{
    ARG_UNUSED(heap);
    return malloc(size);
}

void
platform_free(QORAAL_HEAP heap, void *mem)
{
    ARG_UNUSED(heap);
    free(mem);
}

void
platform_print(const char *format)
{
    //console_write (format, strlen(format), 1000) ;
   printk("%s", format);
}

int32_t 
platform_getch (uint32_t timeout_ms)
{
    return console_getchar(); //console_get_char(timeout_ms);
}

void
platform_assert(const char *format)
{
    printk("%s", format);
    k_oops();
}

uint32_t
platform_current_time(void)
{
    return k_uptime_get_32() / 1000U;
}

uint32_t
platform_rand(void)
{
    return rand();
}

uint32_t
platform_wdt_kick(void)
{
    return 20U;
}

int32_t
platform_flash_erase(uint32_t addr_start, uint32_t addr_end)
{
    ARG_UNUSED(addr_start);
    ARG_UNUSED(addr_end);
    return E_NOIMPL;
}

int32_t
platform_flash_write(uint32_t addr, uint32_t len, const uint8_t *data)
{
    ARG_UNUSED(addr);
    ARG_UNUSED(len);
    ARG_UNUSED(data);
    return E_NOIMPL;
}

int32_t
platform_flash_read(uint32_t addr, uint32_t len, uint8_t *data)
{
    ARG_UNUSED(addr);
    ARG_UNUSED(len);
    ARG_UNUSED(data);
    return E_NOIMPL;
}

#endif /* CFG_OS_ZEPHYR */

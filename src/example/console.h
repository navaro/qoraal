#ifndef __CONSOLE_H__
#define __CONSOLE_H__

#include <stddef.h>
#include <stdint.h>

size_t console_write(const uint8_t *data, size_t len, uint32_t timeout_ms) ;
int32_t console_get_char(uint32_t timeout_ms) ;

#endif // __CONSOLE_H__
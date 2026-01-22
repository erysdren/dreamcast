
#ifndef _RUNTIME_H_
#define _RUNTIME_H_
#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
#define RUNTIME_EXTERN extern "C"
#else
#define RUNTIME_EXTERN extern
#ifndef static_assert
#define static_assert _Static_assert
#endif
#endif

typedef volatile uint8_t reg8;
typedef volatile uint16_t reg16;
typedef volatile uint32_t reg32;
typedef volatile float reg32f;

static_assert((sizeof(reg8)) == 1);
static_assert((sizeof(reg16)) == 2);
static_assert((sizeof(reg32)) == 4);
static_assert((sizeof(reg32f)) == 4);

RUNTIME_EXTERN void print_char(const char c);
RUNTIME_EXTERN void print_string(const char* s, int length);
RUNTIME_EXTERN void print_bytes(const uint8_t* s, int length);
RUNTIME_EXTERN void print_chars(const uint16_t* s, int length);
RUNTIME_EXTERN void print_cstring(const char* s);
RUNTIME_EXTERN void print_integer(const int n);

RUNTIME_EXTERN void _printf(const char* format, ...);

#define printf(...) _printf(__VA_ARGS__)
#define printc(c) print_char(c)
#define prints(s) print_cstring(s)

#endif /* _RUNTIME_H_ */

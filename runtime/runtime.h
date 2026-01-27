
#ifndef _RUNTIME_H_
#define _RUNTIME_H_
#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

#include "tinyalloc/tinyalloc.h"

/* registers */
typedef volatile uint8_t reg8;
typedef volatile uint16_t reg16;
typedef volatile uint32_t reg32;
typedef volatile float reg32f;

static_assert((sizeof(reg8)) == 1);
static_assert((sizeof(reg16)) == 2);
static_assert((sizeof(reg32)) == 4);
static_assert((sizeof(reg32f)) == 4);

/* print functions */
void print_char(const char c);
void print_string(const char* s, int length);
void print_bytes(const uint8_t* s, int length);
void print_chars(const uint16_t* s, int length);
void print_cstring(const char* s);
void print_integer(const int n);
void _printf(const char* format, ...);
#define printf(...) _printf(__VA_ARGS__)
void _sprintf(char *s, const char* format, ...);
#define sprintf(s, ...) _sprintf(s, __VA_ARGS__)

/* string functions */
#define strcmp(a, b) __builtin_strcmp(a, b)
#define strncmp(a, b, n) __builtin_strncmp(a, b, n)
#define strlen(a) __builtin_strlen(a)
char *strcat(char *dst, const char *src);
char *strncat(char *dst, const char *src, size_t n);
char *strcpy(char *dst, const char *src);
char *strncpy(char *dst, const char *src, size_t n);
size_t strlcat(char *dst, const char *src, size_t n);
size_t strlcpy(char *dst, const char *src, size_t n);
size_t strnlen(const char *s, size_t n);

/* memory functions */
#define memcmp(a, b, n) __builtin_memcmp(a, b, n)
void *memchr(const void *src, int c, size_t n);
void *memset(void *dst, int c, size_t n);
void *memcpy(void *dst, const void *src, size_t n);
void *memmove(void *dst, const void *src, size_t n);

/* char functions */
char tolower(char c);

/* wildcard compare */
bool wildcmp(const char *wild, const char *string);

/* exit */
[[noreturn]] void exit(int code);

/* heap functions */
#define malloc(n) ta_alloc(n)
#define calloc(n, s) ta_calloc(n, s)
#define realloc(p, n) ta_realloc(p, n)
#define free(p) ta_free(p)

/* math functions */
#define cos(n) __builtin_cosf(n)
#define cosf(n) __builtin_cosf(n)
#define sin(n) __builtin_sinf(n)
#define sinf(n) __builtin_sinf(n)
#define tan(n) __builtin_tanf(n)
#define tanf(n) __builtin_tanf(n)
#define acos(n) __builtin_acosf(n)
#define acosf(n) __builtin_acosf(n)
#define asin(n) __builtin_asinf(n)
#define asinf(n) __builtin_asinf(n)
#define fabs(n) __builtin_fabsf(n)
#define fabsf(n) __builtin_fabsf(n)
#define floor(n) __builtin_floorf(n)
#define floorf(n) __builtin_floorf(n)
#define fmin(a, b) __builtin_fminf(a, b)
#define fminf(a, b) __builtin_fminf(a, b)
#define fmod(a, b) __builtin_fmodf(a, b)
#define fmodf(a, b) __builtin_fmodf(a, b)
#define sqrt(a) __builtin_sqrtf(a)
#define sqrtf(a) __builtin_sqrtf(a)
#define modf(a, b) __builtin_modff(a, b)
#define modff(a, b) __builtin_modff(a, b)
#define abs(n) __builtin_abs(n)
#define fabs(n) __builtin_fabsf(n)
#define fabsf(n) __builtin_fabsf(n)
#define atan(n) __builtin_atanf(n)
#define atanf(n) __builtin_atanf(n)
#define atan2(a, b) __builtin_atan2f(a, b)
#define atan2f(a, b) __builtin_atan2f(a, b)
#define pow(a, b) __builtin_pow(a, b)
#define powf(a, b) __builtin_powf(a, b)

/* cglm */
#include "cglm/cglm.h"

/* file formats */
#include "ibsp.h"
#include "ibsp_trace.h"
#include "iqm.h"
#include "md3.h"
#include "pvr.h"

/* camera */
#include "camera.h"

/* transfer */
#include "texture_cache.h"
#include "transfer.h"

#ifdef __cplusplus
}
#endif
#endif /* _RUNTIME_H_ */

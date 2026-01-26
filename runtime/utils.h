#ifndef _UTILS_H_
#define _UTILS_H_
#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

#include "tinyalloc/tinyalloc.h"

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

void *memchr(const void *src, int c, size_t n);
void *memset(void *dst, int c, size_t n);
void *memcpy(void *dst, const void *src, size_t n);

char tolower(char c);

bool wildcmp(const char *wild, const char *string);

[[noreturn]] void exit(int code);

#define malloc(n) ta_alloc(n)
#define calloc(n, s) ta_calloc(n, s)
#define realloc(p, n) ta_realloc(p, n)
#define free(p) ta_free(p)

#ifdef __cplusplus
}
#endif
#endif // _UTILS_H_

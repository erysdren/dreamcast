#ifndef _UTILS_H_
#define _UTILS_H_
#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

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

#ifdef __cplusplus
}
#endif
#endif // _UTILS_H_

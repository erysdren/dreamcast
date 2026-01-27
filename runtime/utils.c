
#include "runtime.h"

float tanf(float radians)
{
	return sinf(radians) / cosf(radians);
}

void *memchr(const void *src, int c, size_t n)
{
	const uint8_t *s = src;
	c = (uint8_t)c;
	for (; n && *s != c; s++, n--);
	return n ? (void *)s : 0;
}

size_t strnlen(const char *s, size_t n)
{
	const char *p = memchr(s, 0, n);
	return p ? p - s : n;
}

size_t strlcpy(char *dst, const char *src, size_t n)
{
	char *d0 = dst;
	if (!n--) goto finish;
	for (; n && (*dst = *src); n--, src++, dst++);
	*dst = '\0';
finish:
	return dst - d0 + strlen(src);
}

size_t strlcat(char *dst, const char *src, size_t n)
{
	size_t l = strnlen(dst, n);
	if (l == n) return l + strlen(src);
	return l + strlcpy(dst + l, src, n-l);
}

char *strcpy(char *dst, const char *src)
{
	for (; (*dst = *src); src++, dst++);
	return dst;
}

char *strncpy(char *dst, const char *src, size_t n)
{
	for (; n && (*dst = *src); n--, src++, dst++);
	memset(dst, 0, n);
	return dst;
}

char *strcat(char *dst, const char *src)
{
	strcpy(dst + strlen(dst), src);
	return dst;
}

char *strncat(char *dst, const char *src, size_t n)
{
	char *a = dst;
	dst += strlen(dst);
	while (n && *src) n--, *dst++ = *src++;
	*dst++ = 0;
	return a;
}

void *memmove(void *dst, const void *src, size_t n)
{
	char *d = dst;
	const char *s = src;

	if (d==s) return d;
	if ((uintptr_t)s-(uintptr_t)d-n <= -2*n) return memcpy(d, s, n);

	if (d<s) {
		for (; n; n--) *d++ = *s++;
	} else {
		while (n) n--, d[n] = s[n];
	}

	return dst;
}

void *memcpy(void *dst, const void *src, size_t n)
{
	uint8_t *d = dst;
	const uint8_t *s = src;
	for (; n; n--) *d++ = *s++;
	return dst;
}

void *memset(void *dst, int c, size_t n)
{
	uint8_t *s = dst;
	for (; n; n--, s++) *s = c;
	return dst;
}

char tolower(char c)
{
	if (c >= 'A' && c <= 'Z')
		return c-'A'+'a';
	return c;
}

// https://stackoverflow.com/a/69178931
bool wildcmp(const char *pattern, const char *line)
{
	// returns 1 (true) if there is a match
	// returns 0 if the pattern is not whitin the line
	int wildcard = 0;

	const char* last_pattern_start = 0;
	const char* last_line_start = 0;
	do
	{
		if (*pattern == *line)
		{
			if(wildcard == 1)
				last_line_start = line + 1;

			line++;
			pattern++;
			wildcard = 0;
		}
		else if (*pattern == '?')
		{
			if(*(line) == '\0') // the line is ended but char was expected
				return false;
			if(wildcard == 1)
				last_line_start = line + 1;
			line++;
			pattern++;
			wildcard = 0;
		}
		else if (*pattern == '*')
		{
			if (*(pattern+1) == '\0')
			{
				return true;
			}

			last_pattern_start = pattern;
			//last_line_start = line + 1;
			wildcard = 1;

			pattern++;
		}
		else if (wildcard)
		{
			if (*line == *pattern)
			{
				wildcard = 0;
				line++;
				pattern++;
				last_line_start = line + 1 ;
			}
			else
			{
				line++;
			}
		}
		else
		{
			if ((*pattern) == '\0' && (*line) == '\0')  // end of mask
				return true; // if the line also ends here then the pattern match
				else
				{
					if (last_pattern_start != 0) // try to restart the mask on the rest
					{
						pattern = last_pattern_start;
						line = last_line_start;
						last_line_start = 0;
					}
					else
					{
						return false;
					}
				}
		}

	} while (*line);

	if (*pattern == '\0')
	{
		return true;
	}
	else
	{
		return false;
	}
}

[[noreturn]] void exit(int code)
{
	typedef void (*exit_func)(int) __attribute__((__noreturn__));
	exit_func do_exit = (exit_func)(*((uintptr_t *)(0x80000000 | 0x0C0000E0)));
	do_exit(1);
}

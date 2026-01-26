#ifndef _FONT_H_
#define _FONT_H_
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef struct font {
	float inverse_texture_width;
	float inverse_texture_height;
	float glyph_width;
	float glyph_height;
	int hori_advance;
	int row_stride;
} font_t;

#ifdef __cplusplus
}
#endif
#endif // _FONT_H_

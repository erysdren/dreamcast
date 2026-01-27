#ifndef _TEXTURE_CACHE_H_
#define _TEXTURE_CACHE_H_
#ifdef __cplusplus
extern "C" {
#endif

#include "runtime.h"
#include "pvr.h"

typedef struct texture_cache {
	uint32_t texture_control_word;
	uint32_t tsp_instruction_word;
} texture_cache_t;

enum : uint32_t {
	TEXTURE_TYPE_NONE,
	TEXTURE_TYPE_ARGB1555,
	TEXTURE_TYPE_RGB565,
	TEXTURE_TYPE_ARGB4444
};

enum : uint32_t {
	TEXTURE_FLAG_NONE = 0,
	TEXTURE_FLAG_TWIDDLED = 1 << 0,
	TEXTURE_FLAG_VQ = 1 << 1,
	TEXTURE_FLAG_MM = 1 << 2
};

static constexpr uint32_t MAX_TEXTURES = 1024;
static constexpr uint32_t TEXTURE_INVALID = (uint32_t)-1;

void texture_cache_init(uint32_t texture_address);
uint32_t texture_cache_raw(int width, int height, uint32_t type, uint32_t flags, const void *data, size_t len);
uint32_t texture_cache_pvr(const pvr_t *pvr);
const texture_cache_t *texture_cache_get(uint32_t texture_index);

#ifdef __cplusplus
}
#endif
#endif // _TEXTURE_CACHE_H_

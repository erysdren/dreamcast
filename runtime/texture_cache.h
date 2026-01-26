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

static constexpr uint32_t MAX_TEXTURES = 1024;
static constexpr uint32_t TEXTURE_INVALID = (uint32_t)-1;

RUNTIME_EXTERN void texture_cache_init(uint32_t texture_address);
RUNTIME_EXTERN uint32_t texture_cache_pvr(const pvr_t *pvr);
RUNTIME_EXTERN const texture_cache_t *texture_cache_get(uint32_t texture_index);

#ifdef __cplusplus
}
#endif
#endif // _TEXTURE_CACHE_H_

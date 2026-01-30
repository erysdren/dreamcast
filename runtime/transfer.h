#ifndef _TRANSFER_H_
#define _TRANSFER_H_
#ifdef __cplusplus
extern "C" {
#endif

#include "runtime.h"

void transfer_init(void);
void transfer_frame_start(void);
void transfer_frame_end(void);
void transfer_background_polygon(uint32_t color);

//
// palettes
//

enum : uint32_t {
	PALETTE_TYPE_NONE,
	PALETTE_TYPE_ARGB1555,
	PALETTE_TYPE_RGB565,
	PALETTE_TYPE_ARGB4444,
	PALETTE_TYPE_ARGB8888
};

// invalid palette handle
static constexpr uint32_t PALETTE_INVALID = (uint32_t)-1;

// initialize hardware palette type and alpha ref
void transfer_init_palette(uint32_t type, uint32_t alpha_ref);

// returns palette selector index, or PALETTE_INVALID
uint32_t transfer_palette(uint32_t *entries, size_t num_entries);

uint32_t transfer_ta_global_polygon(uint32_t store_queue_ix, uint32_t texture_index);
uint32_t transfer_ta_global_end_of_list(uint32_t store_queue_ix);

#ifdef __cplusplus
}
#endif
#endif // _TRANSFER_H_

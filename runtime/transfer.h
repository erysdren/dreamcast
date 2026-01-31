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

enum : uint32_t {
	SRC_ALPHA_INSTR_ZERO,
	SRC_ALPHA_INSTR_ONE,
	SRC_ALPHA_INSTR_OTHER_COLOR,
	SRC_ALPHA_INSTR_INVERSE_OTHER_COLOR,
	SRC_ALPHA_INSTR_SRC_ALPHA,
	SRC_ALPHA_INSTR_INVERSE_SRC_ALPHA,
	SRC_ALPHA_INSTR_DST_ALPHA,
	SRC_ALPHA_INSTR_INVERSE_DST_ALPHA
};

enum : uint32_t {
	DST_ALPHA_INSTR_ZERO,
	DST_ALPHA_INSTR_ONE,
	DST_ALPHA_INSTR_OTHER_COLOR,
	DST_ALPHA_INSTR_INVERSE_OTHER_COLOR,
	DST_ALPHA_INSTR_SRC_ALPHA,
	DST_ALPHA_INSTR_INVERSE_SRC_ALPHA,
	DST_ALPHA_INSTR_DST_ALPHA,
	DST_ALPHA_INSTR_INVERSE_DST_ALPHA
};

enum : uint32_t {
	FILTER_MODE_POINT,
	FILTER_MODE_BILINEAR,
	FILTER_MODE_TRILINEAR_A,
	FILTER_MODE_TRILINEAR_B
};

typedef struct ta_global_polygon {
	uint32_t texture_index;
	uint32_t src_alpha_instr;
	uint32_t dst_alpha_instr;
	uint32_t filter_mode;
} ta_global_polygon_t;

uint32_t transfer_ta_global_polygon_ex(uint32_t store_queue_ix, ta_global_polygon_t *info);

uint32_t transfer_ta_vertex_triangle_pt3(uint32_t store_queue_ix,
										 float ax, float ay, float az, float au, float av, uint32_t ac,
										 float bx, float by, float bz, float bu, float bv, uint32_t bc,
										 float cx, float cy, float cz, float cu, float cv, uint32_t cc);

#ifdef __cplusplus
}
#endif
#endif // _TRANSFER_H_

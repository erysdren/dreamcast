
#include "transfer.h"
#include "texture_cache.h"

#include "memorymap.h"

#include "holly/core/object_list_bits.hpp"
#include "holly/core/region_array.hpp"
#include "holly/core/region_array_bits.hpp"
#include "holly/core/parameter_bits.hpp"
#include "holly/core/parameter.hpp"
#include "holly/ta/global_parameter.hpp"
#include "holly/ta/vertex_parameter.hpp"
#include "holly/ta/parameter_bits.hpp"
#include "holly/holly.hpp"
#include "holly/holly_bits.hpp"

#include "sh7091/sh7091.hpp"
#include "sh7091/pref.hpp"
#include "sh7091/store_queue_transfer.hpp"

struct texture_memory_alloc__start_end {
	uint32_t start;
	uint32_t end;
};

struct texture_memory_alloc {
	struct texture_memory_alloc__start_end isp_tsp_parameters;
	struct texture_memory_alloc__start_end object_list;
	struct texture_memory_alloc__start_end region_array;
	struct texture_memory_alloc__start_end framebuffer[3];
	struct texture_memory_alloc__start_end background[2];
	struct texture_memory_alloc__start_end texture;
};

static constexpr texture_memory_alloc texture_memory_alloc = {
	// 32-bit addresses
	.isp_tsp_parameters = {0x000000, 0x21bfe0},
	.object_list = {0x400000, 0x495fe0},
	.region_array = {0x21c000, 0x22c000},
	.framebuffer = {{0x496000, 0x53ec00}, {0x22c000, 0x2d4c00}, {0x53ec00, 0x5e7800}},
	.background = {{0x2d4c00, 0x2d4c20}, {0x5e7800, 0x5e7820}},
	// 64-bit addresses
	.texture = {0x5a9840, 0x800000}
};

void transfer_init(void)
{
	using namespace holly::core;
	using namespace holly;
	using holly::holly;

	const int tile_y_num = 480 / 32;
	const int tile_x_num = 640 / 32;

	region_array::list_block_size list_block_size = {
		.opaque = 8 * 4,
	};

	region_array::transfer(
		tile_x_num,
		tile_y_num,
		list_block_size,
		texture_memory_alloc.region_array.start,
		texture_memory_alloc.object_list.start
	);

	transfer_background_polygon(0xff00ff);

	//////////////////////////////////////////////////////////////////////////////
	// initialize texture cache
	//////////////////////////////////////////////////////////////////////////////

	texture_cache_init(texture_memory_alloc.texture.start);

	//////////////////////////////////////////////////////////////////////////////
	// configure the TA
	//////////////////////////////////////////////////////////////////////////////

	// TA_GLOB_TILE_CLIP restricts which "object pointer blocks" are written
	// to.
	//
	// This can also be used to implement "windowing", as long as the desired
	// window size happens to be a multiple of 32 pixels. The "User Tile Clip" TA
	// control parameter can also ~equivalently be used as many times as desired
	// within a single TA initialization to produce an identical effect.
	//
	// See DCDBSysArc990907E.pdf page 183.
	holly.TA_GLOB_TILE_CLIP = ta_glob_tile_clip::tile_y_num(tile_y_num - 1)
							| ta_glob_tile_clip::tile_x_num(tile_x_num - 1);

	// While CORE supports arbitrary-length object lists, the TA uses "object
	// pointer blocks" as a memory allocation strategy. These fixed-length blocks
	// can still have infinite length via "object pointer block links". This
	// mechanism is illustrated in DCDBSysArc990907E.pdf page 188.
	holly.TA_ALLOC_CTRL = ta_alloc_ctrl::opb_mode::increasing_addresses
						| ta_alloc_ctrl::o_opb::_8x4byte;

	// While building object lists, the TA contains an internal index (exposed as
	// the read-only TA_ITP_CURRENT) for the next address that new ISP/TSP will be
	// stored at. The initial value of this index is TA_ISP_BASE.

	// reserve space in ISP/TSP parameters for the background parameter
	using polygon = holly::core::parameter::isp_tsp_parameter<3>;
	uint32_t ta_isp_base_offset = (sizeof (polygon)) * 1;

	holly.TA_ISP_BASE = texture_memory_alloc.isp_tsp_parameters.start + ta_isp_base_offset;
	holly.TA_ISP_LIMIT = texture_memory_alloc.isp_tsp_parameters.end;

	// Similarly, the TA also contains, for up to 600 tiles, an internal index for
	// the next address that an object list entry will be stored for each
	// tile. These internal indicies are partially exposed via the read-only
	// TA_OL_POINTERS.
	holly.TA_OL_BASE = texture_memory_alloc.object_list.start;

	// TA_OL_LIMIT, DCDBSysArc990907E.pdf page 385:
	//
	// >   Because the TA may automatically store data in the address that is
	// >   specified by this register, it must not be used for other data.  For
	// >   example, the address specified here must not be the same as the address
	// >   in the TA_ISP_BASE register.
	holly.TA_OL_LIMIT = texture_memory_alloc.object_list.end;

	//////////////////////////////////////////////////////////////////////////////
	// configure CORE
	//////////////////////////////////////////////////////////////////////////////

	// REGION_BASE is the (texture memory-relative) address of the region array.
	holly.REGION_BASE = texture_memory_alloc.region_array.start;

	// PARAM_BASE is the (texture memory-relative) address of ISP/TSP parameters.
	// Anything that references an ISP/TSP parameter does so relative to this
	// address (and not relative to the beginning of texture memory).
	holly.PARAM_BASE = texture_memory_alloc.isp_tsp_parameters.start;

	// Set the offset of the background ISP/TSP parameter, relative to PARAM_BASE
	// SKIP is related to the size of each vertex
	uint32_t background_offset = 0;

	holly.ISP_BACKGND_T = isp_backgnd_t::tag_address(background_offset / 4)
						| isp_backgnd_t::tag_offset(0)
						| isp_backgnd_t::skip(1);

	// FB_W_SOF1 is the (texture memory-relative) address of the framebuffer that
	// will be written to when a tile is rendered/flushed.
	holly.FB_W_SOF1 = texture_memory_alloc.framebuffer[0].start;

	// without waiting for rendering to actually complete, immediately display the
	// framebuffer.
	holly.FB_R_SOF1 = texture_memory_alloc.framebuffer[0].start;
}

void transfer_frame_start(void)
{
	using namespace holly::core;
	using namespace holly;
	using holly::holly;

	// TA_LIST_INIT needs to be written (every frame) prior to the first FIFO
	// write.
	holly.TA_LIST_INIT = ta_list_init::list_init;

	// dummy TA_LIST_INIT read; DCDBSysArc990907E.pdf in multiple places says this
	// step is required.
	(void)holly.TA_LIST_INIT;
}

void transfer_frame_end(void)
{
	using namespace holly::core;
	using namespace holly;
	using holly::holly;

	//////////////////////////////////////////////////////////////////////////////
	// wait for vertical synchronization (and the TA)
	//////////////////////////////////////////////////////////////////////////////

	while (!(spg_status::vsync(holly.SPG_STATUS)));
	while (spg_status::vsync(holly.SPG_STATUS));

	//////////////////////////////////////////////////////////////////////////////
	// start the actual rasterization
	//////////////////////////////////////////////////////////////////////////////

	// start the actual render--the rendering process begins by interpreting the
	// region array
	holly.STARTRENDER = 1;
}

void transfer_background_polygon(uint32_t color)
{
	using namespace holly::core::parameter;

	using parameter = isp_tsp_parameter<3>;

	volatile parameter * polygon = (volatile parameter *)&texture_memory32[texture_memory_alloc.isp_tsp_parameters.start];

	polygon->isp_tsp_instruction_word = isp_tsp_instruction_word::depth_compare_mode::always
										| isp_tsp_instruction_word::culling_mode::cull_if_negative;

	polygon->tsp_instruction_word = tsp_instruction_word::src_alpha_instr::one
									| tsp_instruction_word::dst_alpha_instr::zero
									| tsp_instruction_word::fog_control::no_fog;

	polygon->texture_control_word = 0;

	polygon->vertex[0].x =  0.0f;
	polygon->vertex[0].y =  0.0f;
	polygon->vertex[0].z =  0.00001f;
	polygon->vertex[0].base_color = color;

	polygon->vertex[1].x = 32.0f;
	polygon->vertex[1].y =  0.0f;
	polygon->vertex[1].z =  0.00001f;
	polygon->vertex[1].base_color = color;

	polygon->vertex[2].x = 32.0f;
	polygon->vertex[2].y = 32.0f;
	polygon->vertex[2].z =  0.00001f;
	polygon->vertex[2].base_color = color;
}

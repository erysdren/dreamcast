
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

#include "systembus/systembus.hpp"
#include "systembus/systembus_bits.hpp"

#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240
#define SCREEN_BYTES_PER_PIXEL 2

const int tile_y_num = 480 / 32;
const int tile_x_num = 640 / 32;

holly::core::region_array::list_block_size list_block_size = {
  .opaque = 8 * 4,
};

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

void spg_set_mode_320x240_ntsc_ni()
{
	using namespace holly::core;
	using namespace holly;
	using holly::holly;

	holly.SPG_CONTROL
	= spg_control::sync_direction::output
	| spg_control::ntsc;

	holly.SPG_LOAD
	= spg_load::vcount(263 - 1)
	| spg_load::hcount(858 - 1);

	holly.SPG_HBLANK
	= spg_hblank::hbend(126)
	| spg_hblank::hbstart(837);

	holly.SPG_VBLANK
	= spg_vblank::vbend(18)
	| spg_vblank::vbstart(258);

	holly.SPG_WIDTH
	= spg_width::eqwidth(16 - 1)
	| spg_width::bpwidth(794 - 1)
	| spg_width::vswidth(3)
	| spg_width::hswidth(64 - 1);

	holly.VO_STARTX
	= vo_startx::horizontal_start_position(164);

	holly.VO_STARTY
	= vo_starty::vertical_start_position_on_field_2(18)
	| vo_starty::vertical_start_position_on_field_1(17);

	holly.VO_CONTROL
	= vo_control::pclk_delay(22)
	| vo_control::pixel_double;

	holly.SPG_HBLANK_INT
	= spg_hblank_int::line_comp_val(837);

	holly.SPG_VBLANK_INT
	= spg_vblank_int::vblank_out_interrupt_line_number(21)
	| spg_vblank_int::vblank_in_interrupt_line_number(258);
}

void spg_set_mode_640x480()
{
	using namespace holly::core;
	using namespace holly;
	using holly::holly;

  holly.SPG_CONTROL
    = spg_control::sync_direction::output;

  holly.SPG_LOAD
    = spg_load::vcount(525 - 1)    // number of lines per field
    | spg_load::hcount(858 - 1);   // number of video clock cycles per line

  holly.SPG_HBLANK
    = spg_hblank::hbend(126)       // H Blank ending position
    | spg_hblank::hbstart(837);    // H Blank starting position

  holly.SPG_VBLANK
    = spg_vblank::vbend(40)        // V Blank ending position
    | spg_vblank::vbstart(520);    // V Blank starting position

  holly.SPG_WIDTH
    = spg_width::eqwidth(16 - 1)   // Specify the equivalent pulse width (number of video clock cycles - 1)
    | spg_width::bpwidth(794 - 1)  // Specify the broad pulse width (number of video clock cycles - 1)
    | spg_width::vswidth(3)        // V Sync width (number of lines)
    | spg_width::hswidth(64 - 1);  // H Sync width (number of video clock cycles - 1)

  holly.VO_STARTX
    = vo_startx::horizontal_start_position(168);

  holly.VO_STARTY
    = vo_starty::vertical_start_position_on_field_2(40)
    | vo_starty::vertical_start_position_on_field_1(40);

  holly.VO_CONTROL
    = vo_control::pclk_delay(22);

  holly.SPG_HBLANK_INT
    = spg_hblank_int::line_comp_val(837);

  holly.SPG_VBLANK_INT
    = spg_vblank_int::vblank_out_interrupt_line_number(21)
    | spg_vblank_int::vblank_in_interrupt_line_number(520);
}

void transfer_init(void)
{
	using namespace holly::core;
	using namespace holly;
	using holly::holly;

	holly.SOFTRESET = softreset::ta_soft_reset | softreset::pipeline_soft_reset;
	holly.SOFTRESET = 0;

	// reset all holly errors and normal interrupts
	systembus::systembus.ISTERR = 0xffffffff;
	systembus::systembus.ISTNRM = 0xffffffff;

	region_array::transfer(
		tile_x_num,
		tile_y_num,
		list_block_size,
		texture_memory_alloc.region_array.start,
		texture_memory_alloc.object_list.start
	);

	transfer_background_polygon(0xff00ff);

	holly.VO_BORDER_COL = 0;

	//////////////////////////////////////////////////////////////////////////////
	// initialize texture cache
	//////////////////////////////////////////////////////////////////////////////

	texture_cache_init(texture_memory_alloc.texture.start);

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
	uint32_t background_offset = texture_memory_alloc.background[0].start - texture_memory_alloc.isp_tsp_parameters.start;

	holly.ISP_BACKGND_T = isp_backgnd_t::tag_address(background_offset / 4)
						| isp_backgnd_t::tag_offset(0)
						| isp_backgnd_t::skip(1);

	// FB_W_SOF1 is the (texture memory-relative) address of the framebuffer that
	// will be written to when a tile is rendered/flushed.

	spg_set_mode_320x240_ntsc_ni();

	//aica_sound.common.VREG(vreg::output_mode::cvbs_yc);
	int output_mode = 0b11; // cvbs_yc
	*((volatile unsigned uint32_t *)(0xa0702C00)) = (((output_mode >> 0) & 0x3) << 8);

	int x_size = SCREEN_WIDTH;
	int y_size = SCREEN_HEIGHT;
	int bytes_per_pixel = SCREEN_BYTES_PER_PIXEL;

	// write
	holly.FB_X_CLIP = fb_x_clip::fb_x_clip_max(x_size - 1) | fb_x_clip::fb_x_clip_min(0);
	holly.FB_Y_CLIP = fb_y_clip::fb_y_clip_max(y_size - 1) | fb_y_clip::fb_y_clip_min(0);

	// read
	while (spg_status::vsync(holly.SPG_STATUS));
	while (!spg_status::vsync(holly.SPG_STATUS));

	holly.FB_R_SIZE = fb_r_size::fb_modulus(1)
					| fb_r_size::fb_y_size(y_size - 1)
					| fb_r_size::fb_x_size((x_size * bytes_per_pixel) / 4 - 1);

	holly.FB_W_SOF1 = texture_memory_alloc.framebuffer[0].start;
	holly.FB_R_SOF1 = texture_memory_alloc.framebuffer[0].start;

	holly.FB_R_CTRL = fb_r_ctrl::vclk_div::pclk_vclk_2
					| fb_r_ctrl::fb_depth::rgb565
					| fb_r_ctrl::fb_enable;

	holly.FB_W_CTRL = fb_w_ctrl::fb_packmode::rgb565;
	holly.FB_W_LINESTRIDE = (x_size * bytes_per_pixel) / 8;

	holly.SCALER_CTL = scaler_ctl::horizontal_scaling_enable | scaler_ctl::vertical_scale_factor(0x0800);
}

void ta_init(void)
{
	using namespace holly::core;
	using namespace holly;
	using holly::holly;


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

	holly.TA_NEXT_OPB_INIT = texture_memory_alloc.object_list.start + list_block_size.total() * tile_x_num * tile_y_num;
}

void transfer_frame_start(void)
{
	using namespace holly::core;
	using namespace holly;
	using holly::holly;

	holly.SOFTRESET = softreset::ta_soft_reset;
	holly.SOFTRESET = 0;

        ta_init();

	// TA_LIST_INIT needs to be written (every frame) prior to the first FIFO
	// write.
	holly.TA_LIST_INIT = ta_list_init::list_init;

	// dummy TA_LIST_INIT read; DCDBSysArc990907E.pdf in multiple places says this
	// step is required.
	(void)holly.TA_LIST_INIT;
}

void core_wait_end_of_render_video()
{
  using namespace systembus;
  using systembus::systembus;
  /*
    "Furthermore, it is strongly recommended that the End of ISP and End of Video interrupts
    be cleared at the same time in order to make debugging easier when an error occurs."
  */
  //serial::string("eorv\n");
  bool timeout = false;
  int64_t count = 0;
  while (1) {
    if (systembus.ISTERR) {
      printf("core_wait_end_of_render_video ISTERR: %d\n", systembus.ISTERR);
    }
    uint32_t istnrm = systembus.ISTNRM;
    if ((istnrm & istnrm::end_of_render_tsp) != 0)
      break;
  }

  systembus.ISTNRM = istnrm::end_of_render_tsp
	  	   | istnrm::end_of_render_isp
		   | istnrm::end_of_render_video;
}

void ta_wait_opaque_list()
{
	using namespace systembus;
	using systembus::systembus;

	while ((systembus.ISTNRM & istnrm::end_of_transferring_opaque_list) == 0) {
		if (systembus.ISTERR) {
			printf("ta_wait_opaque_list ISTERR: %d\n", systembus.ISTERR);
		}
	};

	systembus.ISTNRM = istnrm::end_of_transferring_opaque_list;
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

	// explicitly wait for the TA; if a list other than opaque is added,
	// `ta_wait_opaque_list` should be replaced with `ta_wait_{list_type}_list`
	// for whichever `{list_type}` was sent to the TA last.
	ta_wait_opaque_list();

	//////////////////////////////////////////////////////////////////////////////
	// start the actual rasterization
	//////////////////////////////////////////////////////////////////////////////

	// start the actual render--the rendering process begins by interpreting the
	// region array
	holly.STARTRENDER = 1;

	core_wait_end_of_render_video();
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

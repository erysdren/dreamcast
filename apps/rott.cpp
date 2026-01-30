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

enum {
	TILE_DRAWFLAG_NONE = 0,
	TILE_DRAWFLAG_NORTH = 1 << 0,
	TILE_DRAWFLAG_SOUTH = 1 << 1,
	TILE_DRAWFLAG_EAST = 1 << 2,
	TILE_DRAWFLAG_WEST = 1 << 3,
	TILE_DRAWFLAG_FLOOR = 1 << 4,
	TILE_DRAWFLAG_CEILING = 1 << 5
};

#define TILE_WIDTH (64)
#define TILE_HEIGHT (64)

#define TILEMAP_WIDTH (8)
#define TILEMAP_HEIGHT (8)

#define SCREEN_WIDTH (640)
#define SCREEN_HEIGHT (480)

typedef struct tile {
	uint32_t texture_address;
	uint16_t height;
	uint16_t drawflags;
} tile_t;

static tile_t tilemap[TILEMAP_HEIGHT][TILEMAP_WIDTH];

void construct_tilemap()
{
	for (int y = 0; y < TILEMAP_HEIGHT; y++)
	{
		for (int x = 0; x < TILEMAP_WIDTH; x++)
		{
			if (y == 0 || y == TILEMAP_HEIGHT - 1 || x == 0 || x == TILEMAP_WIDTH - 1)
				tilemap[y][x] = tile_t{ 0x700000, 1, TILE_DRAWFLAG_NONE };
			else
				tilemap[y][x] = tile_t{ 0x700000, 0, TILE_DRAWFLAG_NONE };
		}
	}
}

static inline uint32_t transfer_ta_global_end_of_list(uint32_t store_queue_ix)
{
  using namespace holly::ta;
  using namespace holly::ta::parameter;

  //
  // TA "end of list" global transfer
  //
  volatile global_parameter::end_of_list * end_of_list = (volatile global_parameter::end_of_list *)&store_queue[store_queue_ix];
  store_queue_ix += (sizeof (global_parameter::end_of_list));

  end_of_list->parameter_control_word = parameter_control_word::para_type::end_of_list;

  // start store queue transfer of `end_of_list` to the TA
  pref(end_of_list);

  return store_queue_ix;
}

static inline uint32_t transfer_ta_global_polygon(uint32_t store_queue_ix, uint32_t texture_address)
{
  using namespace holly::core::parameter;
  using namespace holly::ta;
  using namespace holly::ta::parameter;

  //
  // TA polygon global transfer
  //

  volatile global_parameter::polygon_type_0 * polygon = (volatile global_parameter::polygon_type_0 *)&store_queue[store_queue_ix];
  store_queue_ix += (sizeof (global_parameter::polygon_type_0));

  polygon->parameter_control_word = parameter_control_word::para_type::polygon_or_modifier_volume
                                  | parameter_control_word::list_type::translucent
                                  | parameter_control_word::col_type::packed_color
                                  | parameter_control_word::texture;

  polygon->isp_tsp_instruction_word = isp_tsp_instruction_word::depth_compare_mode::greater
                                    | isp_tsp_instruction_word::culling_mode::no_culling;
  // Note that it is not possible to use
  // ISP_TSP_INSTRUCTION_WORD::GOURAUD_SHADING in this isp_tsp_instruction_word,
  // because `gouraud` is one of the bits overwritten by the value in
  // parameter_control_word. See DCDBSysArc990907E.pdf page 200.

  polygon->tsp_instruction_word = tsp_instruction_word::src_alpha_instr::one
                                | tsp_instruction_word::dst_alpha_instr::zero
                                | tsp_instruction_word::fog_control::no_fog
                                | tsp_instruction_word::filter_mode::point_sampled
                                | tsp_instruction_word::texture_shading_instruction::decal
                                | tsp_instruction_word::texture_u_size::_64
                                | tsp_instruction_word::texture_v_size::_64;

  polygon->texture_control_word = texture_control_word::pixel_format::palette_8bpp
                                | texture_control_word::scan_order::twiddled
                                | texture_control_word::palette_selector8(0)
                                | texture_control_word::texture_address(texture_address / 8);

  // start store queue transfer of `polygon` to the TA
  pref(polygon);

  return store_queue_ix;
}

static inline uint32_t transfer_ta_vertex_quad(uint32_t store_queue_ix,
                                               float ax, float ay, float az, float au, float av, uint32_t ac,
                                               float bx, float by, float bz, float bu, float bv, uint32_t bc,
                                               float cx, float cy, float cz, float cu, float cv, uint32_t cc,
                                               float dx, float dy, float dz, float du, float dv, uint32_t dc)
{
  using namespace holly::ta;
  using namespace holly::ta::parameter;

  //
  // TA polygon vertex transfer
  //

  volatile vertex_parameter::polygon_type_3 * vertex = (volatile vertex_parameter::polygon_type_3 *)&store_queue[store_queue_ix];
  store_queue_ix += (sizeof (vertex_parameter::polygon_type_3)) * 4;

  vertex[0].parameter_control_word = parameter_control_word::para_type::vertex_parameter;
  vertex[0].x = ax;
  vertex[0].y = ay;
  vertex[0].z = az;
  vertex[0].u = au;
  vertex[0].v = av;
  vertex[0].base_color = ac;
  vertex[0].offset_color = 0;

  // start store queue transfer of `vertex[0]` to the TA
  pref(&vertex[0]);

  vertex[1].parameter_control_word = parameter_control_word::para_type::vertex_parameter;
  vertex[1].x = bx;
  vertex[1].y = by;
  vertex[1].z = bz;
  vertex[1].u = bu;
  vertex[1].v = bv;
  vertex[1].base_color = bc;
  vertex[1].offset_color = 0;

  // start store queue transfer of `vertex[1]` to the TA
  pref(&vertex[1]);

  vertex[2].parameter_control_word = parameter_control_word::para_type::vertex_parameter;
  vertex[2].x = dx;
  vertex[2].y = dy;
  vertex[2].z = dz;
  vertex[2].u = du;
  vertex[2].v = dv;
  vertex[2].base_color = dc;
  vertex[2].offset_color = 0;

  // start store queue transfer of `params[2]` to the TA
  pref(&vertex[2]);

  vertex[3].parameter_control_word = parameter_control_word::para_type::vertex_parameter
                                   | parameter_control_word::end_of_strip;
  vertex[3].x = cx;
  vertex[3].y = cy;
  vertex[3].z = cz;
  vertex[3].u = cu;
  vertex[3].v = cv;
  vertex[3].base_color = cc;
  vertex[3].offset_color = 0;

  // start store queue transfer of `params[3]` to the TA
  pref(&vertex[3]);

  return store_queue_ix;
}

static inline uint32_t transfer_ta_vertex_triangle(uint32_t store_queue_ix,
                                                   float ax, float ay, float az, float au, float av, uint32_t ac,
                                                   float bx, float by, float bz, float bu, float bv, uint32_t bc,
                                                   float cx, float cy, float cz, float cu, float cv, uint32_t cc)
{
  using namespace holly::ta;
  using namespace holly::ta::parameter;

  //
  // TA polygon vertex transfer
  //

  volatile vertex_parameter::polygon_type_3 * vertex = (volatile vertex_parameter::polygon_type_3 *)&store_queue[store_queue_ix];
  store_queue_ix += (sizeof (vertex_parameter::polygon_type_3)) * 3;

  // bottom left
  vertex[0].parameter_control_word = parameter_control_word::para_type::vertex_parameter;
  vertex[0].x = ax;
  vertex[0].y = ay;
  vertex[0].z = az;
  vertex[0].u = au;
  vertex[0].v = av;
  vertex[0].base_color = ac;
  vertex[0].offset_color = 0;

  // start store queue transfer of `vertex[0]` to the TA
  pref(&vertex[0]);

  // top center
  vertex[1].parameter_control_word = parameter_control_word::para_type::vertex_parameter;
  vertex[1].x = bx;
  vertex[1].y = by;
  vertex[1].z = bz;
  vertex[1].u = bu;
  vertex[1].v = bv;
  vertex[1].base_color = bc;
  vertex[1].offset_color = 0;

  // start store queue transfer of `vertex[1]` to the TA
  pref(&vertex[1]);

  // bottom right
  vertex[2].parameter_control_word = parameter_control_word::para_type::vertex_parameter
                                   | parameter_control_word::end_of_strip;
  vertex[2].x = cx;
  vertex[2].y = cy;
  vertex[2].z = cz;
  vertex[2].u = cu;
  vertex[2].v = cv;
  vertex[2].base_color = cc;
  vertex[2].offset_color = 0;

  // start store queue transfer of `params[2]` to the TA
  pref(&vertex[2]);

  return store_queue_ix;
}

float fclamp(float i, float min, float max)
{
	return __builtin_fmin(__builtin_fmax(i, min), max);
}

int imin(int a, int b)
{
	return a < b ? a : b;
}

int imax(int a, int b)
{
	return a > b ? a : b;
}

int iclamp(int i, int min, int max)
{
	return imin(imax(i, min), max);
}

typedef struct ray_hit {
	vec2 ray_dir;
	vec2 delta_dist;
	vec2 side_dist;
	ivec2 step;
	ivec2 map_pos;
	int side;
} ray_hit_t;

enum : int {
	RAY_HIT_DONE = 0,
	RAY_HIT_WALL = 1,
	RAY_HIT_SKY = 2
};

static inline int ray_cast(ray_hit_t *hit)
{
	tile_t *tile = nullptr;

	while (1)
	{
		if (hit->side_dist[0] < hit->side_dist[1])
		{
			hit->side_dist[0] += hit->delta_dist[0];
			hit->map_pos[0] += hit->step[0];
			hit->side = 0;
		}
		else
		{
			hit->side_dist[1] += hit->delta_dist[1];
			hit->map_pos[1] += hit->step[1];
			hit->side = 1;
		}

		// out of bounds
		if (hit->map_pos[1] < 0 || hit->map_pos[1] >= TILEMAP_HEIGHT)
			return RAY_HIT_DONE;
		if (hit->map_pos[0] < 0 || hit->map_pos[0] >= TILEMAP_WIDTH)
			return RAY_HIT_DONE;

		// get tile
		tile = &tilemap[hit->map_pos[1]][hit->map_pos[0]];

		// hit
		if (tile->height > 0)
		{
			tile->drawflags |= TILE_DRAWFLAG_NORTH | TILE_DRAWFLAG_SOUTH | TILE_DRAWFLAG_EAST | TILE_DRAWFLAG_WEST;
			return RAY_HIT_WALL;
		}
		else
		{
			tile->drawflags |= TILE_DRAWFLAG_FLOOR | TILE_DRAWFLAG_CEILING;
		}

		if (hit->map_pos[0] == 0 || hit->map_pos[0] == TILEMAP_WIDTH - 1)
			return RAY_HIT_SKY;
		if (hit->map_pos[1] == 0 || hit->map_pos[1] == TILEMAP_HEIGHT - 1)
			return RAY_HIT_SKY;
	}

	return RAY_HIT_DONE;
}

static struct {
	vec3 origin;
	float yaw;
	float yaw_cos;
	float yaw_sin;
	int horizon;
} camera;

void raycast_column(int x)
{
	vec2 temp;
	ray_hit_t hit;
	int hit_type;
	float dist, dist2;
	tile_t *tile;
	int line_start, line_end;
	int line_start_c, line_end_c;
	float block_top, block_bottom;
	float pixel_height_scale = SCREEN_HEIGHT / 1.5;
	int w;
	int ystart = SCREEN_HEIGHT;

	// get map pos
	hit.map_pos[0] = (int)camera.origin[0];
	hit.map_pos[1] = (int)camera.origin[1];

	// get ray direction
	hit.ray_dir[0] = ((2.0f / (float)480) * (float)x) - 1.0f;
	hit.ray_dir[1] = 1.0f;

	// rotate around (0, 0) by camera yaw
	glm_vec2_copy(hit.ray_dir, temp);
	hit.ray_dir[0] = (-temp[0] * camera.yaw_cos) - (-temp[1] * camera.yaw_sin);
	hit.ray_dir[1] = (temp[0] * camera.yaw_sin) + (temp[1] * camera.yaw_cos);

	// get delta of ray (prevent divide by 0)
	hit.delta_dist[0] = (hit.ray_dir[0] == 0.0f) ? FLT_MAX : fabs(1.0f / hit.ray_dir[0]);
	hit.delta_dist[1] = (hit.ray_dir[1] == 0.0f) ? FLT_MAX : fabs(1.0f / hit.ray_dir[1]);

	// get step and initial side_dist
	if (hit.ray_dir[0] < 0)
	{
		hit.step[0] = -1;
		hit.side_dist[0] = (camera.origin[0] - (float)hit.map_pos[0]) * hit.delta_dist[0];
	}
	else
	{
		hit.step[0] = 1;
		hit.side_dist[0] = ((float)hit.map_pos[0] + 1.0f - camera.origin[0]) * hit.delta_dist[0];
	}

	if (hit.ray_dir[1] < 0)
	{
		hit.step[1] = -1;
		hit.side_dist[1] = (camera.origin[1] - (float)hit.map_pos[1]) * hit.delta_dist[1];
	}
	else
	{
		hit.step[1] = 1;
		hit.side_dist[1] = ((float)hit.map_pos[1] + 1.0f - camera.origin[1]) * hit.delta_dist[1];
	}

	uint32_t store_queue_ix = 0;

	// do cast
	while ((hit_type = ray_cast(&hit)) != RAY_HIT_DONE)
	{
		// early out
		if (!ystart)
			break;

		// get dist
		if (!hit.side)
		{
			dist = hit.side_dist[0] - hit.delta_dist[0];
			dist2 = fclamp(hit.side_dist[1], dist, dist + hit.delta_dist[0]);
		}
		else
		{
			dist = hit.side_dist[1] - hit.delta_dist[1];
			dist2 = fclamp(hit.side_dist[0], dist, dist + hit.delta_dist[1]);
		}

		// get tile
		tile = &tilemap[hit.map_pos[1]][hit.map_pos[0]];

		// line heights
		block_top = camera.origin[2] - tile->height * 0.125f;
		block_bottom = camera.origin[2];

		// line start and end
		line_start = ((block_top / dist) * pixel_height_scale) + camera.horizon;
		line_end = ((block_bottom / dist) * pixel_height_scale) + camera.horizon;

		// clamp to screen resolution
		line_start_c = iclamp(line_start, 0, ystart);
		line_end_c = iclamp(line_end, 0, ystart);

		if (tile->height > 0)
		{
			float wall_x;

			// get wall impact point
			if (!hit.side)
				wall_x = camera.origin[1] + dist * hit.ray_dir[1];
			else
				wall_x = camera.origin[0] + dist * hit.ray_dir[0];

			wall_x -= __builtin_floorf(wall_x);

			store_queue_ix = transfer_ta_global_polygon(store_queue_ix, 0x700000);

			vec3 vpa = (vec3){x, line_start_c, 0};
			vec3 vpb = (vec3){x, line_end_c, 0};
			vec3 vpc = (vec3){x + 1, line_start_c, 0};
			vec3 vpd = (vec3){x + 1, line_end_c, 0};

			vec2 vta = (vec2){};
			vec2 vtb = (vec2){};
			vec2 vtc = (vec2){};
			vec2 vtd = (vec2){};

			store_queue_ix = transfer_ta_vertex_quad(store_queue_ix,
														vpa[0], vpa[1], vpa[2], vta[0], vta[1], 0,
														vpb[0], vpb[1], vpb[2], vtb[0], vtb[1], 0,
														vpc[0], vpc[1], vpc[2], vtc[0], vtc[1], 0,
														vpd[0], vpd[1], vpd[2], vtd[0], vtd[1], 0);

		}
	}

	store_queue_ix = transfer_ta_global_end_of_list(store_queue_ix);
}

void raycast()
{
	camera.yaw_sin = sin(camera.yaw);
	camera.yaw_cos = cos(camera.yaw);

	for (int y = 0; y < TILEMAP_HEIGHT; y++)
	{
		for (int x = 0; x < TILEMAP_WIDTH; x++)
		{
			tilemap[y][x].drawflags = TILE_DRAWFLAG_NONE;
		}
	}

	for (int x = 0; x < SCREEN_WIDTH; x++)
	{
		raycast_column(x);
	}
}

const uint8_t texture[] __attribute__((aligned(4))) = {
  #embed "rott_wall1.64x64.palette_8bpp.twiddled"
};

void transfer_texture(uint32_t texture_start)
{
  // use 4-byte transfers to texture memory, for slightly increased transfer
  // speed
  //
  // It would be even faster to use the SH4 store queue for this operation, or
  // SH4 DMA.

  sh7091::store_queue_transfer::copy((void *)&texture_memory64[texture_start], texture, (sizeof (texture)));
}

const uint8_t palette[] __attribute__((aligned(4))) = {
  #embed "rott_palette.dat"
};

#define PACK_ARGB8888(r, g, b, a) (((a) << 24) | ((r) << 16) | ((g) << 8) | ((b) << 0))

void main()
{
	transfer_init();

	using namespace holly::core;
	using namespace holly;
	using holly::holly;

	/* palette */
	holly.PAL_RAM_CTRL = holly::pal_ram_ctrl::pixel_format::argb8888;

	for (int i = 0; i < 256; i++)
	{
		holly.PALETTE_RAM[i] = PACK_ARGB8888(palette[i * 3 + 0], palette[i * 3 + 1], palette[i * 3 + 2], 255);
	}

	holly.PT_ALPHA_REF = 0xff;

	// draw 500 frames of cube rotation
	while (1)
	{
		transfer_frame_start();

		raycast();

		transfer_frame_end();
	}
}

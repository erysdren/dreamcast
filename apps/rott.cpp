#include "maple.h"

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

#include "interrupt.cpp"

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
	uint32_t texture_index;
	uint16_t height;
	uint16_t drawflags;
} tile_t;

static tile_t tilemap[TILEMAP_HEIGHT][TILEMAP_WIDTH];

const uint8_t rott_wall1[] __attribute__((aligned(4))) = {
	#embed "rott_wall1.64x64.palette_8bpp.twiddled"
};

static uint32_t palette_index = PALETTE_INVALID;

void construct_tilemap()
{
	uint32_t texture_index = texture_cache_raw_palette(64, 64, TEXTURE_TYPE_PAL8, TEXTURE_FLAG_TWIDDLED, palette_index, rott_wall1, sizeof(rott_wall1));

	if (texture_index == TEXTURE_INVALID)
	{
		printf("failed to transfer rott_wall1!\n");
		return;
	}

	for (int y = 0; y < TILEMAP_HEIGHT; y++)
	{
		for (int x = 0; x < TILEMAP_WIDTH; x++)
		{
			if (y == 0 || y == TILEMAP_HEIGHT - 1 || x == 0 || x == TILEMAP_WIDTH - 1)
				tilemap[y][x] = tile_t{ texture_index, 8, TILE_DRAWFLAG_NONE };
			else
				tilemap[y][x] = tile_t{ texture_index, 0, TILE_DRAWFLAG_NONE };
		}
	}

	tilemap[1][1] = tile_t{ texture_index, 16, TILE_DRAWFLAG_NONE };
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

float fremap(float value, float a1, float a2, float b1, float b2)
{
	return b1 + (value - a1) * (b2 - b1) / (a2 - a1);
}

float fwrap(float value, float mod)
{
	float cmp = value < 0;
	return cmp * mod + __builtin_fmodf(value, mod) - cmp;
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

int iremap(int value, int a1, int a2, int b1, int b2)
{
	return b1 + (value - a1) * (b2 - b1) / (a2 - a1);
}

int iwrap(int value, int mod)
{
	int cmp = value < 0;
	return cmp * mod + (value % mod) - cmp;
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
			float wall_y;

			// get wall impact point
			if (!hit.side)
				wall_x = camera.origin[1] + dist * hit.ray_dir[1];
			else
				wall_x = camera.origin[0] + dist * hit.ray_dir[0];

			wall_x -= (int)wall_x;

			wall_y = 1; // fremap(0, line_start, line_end, 0, tile->height * 0.125f);

			store_queue_ix = transfer_ta_global_polygon(store_queue_ix, tile->texture_index);

			vec3 vpa = (vec3){(float)x, (float)line_start_c, 0.1f};
			vec3 vpb = (vec3){(float)x + 1, (float)line_start_c, 0.1f};
			vec3 vpc = (vec3){(float)x + 1, (float)line_end_c, 0.1f};
			vec3 vpd = (vec3){(float)x, (float)line_end_c, 0.1f};

			vec2 vta = (vec2){0, 0};
			vec2 vtb = (vec2){wall_x, 0};
			vec2 vtc = (vec2){wall_x, wall_y};
			vec2 vtd = (vec2){0, wall_y};

			store_queue_ix = transfer_ta_vertex_quad(store_queue_ix,
														vpa[0], vpa[1], vpa[2], vta[0], vta[1], 0,
														vpb[0], vpb[1], vpb[2], vtb[0], vtb[1], 0,
														vpc[0], vpc[1], vpc[2], vtc[0], vtc[1], 0,
														vpd[0], vpd[1], vpd[2], vtd[0], vtd[1], 0);
		}

		if (hit_type == RAY_HIT_WALL)
			ystart = line_start_c;
	}

	store_queue_ix = transfer_ta_global_end_of_list(store_queue_ix);
}

void transfer_scene()
{
	{
		using namespace sh7091;
		using sh7091::sh7091;

		// set the store queue destination address to the TA Polygon Converter FIFO
		sh7091.CCN.QACR0 = sh7091::ccn::qacr0::address(ta_fifo_polygon_converter);
		sh7091.CCN.QACR1 = sh7091::ccn::qacr1::address(ta_fifo_polygon_converter);
	}

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

const uint8_t rott_palette[] __attribute__((aligned(4))) = {
  #embed "rott_palette.dat"
};

uint32_t palette[256];

#define PACK_ARGB8888(r, g, b, a) (((a) << 24) | ((r) << 16) | ((g) << 8) | ((b) << 0))

void realmain()
{
	transfer_init();

	transfer_init_palette(PALETTE_TYPE_ARGB8888, 0xFF);

	for (int i = 0; i < 256; i++)
		palette[i] = PACK_ARGB8888(rott_palette[i * 3 + 0], rott_palette[i * 3 + 1], rott_palette[i * 3 + 2], 255);

	palette_index = transfer_palette(palette, 256);

	construct_tilemap();

	glm_vec3_copy((vec3){TILEMAP_WIDTH / 2, TILEMAP_HEIGHT / 2, 0.5}, camera.origin);
	camera.yaw = 0;
	camera.horizon = SCREEN_HEIGHT / 2;

	maple_init();

	while (1)
	{
		maple_ft0_data_t maple_data;
		if (maple_read_ft0(&maple_data, 0))
		{
			if (!(maple_data.digital_button & (1 << 7)))
				camera.yaw -= 0.05f;
			else if (!(maple_data.digital_button & (1 << 6)))
				camera.yaw += 0.05f;
		}

		transfer_frame_start();

		transfer_scene();

		transfer_frame_end();
	}
}

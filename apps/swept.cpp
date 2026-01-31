
#include "runtime.h"

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

#include "pvr.h"
#include "texture_cache.h"
#include "maple.h"

static inline uint32_t transfer_ta_vertex_quad(uint32_t store_queue_ix,
                                               float ax, float ay, float az, uint32_t ac,
                                               float bx, float by, float bz, uint32_t bc,
                                               float cx, float cy, float cz, uint32_t cc,
                                               float dx, float dy, float dz, uint32_t dc)
{
  using namespace holly::ta;
  using namespace holly::ta::parameter;

  //
  // TA polygon vertex transfer
  //

  volatile vertex_parameter::polygon_type_0 * vertex = (volatile vertex_parameter::polygon_type_0 *)&store_queue[store_queue_ix];
  store_queue_ix += (sizeof (vertex_parameter::polygon_type_0)) * 4;

  vertex[0].parameter_control_word = parameter_control_word::para_type::vertex_parameter;
  vertex[0].x = ax;
  vertex[0].y = ay;
  vertex[0].z = az;
  vertex[0].base_color = ac;

  // start store queue transfer of `vertex[0]` to the TA
  pref(&vertex[0]);

  vertex[1].parameter_control_word = parameter_control_word::para_type::vertex_parameter;
  vertex[1].x = bx;
  vertex[1].y = by;
  vertex[1].z = bz;
  vertex[1].base_color = bc;

  // start store queue transfer of `vertex[1]` to the TA
  pref(&vertex[1]);

  vertex[2].parameter_control_word = parameter_control_word::para_type::vertex_parameter;
  vertex[2].x = dx;
  vertex[2].y = dy;
  vertex[2].z = dz;
  vertex[2].base_color = dc;

  // start store queue transfer of `params[2]` to the TA
  pref(&vertex[2]);

  vertex[3].parameter_control_word = parameter_control_word::para_type::vertex_parameter
                                   | parameter_control_word::end_of_strip;
  vertex[3].x = cx;
  vertex[3].y = cy;
  vertex[3].z = cz;
  vertex[3].base_color = cc;

  // start store queue transfer of `params[3]` to the TA
  pref(&vertex[3]);

  return store_queue_ix;
}

struct line_t {
	vec2 x;
	vec2 y;
};

struct box_t {
	vec2 mins;
	vec2 maxs;
	vec2 origin;
	vec2 velocity;
};

struct world_t {
	line_t* lines;
	size_t num_lines;
	box_t* boxes;
	size_t num_boxes;
};

static float sweptaabb(box_t& b1, box_t& b2, vec2 normal)
{
	vec2 b1_origin, b1_size;
	vec2 b2_origin, b2_size;

	float xInvEntry, yInvEntry;
	float xInvExit, yInvExit;

	glm_vec2_add(b1.origin, b1.mins, b1_origin);
	glm_vec2_add(b1.origin, b1.maxs, b1_size);
	glm_vec2_sub(b1_size, b1_origin, b1_size);

	glm_vec2_add(b2.origin, b2.mins, b2_origin);
	glm_vec2_add(b2.origin, b2.maxs, b2_size);
	glm_vec2_sub(b2_size, b2_origin, b2_size);

	// find the distance between the objects on the near and far sides for both x and y
	if (b1.velocity[0] > 0.0f)
	{
		xInvEntry = b2_origin[0] - (b1_origin[0] + b1_size[0]);
		xInvExit = (b2_origin[0] + b2_size[0]) - b1_origin[0];
	}
	else
	{
		xInvEntry = (b2_origin[0] + b2_size[0]) - b1_origin[0];
		xInvExit = b2_origin[0] - (b1_origin[0] + b1_size[0]);
	}

	if (b1.velocity[1] > 0.0f)
	{
		yInvEntry = b2_origin[1] - (b1_origin[1] + b1_size[1]);
		yInvExit = (b2_origin[1] + b2_size[1]) - b1_origin[1];
	}
	else
	{
		yInvEntry = (b2_origin[1] + b2_size[1]) - b1_origin[1];
		yInvExit = b2_origin[1] - (b1_origin[1] + b1_size[1]);
	}

	// find time of collision and time of leaving for each axis (if statement is to prevent divide by zero)
	float xEntry, yEntry;
	float xExit, yExit;

	if (b1.velocity[0] == 0.0f)
	{
		xEntry = -__builtin_inff();
		xExit = __builtin_inff();
	}
	else
	{
		xEntry = xInvEntry / b1.velocity[0];
		xExit = xInvExit / b1.velocity[0];
	}

	if (b1.velocity[1] == 0.0f)
	{
		yEntry = -__builtin_inff();
		yExit = __builtin_inff();
	}
	else
	{
		yEntry = yInvEntry / b1.velocity[1];
		yExit = yInvExit / b1.velocity[1];
	}

	float entryTime = fmax(xEntry, yEntry);
	float exitTime = fmin(xExit, yExit);

	// if there was no collision
	if (entryTime > exitTime || xEntry < 0.0f && yEntry < 0.0f || xEntry > 1.0f || yEntry > 1.0f)
	{
		glm_vec2_zero(normal);
		return 1.0f;
	}
	else // if there was a collision
	{
		// calculate normal of collided surface
		if (xEntry > yEntry)
		{
			if (xInvEntry < 0.0f)
			{
				normal[0] = 1.0f;
				normal[1] = 0.0f;
			}
			else
			{
				normal[0] = -1.0f;
				normal[1] = 0.0f;
			}
		}
		else
		{
			if (yInvEntry < 0.0f)
			{
				normal[0] = 0.0f;
				normal[1] = 1.0f;
			}
			else
			{
				normal[0] = 0.0f;
				normal[1] = -1.0f;
			}
		}

		// return the time of collision
		return entryTime;
	}
}

static void transfer_scene(world_t* world)
{
	{
		using namespace sh7091;
		using sh7091::sh7091;

		// set the store queue destination address to the TA Polygon Converter FIFO
		sh7091.CCN.QACR0 = sh7091::ccn::qacr0::address(ta_fifo_polygon_converter);
		sh7091.CCN.QACR1 = sh7091::ccn::qacr1::address(ta_fifo_polygon_converter);
	}

	uint32_t store_queue_ix = 0;

	for (size_t i = 0; i < world->num_boxes; i++)
	{
		vec3 vp[4] = {{0, 0, 0.1f}, {0, 0, 0.1f}, {0, 0, 0.1f}};
		uint32_t vc[4] = {0xff0000, 0x00ff00, 0x0000ff, 0xffff00};

		vp[0][0] = world->boxes[i].origin[0] + world->boxes[i].mins[0];
		vp[0][1] = world->boxes[i].origin[1] + world->boxes[i].mins[1];

		vp[1][0] = world->boxes[i].origin[0] + world->boxes[i].mins[0];
		vp[1][1] = world->boxes[i].origin[1] + world->boxes[i].maxs[1];

		vp[2][0] = world->boxes[i].origin[0] + world->boxes[i].maxs[0];
		vp[2][1] = world->boxes[i].origin[1] + world->boxes[i].maxs[1];

		vp[3][0] = world->boxes[i].origin[0] + world->boxes[i].maxs[0];
		vp[3][1] = world->boxes[i].origin[1] + world->boxes[i].mins[1];

		store_queue_ix = transfer_ta_global_polygon(store_queue_ix, TEXTURE_INVALID);

		store_queue_ix = transfer_ta_vertex_quad(store_queue_ix,
													vp[0][0], vp[0][1], vp[0][2], vc[0],
													vp[1][0], vp[1][1], vp[1][2], vc[1],
													vp[2][0], vp[2][1], vp[2][2], vc[2],
													vp[3][0], vp[3][1], vp[3][2], vc[3]);
	}

	store_queue_ix = transfer_ta_global_end_of_list(store_queue_ix);
}

#define countof(a) (sizeof(a)/sizeof(*(a)))

void main()
{
	//////////////////////////////////////////////////////////////////////////////
	// initialize holly graphics units
	//////////////////////////////////////////////////////////////////////////////

	transfer_init();

	//////////////////////////////////////////////////////////////////////////////
	// initialize maple unit
	//////////////////////////////////////////////////////////////////////////////

	maple_init();

	//////////////////////////////////////////////////////////////////////////////
	// setup scene
	//////////////////////////////////////////////////////////////////////////////

	world_t world;
	line_t lines[5];
	box_t boxes[2];

	// lines
	glm_vec2_copy((vec2){0, 0}, lines[0].x);
	glm_vec2_copy((vec2){640, 0}, lines[0].y);
	glm_vec2_copy((vec2){640, 0}, lines[1].x);
	glm_vec2_copy((vec2){640, 480}, lines[1].y);
	glm_vec2_copy((vec2){640, 480}, lines[2].x);
	glm_vec2_copy((vec2){0, 480}, lines[2].y);
	glm_vec2_copy((vec2){0, 480}, lines[3].x);
	glm_vec2_copy((vec2){0, 0}, lines[3].y);
	glm_vec2_copy((vec2){0, 480 - 16}, lines[4].x);
	glm_vec2_copy((vec2){640, 480 - 64}, lines[4].y);

	// boxes
	glm_vec2_copy((vec2){640/2, 480/2}, boxes[0].origin);
	glm_vec2_copy((vec2){-16, -16}, boxes[0].mins);
	glm_vec2_copy((vec2){16, 16}, boxes[0].maxs);
	glm_vec2_zero(boxes[0].velocity);

	glm_vec2_copy((vec2){640/2, 480-16}, boxes[1].origin);
	glm_vec2_copy((vec2){-320, -16}, boxes[1].mins);
	glm_vec2_copy((vec2){320, 16}, boxes[1].maxs);
	glm_vec2_zero(boxes[1].velocity);

	// world
	world.lines = lines;
	world.num_lines = countof(lines);
	world.boxes = boxes;
	world.num_boxes = countof(boxes);

	//////////////////////////////////////////////////////////////////////////////
	// main loop
	//////////////////////////////////////////////////////////////////////////////

	while (1)
	{
		//////////////////////////////////////////////////////////////////////////////
		// maple
		//////////////////////////////////////////////////////////////////////////////

		maple_ft0_data_t maple_data;
		if (maple_read_ft0(&maple_data, 0))
		{
			if (!(maple_data.digital_button & (1 << 7)))
				world.boxes[0].velocity[0] += 300 * 0.01f;
			else if (!(maple_data.digital_button & (1 << 6)))
				world.boxes[0].velocity[0] -= 300 * 0.01f;
		}

		//////////////////////////////////////////////////////////////////////////////
		// physics
		//////////////////////////////////////////////////////////////////////////////

		world.boxes[0].velocity[1] += 300 * 0.01f;

		vec2 normal = GLM_VEC2_ZERO_INIT;

		float collisiontime = sweptaabb(world.boxes[0], world.boxes[1], normal);
		world.boxes[0].origin[0] += world.boxes[0].velocity[0] * collisiontime;
		world.boxes[0].origin[1] += world.boxes[0].velocity[1] * collisiontime;
		float remainingtime = 1.0f - collisiontime;
		float dotprod = (world.boxes[0].velocity[0] * normal[1] + world.boxes[0].velocity[1] * normal[0]) * remainingtime;
		world.boxes[0].velocity[0] = dotprod * normal[1];
		world.boxes[0].velocity[1] = dotprod * normal[0];

		printf("%f %f\n", world.boxes[0].velocity[0], world.boxes[0].velocity[1]);

		//////////////////////////////////////////////////////////////////////////////
		// start the holly frame
		//////////////////////////////////////////////////////////////////////////////

		transfer_frame_start();

		//////////////////////////////////////////////////////////////////////////////
		// render models
		//////////////////////////////////////////////////////////////////////////////

		transfer_scene(&world);

		//////////////////////////////////////////////////////////////////////////////
		// end the holly frame
		//////////////////////////////////////////////////////////////////////////////

		transfer_frame_end();
	}
}

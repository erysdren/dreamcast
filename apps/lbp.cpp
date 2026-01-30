
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

#include "interrupt.cpp"

#include "tinyphysicsengine.h"

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

static void transfer_scene(TPE_World* world)
{
	{
		using namespace sh7091;
		using sh7091::sh7091;

		// set the store queue destination address to the TA Polygon Converter FIFO
		sh7091.CCN.QACR0 = sh7091::ccn::qacr0::address(ta_fifo_polygon_converter);
		sh7091.CCN.QACR1 = sh7091::ccn::qacr1::address(ta_fifo_polygon_converter);
	}

	uint32_t store_queue_ix = 0;

	for (int i = 0; i < world->bodyCount; i++)
	{
		if (world->bodies[i].jointCount == 5)
		{
			vec3 vp[4] = {{0, 0, 0.1f}, {0, 0, 0.1f}, {0, 0, 0.1f}};
			uint32_t vc[4] = {0xff0000, 0x00ff00, 0x0000ff, 0xffff00};

			for (int j = 0; j < 4; j++)
			{
				vp[j][0] = world->bodies[i].joints[j].position.x / TPE_F * 128;
				vp[j][1] = world->bodies[i].joints[j].position.y / TPE_F * 128;
				vp[j][2] = 0.1f;

				printf("%f %f %f\n", vp[j][0], vp[j][1], vp[j][2]);
			}

			store_queue_ix = transfer_ta_global_polygon(store_queue_ix, TEXTURE_INVALID);

			store_queue_ix = transfer_ta_vertex_quad(store_queue_ix,
														vp[0][0], vp[0][1], vp[0][2], vc[0],
														vp[1][0], vp[1][1], vp[1][2], vc[1],
														vp[2][0], vp[2][1], vp[2][2], vc[2],
														vp[3][0], vp[3][1], vp[3][2], vc[3]);
		}
	}

	store_queue_ix = transfer_ta_global_end_of_list(store_queue_ix);
}

#define ROOM_W (TPE_F * 10)
#define ROOM_H ((480.0f * ROOM_W) / 640.0f)

TPE_Vec3 environmentDistance(TPE_Vec3 p, TPE_Unit maxD)
{
	return TPE_envAABoxInside(p,TPE_vec3(0,0,0),TPE_vec3(ROOM_W,ROOM_H,ROOM_W));
}

void realmain()
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

	TPE_World world;
	TPE_Joint joints[64];
	TPE_Connection connections[64];
	TPE_Body bodies[4];

	// add bodies
	int joints_used = 0;
	int connections_used = 0;
	for (int i = 0; i < 4; i++)
	{
		if (i != 2)
		{
			TPE_makeCenterRectFull(joints + joints_used, connections + connections_used, TPE_F, TPE_F, TPE_F / 5);

			TPE_bodyInit(&bodies[i], &joints[joints_used], 5, &connections[connections_used], 10, TPE_F / 5);

			TPE_bodyRotateByAxis(&bodies[i], TPE_vec3(TPE_F / 4,0,0));

			bodies[i].joints[4].size *= 3; // make center point bigger

			joints_used += 5;
			connections_used += 10;
		}
		else
		{
			joints[joints_used] = TPE_joint(TPE_vec3(0,0,0), 6 * TPE_F / 5);

			TPE_bodyInit(&bodies[i], &joints[joints_used], 1, NULL, 0, TPE_F / 5);

			joints_used += 1;
			connections_used += 0;
		}

		bodies[i].friction = 4 * TPE_F / 5;
		bodies[i].elasticity = TPE_F / 5;

		TPE_bodyMoveBy(&bodies[i], TPE_vec3(-2 * TPE_F + i * 2 * TPE_F,0,0));
	}

	TPE_worldInit(&world, bodies, 4, environmentDistance);

	//////////////////////////////////////////////////////////////////////////////
	// main loop
	//////////////////////////////////////////////////////////////////////////////

	while (1)
	{
		//////////////////////////////////////////////////////////////////////////////
		// physics
		//////////////////////////////////////////////////////////////////////////////

		// step world physics
		TPE_worldStep(&world);

		// apply gravity
		for (int i = 0; i < world.bodyCount; i++)
			TPE_bodyApplyGravity(&world.bodies[i], TPE_F / -100);

		// keep positions and velocity in 2d
		for (int i = 0; i < world.bodyCount; i++)
		{
			for (int j = 0; j < world.bodies[i].jointCount; j++)
			{
				world.bodies[i].joints[j].position.z = 0;
				world.bodies[i].joints[j].velocity[2] = 0;
			}
		}

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


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

#define ROMFS_SOURCE_FILENAME "peggle.pk3.h"
#include "romfs.c"

#include "pvr.h"
#include "texture_cache.h"
#include "maple.h"

#include "interrupt.cpp"

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

static inline uint32_t transfer_ta_global_polygon(uint32_t store_queue_ix, uint32_t texture_index)
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

	const auto* t = texture_cache_get(texture_index);

	polygon->tsp_instruction_word = tsp_instruction_word::src_alpha_instr::src_alpha
									| tsp_instruction_word::dst_alpha_instr::inverse_src_alpha
									| tsp_instruction_word::fog_control::no_fog
									| tsp_instruction_word::filter_mode::bilinear_filter
									| tsp_instruction_word::texture_shading_instruction::decal
									| t->tsp_instruction_word;

	polygon->texture_control_word = t->texture_control_word;

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

static inline void vertex_perspective_divide(vec3 v)
{
	float w = 1.0f / (v[2] + 1.0f);
	v[0] = v[0] * w;
	v[1] = v[1] * w;
	v[2] = w;
}

static inline void vertex_screen_space(vec3 v)
{
	v[0] = v[0] * 240.0f + 320.f;
	v[1] = v[1] * 240.0f + 240.f;
}

#define NUM_TEXTURES 5
#define BALL_TEXTURE textures[0]
#define BLUE_PEG_OFF_TEXTURE textures[1]
#define BLUE_PEG_ON_TEXTURE textures{2}
#define ORANGE_PEG_OFF_TEXTURE textures[3]
#define ORANGE_PEG_ON_TEXTURE textures[4]

static const char *texture_filenames[NUM_TEXTURES] = {
	"textures/peggle/ball.pvr", // ball
	"textures/peggle/peg0a.pvr", // blue peg (off)
	"textures/peggle/peg0b.pvr", // blue peg (on)
	"textures/peggle/peg1a.pvr", // orange peg (off)
	"textures/peggle/peg1b.pvr" // orange peg (on)
};

static uint32_t textures[NUM_TEXTURES] = {
	TEXTURE_INVALID, // ball
	TEXTURE_INVALID, // blue peg (off)
	TEXTURE_INVALID, // blue peg (on)
	TEXTURE_INVALID, // orange peg (off)
	TEXTURE_INVALID // orange peg (on)
};

static void transfer_textures()
{
	for (int i = 0; i < NUM_TEXTURES; i++)
		textures[i] = texture_cache_pvr((const pvr_t *)ROMFS_GetFileFromPath(texture_filenames[i], NULL));
}

struct ball_t {
	vec2 origin;
	vec2 velocity;
	uint32_t texture;
	float radius;
	float mass;
	bool active; ///< take collisions
	bool react; ///< physically react to hits
	float scale;
};

#define NUM_PEGS_X 12
#define NUM_PEGS_Y 6

static ball_t ball;
static ball_t pegs[NUM_PEGS_X * NUM_PEGS_Y];

static uint32_t transfer_ball(uint32_t store_queue_ix, ball_t& ball)
{
	uint32_t vc[4] = {0, 0, 0, 0};
	vec3 vp[4] = {{0, 0, 0.1}, {0, 1, 0.1}, {1, 1, 0.1}, {1, 0, 0.1}};
	vec2 vt[4] = {{0, 0}, {0, 1}, {1, 1}, {1, 0}};

	vp[0][0] = ball.origin[0] - ball.radius * ball.scale;
	vp[0][1] = ball.origin[1] - ball.radius * ball.scale;

	vp[1][0] = ball.origin[0] - ball.radius * ball.scale;
	vp[1][1] = ball.origin[1] + ball.radius * ball.scale;

	vp[2][0] = ball.origin[0] + ball.radius * ball.scale;
	vp[2][1] = ball.origin[1] + ball.radius * ball.scale;

	vp[3][0] = ball.origin[0] + ball.radius * ball.scale;
	vp[3][1] = ball.origin[1] - ball.radius * ball.scale;

	store_queue_ix = transfer_ta_global_polygon(store_queue_ix, ball.texture);

	store_queue_ix = transfer_ta_vertex_quad(store_queue_ix,
												vp[0][0], vp[0][1], vp[0][2], vt[0][0], vt[0][1], vc[0],
												vp[1][0], vp[1][1], vp[1][2], vt[1][0], vt[1][1], vc[1],
												vp[2][0], vp[2][1], vp[2][2], vt[2][0], vt[2][1], vc[2],
												vp[3][0], vp[3][1], vp[3][2], vt[3][0], vt[3][1], vc[3]);

	return store_queue_ix;
}

static void transfer_scene()
{
	{
		using namespace sh7091;
		using sh7091::sh7091;

		// set the store queue destination address to the TA Polygon Converter FIFO
		sh7091.CCN.QACR0 = sh7091::ccn::qacr0::address(ta_fifo_polygon_converter);
		sh7091.CCN.QACR1 = sh7091::ccn::qacr1::address(ta_fifo_polygon_converter);
	}

	uint32_t vc[4] = {0, 0, 0, 0};
	vec3 vp[4] = {{0, 0, 0.1}, {0, 1, 0.1}, {1, 1, 0.1}, {1, 0, 0.1}};
	vec2 vt[4] = {{0, 0}, {0, 1}, {1, 1}, {1, 0}};

	uint32_t store_queue_ix = 0;

	// draw ball
	store_queue_ix = transfer_ball(store_queue_ix, ball);

	// draw pegs
	for (int y = 0; y < NUM_PEGS_Y; y++)
	{
		for (int x = 0; x < NUM_PEGS_X; x++)
		{
			auto& peg = pegs[y * NUM_PEGS_X + x];

			if (!peg.active)
				continue;

			store_queue_ix = transfer_ball(store_queue_ix, peg);
		}
	}

	store_queue_ix = transfer_ta_global_end_of_list(store_queue_ix);
}

ball_t* closest_peg(vec2 origin, float maxdist)
{
	int pen = -1;
	float dist = FLT_MAX;

	for (int i = 0; i < NUM_PEGS_X * NUM_PEGS_Y; i++)
	{
		if (!pegs[i].active)
			continue;

		float rank = glm_vec2_distance(origin, pegs[i].origin);

		if (rank > maxdist)
			continue;

		if (rank < dist)
		{
			pen  = i;
			dist = rank;
		}
	}

	if (pen < 0)
		return NULL;

	return &pegs[pen];
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
	// transfer the texture images to texture ram
	//////////////////////////////////////////////////////////////////////////////

	transfer_textures();

	//////////////////////////////////////////////////////////////////////////////
	// setup map
	//////////////////////////////////////////////////////////////////////////////

	ball.radius = 8;
	ball.mass = 200;
	ball.origin[0] = (640/2) - (ball.radius / 2);
	ball.origin[1] = 16;
	ball.active = true;
	ball.react = true;
	ball.texture = BALL_TEXTURE;
	ball.scale = 1;
	glm_vec2_zero(ball.velocity);

	for (int y = 0; y < NUM_PEGS_Y; y++)
	{
		for (int x = 0; x < NUM_PEGS_X; x++)
		{
			auto& peg = pegs[y * NUM_PEGS_X + x];

			peg.radius = 8;
			peg.mass = 200;
			peg.origin[0] = (640/2) - (NUM_PEGS_X*32/2) + (x * 32);
			peg.origin[1] = (480/2) - (NUM_PEGS_Y*32/2) + (y * 32);
			peg.active = true;
			peg.react = false;
			peg.texture = BLUE_PEG_OFF_TEXTURE;
			peg.scale = 2;
			glm_vec2_zero(peg.velocity);

			if (y % 2)
				peg.origin[0] += 16;
			else
				peg.origin[0] -= 8;
		}
	}

	//////////////////////////////////////////////////////////////////////////////
	// main loop
	//////////////////////////////////////////////////////////////////////////////

	while (1)
	{
#if 0
		//////////////////////////////////////////////////////////////////////////////
		// maple
		//////////////////////////////////////////////////////////////////////////////

		maple_ft0_data_t maple_data;
		if (maple_read_ft0(&maple_data, 0))
		{
			if (!(maple_data.digital_button & (1 << 7)))
				r_camera.angles[1] += 2;
			else if (!(maple_data.digital_button & (1 << 6)))
				r_camera.angles[1] -= 2;

			if (!(maple_data.digital_button & (1 << 4)))
				pmove.cmd.forwardmove = 100;
			else if (!(maple_data.digital_button & (1 << 5)))
				pmove.cmd.forwardmove = -100;
			else
				pmove.cmd.forwardmove = 0;
		}
#endif

		//////////////////////////////////////////////////////////////////////////////
		// physics
		//////////////////////////////////////////////////////////////////////////////

		// create acceleration vector
		vec2 accel = GLM_VEC2_ZERO_INIT;

		// apply gravity
		accel[1] += 300;

		// do peg collision
		float dt = 0.01f;
		float trace_fraction = 1.0f;
		vec2 trace_endpos;
		glm_vec2_muladds(accel, dt, ball.velocity);
		glm_vec2_copy(ball.origin, trace_endpos);
		for (int bump = 0; bump < 3; bump++)
		{
			vec2 neworg, temp;
			glm_vec2_scale(ball.velocity, dt, neworg);
			glm_vec2_add(trace_endpos, neworg, trace_endpos);
			auto* peg = closest_peg(trace_endpos, FLT_MAX);
			trace_fraction = 1.0f;
			if (peg)
			{
				// resolve collision
				float closestdistsq = glm_vec2_distance2(peg->origin, trace_endpos);
				float dist = pow(ball.radius + peg->radius, 2);
				if (closestdistsq <= dist)
				{
					trace_fraction = 1.0f - (dist / closestdistsq);
					float p;
					vec2 c, n, ball_velocity_normalized, peg_origin_minus_c;
					vec2 peg_mass_times_n;
					vec2 ball_mass_times_n;
					// vec2 c = trace_endpos - sqrtf(dist - closestdistsq) * normalize(ball.velocity);
					glm_vec2_normalize_to(ball.velocity, ball_velocity_normalized);
					glm_vec2_copy(trace_endpos, c);
					glm_vec2_mulsubs(ball_velocity_normalized, sqrtf(dist - closestdistsq), c);
					// vec2 n = normalize(peg->origin - c);
					glm_vec2_sub(peg->origin, c, peg_origin_minus_c);
					glm_vec2_normalize_to(peg_origin_minus_c, n);
					// float p = 2.0f * (ball.velocity * n) / (ball.mass + peg->mass);
					p = 2.0f * glm_vec2_dot(ball.velocity, n) / (ball.mass + peg->mass);
					// ball.velocity -= p * ball.mass * n + p * peg->mass * n
					glm_vec2_scale(n, p * peg->mass, peg_mass_times_n);
					glm_vec2_scale(n, p * ball.mass, ball_mass_times_n);
					glm_vec2_addsub(ball_mass_times_n, peg_mass_times_n, ball.velocity);
				}
			}
			else
			{
				// no pegs around
				break;
			}

			dt *= 1.0f - trace_fraction;
		}

		glm_vec2_copy(trace_endpos, ball.origin);

#if 0
		//////////////////////////////////////////////////////////////////////////////
		// physics
		//////////////////////////////////////////////////////////////////////////////

		// get direction vectors
		vec3 v_forward, v_right, v_up;
		makevectors(r_camera.angles, v_forward, v_right, v_up);

		// create acceleration vector
		vec3 accel;
		glm_vec3_scale(v_forward, 6, accel);

		// apply gravity
		// accel[2] -= 300;

		// do world collision
		float friction = 1;
		ibsp_trace_t trace;
		float dt = 0.01f;
		glm_vec3_muladds(accel, dt, velocity);
		glm_vec3_copy(r_camera.origin, trace.end);
		for (int bump = 0; bump < 3; bump++)
		{
			vec3 p, temp;
			glm_vec3_scale(velocity, dt, p);
			glm_vec3_add(trace.end, p, p);
			ibsp_trace(&ibsp, &trace, trace.end, p, (vec3){-32, -32, -36}, (vec3){32, 32, 36});
			if (trace.fraction == 1.0f)
				break;
			glm_vec3_mulsubs(trace.plane, glm_vec3_dot(velocity, trace.plane), velocity);
			glm_vec3_scale(velocity, friction, temp);
			glm_vec3_mulsubs(temp, dt, velocity);
			dt *= 1.0f - trace.fraction;
		}

		glm_vec3_copy(trace.end, r_camera.origin);
#endif

		//////////////////////////////////////////////////////////////////////////////
		// start the holly frame
		//////////////////////////////////////////////////////////////////////////////

		transfer_frame_start();

		//////////////////////////////////////////////////////////////////////////////
		// render models
		//////////////////////////////////////////////////////////////////////////////

		transfer_scene();

		//////////////////////////////////////////////////////////////////////////////
		// end the holly frame
		//////////////////////////////////////////////////////////////////////////////

		transfer_frame_end();
	}
}

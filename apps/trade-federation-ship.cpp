
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

#define ROMFS_SOURCE_FILENAME "trade-federation-ship.pk3.h"
#include "romfs.c"

#include "pvr.h"
#include "ibsp.h"
#include "texture_cache.h"

static uint32_t r_ibsp_shader_textures[256];
static uint32_t r_ibsp_lightmap_textures[256];

static mat4 q3_world_matrix = {{0, -1, 0, 0}, {0, 0, 1, 0}, {-1, 0, 0, 0}, {0, 0, 0, 1}};

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

static inline uint32_t transfer_ta_global_polygon_lightmap(uint32_t store_queue_ix, uint32_t texture_index)
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

	polygon->tsp_instruction_word = tsp_instruction_word::src_alpha_instr::other_color
									| tsp_instruction_word::dst_alpha_instr::zero
									| tsp_instruction_word::fog_control::no_fog
									| tsp_instruction_word::filter_mode::bilinear_filter
									| tsp_instruction_word::texture_shading_instruction::decal;

	const auto* t = texture_cache_get(texture_index);

	polygon->tsp_instruction_word |= t->tsp_instruction_word;

	polygon->texture_control_word = t->texture_control_word;

	// start store queue transfer of `polygon` to the TA
	pref(polygon);

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

	polygon->tsp_instruction_word = tsp_instruction_word::src_alpha_instr::one
									| tsp_instruction_word::dst_alpha_instr::zero
									| tsp_instruction_word::fog_control::no_fog
									| tsp_instruction_word::filter_mode::bilinear_filter
									| tsp_instruction_word::texture_shading_instruction::decal;

	const auto* t = texture_cache_get(texture_index);

	polygon->tsp_instruction_word |= t->tsp_instruction_word;

	polygon->texture_control_word = t->texture_control_word;

	// start store queue transfer of `polygon` to the TA
	pref(polygon);

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

static ibsp_t ibsp;
static bool r_visible_leafs[IBSP_MAX_LEAFS];
static bool r_visible_faces[IBSP_MAX_FACES];
static bool r_use_vis = true;
static bool r_use_lightmaps = true;
static uint32_t r_num_visible_leafs = 0;
static int32_t r_camera_cluster = -1;
static int32_t r_camera_prev_cluster = -1;

static camera_t r_camera;

ibsp_leaf& leaf_for_point(vec3 p)
{
	int32_t index = 0;

	while (index >= 0)
	{
		ibsp_node_t *node = &ibsp.nodes[index];
		ibsp_plane_t *plane = &ibsp.planes[node->plane];
		float dist = glm_vec3_dot(plane->normal, p) - plane->dist;

		if (dist >= 0)
			index = node->children[0];
		else
			index = node->children[1];
	}

	return ibsp.leafs[-index - 1];
}

uint32_t mark_visible_leafs(int32_t cluster)
{
	size_t i, j;
	uint32_t num_visible = 0;

	memset(r_visible_leafs, 0, sizeof(r_visible_leafs));
	memset(r_visible_faces, 0, sizeof(r_visible_faces));

	for (i = 0; i < ibsp.num_leafs; i++)
	{
		ibsp_leaf_t *leaf = &ibsp.leafs[i];
		int32_t v = (cluster * ibsp.visdata->len_vec) + (ibsp.leafs[i].cluster >> 3);

		// check if this cluster is visible from the source cluster
		if (!r_use_vis || cluster == -1 || ibsp.visdata->vecs[v] & (1 << (ibsp.leafs[i].cluster & 7)))
		{
			r_visible_leafs[i] = true;

			// mark faces as visible
			for (j = leaf->first_leafface; j < leaf->first_leafface + leaf->num_leaffaces; j++)
			{
				ibsp_face_t *face = &ibsp.faces[ibsp.leaffaces[j]];

				r_visible_faces[ibsp.leaffaces[j]] = true;
			}

			num_visible++;
		}
	}

	return num_visible;
}

static void transfer_ibsp(mat4 mvp)
{
	{
		using namespace sh7091;
		using sh7091::sh7091;

		// set the store queue destination address to the TA Polygon Converter FIFO
		sh7091.CCN.QACR0 = sh7091::ccn::qacr0::address(ta_fifo_polygon_converter);
		sh7091.CCN.QACR1 = sh7091::ccn::qacr1::address(ta_fifo_polygon_converter);
	}

	uint32_t store_queue_ix = 0;

	for (int i = 0; i < ibsp.num_faces; i++)
	{
		if (!r_visible_faces[i])
			continue;

		ibsp_face_t *face = ibsp.faces + i;

		if (strncmp(ibsp.textures[face->texture].name, "textures/", 9) != 0)
			continue;

		for (int j = 0; j < face->num_meshverts; j += 3)
		{
			bool skip = false;
			vec3 vp[3];
			vec2 vt[3];
			uint32_t vc[3];

			for (int k = 0; k < 3; k++)
			{
				ibsp_vertex_t *vertex = &ibsp.vertices[face->first_vert + ibsp.meshverts[face->first_meshvert + j + k]];
				glm_vec3_copy(vertex->position, vp[k]);
				glm_vec2_copy(vertex->texcoords[0], vt[k]);
				vc[k] = 0;

				glm_mat4_mulv3(mvp, vp[k], 1.0f, vp[k]);
				vertex_perspective_divide(vp[k]);
				vertex_screen_space(vp[k]);

				if (vp[k][2] < 0)
				{
					skip = true;
					break;
				}
			}

			if (skip)
				continue;

			store_queue_ix = transfer_ta_global_polygon(store_queue_ix, r_ibsp_shader_textures[face->texture]);

			if (r_ibsp_shader_textures[face->texture] == TEXTURE_INVALID)
				printf("invalid shader texture: %s\n", ibsp.textures[face->texture].name);

			store_queue_ix = transfer_ta_vertex_triangle(store_queue_ix,
														vp[0][0], vp[0][1], vp[0][2], vt[0][0], vt[0][1], vc[0],
														vp[1][0], vp[1][1], vp[1][2], vt[1][0], vt[1][1], vc[1],
														vp[2][0], vp[2][1], vp[2][2], vt[2][0], vt[2][1], vc[2]);

			if (r_use_lightmaps)
			{
				for (int k = 0; k < 3; k++)
				{
					ibsp_vertex_t *vertex = &ibsp.vertices[face->first_vert + ibsp.meshverts[face->first_meshvert + j + k]];
					glm_vec2_copy(vertex->texcoords[1], vt[k]);
				}

				if (r_ibsp_lightmap_textures[face->lightmap] == TEXTURE_INVALID)
					printf("invalid shader lightmap: %u\n", face->lightmap);

				store_queue_ix = transfer_ta_global_polygon_lightmap(store_queue_ix, r_ibsp_lightmap_textures[face->lightmap]);

				store_queue_ix = transfer_ta_vertex_triangle(store_queue_ix,
															vp[0][0], vp[0][1], vp[0][2], vt[0][0], vt[0][1], vc[0],
															vp[1][0], vp[1][1], vp[1][2], vt[1][0], vt[1][1], vc[1],
															vp[2][0], vp[2][1], vp[2][2], vt[2][0], vt[2][1], vc[2]);
			}
		}
	}

	store_queue_ix = transfer_ta_global_end_of_list(store_queue_ix);
}

static void transfer_textures()
{
	for (int i = 0; i < ibsp.num_textures; i++)
	{
		char filename[256];
		strlcpy(filename, ibsp.textures[i].name, sizeof(filename));
		strlcat(filename, ".pvr", sizeof(filename));

		const pvr_t *pvr = (const pvr_t *)ROMFS_GetFileFromPath(filename, NULL);

		if (pvr)
		{
			r_ibsp_shader_textures[i] = texture_cache_pvr(pvr);
		}
		else
		{
			r_ibsp_shader_textures[i] = TEXTURE_INVALID;
		}
	}
}

#define PACK_RGB565(r, g, b) ((((r) >> 3) << 11) | (((g) >> 2) << 5) | (((b) >> 3) << 0))

static void transfer_lightmaps()
{
	uint16_t lightmap565[128][128];

	for (int i = 0; i < ibsp.num_lightmaps; i++)
	{
		ibsp_lightmap_t *lightmap = ibsp.lightmaps + i;

		for (int y = 0; y < 128; y++)
		{
			for (int x = 0; x < 128; x++)
			{
				uint8_t *rgb = lightmap->lightmap[y][x];
				lightmap565[y][x] = PACK_RGB565(rgb[0], rgb[1], rgb[2]);
			}
		}

		r_ibsp_lightmap_textures[i] = texture_cache_raw(128, 128, TEXTURE_TYPE_RGB565, TEXTURE_FLAG_NONE, lightmap565, sizeof(lightmap565));
	}
}

void main()
{
	transfer_init();

	//////////////////////////////////////////////////////////////////////////////
	// load bsp data
	//////////////////////////////////////////////////////////////////////////////

	if (!ibsp_load(ROMFS_GetFileFromPath("maps/trade-federation-ship.bsp", NULL), &ibsp))
		exit(1);

	//////////////////////////////////////////////////////////////////////////////
	// transfer the texture images to texture ram
	//////////////////////////////////////////////////////////////////////////////

	transfer_textures();
	transfer_lightmaps();

	//////////////////////////////////////////////////////////////////////////////
	// setup camera
	//////////////////////////////////////////////////////////////////////////////

	mat4 model = GLM_MAT4_IDENTITY_INIT, viewproj, mvp;
	camera_init(&r_camera);
	glm_vec3_copy((vec3){0, 0, 64}, r_camera.origin);

#if 0
	mat4 model = GLM_MAT4_IDENTITY_INIT, proj, view = GLM_MAT4_IDENTITY_INIT, viewproj, mvp;
	glm_lookat(r_camera.origin, (vec3){0, 0, 64}, (vec3){0, 0, -1}, view);
	glm_perspective(45, 640.0f/480.0f, 1.0f, 1024.0f, proj);
	glm_mat4_mul(proj, view, viewproj);
	glm_mat4_mul(viewproj, model, mvp);
#endif

	float theta = 0;
	while (1)
	{
		//////////////////////////////////////////////////////////////////////////////
		// update visible faces
		//////////////////////////////////////////////////////////////////////////////

		// get camera cluster
		r_camera_cluster = leaf_for_point(r_camera.origin).cluster;

		// mark visible leafs
		if (r_camera_prev_cluster != r_camera_cluster)
		{
			r_num_visible_leafs = mark_visible_leafs(r_camera_cluster);
			r_camera_prev_cluster = r_camera_cluster;
		}

		//////////////////////////////////////////////////////////////////////////////
		// update camera matrix
		//////////////////////////////////////////////////////////////////////////////

		glm_vec3_copy((vec3){0, theta, 0}, r_camera.angles);

		camera_make_viewproj(&r_camera, viewproj);
		glm_mat4_mul(viewproj, model, mvp);

		theta += 0.1f;

		//////////////////////////////////////////////////////////////////////////////
		// start the holly frame
		//////////////////////////////////////////////////////////////////////////////

		transfer_frame_start();

		//////////////////////////////////////////////////////////////////////////////
		// render models
		//////////////////////////////////////////////////////////////////////////////

		transfer_ibsp(mvp);

		//////////////////////////////////////////////////////////////////////////////
		// end the holly frame
		//////////////////////////////////////////////////////////////////////////////

		transfer_frame_end();
	}

	// return from main; this will effectively jump back to the serial loader
}


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

#define ROMFS_SOURCE_FILENAME "bh.pk3.h"
#include "romfs.c"

#include "pvr.h"
#include "ibsp.h"
#include "texture_cache.h"
#include "maple.h"

static uint32_t r_ibsp_shader_textures[256];
static uint32_t r_ibsp_lightmap_textures[256];

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

			store_queue_ix = transfer_ta_vertex_triangle_pt3(store_queue_ix,
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

				ta_global_polygon_t info = {
					r_ibsp_lightmap_textures[face->lightmap],
					SRC_ALPHA_INSTR_OTHER_COLOR,
					DST_ALPHA_INSTR_ZERO,
					FILTER_MODE_BILINEAR
				};

				store_queue_ix = transfer_ta_global_polygon_ex(store_queue_ix, &info);

				store_queue_ix = transfer_ta_vertex_triangle_pt3(store_queue_ix,
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
	//////////////////////////////////////////////////////////////////////////////
	// initialize holly graphics units
	//////////////////////////////////////////////////////////////////////////////

	transfer_init();

	//////////////////////////////////////////////////////////////////////////////
	// initialize maple unit
	//////////////////////////////////////////////////////////////////////////////

	maple_init();

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

	glm_vec3_copy((vec3){0, 0, 72}, r_camera.origin);

	ibsp_pmove_vars_t pmove_vars;
	ibsp_pmove_t pmove;

	pmove.dead = false;
	pmove.onground = false;

	glm_vec3_copy(r_camera.origin, pmove.origin);
	glm_vec3_copy(r_camera.angles, pmove.angles);
	glm_vec3_zero(pmove.velocity);
	glm_vec3_copy((vec3){-32, -32, -64}, pmove.mins);
	glm_vec3_copy((vec3){32, 32, 8}, pmove.maxs);

	pmove.cmd.forwardmove = 0;
	pmove.cmd.sidemove = 0;
	pmove.cmd.upmove = 0;

	pmove_vars.gravity = 300;
	pmove_vars.stopspeed = 1;
	pmove_vars.maxspeed = 300;
	pmove_vars.accelerate = 10;
	pmove_vars.friction = 10;
	pmove_vars.entgravity = 1;

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

		//////////////////////////////////////////////////////////////////////////////
		// update camera matrix
		//////////////////////////////////////////////////////////////////////////////

		camera_make_viewproj(&r_camera, viewproj);
		glm_mat4_mul(viewproj, model, mvp);

		//////////////////////////////////////////////////////////////////////////////
		// physics
		//////////////////////////////////////////////////////////////////////////////

		glm_vec3_copy(r_camera.angles, pmove.angles);

		ibsp_pmove(&ibsp, &pmove, &pmove_vars, 0.01f);

		glm_vec3_copy(pmove.origin, r_camera.origin);

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
}

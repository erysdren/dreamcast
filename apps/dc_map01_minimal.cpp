
#include "utils.h"

#include <cglm/cglm.h>

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

#define ROMFS_SOURCE_FILENAME "dc_map01_minimal.pk3.h"
#include "romfs.c"

#define MAX_TEXTURES 256

static struct {
	const char *name;
	uint32_t ofs;
} textures[MAX_TEXTURES];
static size_t num_textures = 0;

static uint32_t get_texture_address(const char *name)
{
	for (int i = 0; i < num_textures; i++)
		if (strcmp(name, textures[i].name) == 0)
			return textures[i].ofs;
	return 0;
}

#include "pvr.h"
#include "ibsp.h"

void transfer_background_polygon(uint32_t isp_tsp_parameter_start)
{
	using namespace holly::core::parameter;

	using parameter = isp_tsp_parameter<3>;

	volatile parameter * polygon = (volatile parameter *)&texture_memory32[isp_tsp_parameter_start];

	polygon->isp_tsp_instruction_word = isp_tsp_instruction_word::depth_compare_mode::always
										| isp_tsp_instruction_word::culling_mode::no_culling;

	polygon->tsp_instruction_word = tsp_instruction_word::src_alpha_instr::one
									| tsp_instruction_word::dst_alpha_instr::zero
									| tsp_instruction_word::fog_control::no_fog;

	polygon->texture_control_word = 0;

	polygon->vertex[0].x =  0.0f;
	polygon->vertex[0].y =  0.0f;
	polygon->vertex[0].z =  0.00001f;
	polygon->vertex[0].base_color = 0xff00ff;

	polygon->vertex[1].x = 32.0f;
	polygon->vertex[1].y =  0.0f;
	polygon->vertex[1].z =  0.00001f;
	polygon->vertex[1].base_color = 0xff00ff;

	polygon->vertex[2].x = 32.0f;
	polygon->vertex[2].y = 32.0f;
	polygon->vertex[2].z =  0.00001f;
	polygon->vertex[2].base_color = 0xff00ff;
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

static inline uint32_t transfer_ta_global_polygon(uint32_t store_queue_ix, const char *texture_name)
{
	using namespace holly::core::parameter;
	using namespace holly::ta;
	using namespace holly::ta::parameter;

	// validate pvr
	int pvr_error = PVR_ERROR_NONE;
	const pvr_t *pvr = pvr_validate(ROMFS_GetFileFromPath(texture_name, NULL), &pvr_error);
	if (!pvr)
	{
		printf("pvr failed to validate: %s (%d)\n", texture_name, pvr_error);
		return store_queue_ix;
	}

	//
	// TA polygon global transfer
	//

	volatile global_parameter::polygon_type_0 * polygon = (volatile global_parameter::polygon_type_0 *)&store_queue[store_queue_ix];
	store_queue_ix += (sizeof (global_parameter::polygon_type_0));

	polygon->parameter_control_word = parameter_control_word::para_type::polygon_or_modifier_volume
									| parameter_control_word::list_type::opaque
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

	auto pixel_type = PVR_GET_PIXEL_TYPE(pvr);
	auto image_type = PVR_GET_IMAGE_TYPE(pvr);

	polygon->tsp_instruction_word |= tsp_instruction_word::texture_u_size::from_int(pvr->width);
	polygon->tsp_instruction_word |= tsp_instruction_word::texture_v_size::from_int(pvr->height);

	uint32_t texture_address = get_texture_address(texture_name);

	polygon->texture_control_word = texture_control_word::texture_address(texture_address / 8);

	switch (image_type)
	{
		case PVR_IMAGE_TYPE_TWIDDLED:
			polygon->texture_control_word |= texture_control_word::scan_order::twiddled;
			break;
		case PVR_IMAGE_TYPE_TWIDDLED_MM:
			polygon->texture_control_word |= texture_control_word::scan_order::twiddled | texture_control_word::mip_mapped;
			break;
		case PVR_IMAGE_TYPE_VQ:
			polygon->texture_control_word |= texture_control_word::scan_order::non_twiddled | texture_control_word::vq_compressed;
			break;
		case PVR_IMAGE_TYPE_VQ_MM:
			polygon->texture_control_word |= texture_control_word::scan_order::non_twiddled | texture_control_word::vq_compressed | texture_control_word::mip_mapped;
			break;
		case PVR_IMAGE_TYPE_RECTANGULAR:
			polygon->texture_control_word |= texture_control_word::scan_order::non_twiddled;
			break;
		case PVR_IMAGE_TYPE_RECTANGULAR_MM:
			polygon->texture_control_word |= texture_control_word::scan_order::non_twiddled | texture_control_word::mip_mapped;
			break;
		default:
			printf("warning: unexpected PVR image type %d\n", image_type);
			break;
	}

	switch (pixel_type)
	{
		case PVR_PIXEL_TYPE_ARGB1555: polygon->texture_control_word |= texture_control_word::pixel_format::argb1555; break;
		case PVR_PIXEL_TYPE_RGB565: polygon->texture_control_word |= texture_control_word::pixel_format::rgb565; break;
		case PVR_PIXEL_TYPE_ARGB4444: polygon->texture_control_word |= texture_control_word::pixel_format::argb4444; break;
		default:
			printf("warning: unexpected PVR pixel type %d\n", pixel_type);
			break;
	}

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
	float w = 1.0f / (v[2] + 3.0f);
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
static uint32_t r_num_visible_leafs = 0;
static int32_t r_camera_cluster = -1;
static int32_t r_camera_prev_cluster = -1;

static struct camera_t {
	vec3 origin;
} r_camera;

int32_t cluster_for_point(vec3 p)
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

	return ibsp.leafs[-index - 1].cluster;
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

				if (r_visible_faces[ibsp.leaffaces[j]])
					continue;

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

		char texture_name[256];
		memset(texture_name, 0, sizeof(texture_name));
		strlcat(texture_name, ibsp.textures[face->texture].name, sizeof(texture_name));
		strlcat(texture_name, ".pvr", sizeof(texture_name));

		store_queue_ix = transfer_ta_global_polygon(store_queue_ix, texture_name);

		for (int j = 0; j < face->num_meshverts; j += 3)
		{
			vec3 vp[3];
			vec2 vt[3];
			uint32_t vc[3];

			for (int k = 0; k < 3; k++)
			{
				ibsp_vertex_t *vertex = &ibsp.vertices[face->first_vert + ibsp.meshverts[face->first_meshvert + j + k]];
				glm_vec3_copy(vertex->position, vp[k]);
				glm_vec2_copy(vertex->texcoords[0], vt[k]);
				vc[k] = 0;
			}

			for (int k = 0; k < 3; k++)
			{
				glm_mat4_mulv3(mvp, vp[k], 1.0f, vp[k]);
				vertex_perspective_divide(vp[k]);
				vertex_screen_space(vp[k]);
			}

			store_queue_ix = transfer_ta_vertex_triangle(store_queue_ix,
														vp[0][0], vp[0][1], vp[0][2], vt[0][0], vt[0][1], vc[0],
														vp[1][0], vp[1][1], vp[1][2], vt[1][0], vt[1][1], vc[1],
														vp[2][0], vp[2][1], vp[2][2], vt[2][0], vt[2][1], vc[2]);
		}
	}

	store_queue_ix = transfer_ta_global_end_of_list(store_queue_ix);
}

uint32_t transfer_texture(const char *name, uint32_t texture_address)
{
	// use 4-byte transfers to texture memory, for slightly increased transfer
	// speed
	//
	// It would be even faster to use the SH4 store queue for this operation, or
	// SH4 DMA.

	const pvr_t *pvr = pvr_validate(ROMFS_GetFileFromPath(name, NULL), NULL);

	sh7091::store_queue_transfer::copy((void *)&texture_memory64[texture_address], PVR_GET_PIXEL_DATA(pvr), PVR_GET_PIXEL_DATA_SIZE(pvr));

	return texture_address + PVR_GET_PIXEL_DATA_SIZE(pvr);
}

void transfer_textures(uint32_t texture_start)
{
	const char *matched[MAX_TEXTURES];

	num_textures = ROMFS_GlobFiles("*.pvr", matched, MAX_TEXTURES);

	uint32_t texture_address = texture_start;
	for (int i = 0; i < num_textures; i++)
	{
		textures[i].name = matched[i];
		textures[i].ofs = texture_address;
		texture_address = transfer_texture(textures[i].name, texture_address);
	}

	printf("texture memory used: %zu bytes\n", texture_address - texture_start);
}

void main()
{
  /*
    a very simple memory map:

    the ordering within texture memory is not significant, and could be
    anything
  */
  uint32_t framebuffer_start       = 0x496000;
  uint32_t isp_tsp_parameter_start = 0x000000;
  uint32_t region_array_start      = 0x21c000;
  uint32_t object_list_start       = 0x400000;

  // these addresses are in "64-bit" texture memory address space:
  uint32_t texture_start           = 0x5a9840;

  const int tile_y_num = 480 / 32;
  const int tile_x_num = 640 / 32;

  using namespace holly::core;

  region_array::list_block_size list_block_size = {
    .opaque = 8 * 4,
  };

  region_array::transfer(tile_x_num,
                         tile_y_num,
                         list_block_size,
                         region_array_start,
                         object_list_start);

  transfer_background_polygon(isp_tsp_parameter_start);

	//////////////////////////////////////////////////////////////////////////////
	// transfer the texture images to texture ram
	//////////////////////////////////////////////////////////////////////////////

	transfer_textures(texture_start);

	//////////////////////////////////////////////////////////////////////////////
	// load bsp data
	//////////////////////////////////////////////////////////////////////////////

	if (!ibsp_load(ROMFS_GetFileFromPath("maps/dc_map01.bsp", NULL), &ibsp))
		exit(1);

  //////////////////////////////////////////////////////////////////////////////
  // configure the TA
  //////////////////////////////////////////////////////////////////////////////

  using namespace holly;
  using holly::holly;

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

  holly.TA_ISP_BASE = isp_tsp_parameter_start + ta_isp_base_offset;
  holly.TA_ISP_LIMIT = 0x21bfe0;

  // Similarly, the TA also contains, for up to 600 tiles, an internal index for
  // the next address that an object list entry will be stored for each
  // tile. These internal indicies are partially exposed via the read-only
  // TA_OL_POINTERS.
  holly.TA_OL_BASE = object_list_start;

  // TA_OL_LIMIT, DCDBSysArc990907E.pdf page 385:
  //
  // >   Because the TA may automatically store data in the address that is
  // >   specified by this register, it must not be used for other data.  For
  // >   example, the address specified here must not be the same as the address
  // >   in the TA_ISP_BASE register.
  holly.TA_OL_LIMIT = 0x495fe0;

  //////////////////////////////////////////////////////////////////////////////
  // configure CORE
  //////////////////////////////////////////////////////////////////////////////

  // REGION_BASE is the (texture memory-relative) address of the region array.
  holly.REGION_BASE = region_array_start;

  // PARAM_BASE is the (texture memory-relative) address of ISP/TSP parameters.
  // Anything that references an ISP/TSP parameter does so relative to this
  // address (and not relative to the beginning of texture memory).
  holly.PARAM_BASE = isp_tsp_parameter_start;

  // Set the offset of the background ISP/TSP parameter, relative to PARAM_BASE
  // SKIP is related to the size of each vertex
  uint32_t background_offset = 0;

  holly.ISP_BACKGND_T = isp_backgnd_t::tag_address(background_offset / 4)
                      | isp_backgnd_t::tag_offset(0)
                      | isp_backgnd_t::skip(1);

  // FB_W_SOF1 is the (texture memory-relative) address of the framebuffer that
  // will be written to when a tile is rendered/flushed.
  holly.FB_W_SOF1 = framebuffer_start;

  // without waiting for rendering to actually complete, immediately display the
  // framebuffer.
  holly.FB_R_SOF1 = framebuffer_start;

	mat4 model = GLM_MAT4_IDENTITY_INIT, proj, view, viewproj, mvp;
	glm_vec3_copy((vec3){116, 191, 37}, r_camera.origin);
	glm_lookat(r_camera.origin, (vec3){207, -364, 155}, GLM_ZUP, view);
	glm_perspective(80, 640.0f/480.0f, 0.1f, 1024.0f, proj);
	glm_mat4_mul(proj, view, viewproj);
	glm_mat4_mul(viewproj, model, mvp);

	// draw 500 frames of cube rotation
	while (1)
	{
		// get camera cluster
		r_camera_cluster = cluster_for_point(r_camera.origin);

		// mark visible leafs
		if (r_camera_prev_cluster != r_camera_cluster)
		{
			r_num_visible_leafs = mark_visible_leafs(r_camera_cluster);
			r_camera_prev_cluster = r_camera_cluster;
		}

		// TA_LIST_INIT needs to be written (every frame) prior to the first FIFO
		// write.
		holly.TA_LIST_INIT = ta_list_init::list_init;

		// dummy TA_LIST_INIT read; DCDBSysArc990907E.pdf in multiple places says this
		// step is required.
		(void)holly.TA_LIST_INIT;

		//////////////////////////////////////////////////////////////////////////////
		// render models
		//////////////////////////////////////////////////////////////////////////////

		transfer_ibsp(mvp);

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

	// return from main; this will effectively jump back to the serial loader
}

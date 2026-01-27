
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

#define ROMFS_SOURCE_FILENAME "dreamcasko.pk3.h"
#include "romfs.c"

#define MAX_TEXTURES 64

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
#include "md3.h"
#include "iqm.h"

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
									| tsp_instruction_word::filter_mode::point_sampled
									| tsp_instruction_word::texture_shading_instruction::decal;

	const pvr_t *pvr = pvr_validate(ROMFS_GetFileFromPath(texture_name, NULL), NULL);

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
	}

	switch (pixel_type)
	{
		case PVR_PIXEL_TYPE_ARGB1555: polygon->texture_control_word |= texture_control_word::pixel_format::argb1555; break;
		case PVR_PIXEL_TYPE_RGB565: polygon->texture_control_word |= texture_control_word::pixel_format::rgb565; break;
		case PVR_PIXEL_TYPE_ARGB4444: polygon->texture_control_word |= texture_control_word::pixel_format::argb4444; break;
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

static float theta = 0;

static inline void vertex_rotate(vec3 v)
{
	// to make the cube's appearance more interesting, rotate the vertex on two
	// axes

	float x0 = v[0];
	float y0 = v[1];
	float z0 = v[2];

	float x1 = x0 * cos(theta) - z0 * sin(theta);
	float y1 = y0;
	float z1 = x0 * sin(theta) + z0 * cos(theta);

	float x2 = x1;
	float y2 = y1 * cos(theta) - z1 * sin(theta);
	float z2 = y1 * sin(theta) + z1 * cos(theta);

	v[0] = x2;
	v[1] = y2;
	v[2] = z2;
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

static inline void decompress_vertex(md3_vertex_t *vertex, vec3 v)
{
	v[0] = MD3_UNCOMPRESS_POSITION(vertex->position[0]);
	v[1] = MD3_UNCOMPRESS_POSITION(vertex->position[1]);
	v[2] = MD3_UNCOMPRESS_POSITION(vertex->position[2]);
}

void transfer_md3(const char *model_path, mat4 mvp)
{
	md3_t *md3 = (md3_t *)ROMFS_GetFileFromPath(model_path, NULL);
	md3_surface_t *surfaces = MD3_GET_SURFACES(md3);

	{
		using namespace sh7091;
		using sh7091::sh7091;

		// set the store queue destination address to the TA Polygon Converter FIFO
		sh7091.CCN.QACR0 = sh7091::ccn::qacr0::address(ta_fifo_polygon_converter);
		sh7091.CCN.QACR1 = sh7091::ccn::qacr1::address(ta_fifo_polygon_converter);
	}

	uint32_t store_queue_ix = 0;

	md3_surface_t *surface = surfaces;

	for (int i = 0; i < md3->num_surfaces; i++)
	{
		char texture_name[256];
		md3_triangle_t *triangles = MD3_SURFACE_GET_TRIANGLES(surface);
		md3_shader_t *shaders = MD3_SURFACE_GET_SHADERS(surface);
		md3_texcoord_t *texcoords = MD3_SURFACE_GET_TEXCOORDS(surface);
		md3_vertex_t *vertices = MD3_SURFACE_GET_VERTICES(surface);

		memset(texture_name, 0, sizeof(texture_name));
		strlcat(texture_name, "textures/", sizeof(texture_name));
		strlcat(texture_name, shaders->name, sizeof(texture_name));
		strlcat(texture_name, ".pvr", sizeof(texture_name));

		store_queue_ix = transfer_ta_global_polygon(store_queue_ix, texture_name);

		for (int j = 0; j < surface->num_triangles; j++)
		{
			md3_triangle_t *triangle = triangles + j;
			md3_vertex_t *va = vertices + triangle->indices[0];
			md3_vertex_t *vb = vertices + triangle->indices[1];
			md3_vertex_t *vc = vertices + triangle->indices[2];
			md3_texcoord_t *ta = texcoords + triangle->indices[0];
			md3_texcoord_t *tb = texcoords + triangle->indices[1];
			md3_texcoord_t *tc = texcoords + triangle->indices[2];

			vec3 vpa, vpb, vpc;

			// decompress MD3 vertices
			decompress_vertex(va, vpa);
			decompress_vertex(vb, vpb);
			decompress_vertex(vc, vpc);

			// rotate around two different axes by theta
			glm_vec3_rotate(vpa, theta, GLM_ZUP);
			glm_vec3_rotate(vpb, theta, GLM_ZUP);
			glm_vec3_rotate(vpc, theta, GLM_ZUP);

			// multiply by mvp matrix
			glm_mat4_mulv3(mvp, vpa, 1.0f, vpa);
			glm_mat4_mulv3(mvp, vpb, 1.0f, vpb);
			glm_mat4_mulv3(mvp, vpc, 1.0f, vpc);

			// do perspective divide
			vertex_perspective_divide(vpa);
			vertex_perspective_divide(vpb);
			vertex_perspective_divide(vpc);

			// translate to screen space
			vertex_screen_space(vpa);
			vertex_screen_space(vpb);
			vertex_screen_space(vpc);

			// vertex color is irrelevant in "decal" mode
			uint32_t va_color = 0;
			uint32_t vb_color = 0;
			uint32_t vc_color = 0;

			store_queue_ix = transfer_ta_vertex_triangle(store_queue_ix,
														vpa[0], vpa[1], vpa[2], ta->coords[0], ta->coords[1], va_color,
														vpb[0], vpb[1], vpb[2], tb->coords[0], tb->coords[1], vb_color,
														vpc[0], vpc[1], vpc[2], tc->coords[0], tc->coords[1], vc_color);
		}

		surface = MD3_SURFACE_GET_NEXT(surface);
	}

	store_queue_ix = transfer_ta_global_end_of_list(store_queue_ix);
}

void transfer_iqm(const char *model_path, mat4 mvp)
{
	iqm_t *iqm = (iqm_t *)ROMFS_GetFileFromPath(model_path, NULL);
	iqm_mesh_t *meshes = IQM_GET_MESHES(iqm);
	iqm_vertex_array_t *vertex_arrays = IQM_GET_VERTEX_ARRAYS(iqm);
	iqm_triangle_t *triangles = IQM_GET_TRIANGLES(iqm);

	{
		using namespace sh7091;
		using sh7091::sh7091;

		// set the store queue destination address to the TA Polygon Converter FIFO
		sh7091.CCN.QACR0 = sh7091::ccn::qacr0::address(ta_fifo_polygon_converter);
		sh7091.CCN.QACR1 = sh7091::ccn::qacr1::address(ta_fifo_polygon_converter);
	}

	uint32_t store_queue_ix = 0;

	for (int i = 0; i < iqm->num_meshes; i++)
	{
		char texture_name[256];
		iqm_mesh_t *mesh = meshes + i;

		memset(texture_name, 0, sizeof(texture_name));
		strlcat(texture_name, "textures/", sizeof(texture_name));
		strlcat(texture_name, IQM_GET_TEXT(iqm) + mesh->material, sizeof(texture_name));
		strlcat(texture_name, ".pvr", sizeof(texture_name));

		store_queue_ix = transfer_ta_global_polygon(store_queue_ix, texture_name);

		for (int j = 0; j < mesh->num_triangles; j++)
		{
			iqm_triangle_t *triangle = triangles + mesh->first_triangle + j;

			vec3 vp[3];
			vec2 vt[3];

			for (int k = 0; k < iqm->num_vertex_arrays; k++)
			{
				iqm_vertex_array_t *vertex_array = vertex_arrays + k;
				void *data = (uint8_t *)iqm + vertex_array->offset;

				switch (vertex_array->type)
				{
					case IQM_VERTEX_ARRAY_TYPE_POSITION:
					{
						for (int l = 0; l < 3; l++)
						{
							float *ptr = (float *)data + (3 * triangle->vertices[l]);
							glm_vec3_copy(ptr, vp[l]);
						}
						break;
					}

					case IQM_VERTEX_ARRAY_TYPE_TEXCOORD:
					{
						for (int l = 0; l < 3; l++)
						{
							float *ptr = (float *)data + (2 * triangle->vertices[l]);
							glm_vec2_copy(ptr, vt[l]);
						}
						break;
					}

					default:
					{
						break;
					}
				}
			}

			for (int l = 0; l < 3; l++)
			{
				glm_vec3_rotate(vp[l], theta, GLM_ZUP);
				glm_mat4_mulv3(mvp, vp[l], 1.0f, vp[l]);
				vertex_perspective_divide(vp[l]);
				vertex_screen_space(vp[l]);
			}

			// vertex color is irrelevant in "decal" mode
			uint32_t va_color = 0;
			uint32_t vb_color = 0;
			uint32_t vc_color = 0;

			store_queue_ix = transfer_ta_vertex_triangle(store_queue_ix,
														vp[0][0], vp[0][1], vp[0][2], vt[0][0], vt[0][1], va_color,
														vp[1][0], vp[1][1], vp[1][2], vt[1][0], vt[1][1], vb_color,
														vp[2][0], vp[2][1], vp[2][2], vt[2][0], vt[2][1], vc_color);
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
}

void main()
{
  /*
    a very simple memory map:

    the ordering within texture memory is not significant, and could be
    anything
  */
  uint32_t framebuffer_start       = 0x200000; // intentionally the same address that the boot rom used to draw the SEGA logo
  uint32_t isp_tsp_parameter_start = 0x400000;
  uint32_t region_array_start      = 0x500000;
  uint32_t object_list_start       = 0x100000;

  // these addresses are in "64-bit" texture memory address space:
  uint32_t texture_start           = 0x700000;

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
  holly.TA_ISP_LIMIT = isp_tsp_parameter_start + 0x100000;

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
  holly.TA_OL_LIMIT = object_list_start + 0x100000 - 32;

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

	mat4 proj, view, viewproj;
	glm_lookat((vec3){64, 0, 0}, (vec3){0, 0, 0}, GLM_ZUP, view);
	glm_perspective(80, 640.0f/480.0f, 0.1f, 1024.0f, proj);
	glm_mat4_mul(proj, view, viewproj);

	// draw 500 frames of cube rotation
	while (1)
	{
		for (int i = 0; i < 500; i++)
		{
			//////////////////////////////////////////////////////////////////////////////
			// transfer cube to texture memory via the TA polygon converter FIFO
			//////////////////////////////////////////////////////////////////////////////

			// TA_LIST_INIT needs to be written (every frame) prior to the first FIFO
			// write.
			holly.TA_LIST_INIT = ta_list_init::list_init;

			// dummy TA_LIST_INIT read; DCDBSysArc990907E.pdf in multiple places says this
			// step is required.
			(void)holly.TA_LIST_INIT;

			//////////////////////////////////////////////////////////////////////////////
			// render models
			//////////////////////////////////////////////////////////////////////////////

			mat4 model, mvp;

			glm_mat4_identity(model);
			glm_translate_y(model, -32);
			glm_mat4_mul(viewproj, model, mvp);

			transfer_md3("models/dreamcasko.md3", mvp);

			glm_mat4_identity(model);
			glm_translate_y(model, 32);
			glm_mat4_mul(viewproj, model, mvp);

			transfer_iqm("models/dreamcasko.iqm", mvp);

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

			// increment theta for the cube rotation animation
			// (used by the `vertex_rotate` function)
			theta += 0.01f;
		}
	}

	// return from main; this will effectively jump back to the serial loader
}

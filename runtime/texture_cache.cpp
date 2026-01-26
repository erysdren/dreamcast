
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

#include "texture_cache.h"

static texture_cache_t textures[MAX_TEXTURES];
static size_t num_textures = 0;
static uint32_t texture_address_next = TEXTURE_INVALID;

RUNTIME_EXTERN void texture_cache_init(uint32_t texture_address)
{
	texture_address_next = texture_address;
	num_textures = 0;
}

static uint32_t transfer_pvr(const pvr_t *pvr, uint32_t texture_address)
{
	sh7091::store_queue_transfer::copy((void *)&texture_memory64[texture_address], PVR_GET_PIXEL_DATA(pvr), PVR_GET_PIXEL_DATA_SIZE(pvr));
	return texture_address + PVR_GET_PIXEL_DATA_SIZE(pvr);
}

RUNTIME_EXTERN uint32_t texture_cache_pvr(const pvr_t *pvr)
{
	using namespace holly::core::parameter;
	using namespace holly::ta;
	using namespace holly::ta::parameter;

	if (texture_address_next == TEXTURE_INVALID)
		return TEXTURE_INVALID;

	if (num_textures >= MAX_TEXTURES)
		return TEXTURE_INVALID;

	int error_code = PVR_ERROR_NONE;
	pvr = pvr_validate(pvr, &error_code);
	if (!pvr || error_code != PVR_ERROR_NONE)
		return TEXTURE_INVALID;

	texture_cache_t& t = textures[num_textures];

	t.tsp_instruction_word = tsp_instruction_word::texture_u_size::from_int(pvr->width) | tsp_instruction_word::texture_v_size::from_int(pvr->height);

	t.texture_control_word = texture_control_word::texture_address(texture_address_next);

	switch (PVR_GET_IMAGE_TYPE(pvr))
	{
		case PVR_IMAGE_TYPE_TWIDDLED:
			t.texture_control_word |= texture_control_word::scan_order::twiddled;
			break;
		case PVR_IMAGE_TYPE_TWIDDLED_MM:
			t.texture_control_word |= texture_control_word::scan_order::twiddled | texture_control_word::mip_mapped;
			break;
		case PVR_IMAGE_TYPE_VQ:
			t.texture_control_word |= texture_control_word::scan_order::twiddled | texture_control_word::vq_compressed;
			break;
		case PVR_IMAGE_TYPE_VQ_MM:
			t.texture_control_word |= texture_control_word::scan_order::twiddled | texture_control_word::vq_compressed | texture_control_word::mip_mapped;
			break;
		case PVR_IMAGE_TYPE_RECTANGULAR:
			t.texture_control_word |= texture_control_word::scan_order::non_twiddled;
			break;
		case PVR_IMAGE_TYPE_RECTANGULAR_MM:
			t.texture_control_word |= texture_control_word::scan_order::non_twiddled | texture_control_word::mip_mapped;
			break;
	}

	switch (PVR_GET_PIXEL_TYPE(pvr))
	{
		case PVR_PIXEL_TYPE_ARGB1555: t.texture_control_word |= texture_control_word::pixel_format::argb1555; break;
		case PVR_PIXEL_TYPE_RGB565: t.texture_control_word |= texture_control_word::pixel_format::rgb565; break;
		case PVR_PIXEL_TYPE_ARGB4444: t.texture_control_word |= texture_control_word::pixel_format::argb4444; break;
	}

	texture_address_next = transfer_pvr(pvr, texture_address_next);

	return num_textures++;
}

RUNTIME_EXTERN const texture_cache_t *texture_cache_get(uint32_t texture_index)
{
	if (texture_index == TEXTURE_INVALID || texture_index >= num_textures)
		return NULL;

	return &textures[texture_index];
}

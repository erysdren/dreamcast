
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

void texture_cache_init(uint32_t texture_address)
{
	texture_address_next = texture_address;
	num_textures = 0;
}

static uint32_t transfer_texture(const void *data, size_t len, uint32_t texture_address)
{
	sh7091::store_queue_transfer::copy((void *)&texture_memory64[texture_address], data, len);
	return texture_address + len;
}

uint32_t texture_cache_raw_palette(int width, int height, uint32_t type, uint32_t flags, uint32_t palette_index, const void *data, size_t len)
{
	using namespace holly::core::parameter;
	using namespace holly::ta;
	using namespace holly::ta::parameter;

	if (texture_address_next == TEXTURE_INVALID)
		return TEXTURE_INVALID;

	if (num_textures >= MAX_TEXTURES)
		return TEXTURE_INVALID;

	if (type == TEXTURE_TYPE_NONE)
		return TEXTURE_INVALID;

	if (width < 8 || width > 1024 || (width & (width - 1)) != 0)
		return TEXTURE_INVALID;

	if (height < 8 || height > 1024 || (height & (height - 1)) != 0)
		return TEXTURE_INVALID;

	if ((type == TEXTURE_TYPE_PAL4 || type == TEXTURE_TYPE_PAL8) && palette_index == PALETTE_INVALID)
		return TEXTURE_INVALID;

	if (type == TEXTURE_TYPE_PAL4 && palette_index % 16)
		return TEXTURE_INVALID;

	if (type == TEXTURE_TYPE_PAL8 && palette_index % 256)
		return TEXTURE_INVALID;

	auto& t = textures[num_textures];

	t.tsp_instruction_word = tsp_instruction_word::texture_u_size::from_int(width) | tsp_instruction_word::texture_v_size::from_int(height);

	t.texture_control_word = texture_control_word::texture_address(texture_address_next / 8);

	if (flags & TEXTURE_FLAG_TWIDDLED)
	{
		t.texture_control_word |= texture_control_word::scan_order::twiddled;
	}
	else
	{
		t.texture_control_word |= texture_control_word::scan_order::non_twiddled;
	}

	if (flags & TEXTURE_FLAG_VQ)
	{
		t.texture_control_word |= texture_control_word::vq_compressed;
	}

	if (flags & TEXTURE_FLAG_MM)
	{
		t.texture_control_word |= texture_control_word::mip_mapped;
	}

	if (type == TEXTURE_TYPE_PAL4)
	{
		t.texture_control_word |= texture_control_word::palette_selector4(palette_index / 16);
	}

	if (type == TEXTURE_TYPE_PAL8)
	{
		t.texture_control_word |= texture_control_word::palette_selector8(palette_index / 256);
	}

	switch (type)
	{
		case TEXTURE_TYPE_ARGB1555: t.texture_control_word |= texture_control_word::pixel_format::argb1555; break;
		case TEXTURE_TYPE_RGB565: t.texture_control_word |= texture_control_word::pixel_format::rgb565; break;
		case TEXTURE_TYPE_ARGB4444: t.texture_control_word |= texture_control_word::pixel_format::argb4444; break;
		case TEXTURE_TYPE_PAL4: t.texture_control_word |= texture_control_word::pixel_format::palette_4bpp; break;
		case TEXTURE_TYPE_PAL8: t.texture_control_word |= texture_control_word::pixel_format::palette_8bpp; break;
	}

	texture_address_next = transfer_texture(data, len, texture_address_next);

	return num_textures++;
}

uint32_t texture_cache_raw(int width, int height, uint32_t type, uint32_t flags, const void *data, size_t len)
{
	return texture_cache_raw_palette(width, height, type, flags, PALETTE_INVALID, data, len);
}

uint32_t texture_cache_pvr(const pvr_t *pvr)
{
	int error_code = PVR_ERROR_NONE;
	pvr = pvr_validate(pvr, &error_code);
	if (!pvr || error_code != PVR_ERROR_NONE)
		return TEXTURE_INVALID;

	uint32_t type = TEXTURE_TYPE_NONE;
	uint32_t flags = TEXTURE_FLAG_NONE;

	switch (PVR_GET_IMAGE_TYPE(pvr))
	{
		case PVR_IMAGE_TYPE_TWIDDLED:
			flags |= TEXTURE_FLAG_TWIDDLED;
			break;
		case PVR_IMAGE_TYPE_TWIDDLED_MM:
			flags |= TEXTURE_FLAG_TWIDDLED | TEXTURE_FLAG_MM;
			break;
		case PVR_IMAGE_TYPE_VQ:
			flags |= TEXTURE_FLAG_TWIDDLED | TEXTURE_FLAG_VQ;
			break;
		case PVR_IMAGE_TYPE_VQ_MM:
			flags |= TEXTURE_FLAG_TWIDDLED | TEXTURE_FLAG_VQ | TEXTURE_FLAG_MM;
			break;
		case PVR_IMAGE_TYPE_RECTANGULAR:
			break;
		case PVR_IMAGE_TYPE_RECTANGULAR_MM:
			flags |= TEXTURE_FLAG_MM;
			break;
	}

	switch (PVR_GET_PIXEL_TYPE(pvr))
	{
		case PVR_PIXEL_TYPE_ARGB1555: type = TEXTURE_TYPE_ARGB1555; break;
		case PVR_PIXEL_TYPE_RGB565: type = TEXTURE_TYPE_RGB565; break;
		case PVR_PIXEL_TYPE_ARGB4444: type = TEXTURE_TYPE_ARGB4444; break;
	}

	return texture_cache_raw(pvr->width, pvr->height, type, flags, PVR_GET_PIXEL_DATA(pvr), PVR_GET_PIXEL_DATA_SIZE(pvr));
}

const texture_cache_t *texture_cache_get(uint32_t texture_index)
{
	if (texture_index == TEXTURE_INVALID || texture_index >= num_textures)
		return NULL;

	return &textures[texture_index];
}

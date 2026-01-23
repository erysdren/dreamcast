
#include "runtime.h"
#include "pvr.h"

const pvr_t *pvr_validate(const void *ptr, int *error_code)
{
	const gbix_t *gbix = (const gbix_t *)ptr;
	const pvr_t *pvr = (const pvr_t *)ptr;

	if (!ptr)
	{
		if (error_code) *error_code = PVR_ERROR_INVALID;
		return NULL;
	}

	if (gbix->magic == GBIX_MAGIC)
	{
		pvr = (const pvr_t *)((const uint8_t *)ptr + gbix->len + sizeof(gbix_t));
	}

	if (pvr->magic != PVR_MAGIC)
	{
		if (error_code) *error_code = PVR_ERROR_MAGIC;
		return NULL;
	}

	if (pvr->width < 8 || pvr->width > 1024 || (pvr->width & (pvr->width - 1)) != 0)
	{
		if (error_code) *error_code = PVR_ERROR_WIDTH;
		return NULL;
	}

	if (pvr->height < 8 || pvr->height > 1024 || (pvr->height & (pvr->height - 1)) != 0)
	{
		if (error_code) *error_code = PVR_ERROR_HEIGHT;
		return NULL;
	}

	if (error_code) *error_code = PVR_ERROR_NONE;
	return pvr;
}

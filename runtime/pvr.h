#ifndef _PVR_H_
#define _PVR_H_
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

enum {
	PVR_PIXEL_TYPE_ARGB1555 = 0,
	PVR_PIXEL_TYPE_RGB565 = 1,
	PVR_PIXEL_TYPE_ARGB4444 = 2
};

enum {
	PVR_IMAGE_TYPE_TWIDDLED = 1,
	PVR_IMAGE_TYPE_TWIDDLED_MM = 2,
	PVR_IMAGE_TYPE_VQ = 3,
	PVR_IMAGE_TYPE_VQ_MM = 4,
	PVR_IMAGE_TYPE_RECTANGULAR = 9,
	PVR_IMAGE_TYPE_RECTANGULAR_MM = 10
};

typedef struct gbix {
	uint32_t magic;
	uint32_t len;
} gbix_t;

#define GBIX_MAGIC (0x58494247)

typedef struct pvr {
	uint32_t magic;
	uint32_t len;
	uint32_t type;
	uint16_t width;
	uint16_t height;
} pvr_t;

#define PVR_MAGIC (0x54525650)

#define PVR_GET_PIXEL_TYPE(pvr) ((pvr)->type & 0xFF)
#define PVR_GET_IMAGE_TYPE(pvr) (((pvr)->type & 0xFF00) >> 8)

#define PVR_GET_PIXEL_DATA(pvr) (((uint8_t *)(pvr)) + sizeof(pvr_t))
#define PVR_GET_PIXEL_DATA_SIZE(pvr) ((pvr)->len - sizeof(pvr_t) + (sizeof(uint32_t) * 2))

enum {
	PVR_ERROR_NONE, ///< no error
	PVR_ERROR_INVALID, ///< ptr is NULL
	PVR_ERROR_MAGIC, ///< magic identifer doesn't match
	PVR_ERROR_WIDTH, ///< width is not a power or two or greater than 1024
	PVR_ERROR_HEIGHT, ///< height is not a power or two or greater than 1024
	PVR_ERROR_TYPE ///< type is invalid
};

const pvr_t *pvr_validate(const void *ptr, int *error_code);

#ifdef __cplusplus
}
#endif
#endif // _PVR_H_

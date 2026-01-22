#ifndef _PVR_H_
#define _PVR_H_

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
	uint32_t len_file;
	uint32_t type;
	uint16_t width;
	uint16_t height;
} pvr_t;

#define PVR_MAGIC (0x54525650)

#define PVR_GET_PIXEL_TYPE(pvr) ((pvr)->type & 0xFF)
#define PVR_GET_IMAGE_TYPE(pvr) (((pvr)->type & 0xFF00) >> 8)

#endif // _PVR_H_

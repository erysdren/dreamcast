#ifndef _TRANSFER_H_
#define _TRANSFER_H_
#ifdef __cplusplus
extern "C" {
#endif

#include "runtime.h"

void transfer_init(void);
void transfer_frame_start(void);
void transfer_frame_end(void);
void transfer_background_polygon(uint32_t color);

#ifdef __cplusplus
}
#endif
#endif // _TRANSFER_H_

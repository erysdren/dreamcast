#ifndef _CAMERA_H_
#define _CAMERA_H_
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include <cglm/cglm.h>

typedef struct camera {
	vec3 origin;
	vec3 angles;
	vec3 up;
	float near;
	float far;
	float fov;
} camera_t;

#ifdef __cplusplus
}
#endif
#endif // _CAMERA_H_

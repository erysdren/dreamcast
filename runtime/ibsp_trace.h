#ifndef _IBSP_TRACE_H_
#define _IBSP_TRACE_H_
#ifdef __cplusplus
extern "C" {
#endif

#include "runtime.h"

typedef struct ibsp_trace {
	vec3 start;
	vec3 end;
	float fraction;
	vec4 plane;
	bool all_solid;
	bool start_solid;
} ibsp_trace_t;

void ibsp_trace(ibsp_t *ibsp, ibsp_trace_t *trace, vec3 start, vec3 end, vec3 mins, vec3 maxs);

#ifdef __cplusplus
}
#endif
#endif // _IBSP_TRACE_H_

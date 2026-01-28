#ifndef _IBSP_PMOVE_H_
#define _IBSP_PMOVE_H_
#ifdef __cplusplus
extern "C" {
#endif

#include "runtime.h"

typedef struct ibsp_pmove_vars {
	float gravity;
	float stopspeed;
	float maxspeed;
	float spectatormaxspeed;
	float accelerate;
	float airaccelerate;
	float wateraccelerate;
	float friction;
	float waterfriction;
	float entgravity;
} ibsp_pmove_vars_t;

typedef struct ibsp_pmove_cmd {
	int16_t forwardmove, sidemove, upmove;
	uint8_t buttons;
	uint8_t impulse;
} ibsp_pmove_cmd_t;

typedef struct ibsp_pmove {
	vec3 origin;
	vec3 angles;
	vec3 velocity;
	vec3 mins;
	vec3 maxs;
	uint8_t oldbuttons;
	bool dead;
	bool onground;
	ibsp_pmove_cmd_t cmd;
} ibsp_pmove_t;

void ibsp_pmove(ibsp_t *ibsp, ibsp_pmove_t *pmove, ibsp_pmove_vars_t *vars, float frametime);

#ifdef __cplusplus
}
#endif
#endif // _IBSP_PMOVE_H_

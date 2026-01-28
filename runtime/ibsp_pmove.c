
#include "ibsp_pmove.h"

#define STEPSIZE 18

typedef struct pmove_work {
	ibsp_t *ibsp;
	ibsp_pmove_t *pmove;
	ibsp_pmove_vars_t *vars;
	float frametime;
	vec3 forward, right, up;
} pmove_work_t;

bool pmove_onground(pmove_work_t *pmw)
{
	vec3 end;
	ibsp_trace_t trace;

	if (pmw->pmove->velocity[2] > 180)
		return false;

	glm_vec3_copy(pmw->pmove->origin, end);
	end[2] -= 1;

	ibsp_trace(pmw->ibsp, &trace, pmw->pmove->origin, end, pmw->pmove->mins, pmw->pmove->maxs);

	if (trace.plane[2] < 0.7f)
		return false;

	return true;
}

void pmove_friction(pmove_work_t *pmw)
{
	float speed, newspeed, friction, drop, control;
	vec3 start, end;
	ibsp_trace_t trace;

	speed = glm_vec3_norm2(pmw->pmove->velocity);
	if (speed < 1.0f)
	{
		pmw->pmove->velocity[0] = 0.0f;
		pmw->pmove->velocity[1] = 0.0f;
		return;
	}

	friction = pmw->vars->friction;

	if (pmw->pmove->onground)
	{
		start[0] = end[0] = pmw->pmove->origin[0] + pmw->pmove->velocity[0] / speed * 16;
		start[1] = end[1] = pmw->pmove->origin[1] + pmw->pmove->velocity[1] / speed * 16;
		start[2] = pmw->pmove->origin[2] + pmw->pmove->mins[2];
		end[2] = start[2] - 34;

		ibsp_trace(pmw->ibsp, &trace, start, end, pmw->pmove->mins, pmw->pmove->maxs);

		if (trace.fraction == 1.0f)
			friction *= 2.0f;
	}

	drop = 0;

	if (pmw->pmove->onground)
	{
		control = speed < pmw->vars->stopspeed ? pmw->vars->stopspeed : speed;
		drop += control * friction * pmw->frametime;
	}

	newspeed = speed - drop;
	if (newspeed < 0.0f)
		newspeed = 0.0f;
	newspeed /= speed;

	glm_vec3_scale(pmw->pmove->velocity, newspeed, pmw->pmove->velocity);
}

void pmove_accelerate(pmove_work_t *pmw, vec3 wishdir, float wishspeed, float accel)
{
	float addspeed, accelspeed, currentspeed;

	currentspeed = glm_vec3_dot(pmw->pmove->velocity, wishdir);
	addspeed = wishspeed - currentspeed;
	if (addspeed <= 0.0f)
		return;
	accelspeed = accel * pmw->frametime * wishspeed;
	if (accelspeed > addspeed)
		accelspeed = addspeed;
	for (int i = 0; i < 3; i++)
		pmw->pmove->velocity[i] += accelspeed * wishdir[i];
}

void pmove_air_accelerate(pmove_work_t *pmw, vec3 wishdir, float wishspeed, float accel)
{
	float addspeed, accelspeed, currentspeed, wishspd = wishspeed;

	if (wishspd > 30)
		wishspd = 30;
	currentspeed = glm_vec3_dot(pmw->pmove->velocity, wishdir);
	addspeed = wishspd - currentspeed;
	if (addspeed <= 0.0f)
		return;
	accelspeed = accel * wishspeed * pmw->frametime;
	if (accelspeed > addspeed)
		accelspeed = addspeed;
	for (int i = 0; i < 3; i++)
		pmw->pmove->velocity[i] += accelspeed * wishdir[i];
}

#define	STOP_EPSILON 0.1

int pmove_clip_velocity(vec3 in, vec3 normal, vec3 out, float overbounce)
{
	float backoff;
	float change;
	int blocked;

	blocked = 0;

	// floor
	if (normal[2] > 0)
		blocked |= 1;

	// step
	if (!normal[2])
		blocked |= 2;

	backoff = glm_vec3_dot(in, normal) * overbounce;

	for (int i = 0; i < 3; i++)
	{
		change = normal[i] * backoff;
		out[i] = in[i] - change;
		if (out[i] > -STOP_EPSILON && out[i] < STOP_EPSILON)
			out[i] = 0;
	}

	return blocked;
}

#define	MAX_CLIP_PLANES	5

int pmove_fly(pmove_work_t *pmw)
{
	vec3 dir;
	float d;
	int num_planes;
	vec3 planes[MAX_CLIP_PLANES];
	vec3 primal_velocity, original_velocity;
	ibsp_trace_t trace;
	vec3 end;
	float time_left;
	int blocked;
	int i, j;

	blocked = 0;
	glm_vec3_copy(pmw->pmove->velocity, original_velocity);
	glm_vec3_copy(pmw->pmove->velocity, primal_velocity);
	num_planes = 0;

	time_left = pmw->frametime;

	for (int bump = 0 ; bump < 4 ; bump++)
	{
		for (i = 0; i < 3; i++)
			end[i] = pmw->pmove->origin[i] + time_left * pmw->pmove->velocity[i];

		ibsp_trace(pmw->ibsp, &trace, pmw->pmove->origin, end, pmw->pmove->mins, pmw->pmove->maxs);

		if (trace.start_solid || trace.all_solid)
		{
			glm_vec3_zero(pmw->pmove->velocity);
			return 3;
		}

		// actually covered some distance
		if (trace.fraction > 0.0f)
		{
			glm_vec3_copy(trace.end, pmw->pmove->origin);
			num_planes = 0;
		}

		// moved the entire distance
		if (trace.fraction == 1.0f)
			break;

		// floor
		if (trace.plane[2] > 0.7)
		{
			blocked |= 1;
		}
		// step
		if (!trace.plane[2])
		{
			blocked |= 2;
		}

		time_left -= time_left * trace.fraction;

		// cliped to another plane
		if (num_planes >= MAX_CLIP_PLANES)
		{
			glm_vec3_zero(pmw->pmove->velocity);
			break;
		}

		glm_vec3_copy(trace.plane, planes[num_planes]);
		num_planes++;

		// modify original_velocity so it parallels all of the clip planes
		for (i = 0; i < num_planes; i++)
		{
			pmove_clip_velocity(original_velocity, planes[i], pmw->pmove->velocity, 1);
			for (j = 0; j < num_planes; j++)
			{
				if (j != i)
				{
					if (glm_vec3_dot(pmw->pmove->velocity, planes[j]) < 0)
						break;
				}
			}
			if (j == num_planes)
				break;
		}

		if (i != num_planes)
		{
			// go along this plane
		}
		else
		{
			// go along the crease
			if (num_planes != 2)
			{
				glm_vec3_zero(pmw->pmove->velocity);
				break;
			}
			glm_vec3_cross(planes[0], planes[1], dir);
			d = glm_vec3_dot(dir, pmw->pmove->velocity);
			glm_vec3_scale(dir, d, pmw->pmove->velocity);
		}

		//
		// if original velocity is against the original velocity, stop dead
		// to avoid tiny occilations in sloping corners
		//
		if (glm_vec3_dot(pmw->pmove->velocity, primal_velocity) <= 0)
		{
			glm_vec3_zero(pmw->pmove->velocity);
			break;
		}
	}
	return blocked;
}

void pmove_ground(pmove_work_t *pmw)
{
	vec3 start, dest;
	ibsp_trace_t trace;
	vec3 original, originalvel, down, up, downvel;
	float	downdist, updist;

	pmw->pmove->velocity[2] = 0;
	if (!pmw->pmove->velocity[0] && !pmw->pmove->velocity[1] && !pmw->pmove->velocity[2])
		return;

	// first try just moving to the destination
	dest[0] = pmw->pmove->origin[0] + pmw->pmove->velocity[0] * pmw->frametime;
	dest[1] = pmw->pmove->origin[1] + pmw->pmove->velocity[1] * pmw->frametime;
	dest[2] = pmw->pmove->origin[2];

	// first try moving directly to the next spot
	glm_vec3_copy(dest, start);
	ibsp_trace(pmw->ibsp, &trace, pmw->pmove->origin, dest, pmw->pmove->mins, pmw->pmove->maxs);
	if (trace.fraction == 1.0f)
	{
		glm_vec3_copy(trace.end, pmw->pmove->origin);
		return;
	}

	// try sliding forward both on ground and up 16 pixels
	// take the move that goes farthest
	glm_vec3_copy(pmw->pmove->origin, original);
	glm_vec3_copy(pmw->pmove->velocity, originalvel);

	// slide move
	pmove_fly(pmw);

	glm_vec3_copy(pmw->pmove->origin, down);
	glm_vec3_copy(pmw->pmove->velocity, downvel);

	glm_vec3_copy(original, pmw->pmove->origin);
	glm_vec3_copy(originalvel, pmw->pmove->velocity);

	// move up a stair height
	glm_vec3_copy(pmw->pmove->origin, dest);
	dest[2] += STEPSIZE;
	ibsp_trace(pmw->ibsp, &trace, pmw->pmove->origin, dest, pmw->pmove->mins, pmw->pmove->maxs);
	if (!trace.start_solid && !trace.all_solid)
		glm_vec3_copy(trace.end, pmw->pmove->origin);

	// slide move
	pmove_fly(pmw);

	// press down the stepheight
	glm_vec3_copy(pmw->pmove->origin, dest);
	dest[2] -= STEPSIZE;
	ibsp_trace(pmw->ibsp, &trace, pmw->pmove->origin, dest, pmw->pmove->mins, pmw->pmove->maxs);
	if (trace.plane[2] < 0.7)
		goto usedown;
	if (!trace.start_solid && !trace.all_solid)
		glm_vec3_copy(trace.end, pmw->pmove->origin);
	glm_vec3_copy(pmw->pmove->origin, up);

	// decide which one went farther
	downdist = (down[0] - original[0])*(down[0] - original[0]) + (down[1] - original[1])*(down[1] - original[1]);
	updist = (up[0] - original[0])*(up[0] - original[0]) + (up[1] - original[1])*(up[1] - original[1]);

	if (downdist > updist)
	{
usedown:
		glm_vec3_copy(down, pmw->pmove->origin);
		glm_vec3_copy(downvel, pmw->pmove->velocity);
	}
	else
	{
		pmw->pmove->velocity[2] = downvel[2];
	}
}

void pmove_air(pmove_work_t *pmw)
{
	vec3 wishvel, wishdir;
	float fmove, smove, wishspeed;

	fmove = pmw->pmove->cmd.forwardmove;
	smove = pmw->pmove->cmd.sidemove;

	pmw->forward[2] = 0;
	pmw->right[2] = 0;
	glm_vec3_normalize(pmw->forward);
	glm_vec3_normalize(pmw->right);

	for (int i = 0; i < 2; i++)
		wishvel[i] = pmw->forward[i] * fmove + pmw->right[i] * smove;
	wishvel[2] = 0;

	glm_vec3_copy(wishvel, wishdir);
	wishspeed = glm_vec3_norm2(wishdir);
	glm_vec3_normalize(wishdir);

	if (wishspeed > pmw->vars->maxspeed)
	{
		glm_vec3_scale(wishvel, pmw->vars->maxspeed/wishspeed, wishvel);
		wishspeed = pmw->vars->maxspeed;
	}

	if (pmw->pmove->onground)
	{
		pmw->pmove->velocity[2] = 0;
		pmove_accelerate(pmw, wishdir, wishspeed, pmw->vars->accelerate);
		pmw->pmove->velocity[2] -= pmw->vars->entgravity * pmw->vars->gravity * pmw->frametime;
		pmove_ground(pmw);
	}
	else
	{
		pmove_air_accelerate(pmw, wishdir, wishspeed, pmw->vars->accelerate);
		pmw->pmove->velocity[2] -= pmw->vars->entgravity * pmw->vars->gravity * pmw->frametime;
		pmove_fly(pmw);
	}
}

void ibsp_pmove(ibsp_t *ibsp, ibsp_pmove_t *pmove, ibsp_pmove_vars_t *vars, float frametime)
{
	pmove_work_t pmw;

	// setup work
	pmw.ibsp = ibsp;
	pmw.pmove = pmove;
	pmw.vars = vars;
	pmw.frametime = frametime;

	// get look vectors
	makevectors(pmw.pmove->angles, pmw.forward, pmw.right, pmw.up);

	// categorize start position
	pmw.pmove->onground = pmove_onground(&pmw);

	// do friction
	pmove_friction(&pmw);

	// do movement
	pmove_air(&pmw);

	// categorize final position
	pmw.pmove->onground = pmove_onground(&pmw);
}


#include "ibsp_trace.h"

typedef struct trace_work {
	ibsp_t *ibsp;
	ibsp_trace_t *trace;
	vec3 start;
	vec3 end;
	vec3 size[2];
	vec3 offsets[8];
	vec3 bounds[2];
	vec3 extents;
	bool is_point;
} trace_work_t;

static uint8_t brush_check_count = 0;
static uint8_t brush_check_counts[IBSP_MAX_BRUSHES];

#define TRACE_EPSILON (0.125f)

#define PLANE_TYPE(v) (v[0] == 1.0f ? 0 : (v[1] == 1.0f ? 1 : (v[2] == 1.0f ? 2 : 3)))

int plane_signbits(ibsp_plane_t *plane)
{
	int	bits = 0;
	for (int i = 0; i < 3; i++)
	{
		if (plane->normal[i] < 0)
			bits |= 1 << i;
	}
	return bits;
}

static void clip_brush(trace_work_t *tw, ibsp_brush_t *brush)
{
	int i, j;
	float frac;
	float ofs[3];
	float dist, dist1, dist2;
	ibsp_brushside_t *side;
	ibsp_plane_t *plane;
	ibsp_plane_t *clipplane;
	bool get_out, start_out;
	float enter_frac, exit_frac;
	int signbits;

	enter_frac = -1.0;
	exit_frac = 1.0;
	get_out = false;
	start_out = false;
	clipplane = NULL;

	for (i = brush->first_brushside; i < brush->first_brushside + brush->num_brushsides; i++)
	{
		side = &tw->ibsp->brushsides[i];
		plane = &tw->ibsp->planes[side->plane];

		signbits = plane_signbits(plane);

		dist = plane->dist - glm_vec3_dot(tw->offsets[signbits], plane->normal);

		dist1 = glm_vec3_dot(tw->start, plane->normal) - dist;
		dist2 = glm_vec3_dot(tw->end, plane->normal) - dist;

		if (dist2 > 0.0f)
			get_out = true;

		if (dist1 > 0.0f)
			start_out = true;

		if (dist1 > 0.0f && (dist2 >= TRACE_EPSILON || dist2 >= dist1))
			return;

		if (dist1 <= 0.0f && dist2 <= 0.0f)
			continue;

		if (dist1 > dist2)
		{
			frac = (dist1 - TRACE_EPSILON) / (dist1 - dist2);
			if (frac < 0.0f)
				frac = 0.0f;
			if (frac > enter_frac)
			{
				enter_frac = frac;
				clipplane = plane;
			}
		}
		else
		{
			frac = (dist1 + TRACE_EPSILON) / (dist1 - dist2);
			if (frac > 1.0f)
				frac = 1.0f;
			if (frac < exit_frac)
				exit_frac = frac;
		}
	}

	if (!start_out)
	{
		tw->trace->start_solid = true;
		if (!get_out)
		{
			tw->trace->all_solid = true;
			tw->trace->fraction = 0;
		}
		return;
	}

	if (enter_frac < exit_frac)
	{
		if (enter_frac > -1.0f && enter_frac < tw->trace->fraction)
		{
			if (enter_frac < 0.0f)
				enter_frac = 0.0f;
			tw->trace->fraction = enter_frac;
			if (clipplane)
			{
				glm_vec3_copy(clipplane->normal, tw->trace->plane);
				tw->trace->plane[3] = clipplane->dist;
			}
		}
	}
}

static void clip_leaf(trace_work_t *tw, ibsp_leaf_t *leaf)
{
	for (int i = leaf->first_leafbrush; i < leaf->first_leafbrush + leaf->num_leafbrushes; i++)
	{
		ibsp_brush_t *brush = &tw->ibsp->brushes[tw->ibsp->leafbrushes[i]];

		// already checked in another leaf
		if (brush_check_counts[tw->ibsp->leafbrushes[i]] == brush_check_count)
			continue;

		brush_check_counts[tw->ibsp->leafbrushes[i]] = brush_check_count;

		clip_brush(tw, brush);

		if (!tw->trace->fraction)
			return;
	}
}

static void trace_recurse(trace_work_t *tw, int32_t node_index, float start_frac, float end_frac, vec3 start, vec3 end)
{
	ibsp_plane_t *plane;
	ibsp_node_t *node;
	float dot1, dot2;
	float frac1, frac2;
	float mid_frac;
	vec3 mid;
	float dist;
	int side;
	int plane_type;
	float offset;

	// hit something already
	if (tw->trace->fraction <= start_frac)
		return;

	// leaf
	if (node_index < 0)
	{
		clip_leaf(tw, &tw->ibsp->leafs[-1 - node_index]);
		return;
	}

	node = &tw->ibsp->nodes[node_index];
	plane = &tw->ibsp->planes[node->plane];

	plane_type = PLANE_TYPE(plane->normal);

	if (plane_type < 3)
	{
		dot1 = start[plane_type] - plane->dist;
		dot2 = end[plane_type] - plane->dist;
		offset = tw->extents[plane_type];
	}
	else
	{
		dot1 = glm_vec3_dot(plane->normal, start) - plane->dist;
		dot2 = glm_vec3_dot(plane->normal, end) - plane->dist;
		if (tw->is_point)
			offset = 0;
		else
			offset = 2048;
	}

	if (dot1 >= offset + 1.0f && dot2 >= offset + 1.0f)
	{
		trace_recurse(tw, node->children[0], start_frac, end_frac, start, end);
		return;
	}

	if (dot1 < -offset - 1.0f && dot2 < -offset - 1.0f)
	{
		trace_recurse(tw, node->children[1], start_frac, end_frac, start, end);
		return;
	}

	if (dot1 < dot2)
	{
		dist = 1.0 / (dot1 - dot2);
		side = 1;
		frac1 = (dot1 - offset + TRACE_EPSILON) * dist;
		frac2 = (dot1 + offset + TRACE_EPSILON) * dist;
	}
	else if (dot1 > dot2)
	{
		dist = 1.0f / (dot1 - dot2);
		side = 0;
		frac1 = (dot1 + offset + TRACE_EPSILON) * dist;
		frac2 = (dot1 - offset - TRACE_EPSILON) * dist;
	}
	else
	{
		side = 0;
		frac1 = 1;
		frac2 = 0;
	}

	if (frac1 < 0.0f) frac1 = 0.0f;
	if (frac1 > 1.0f) frac1 = 1.0f;

	mid_frac = start_frac + (end_frac - start_frac) * frac1;

	mid[0] = start[0] + frac1 * (end[0] - start[0]);
	mid[1] = start[1] + frac1 * (end[1] - start[1]);
	mid[2] = start[2] + frac1 * (end[2] - start[2]);

	trace_recurse(tw, node->children[side], start_frac, mid_frac, start, mid);

	if (frac2 < 0.0f) frac2 = 0.0f;
	if (frac2 > 1.0f) frac2 = 1.0f;

	mid_frac = start_frac + (end_frac - start_frac) * frac2;

	mid[0] = start[0] + frac2 * (end[0] - start[0]);
	mid[1] = start[1] + frac2 * (end[1] - start[1]);
	mid[2] = start[2] + frac2 * (end[2] - start[2]);

	trace_recurse(tw, node->children[side ^ 1], mid_frac, end_frac, mid, end);
}

void ibsp_trace(ibsp_t *ibsp, ibsp_trace_t *trace, vec3 start, vec3 end, vec3 mins, vec3 maxs)
{
	trace_work_t tw;
	vec3 offset;

	brush_check_count++;

	if (!mins) mins = GLM_VEC3_ZERO;
	if (!maxs) maxs = GLM_VEC3_ZERO;

	tw.ibsp = ibsp;
	tw.trace = trace;

	glm_vec3_copy(start, trace->start);
	trace->fraction = 1.0f;

	for (int i = 0; i < 3; i++)
	{
		offset[i] = (mins[i] + maxs[i]) * 0.5;
		tw.size[0][i] = mins[i] - offset[i];
		tw.size[1][i] = maxs[i] - offset[i];
		tw.start[i] = start[i] + offset[i];
		tw.end[i] = end[i] + offset[i];
	}

	tw.offsets[0][0] = tw.size[0][0];
	tw.offsets[0][1] = tw.size[0][1];
	tw.offsets[0][2] = tw.size[0][2];

	tw.offsets[1][0] = tw.size[1][0];
	tw.offsets[1][1] = tw.size[0][1];
	tw.offsets[1][2] = tw.size[0][2];

	tw.offsets[2][0] = tw.size[0][0];
	tw.offsets[2][1] = tw.size[1][1];
	tw.offsets[2][2] = tw.size[0][2];

	tw.offsets[3][0] = tw.size[1][0];
	tw.offsets[3][1] = tw.size[1][1];
	tw.offsets[3][2] = tw.size[0][2];

	tw.offsets[4][0] = tw.size[0][0];
	tw.offsets[4][1] = tw.size[0][1];
	tw.offsets[4][2] = tw.size[1][2];

	tw.offsets[5][0] = tw.size[1][0];
	tw.offsets[5][1] = tw.size[0][1];
	tw.offsets[5][2] = tw.size[1][2];

	tw.offsets[6][0] = tw.size[0][0];
	tw.offsets[6][1] = tw.size[1][1];
	tw.offsets[6][2] = tw.size[1][2];

	tw.offsets[7][0] = tw.size[1][0];
	tw.offsets[7][1] = tw.size[1][1];
	tw.offsets[7][2] = tw.size[1][2];

	for (int i = 0; i < 3; i++)
	{
		if (tw.start[i] < tw.end[i])
		{
			tw.bounds[0][i] = tw.start[i] + tw.size[0][i];
			tw.bounds[1][i] = tw.end[i] + tw.size[1][i];
		}
		else
		{
			tw.bounds[0][i] = tw.end[i] + tw.size[0][i];
			tw.bounds[1][i] = tw.start[i] + tw.size[1][i];
		}
	}

	if (start[0] == end[0] && start[1] == end[1] && start[2] == end[2])
	{
		// CM_PositionTest( &tw );
	}
	else
	{
		if (tw.size[0][0] == 0 && tw.size[0][1] == 0 && tw.size[0][2] == 0)
		{
			tw.is_point = true;
			glm_vec3_zero(tw.extents);
		}
		else
		{
			tw.is_point = false;
			tw.extents[0] = tw.size[1][0];
			tw.extents[1] = tw.size[1][1];
			tw.extents[2] = tw.size[1][2];
		}

		trace_recurse(&tw, 0, 0, 1, tw.start, tw.end);
	}

	if (tw.trace->fraction == 1.0f)
	{
		glm_vec3_copy(end, tw.trace->end);
	}
	else
	{
		for (int i = 0; i < 3; i++)
			tw.trace->end[i] = start[i] + tw.trace->fraction * (end[i] - start[i]);
	}
}

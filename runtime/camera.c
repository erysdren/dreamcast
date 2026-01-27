
#include "runtime.h"
#include "camera.h"

void camera_init(camera_t *camera)
{
	glm_vec3_zero(camera->origin);
	glm_vec3_zero(camera->angles);

	glm_vec3_copy(GLM_ZUP, camera->up);

	camera->aspect = 640.0f/480.0f;
	camera->near = 0.1f;
	camera->far = 1024.0f;
	camera->fov = 45;
}

void camera_make_viewproj(camera_t *camera, mat4 viewproj)
{
	vec3 look;
	mat4 view, proj;

	look[0] = cos(glm_rad(camera->angles[0])) * cos(glm_rad(camera->angles[1]));
	look[1] = sin(glm_rad(camera->angles[1]));
	look[2] = sin(glm_rad(camera->angles[0])) * cos(glm_rad(camera->angles[1]));

	glm_look(camera->origin, look, camera->up, view);
	glm_perspective(camera->fov, camera->aspect, camera->near, camera->far, proj);
	glm_mat4_mul(proj, view, viewproj);
}

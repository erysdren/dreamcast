#ifndef _MD3_H_
#define _MD3_H_

#include <stdint.h>

#define MD3_MAGIC (0x51806873)
#define MD3_VERSION (15)

typedef struct md3 {
	uint32_t magic; ///< MD3_MAGIC
	uint32_t version; ///< MD3_VERSION
	char name[64]; ///< path to md3 object i.e. models/example.md3
	uint32_t flags; ///<
	uint32_t num_frames; ///< number of animation frames
	uint32_t num_tags; ///< number of tags
	uint32_t num_surfaces; ///< number of surfaces
	uint32_t num_skins; ///< number of skins
	uint32_t ofs_frames;
	uint32_t ofs_tags;
	uint32_t ofs_surfaces;
	uint32_t ofs_end; ///< size of md3 object
} md3_t;

#define MD3_GET_FRAMES(md3) ((md3_frame_t *)(((uint8_t *)(md3)) + (md3)->ofs_frames))
#define MD3_GET_TAGS(md3) ((md3_tag_t *)(((uint8_t *)(md3)) + (md3)->ofs_tags))
#define MD3_GET_SURFACES(md3) ((md3_surface_t *)(((uint8_t *)(md3)) + (md3)->ofs_surfaces))

typedef struct md3_frame {
	float mins[3];
	float maxs[3];
	float origin[3];
	float radius;
	char name[16];
} md3_frame_t;

typedef struct md3_tag {
	char name[64];
	float origin[3];
	float axis[3][3];
} md3_tag_t;

typedef struct md3_surface {
	uint32_t magic;
	char name[64];
	uint32_t flags;
	uint32_t num_frames;
	uint32_t num_shaders;
	uint32_t num_vertices;
	uint32_t num_triangles;
	uint32_t ofs_triangles;
	uint32_t ofs_shaders;
	uint32_t ofs_texcoords;
	uint32_t ofs_vertices;
	uint32_t ofs_end;
} md3_surface_t;

#define MD3_SURFACE_GET_TRIANGLES(surface) ((md3_triangle_t *)(((uint8_t *)(surface)) + (surface)->ofs_triangles))
#define MD3_SURFACE_GET_SHADERS(surface) ((md3_shader_t *)(((uint8_t *)(surface)) + (surface)->ofs_shaders))
#define MD3_SURFACE_GET_TEXCOORDS(surface) ((md3_texcoord_t *)(((uint8_t *)(surface)) + (surface)->ofs_texcoords))
#define MD3_SURFACE_GET_VERTICES(surface) ((md3_vertex_t *)(((uint8_t *)(surface)) + (surface)->ofs_vertices))

typedef struct md3_triangle {
	uint32_t indices[3];
} md3_triangle_t;

typedef struct md3_shader {
	char name[64];
	uint32_t index;
} md3_shader_t;

typedef struct md3_texcoord {
	float coords[2];
} md3_texcoord_t;

#define MD3_POSITION_SCALE (1.0f/64.0f)
#define MD3_UNCOMPRESS_POSITION(x) ((float)(x) * MD3_POSITION_SCALE)

typedef struct md3_vertex {
	int16_t position[3];
	int16_t normal;
} md3_vertex_t;

#endif // _MD3_H_

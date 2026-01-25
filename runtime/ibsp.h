#ifndef _IBSP_H_
#define _IBSP_H_
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#pragma pack(push, 1)

#define IBSP_MAGIC (0x50534249)
#define IBSP_VERSION (0x0000002E)

enum {
	IBSP_LUMP_ENTITES = 0,
	IBSP_LUMP_TEXTURES = 1,
	IBSP_LUMP_PLANES = 2,
	IBSP_LUMP_NODES = 3,
	IBSP_LUMP_LEAFS = 4,
	IBSP_LUMP_LEAFFACES = 5,
	IBSP_LUMP_LEAFBRUSHES = 6,
	IBSP_LUMP_MODELS = 7,
	IBSP_LUMP_BRUSHES = 8,
	IBSP_LUMP_BRUSHSIDES = 9,
	IBSP_LUMP_VERTICES = 10,
	IBSP_LUMP_MESHVERTS = 11,
	IBSP_LUMP_EFFECTS = 12,
	IBSP_LUMP_FACES = 13,
	IBSP_LUMP_LIGHTMAPS = 14,
	IBSP_LUMP_LIGHTVOLS = 15,
	IBSP_LUMP_VISDATA = 16,
	IBSP_NUM_LUMPS = 17
};

#define IBSP_MAX_ENTITIES (16384)
#define IBSP_MAX_TEXTURES (32)
#define IBSP_MAX_PLANES (8192)
#define IBSP_MAX_NODES (8192)
#define IBSP_MAX_LEAFS (8192)
#define IBSP_MAX_LEAFFACES (8192)
#define IBSP_MAX_LEAFBRUSHES (8192)
#define IBSP_MAX_MODELS (1024)
#define IBSP_MAX_BRUSHES (16384)
#define IBSP_MAX_BRUSHSIDES (8192)
#define IBSP_MAX_VERTICES (32768)
#define IBSP_MAX_MESHVERTS (32768)
#define IBSP_MAX_FACES (8192)
#define IBSP_MAX_VISDATA (32768)

typedef struct ibsp_plane {
	float normal[3];
	float dist;
} ibsp_plane_t;

typedef struct ibsp_node {
	int32_t plane;
	int32_t children[2];
	int32_t mins[3];
	int32_t maxs[3];
} ibsp_node_t;

typedef struct ibsp_leaf {
	int32_t cluster;
	int32_t area;
	int32_t mins[3];
	int32_t maxs[3];
	int32_t first_leafface;
	int32_t num_leaffaces;
	int32_t first_leafbrush;
	int32_t num_leafbrushes;
} ibsp_leaf_t;

typedef struct ibsp_texture {
	char name[64];
	uint32_t flags;
	uint32_t contents;
} ibsp_texture_t;

typedef struct ibsp_brush {
	int32_t first_brushside;
	int32_t num_brushsides;
	int32_t texture;
} ibsp_brush_t;

typedef struct ibsp_brushside {
	int32_t plane;
	int32_t texture;
} ibsp_brushside_t;

typedef struct ibsp_visdata {
	int32_t num_vecs;
	int32_t len_vec;
	uint8_t vecs[IBSP_MAX_VISDATA];
} ibsp_visdata_t;

typedef struct ibsp_face {
	int32_t texture;
	int32_t effect;
	int32_t type;
	int32_t first_vert;
	int32_t num_verts;
	int32_t first_meshvert;
	int32_t num_meshverts;
	int32_t lightmap;
	int32_t lightmap_start[2];
	int32_t lightmap_size[2];
	float lightmap_origin[3];
	float lightmap_texcoords[2][3];
	float normal[3];
	int32_t patch_size[2];
} ibsp_face_t;

typedef struct ibsp_model {
	float mins[3];
	float maxs[3];
	int32_t first_face;
	int32_t num_faces;
	int32_t first_brush;
	int32_t num_brushes;
} ibsp_model_t;

typedef struct ibsp_vertex {
	float position[3];
	float texcoords[2][2];
	float normal[3];
	uint8_t colour[4];
} ibsp_vertex_t;

typedef struct ibsp_lightmap {
	uint8_t lightmap[128][128][3];
} ibsp_lightmap_t;

typedef struct ibsp_lump {
	uint32_t offset;
	uint32_t length;
} ibsp_lump_t;

typedef struct ibsp_header {
	uint32_t magic;
	uint32_t version;
	ibsp_lump_t lumps[IBSP_NUM_LUMPS];
} ibsp_header_t;

extern size_t num_ibsp_textures;
extern ibsp_texture_t *ibsp_textures;

extern size_t num_ibsp_planes;
extern ibsp_plane_t *ibsp_planes;

extern size_t num_ibsp_nodes;
extern ibsp_node_t *ibsp_nodes;

extern size_t num_ibsp_leafs;
extern ibsp_leaf_t *ibsp_leafs;

extern size_t num_ibsp_leaffaces;
extern int32_t *ibsp_leaffaces;

extern size_t num_ibsp_leafbrushes;
extern int32_t *ibsp_leafbrushes;

extern size_t num_ibsp_models;
extern ibsp_model_t *ibsp_models;

extern size_t num_ibsp_brushes;
extern ibsp_brush_t *ibsp_brushes;

extern size_t num_ibsp_brushsides;
extern ibsp_brushside_t *ibsp_brushsides;

extern size_t num_ibsp_vertices;
extern ibsp_vertex_t *ibsp_vertices;

extern size_t num_ibsp_meshverts;
extern int32_t *ibsp_meshverts;

extern size_t num_ibsp_faces;
extern ibsp_face_t *ibsp_faces;

extern size_t num_ibsp_lightmaps;
extern ibsp_lightmap_t *ibsp_lightmaps;

extern ibsp_visdata_t *ibsp_visdata;

bool ibsp_load(const void *ptr);

#pragma pack(pop)

#ifdef __cplusplus
}
#endif
#endif // _IBSP_H_

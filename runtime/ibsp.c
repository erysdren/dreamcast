
#include "utils.h"
#include "ibsp.h"

/* ibsp data pointers */

size_t num_ibsp_entities;
char *ibsp_entities;

size_t num_ibsp_textures;
ibsp_texture_t *ibsp_textures;

size_t num_ibsp_planes;
ibsp_plane_t *ibsp_planes;

size_t num_ibsp_nodes;
ibsp_node_t *ibsp_nodes;

size_t num_ibsp_leafs;
ibsp_leaf_t *ibsp_leafs;

size_t num_ibsp_leaffaces;
int32_t *ibsp_leaffaces;

size_t num_ibsp_leafbrushes;
int32_t *ibsp_leafbrushes;

size_t num_ibsp_models;
ibsp_model_t *ibsp_models;

size_t num_ibsp_brushes;
ibsp_brush_t *ibsp_brushes;

size_t num_ibsp_brushsides;
ibsp_brushside_t *ibsp_brushsides;

size_t num_ibsp_vertices;
ibsp_vertex_t *ibsp_vertices;

size_t num_ibsp_meshverts;
int32_t *ibsp_meshverts;

size_t num_ibsp_faces;
ibsp_face_t *ibsp_faces;

size_t num_ibsp_lightmaps;
ibsp_lightmap_t *ibsp_lightmaps;

size_t num_ibsp_visdata;
ibsp_visdata_t *ibsp_visdata;

bool ibsp_load(const void *ptr)
{
	if (!ptr)
		return false;

	ibsp_header_t *header = (ibsp_header_t *)ptr;

	if (header->magic != IBSP_MAGIC || header->version != IBSP_VERSION)
		return false;

#define LOADLUMP(d, l) \
	d = (typeof(d))((uint8_t *)ptr + header->lumps[l].offset); \
	num_##d = header->lumps[l].length / sizeof(*d)

	LOADLUMP(ibsp_entities, IBSP_LUMP_ENTITES);
	LOADLUMP(ibsp_textures, IBSP_LUMP_TEXTURES);
	LOADLUMP(ibsp_planes, IBSP_LUMP_PLANES);
	LOADLUMP(ibsp_nodes, IBSP_LUMP_NODES);
	LOADLUMP(ibsp_leafs, IBSP_LUMP_LEAFS);
	LOADLUMP(ibsp_leaffaces, IBSP_LUMP_LEAFFACES);
	LOADLUMP(ibsp_leafbrushes, IBSP_LUMP_LEAFBRUSHES);
	LOADLUMP(ibsp_models, IBSP_LUMP_MODELS);
	LOADLUMP(ibsp_brushes, IBSP_LUMP_BRUSHES);
	LOADLUMP(ibsp_brushsides, IBSP_LUMP_BRUSHSIDES);
	LOADLUMP(ibsp_vertices, IBSP_LUMP_VERTICES);
	LOADLUMP(ibsp_meshverts, IBSP_LUMP_MESHVERTS);
	LOADLUMP(ibsp_faces, IBSP_LUMP_FACES);
	LOADLUMP(ibsp_lightmaps, IBSP_LUMP_LIGHTMAPS);
	LOADLUMP(ibsp_visdata, IBSP_LUMP_TEXTURES);

#undef LOADLUMP

	return true;
}

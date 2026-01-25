
#include "utils.h"
#include "ibsp.h"

bool ibsp_load(const void *ptr, ibsp_t *ibsp)
{
	if (!ptr)
		return false;

	ibsp->header = (ibsp_header_t *)ptr;

	if (ibsp->header->magic != IBSP_MAGIC || ibsp->header->version != IBSP_VERSION)
		return false;

#define LOADLUMP(d, l) \
	ibsp->d = (typeof(ibsp->d))((uint8_t *)ptr + ibsp->header->lumps[l].offset); \
	ibsp->num_##d = ibsp->header->lumps[l].length / sizeof(*(ibsp->d))

	LOADLUMP(entities, IBSP_LUMP_ENTITES);
	LOADLUMP(textures, IBSP_LUMP_TEXTURES);
	LOADLUMP(planes, IBSP_LUMP_PLANES);
	LOADLUMP(nodes, IBSP_LUMP_NODES);
	LOADLUMP(leafs, IBSP_LUMP_LEAFS);
	LOADLUMP(leaffaces, IBSP_LUMP_LEAFFACES);
	LOADLUMP(leafbrushes, IBSP_LUMP_LEAFBRUSHES);
	LOADLUMP(models, IBSP_LUMP_MODELS);
	LOADLUMP(brushes, IBSP_LUMP_BRUSHES);
	LOADLUMP(brushsides, IBSP_LUMP_BRUSHSIDES);
	LOADLUMP(vertices, IBSP_LUMP_VERTICES);
	LOADLUMP(meshverts, IBSP_LUMP_MESHVERTS);
	LOADLUMP(effects, IBSP_LUMP_EFFECTS);
	LOADLUMP(faces, IBSP_LUMP_FACES);
	LOADLUMP(lightmaps, IBSP_LUMP_LIGHTMAPS);
	LOADLUMP(lightvols, IBSP_LUMP_LIGHTVOLS);
	LOADLUMP(visdata, IBSP_LUMP_VISDATA);

#undef LOADLUMP

	return true;
}

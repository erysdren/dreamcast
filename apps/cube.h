static vec3 cube_vertex_position[] = {
	{  1.0f,  1.0f, -1.0f },
	{  1.0f, -1.0f, -1.0f },
	{  1.0f,  1.0f,  1.0f },
	{  1.0f, -1.0f,  1.0f },
	{ -1.0f,  1.0f, -1.0f },
	{ -1.0f, -1.0f, -1.0f },
	{ -1.0f,  1.0f,  1.0f },
	{ -1.0f, -1.0f,  1.0f },
};

static uint32_t cube_vertex_color[] = {
	0xff0000, // red
	0x00ff00, // green
	0x0000ff, // blue
	0xffff00, // yellow
	0x00ffff, // cyan
	0xff00ff, // magenta
	0xff7f00, // orange
	0x7f00ff, // violet
};

static ivec3 cube_faces[] = {
	{4, 2, 0},
	{2, 7, 3},
	{6, 5, 7},
	{1, 7, 5},
	{0, 3, 1},
	{4, 1, 5},
	{4, 6, 2},
	{2, 6, 7},
	{6, 4, 5},
	{1, 3, 7},
	{0, 2, 3},
	{4, 0, 1},
};

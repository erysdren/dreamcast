
#include <stdint.h>
#include <stddef.h>

#include "utils.h"

#ifndef ROMFS_SOURCE_FILENAME
#error
#endif

#include ROMFS_SOURCE_FILENAME

int ROMFS_LocateFile(const char *path, size_t *index, size_t *size)
{
	for (size_t i = 0; i < zip_dir_num_files; i++)
	{
		if (__builtin_strcmp(path, zip_dir[i].path) == 0)
		{
			if (index)
				*index = i;
			if (size)
				*size = zip_dir[i].size;
			return 0;
		}
	}

	return 1;
}

const void *ROMFS_GetFileFromIndex(size_t index, size_t *size)
{
	if (index >= zip_dir_num_files)
		return NULL;
	if (size)
		*size = zip_dir[index].size;
	return zip_dir[index].ptr;
}

const void *ROMFS_GetFileFromPath(const char *path, size_t *size)
{
	size_t index;
	if (ROMFS_LocateFile(path, &index, size) == 0)
		return zip_dir[index].ptr;
	return NULL;
}

size_t ROMFS_GlobFiles(const char *wild, const char **matched, size_t max_matched)
{
	size_t num_matched = 0;
	for (size_t i = 0; i < zip_dir_num_files; i++)
	{
		if (num_matched >= max_matched)
			break;

		if (wildcmp(wild, zip_dir[i].path))
		{
			matched[num_matched++] = zip_dir[i].path;
		}
	}
	return num_matched;
}

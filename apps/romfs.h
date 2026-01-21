#ifndef _ROMFS_H_
#define _ROMFS_H_

#include <stdint.h>
#include <stddef.h>

// get file index and size from the given path
// returns 0 on success or nonzero for error
int ROMFS_LocateFile(const char *path, size_t *index, size_t *size);

// retrieve file data pointer and size from the given index
// returns NULL on error
const void *ROMFS_GetFileFromIndex(size_t index, size_t *size);

// retrieve file data pointer and size from the given path
// returns NULL on error
const void *ROMFS_GetFileFromPath(const char *path, size_t *size);

#endif // _ROMFS_H_

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "disk.h"
#include "fs.h"

#define EXIT_NOERR 0
#define EXIT_ERR -1

#define UNUSED(x) (void)(x)

struct __attribute__((__packed__)) super_block {
	int32_t signiture[2];
	int16_t block_total;
	int16_t root_index;
	int16_t data_index;
	int16_t data_block_total;
	int8_t FAT_block_total;
	int8_t padding[BLOCK_SIZE - 17];
};

struct __attribute__((__packed__)) FAT {
	int16_t** block_table;
	int16_t length;
};

struct __attribute__((__packed__)) root {
	int32_t filename[4];
	int32_t file_size;
	int16_t data_index;
	int8_t padding[10];
};

/* TODO: Phase 1 */

int fs_mount(const char *diskname)
{
	/* TODO: Phase 1 */
	UNUSED(diskname);
	return EXIT_NOERR;
}

int fs_umount(void)
{
	/* TODO: Phase 1 */
	return EXIT_NOERR;
}

int fs_info(void)
{
	/* TODO: Phase 1 */
	return EXIT_NOERR;
}

int fs_create(const char *filename)
{
	/* TODO: Phase 2 */
	UNUSED(filename);
	return EXIT_NOERR;
}

int fs_delete(const char *filename)
{
	/* TODO: Phase 2 */
	UNUSED(filename);
	return EXIT_NOERR;
}

int fs_ls(void)
{
	/* TODO: Phase 2 */
	return EXIT_NOERR;
}

int fs_open(const char *filename)
{
	/* TODO: Phase 3 */
	UNUSED(filename);
	return EXIT_NOERR;
}

int fs_close(int fd)
{
	/* TODO: Phase 3 */
	UNUSED(fd);
	return EXIT_NOERR;
}

int fs_stat(int fd)
{
	/* TODO: Phase 3 */
	UNUSED(fd);
	return EXIT_NOERR;
}

int fs_lseek(int fd, size_t offset)
{
	/* TODO: Phase 3 */
	UNUSED(fd);
	UNUSED(offset);
	return EXIT_NOERR;
}

int fs_write(int fd, void *buf, size_t count)
{
	/* TODO: Phase 4 */
	UNUSED(fd);
	UNUSED(buf);
	UNUSED(count);
	return EXIT_NOERR;
}

int fs_read(int fd, void *buf, size_t count)
{
	/* TODO: Phase 4 */
	UNUSED(fd);
	UNUSED(buf);
	UNUSED(count);
	return EXIT_NOERR;
}


#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>
#include <stdbool.h>

#include "disk.h"
#include "fs.h"

#define EXIT_NOERR 0
#define EXIT_ERR -1

#define UNUSED(x) (void)(x)

struct __attribute__((__packed__)) super_block {
	char signiture[8];
	uint16_t block_total;
	uint16_t root_index;
	uint16_t data_index;
	uint16_t data_block_total;
	uint8_t fat_block_total;
	uint8_t padding[BLOCK_SIZE - 17];
};

struct __attribute__((__packed__)) FAT {
	uint16_t** block_table;
};

struct __attribute__((__packed__)) root {
	char filename[FS_FILENAME_LEN];
	uint32_t file_size;
	uint16_t data_index;
	uint8_t padding[10];
};

struct super_block superblock;
struct FAT fatblock;
struct root rootdirectory[FS_FILE_MAX_COUNT];
uint16_t* table;
bool file_open = false;

/* TODO: Phase 1 */

int fs_mount(const char *diskname)
{
	/* TODO: Phase 1 */
	if (block_disk_open(diskname)) {
		printf("diskname\n");
		return EXIT_ERR;
	}	
	if (block_read(0, &superblock)) {
		printf("read\n");
		return EXIT_ERR;
	}
	table = malloc(superblock.fat_block_total * BLOCK_SIZE);
	fatblock.block_table = &table;
	
	for (int i = 0; i < superblock.fat_block_total; i++) {
		if (block_read(i + 1, table + ((i * BLOCK_SIZE) / 2))) {
			printf("read\n");
			free(table);
			return EXIT_ERR;
		}
	}
	if (block_read(superblock.root_index, rootdirectory)) {
		printf("read\n");
		free(table);
		return EXIT_ERR;
	}
	file_open = true;
	return EXIT_NOERR;
}

int fs_umount(void)
{
	/* TODO: Phase 1 */
	if (block_disk_close()) {
		printf("none open\n");
		return EXIT_ERR;
	}
	free(table);
	return EXIT_NOERR;
}

int fs_info(void)
{
	/* TODO: Phase 1 */
	if (!file_open) {
		printf("file\n");
		return EXIT_ERR;
	}
	
	printf("FS Info:\n");
	printf("total_blk_count=%d\n", superblock.block_total);
	printf("fat_blk_count=%d\n", superblock.fat_block_total);
	printf("rdir_blk=%d\n", superblock.root_index);
	printf("data_blk=%d\n", superblock.data_index);
	printf("data_blk_count=%d\n", superblock.data_block_total);
	int count = 0;
	for (int i = 0; i < superblock.data_block_total; i++) {
		if ((*fatblock.block_table)[i] == 0) {
			count++;
		}
	}
	printf("fat_free_ratio=%d/%d\n", count, superblock.data_block_total);
	count = 0;
	for (int i = 0; i < FS_FILE_MAX_COUNT; i++) {
		if (rootdirectory[i].data_index == 0) {
			count++;
		}
	}
	printf("rdir_free_ratio=%d/%d\n", count, FS_FILE_MAX_COUNT);
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


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
#define FAT_EOC 0xFFFF

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
	uint16_t **block_table;
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

int fs_mount(const char *diskname)
{
	if (block_disk_open(diskname)) {
		printf("diskname\n");
		return EXIT_ERR;
	}	
	if (block_read(0, &superblock)) {
		printf("read super\n");
		return EXIT_ERR;
	}
	table = malloc(superblock.fat_block_total * BLOCK_SIZE);
	fatblock.block_table = &table;
	
	for (int i = 0; i < superblock.fat_block_total; i++) {
		if (block_read(i + 1, table + ((i * BLOCK_SIZE) / 2))) {
			printf("read fat\n");
			free(table);
			return EXIT_ERR;
		}
	}
	if (block_read(superblock.root_index, rootdirectory)) {
		printf("read root\n");
		free(table);
		return EXIT_ERR;
	}
	file_open = true;
	return EXIT_NOERR;
}

int fs_umount(void)
{
	if (block_disk_close()) {
		printf("no file open\n");
		return EXIT_ERR;
	}
	free(table);
	file_open = false;
	return EXIT_NOERR;
}

int fs_info(void)
{
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
	if (filename == NULL) {
		/* filename invalid */
		return EXIT_ERR;
	}

	if (strlen(filename) > FS_FILENAME_LEN) {
		/* filename too long */
		return EXIT_ERR;
	}

	/* check if file already exists */
	int i;
	for (i = 0; i < FS_FILE_MAX_COUNT; i++) {
		if (!strcmp(rootdirectory[i].filename, filename)) {
			return EXIT_ERR;
		}
	}

	/* find empty entry in root directory */
	int empty = -1;
	for (i = 0; i < FS_FILE_MAX_COUNT; i++) {
		if (rootdirectory[i].filename[0] == '\0') {
			// empty
			empty = i;
			break;
		}
	}

	if (empty < 0) {
		/* root directory is full */
		return EXIT_ERR;
	}

	// and fill it out with the proper info
	// at first only specify name of the file and reset the other info since there is
	// no content at this point
	// size should be set to 0
	// and the first index on the data blocks should be set to FAT_EOC
	strcpy(rootdirectory[empty].filename, filename);
	rootdirectory[empty].file_size = 0;
	rootdirectory[empty].data_index = FAT_EOC;
	// padding[10];

	return EXIT_NOERR;
}

int fs_delete(const char *filename)
{
	/* TODO: Phase 2 */
	if (filename == NULL) {
		/* filename invalid */
		return EXIT_ERR;
	}

	/* find the file */
	int i;
	int file_index = -1;
	for (i = 0; i < FS_FILE_MAX_COUNT; i++) {
		if (!strcmp(rootdirectory[i].filename, filename)) {
			file_index = i;
			break;
		}
	}

	if (file_index == -1) {
		/* no file filename to delete */
		return EXIT_ERR;
	}

	if (file_open) {
		/* file is currently open */
		return EXIT_ERR;
	}

	/* free all data blocks containing file's contents in the FAT */
	uint16_t old_index, next_index = rootdirectory[file_index].data_index;
	while (next_index != FAT_EOC) {
		old_index = next_index;
		next_index = *(fatblock.block_table[next_index]);
		*(fatblock.block_table[old_index]) = 0;
		// free();
	}

	/* empty file's entry */
	rootdirectory[file_index].filename[0] = '\0';
	rootdirectory[file_index].file_size = 0;
	*(fatblock.block_table[next_index]) = 0;
	//	rootdirectory[next_index].data_index = 0;


	UNUSED(filename);
	return EXIT_NOERR;
}

int fs_ls(void)
{
	/* TODO: Phase 2 */
	printf("FS Ls:\n");
	// return -1...
	int i;
	for (i = 0; i < FS_FILE_MAX_COUNT; i++) {
		if (rootdirectory[i].filename[0] != '\0') {
			printf("file: %s, size: %u, data_blk: %u\n",
					rootdirectory[i].filename,
					rootdirectory[i].file_size,
					rootdirectory[i].data_index);
		}
	}
	
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


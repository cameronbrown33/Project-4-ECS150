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

struct __attribute__((__packed__)) file_descriptor {
	int fd;
	size_t offset;
	char filename[FS_FILENAME_LEN];
};

struct super_block superblock;
struct FAT fatblock;
struct root rootdirectory[FS_FILE_MAX_COUNT];
uint16_t* table;
bool file_system_open = false;
struct file_descriptor fd_open_list[FS_OPEN_MAX_COUNT];
int open_files = 0;
int global_fd = 0;

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
	file_system_open = true;
	return EXIT_NOERR;
}

int fs_umount(void)
{
	if (block_disk_close()) {
		printf("no file open\n");
		return EXIT_ERR;
	}
	free(table);
	file_system_open = false;
	return EXIT_NOERR;
}

int fs_info(void)
{
	if (!file_system_open) {
		printf("file\n");
		return EXIT_ERR;
	}
	
	printf("FS Info:\n");
	printf("total_blk_count=%d\n", superblock.block_total);
	printf("fat_blk_count=%d\n", superblock.fat_block_total);
	printf("rdir_blk=%d\n", superblock.root_index);
	printf("data_blk=%d\n", superblock.data_index);
	printf("data_blk_count=%d\n", superblock.data_block_total);
	int i;
	int count = 0;
	for (i = 0; i < superblock.data_block_total; i++) {
		if ((*fatblock.block_table)[i] == 0) {
			count++;
		}
	}
	printf("fat_free_ratio=%d/%d\n", count, superblock.data_block_total);
	count = 0;
	for (i = 0; i < FS_FILE_MAX_COUNT; i++) {
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
	// printf("File: %s\n", rootdirectory[empty].filename);
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
//			printf("file: %s\n", rootdirectory[file_index].filename);
			break;
		}
	}
	
	if (file_index < 0) {
		/* no file filename to delete */
		return EXIT_ERR;
	}

	/* file is still open */
	for (i = 0; i < open_files; i++) {
		if (!strcmp(fd_open_list[i].filename, filename)) {
			return EXIT_ERR;
		}
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
	//	*(fatblock.block_table[next_index]) = 0;
	//	rootdirectory[next_index].data_index = 0;

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
	struct file_descriptor file_des;
//	int fd = open(filename, O_RDWR | O_TRUNC |
//		O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	
	file_des.fd = global_fd;
	file_des.offset = 0;
	strcpy(file_des.filename, filename);

	fd_open_list[open_files] = file_des;
	open_files += 1;
	global_fd += 1;

	return file_des.fd;
}

int fs_close(int fd)
{
	/* TODO: Phase 3 */
	if (fd < 0 || fd >= FS_OPEN_MAX_COUNT) {
		return EXIT_ERR;
	}
	int i;
	int index = -1;
	for (i = 0; i < open_files; i++) {
		if (fd_open_list[i].fd == fd) {
			index = i;
			break;
		}
	}

	if (index < 0) {
		return EXIT_ERR;
	}

	for (i = index; i < open_files - 1; i++) {
		fd_open_list[i] = fd_open_list[i + 1];
	}

	open_files -= 1;

	return EXIT_NOERR;
}

int fs_stat(int fd)
{
	/* TODO: Phase 3 */
	if (fd < 0 || fd >= FS_OPEN_MAX_COUNT) {
		return EXIT_ERR;
	}
	
	int i;
	char filename[FS_FILENAME_LEN];
	bool is_open = false;
	
	for (i = 0; i < open_files; i++) {
		if (fd_open_list[i].fd == fd) {
			strcpy(filename, fd_open_list[i].filename);
			is_open = true;
			break;
		}
	}

	if (!is_open) {
		return EXIT_ERR;
	}

	// printf("+ %s\n", filename);

	int size = -1;
	for (i = 0; i < FS_FILE_MAX_COUNT; i++) {
		if (!strcmp(rootdirectory[i].filename, filename)) {
			size = rootdirectory[i].file_size;
			break;
		}
	}

	return size;
}

int fs_lseek(int fd, size_t offset)
{
	/* TODO: Phase 3 */
	UNUSED(fd);
	UNUSED(offset);
	return EXIT_NOERR;
}

/* returns the index of the data block corresponding to file's offset */
static uint16_t find_block(int fd, int offset)
{
	uint16_t index;
	// get filename from fd
	char filename[FS_FILENAME_LEN];
	int i;
	for (i = 0; i < open_files; i++) {
		if (fd_open_list[i].fd == fd) {
			strcpy(filename, fd_open_list[i].filename);
			break;
		}
	}
	for (i = 0; i < FS_FILE_MAX_COUNT; i++) {
		if (!strcmp(rootdirectory[i].filename, filename)) {
			index = rootdirectory[i].data_index;
			break;
		}
	}
	while (offset >= BLOCK_SIZE) {
		index = *(fatblock.block_table[index]);
		offset -= BLOCK_SIZE;
	}
	/* account for super block, fat, and rootdirectory */
//	uint16_t fat_size;
//	fat_size = data_blocks * 2 / BLOCK_SIZE;
//	index += 1 + fat_size + 1;
	return index + superblock.data_index;
}

// in case file has to be extended in size
// allocates a new data block and link it at the end of the file's data block chain
// allocation of new blocks should follow the first fit strategy - first block available from beginning of fat

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
	// get offset from fd
	uint16_t block_index;
	int buf_offset = 0;
	int offset = 0;
	char *bounce_buffer = malloc(BLOCK_SIZE);
	memset(bounce_buffer, 0, BLOCK_SIZE);

	// return -1...

	while (1) {
		block_index = find_block(fd, offset);

		// copy entire block into bounce buffer
		if (block_read(block_index, bounce_buffer) == EXIT_ERR) {
			return EXIT_ERR;
		}
	
		// then copy only the right amount of bytes from the bounce buffer into 
		// the user supplied buffer
		if ((offset % BLOCK_SIZE) + count > BLOCK_SIZE) {
			memcpy(buf + buf_offset, bounce_buffer + (offset % BLOCK_SIZE)
					, BLOCK_SIZE - (offset % BLOCK_SIZE));
			buf_offset += BLOCK_SIZE - (offset % BLOCK_SIZE);
			offset += BLOCK_SIZE - (offset % BLOCK_SIZE);
			count -= BLOCK_SIZE - (offset % BLOCK_SIZE);
		} else {
			memcpy(buf + buf_offset, bounce_buffer + 
					(offset % BLOCK_SIZE), count);
			buf_offset += count;
			offset += count;
			break;
		}
		// get to eof?
	}

	return buf_offset; //strlen(buf)
}


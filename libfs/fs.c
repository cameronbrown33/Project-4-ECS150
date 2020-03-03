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
	uint16_t *block_table;
};

struct __attribute__((__packed__)) root {
	char filename[FS_FILENAME_LEN];
	uint32_t file_size;
	uint16_t data_index;
	uint8_t padding[10];
};

struct __attribute__((__packed__)) file_descriptor {
	int32_t fd;
	uint32_t offset;
	uint32_t file_size;
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
//	printf("mount start File: %s\n", rootdirectory[0].filename);
	if (block_disk_open(diskname)) {
		printf("diskname\n");
		return EXIT_ERR;
	}	
	if (block_read(0, &superblock)) {
		printf("read super\n");
		return EXIT_ERR;
	}
	table = malloc(superblock.fat_block_total * BLOCK_SIZE);
	fatblock.block_table = table;
	
	for (int i = 0; i < superblock.fat_block_total; i++) {
		if (block_read(i + 1, table + ((i * BLOCK_SIZE) / 2))) {
			printf("read fat\n");
			free(table);
			return EXIT_ERR;
		}
	}
//	printf("Root index: %u\n", superblock.root_index);
//	printf("rootdirectory[0].filename: %s\n", rootdirectory[0].filename);
	if (block_read(superblock.root_index, &rootdirectory)) {
		printf("read root\n");
		free(table);
		return EXIT_ERR;
	}
//	printf("rootdirectory[0].filename: %s\n", rootdirectory[0].filename);
	file_system_open = true;
//	printf("umount end File: %s\n", rootdirectory[0].filename);
	return EXIT_NOERR;
}

int fs_umount(void)
{
//	printf("umount start File: %s\n", rootdirectory[0].filename);
	
	block_write(0, &superblock); // warnings above
	
	for (int i = 0; i < superblock.fat_block_total; i++) {
		if (block_write(i + 1, table + ((i * BLOCK_SIZE) / 2))) {
			printf("read fat\n");
			free(table);
			return EXIT_ERR;
		}
	}

	block_write(superblock.root_index, &rootdirectory); // warnings above
	if (block_disk_close()) {
		printf("no file open\n");
		return EXIT_ERR;
	}
	free(table);
	file_system_open = false;
//	printf("mount end File: %s\n", rootdirectory[0].filename);
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
		if (fatblock.block_table[i] == 0) {
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
		//	printf("Empty %s\n", rootdirectory[i].filename);
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
//	printf("index: %u\n", empty);
//	printf("File: %s\n", rootdirectory[empty].filename);
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
		next_index = fatblock.block_table[next_index];
		fatblock.block_table[old_index] = 0;
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
//	printf("Ls File: %s\n", rootdirectory[0].filename);
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
	if (filename == NULL) {
		return EXIT_ERR;
	}

	int i;
	int root_index = -1;
	for (i = 0; i < FS_FILE_MAX_COUNT; i++) {
		if (!strcmp(rootdirectory[i].filename, filename)) {
			root_index = i;
			break;
		}
	}

	if (root_index < 0) {
		return EXIT_ERR;
	}
	
	struct file_descriptor file_des;
	
	file_des.fd = global_fd;
	file_des.offset = 0;
	strcpy(file_des.filename, filename);
	file_des.file_size = rootdirectory[root_index].file_size;

	fd_open_list[open_files] = file_des;
	open_files += 1;
	global_fd += 1;

	return file_des.fd;
}

int fs_close(int fd)
{
	/* TODO: Phase 3 */
//	printf("close start File: %s\n", rootdirectory[0].filename);
	if (fd < 0) {
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
//	printf("close end File: %s\n", rootdirectory[0].filename);

	return EXIT_NOERR;
}

int fs_stat(int fd)
{
	/* TODO: Phase 3 */
	if (fd < 0) {
		return EXIT_ERR;
	}
	
	int i;
	char filename[FS_FILENAME_LEN];
	int index = -1;
	
	for (i = 0; i < open_files; i++) {
		if (fd_open_list[i].fd == fd) {
			strcpy(filename, fd_open_list[i].filename);
			index = i;
			break;
		}
	}

	if (index < 0) {
		return EXIT_ERR;
	}

	// printf("+ %s\n", filename);

	return fd_open_list[index].file_size;
}

int fs_lseek(int fd, size_t offset)
{
	/* TODO: Phase 3 */
	if (fd < 0) {
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

	if (offset > fd_open_list[index].file_size) {
		return EXIT_ERR;
	}

	fd_open_list[index].offset = offset;

	// printf("+ %lu\n", offset);
	
	return EXIT_NOERR;
}

/* returns the index of the data block corresponding to file's offset */
static uint16_t find_block(int fd, int offset)
{
	uint16_t index = -1;
	// get filename from fd
	char filename[FS_FILENAME_LEN];
	int i, j;
	for (i = 0; i < open_files; i++) {
		if (fd_open_list[i].fd == fd) {
			strcpy(filename, fd_open_list[i].filename);
			break;
		}
	}
//	printf("find_block filename: %s\n", filename);
	for (i = 0; i < FS_FILE_MAX_COUNT; i++) {
		if (!strcmp(rootdirectory[i].filename, filename)) {
//		printf("data_index: %u\n", rootdirectory[i].data_index);
			if (rootdirectory[i].data_index == FAT_EOC) {
				// put this in other helper function?
				for (j = 0; j < superblock.data_block_total; j++) {
					if (fatblock.block_table[j] == 0) {
						fatblock.block_table[j] = FAT_EOC;
						rootdirectory[i].data_index = j;
						printf("j: %u\n", j);
						break;
					}
				}
			}
			index = rootdirectory[i].data_index;
//			printf("index: %u\n", index);
			break;
		}
	}
	while (offset >= BLOCK_SIZE) {
		index = fatblock.block_table[index];
		offset -= BLOCK_SIZE;
	}
	/* account for super block, fat, and rootdirectory */
//	uint16_t fat_size;
//	fat_size = data_blocks * 2 / BLOCK_SIZE;
//	index += 1 + fat_size + 1;
//	printf("index: %u +: %u\n", index, index + superblock.data_index);

	return index + superblock.data_index; // -1
}

// in case file has to be extended in size
// allocates a new data block and link it at the end of the file's data block chain
// allocation of new blocks should follow the first fit strategy - first block available from beginning of fat

int fs_write(int fd, void *buf, size_t count)
{
	// -1 if fd is invalid (out of bounds or not open)
//	printf("in write File: %s\n", rootdirectory[0].filename);
	if (fd < 0) {
		return EXIT_ERR;
	}
	
	// get offset from fd
	int i;
	int fd_index = -1;
	uint32_t offset, buf_offset = 0;
	for (i = 0; i < open_files; i++) {
		if (fd_open_list[i].fd == fd) {
			fd_index = i;
			offset = fd_open_list[i].offset;
			break;
		}
	}

	if (fd_index < 0) {
		return EXIT_ERR;
	}


	int bytes_written = 0;
	int bytes_added = 0;
	uint16_t block_index;
	char *bounce_buffer = malloc(BLOCK_SIZE);
	memset(bounce_buffer, 0, BLOCK_SIZE);

	while (1) {
		block_index = find_block(fd, offset);
		offset = offset % BLOCK_SIZE;
//		printf("write offset: %u\n", offset);

		// read entire block into bounce buffer
		// read block from disk
		if (block_read(block_index, bounce_buffer) == EXIT_ERR) {
			return EXIT_ERR;
		}
	
		// attempt to write count bytes of data from buffer buf 
		// into file referenced by fd
		// then modify only the part starting from offset with buf
		// write whole buffer back to block
		if (offset + count > BLOCK_SIZE) {
			memcpy(bounce_buffer + offset, buf + buf_offset, BLOCK_SIZE - offset);
			block_write(block_index, bounce_buffer);
			buf_offset += BLOCK_SIZE - offset;
			offset += BLOCK_SIZE - offset;
			bytes_written += BLOCK_SIZE - offset;
			count -= BLOCK_SIZE - offset;
			// call helper function
			// when attempts to write past end of file, 
			// file automatically extended to 
			// hold the additional bytes
			// if disk runs out of space write as many bytes as possible
			// so number of bytes can be less than count - can be 0
		} else {
			memcpy(bounce_buffer + offset, buf + buf_offset, count);
//			printf("write index: %u\n", block_index);
//			printf("bounce_buffer: %s\n", bounce_buffer);
			block_write(block_index, bounce_buffer);
			offset += count;
			bytes_written += count;
			break;
		}
		// get to eof?
	}
	int x = -1;
	for (i = 0; i < FS_FILE_MAX_COUNT; i++) {
		if (!strcmp(rootdirectory[i].filename, fd_open_list[fd_index].filename)) {
			x = i;
			break;
		}
	}

	rootdirectory[x].file_size += bytes_added;

	fd_open_list[fd_index].offset = offset;
//	printf("write offset: %u\n", offset);
//	printf("write end File: %s\n", rootdirectory[0].filename);

	return bytes_written;
}

int fs_read(int fd, void *buf, size_t count)
{
	/* TODO: Phase 4 */

	if (fd < 0) {
		return EXIT_ERR;
	}
	
	// get offset from fd
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

	uint16_t block_index;
	int buf_offset = 0;
	int offset = fd_open_list[index].offset;
	char *bounce_buffer = malloc(BLOCK_SIZE);
	memset(bounce_buffer, 0, BLOCK_SIZE);

	// return -1...

	while (1) {
		block_index = find_block(fd, offset);
		offset = offset % BLOCK_SIZE;

		// copy entire block into bounce buffer
		if (block_read(block_index, bounce_buffer) == EXIT_ERR) {
			return EXIT_ERR;
		}
	
		// then copy only the right amount of bytes from the bounce buffer into 
		// the user supplied buffer
		if (offset + count > BLOCK_SIZE) {
			memcpy(buf + buf_offset, bounce_buffer + offset, BLOCK_SIZE - offset);
			buf_offset += BLOCK_SIZE - offset;
			offset += BLOCK_SIZE - offset;
			count -= BLOCK_SIZE - offset;
		} else {
			memcpy(buf + buf_offset, bounce_buffer + offset, count);
			buf_offset += count;
			offset += count;
			break;
		}
		// get to eof?
	}

	fd_open_list[index].offset = offset;
	return buf_offset; //strlen(buf)
}


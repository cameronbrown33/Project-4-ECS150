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
#define WRITE_MODE 1
#define READ_MODE 0

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
	if (block_read(superblock.root_index, &rootdirectory)) {
		printf("read root\n");
		free(table);
		return EXIT_ERR;
	}
	file_system_open = true;
	return EXIT_NOERR;
}

int fs_umount(void)
{
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
	/* filename invalid */
	if (filename == NULL) {
		return EXIT_ERR;
	}

	/* filename too long */
	if (strlen(filename) > FS_FILENAME_LEN) {
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
			/* empty */
			empty = i;
			break;
		}
	}

	/* root directory is full */
	if (empty < 0) {
		return EXIT_ERR;
	}

	strcpy(rootdirectory[empty].filename, filename);
	rootdirectory[empty].file_size = 0;
	rootdirectory[empty].data_index = FAT_EOC;

	return EXIT_NOERR;
}

int fs_delete(const char *filename)
{
	/* filename invalid */
	if (filename == NULL) {
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
	
	/* no file filename to delete */
	if (file_index < 0) {
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

	return EXIT_NOERR;
}

int fs_stat(int fd)
{
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

// in case file has to be extended in size
// allocates a new data block and link it at the end of the file's data block chain
// allocation of new blocks should follow the first fit strategy - first block available from beginning of fat
				// update blocks size?
void new_block(uint16_t i)
{	
	int j;
	uint16_t next_index;
	for (j = 0; j < superblock.data_block_total; j++) {
		if (fatblock.block_table[j] == 0) {
			fatblock.block_table[j] = FAT_EOC;
			if (rootdirectory[i].data_index == FAT_EOC) {
				rootdirectory[i].data_index = j;
			} else {
				next_index = rootdirectory[i].data_index;
				while (fatblock.block_table[next_index] != FAT_EOC) {
					next_index = fatblock.block_table[next_index];
				}
				fatblock.block_table[next_index] = j;
			}
			break;
		}
	}
}
		
/* returns the index of the data block corresponding to file's offset */
static uint16_t find_block(int fd, int offset, int* mode)
{
	uint16_t index = -1;
	
	/* get filename from fd */
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
			if (rootdirectory[i].data_index == FAT_EOC) {
				if (*mode == WRITE_MODE) {
					new_block(i);
					*mode = 2;
				}
				else {
					return -1;
				}
			}
			index = rootdirectory[i].data_index;
			break;
		}
	}

	while (offset >= BLOCK_SIZE) {
		if (fatblock.block_table[index] == FAT_EOC) {
			if (*mode == WRITE_MODE) {
				new_block(i);
				*mode = 3;
			}
			else {
				return -1;
			}
		}
		index = fatblock.block_table[index];
		offset -= BLOCK_SIZE;
	}
	
	/* account for super block, fat, and rootdirectory */
	return index + superblock.data_index; // -1
}
	
int fs_write(int fd, void *buf, size_t count)
{
	// -1 if fd is invalid (out of bounds or not open)
	if (fd < 0) {
		return EXIT_ERR;
	}
	
	int i;
	int fd_index = -1;
	uint32_t offset, old_offset, buf_offset = 0;
	uint32_t file_size = 0;
	/* get offset from fd */
	for (i = 0; i < open_files; i++) {
		if (fd_open_list[i].fd == fd) {
			fd_index = i;
			offset = fd_open_list[fd_index].offset;
			file_size = fd_open_list[fd_index].file_size;
			old_offset = offset;
			// printf("offset: %u\n", offset);
			break;
		}
	}

	if (fd_index < 0) {
		return EXIT_ERR;
	}


	int bytes_written = 0;
	int bytes_added = 0;
	int mode = WRITE_MODE;
	uint16_t block_index;
	char *bounce_buffer = malloc(BLOCK_SIZE);
	memset(bounce_buffer, 0, BLOCK_SIZE);

	while (1) {
		block_index = find_block(fd, offset, &mode);
//		printf("offset in while: %u\n", offset);
//		printf("block_index: %u\n", block_index);
		// read entire block from disk into bounce buffer
		offset = offset % BLOCK_SIZE;
//		printf("offset: %u\n", offset);
		if (block_read(block_index, bounce_buffer) == EXIT_ERR) {
			return EXIT_ERR;
		}
	
		// attempt to write count bytes of data from buffer buf 
		// into file referenced by fd
		// then modify only the part starting from offset with buf
		// write whole buffer back to block
		if (offset + count > BLOCK_SIZE) {
		//	printf("count: %lu\n", count);
			memcpy(bounce_buffer + offset, buf + buf_offset, BLOCK_SIZE - offset);
			block_write(block_index, bounce_buffer);
			buf_offset += BLOCK_SIZE - offset;
			if (mode > 1) {
				bytes_added += BLOCK_SIZE - offset;
			}
			bytes_written += BLOCK_SIZE - offset;
			count -= BLOCK_SIZE - offset;
			file_size -= BLOCK_SIZE - offset;
			offset += BLOCK_SIZE - offset;
			//	printf("BLOCK_SIZE - offset: %u offset: %u \n\n", BLOCK_SIZE - offset, offset);
			//	printf("count: %lu\n\n", count);
		// call helper function
		// when attempts to write past end of file, 
		// file automatically extended to 
		// hold the additional bytes
		// if disk runs out of space write as many bytes as possible
		// so number of bytes can be less than count - can be 0
		} else {
			memcpy(bounce_buffer + offset, buf + buf_offset, count);
			block_write(block_index, bounce_buffer);
			offset += count;
			if (mode > 1) {
				bytes_added += count;
			}
			bytes_written += count;
			break;
		}
		mode = WRITE_MODE;
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

	fd_open_list[fd_index].offset = old_offset + bytes_written;

	return bytes_written;
}

int fs_read(int fd, void *buf, size_t count)
{
	if (fd < 0) {
		return EXIT_ERR;
	}
	
	/* get offset from fd */
	int i;
	int index = -1;
	uint32_t offset, file_size = 0;
	for (i = 0; i < open_files; i++) {
		if (fd_open_list[i].fd == fd) {
			index = i;
			offset = fd_open_list[index].offset;
			file_size = fd_open_list[index].file_size;
			break;
		}
	}

	if (index < 0) {
		return EXIT_ERR;
	}

	uint16_t block_index;
	int buf_offset = 0;
	int mode = READ_MODE;
	char *bounce_buffer = malloc(BLOCK_SIZE);
	memset(bounce_buffer, 0, BLOCK_SIZE);

	// return -1...

	while (1) {
		block_index = find_block(fd, offset, &mode);
		if (block_index == 0xFFFF) {
			break;
		}
		offset = offset % BLOCK_SIZE;
		if (offset >= file_size) {
			break;
		}
		/* copy entire block from disk into bounce buffer */
		if (block_read(block_index, bounce_buffer) == EXIT_ERR) {
			return EXIT_ERR;
		}
	
		// then copy only the right amount of bytes from the bounce buffer into 
		// the user supplied buffer
		if (offset + count > BLOCK_SIZE) {
			if (file_size < BLOCK_SIZE) {
				memcpy(buf + buf_offset, bounce_buffer + offset, file_size - offset);
				buf_offset += file_size - offset;
				break;
			}
			else {
				memcpy(buf + buf_offset, bounce_buffer + offset, BLOCK_SIZE - offset);
				buf_offset += BLOCK_SIZE - offset;
				count -= BLOCK_SIZE - offset;
				offset += BLOCK_SIZE - offset;
				file_size -= BLOCK_SIZE - offset;
			}
		} else {
			if (offset + count > file_size) {
				memcpy(buf + buf_offset, bounce_buffer + offset, file_size - offset);
				buf_offset += file_size - offset;
			}
			else {
				memcpy(buf + buf_offset, bounce_buffer + offset, count);
				buf_offset += count;
			}
			break;
		}
		// get to eof?
	}

	fd_open_list[index].offset = buf_offset;
	return buf_offset; //strlen(buf)
}

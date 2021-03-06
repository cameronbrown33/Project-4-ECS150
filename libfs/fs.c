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
	int root_index;
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
	/* disk cannot be opened */
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
	/* still open file descriptors */
	if (open_files) {
		printf("open file descriptors\n");
		return EXIT_ERR;
	}

	if (block_write(0, &superblock)) {
		printf("write super\n");
		return EXIT_ERR;
	}

	for (int i = 0; i < superblock.fat_block_total; i++) {
		if (block_write(i + 1, table + ((i * BLOCK_SIZE) / 2))) {
			printf("write fat\n");
			free(table);
			return EXIT_ERR;
		}
	}

	if (block_write(superblock.root_index, &rootdirectory)) {
		printf("write root\n");
		return EXIT_ERR;
	}

	/* close underlying virtual disk file */
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
	int i, count = 0;
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
	if (strlen(filename) >= FS_FILENAME_LEN) {
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
	}

	/* empty file's entry */
	rootdirectory[file_index].filename[0] = '\0';
	rootdirectory[file_index].file_size = 0;
	rootdirectory[file_index].data_index = 0;

	return EXIT_NOERR;
}

int fs_ls(void)
{
	printf("FS Ls:\n");

	/* no underlying virtual disk open */
	if (!file_system_open) {
		return EXIT_ERR;
	}
	
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
	int root_index = -1;
	struct file_descriptor file_des;
	
	if (filename == NULL) {
		return EXIT_ERR;
	}

	if (open_files == FS_OPEN_MAX_COUNT) {
		return EXIT_ERR;
	}

	int i;
	for (i = 0; i < FS_FILE_MAX_COUNT; i++) {
		if (!strcmp(rootdirectory[i].filename, filename)) {
			root_index = i;
			break;
		}
	}

	if (root_index < 0) {
		return EXIT_ERR;
	}
	
	file_des.fd = global_fd;
	file_des.offset = 0;
	strcpy(file_des.filename, filename);
	file_des.file_size = rootdirectory[root_index].file_size;
	file_des.root_index = root_index;

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

	/* file fd not currently open */
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
	int index = -1;
	for (i = 0; i < open_files; i++) {
		if (fd_open_list[i].fd == fd) {
			index = i;
			break;
		}
	}

	/* file fd not currently open */
	if (index < 0) {
		return EXIT_ERR;
	}

	return rootdirectory[fd_open_list[index].root_index].file_size;
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

	return EXIT_NOERR;
}

/* allocates new data block and links it at end of file's data block chain */
bool new_block(uint16_t i)
{	
	int j;
	uint16_t next_index;
	bool found_empty = false;
	
	for (j = 0; j < superblock.data_block_total; j++) {
		if (fatblock.block_table[j] == 0) {
			found_empty = true;
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
	return found_empty;
}
		
/* returns the index of the data block corresponding to file's offset */
static int16_t find_block(int fd, int offset, int* mode)
{
	int16_t index = -1;
	char filename[FS_FILENAME_LEN];
	int i;
	
	/* get filename from fd */
	for (i = 0; i < open_files; i++) {
		if (fd_open_list[i].fd == fd) {
			strcpy(filename, fd_open_list[i].filename);
			break;
		}
	}
	
	/* locate start of file's data blocks */
	for (i = 0; i < FS_FILE_MAX_COUNT; i++) {
		if (!strcmp(rootdirectory[i].filename, filename)) {
			if (rootdirectory[i].data_index == FAT_EOC) {
				if (*mode == WRITE_MODE) {
					if (new_block(i)) {
						*mode += 1;
					} else {
						return EXIT_ERR;
					}
				}
				else {
					return EXIT_ERR;
				}
			}
			index = rootdirectory[i].data_index;
			break;
		}
	}

	/* traverse the data blocks until reach offset */
	while (offset >= BLOCK_SIZE) {
		if (fatblock.block_table[index] == FAT_EOC) {
			if (*mode == WRITE_MODE) {
				if (new_block(i)) {
					*mode += 1;
				} else {
					return EXIT_ERR;
				}
			}
			else {
				return EXIT_ERR;
			}
		}
		index = fatblock.block_table[index];
		offset -= BLOCK_SIZE;
	}
	
	/* account for super block, fat, and rootdirectory */
	return index + superblock.data_index;
}
	
int fs_write(int fd, void *buf, size_t count)
{
	/* invalid */
	if (fd < 0) {
		return EXIT_ERR;
	}
	
	int i;
	int fd_index = -1;
	uint32_t offset, old_offset, buf_offset = 0;
	uint32_t file_size = 0;	
	int bytes_written = 0;
	int bytes_added = 0;
	int mode = WRITE_MODE;
	int16_t block_index;

	/* get offset from fd */
	for (i = 0; i < open_files; i++) {
		if (fd_open_list[i].fd == fd) {
			fd_index = i;
			offset = fd_open_list[fd_index].offset;
			file_size = fd_open_list[fd_index].file_size;
			old_offset = offset;
			break;
		}
	}

	/* file fd not currently open */
	if (fd_index < 0) {
		return EXIT_ERR;
	}

	if (offset > file_size) {
		return EXIT_ERR;
	}
	
	char *bounce_buffer = malloc(BLOCK_SIZE);
	memset(bounce_buffer, 0, BLOCK_SIZE);

	file_size -= (offset / BLOCK_SIZE) * BLOCK_SIZE;

	while (1) {
		uint16_t tmp_offset = offset % BLOCK_SIZE;
		block_index = find_block(fd, offset, &mode);
		
		/* data blocks are all full */
		if (block_index == -1) {
			break;
		}

		/* read entire block from disk into bounce buffer */
		if (block_read(block_index, bounce_buffer) == EXIT_ERR) {
			return EXIT_ERR;
		}
	
		/* write count bytes of data from buf and write whole buffer back to block */
		if (tmp_offset + count > BLOCK_SIZE) {
			memcpy(bounce_buffer + tmp_offset, 
					buf + buf_offset, BLOCK_SIZE - tmp_offset);
			block_write(block_index, bounce_buffer);
			buf_offset += BLOCK_SIZE - tmp_offset;
			bytes_written += BLOCK_SIZE - tmp_offset;
			count -= BLOCK_SIZE - tmp_offset;
			if (file_size > BLOCK_SIZE) {
				file_size -= BLOCK_SIZE;
			}
			else {
				bytes_added += BLOCK_SIZE - file_size;
				file_size = 0;
			}
			offset += BLOCK_SIZE - tmp_offset;
		} else {
			memcpy(bounce_buffer + tmp_offset, buf + buf_offset, count);
			block_write(block_index, bounce_buffer);
			if (count + tmp_offset > file_size) {
				bytes_added += count + tmp_offset - file_size;
			}
			bytes_written += count;
			offset += count;
			break;
		}
		mode = WRITE_MODE;
	}

	int index = fd_open_list[fd_index].root_index;
	rootdirectory[index].file_size += bytes_added;
	fd_open_list[fd_index].offset = old_offset + bytes_written;

	return bytes_written;
}

int fs_read(int fd, void *buf, size_t count)
{
	int i;
	int index = -1;	
	uint16_t block_index;
	int buf_offset = 0;
	int mode = READ_MODE;
	uint32_t offset, file_size = 0;

	if (fd < 0) {
		return EXIT_ERR;
	}

	/* get offset from fd */
	for (i = 0; i < open_files; i++) {
		if (fd_open_list[i].fd == fd) {
			index = i;
			offset = fd_open_list[index].offset;
			file_size = rootdirectory[fd_open_list[index].root_index].file_size;
			break;
		}
	}

	/* file fd not currently open */
	if (index < 0) {
		return EXIT_ERR;
	}
	
	char *bounce_buffer = malloc(BLOCK_SIZE);
	memset(bounce_buffer, 0, BLOCK_SIZE);

	while (1) {
		block_index = find_block(fd, offset, &mode);
		if (block_index == FAT_EOC) {
			break;
		}
		uint16_t tmp_offset = offset % BLOCK_SIZE;
		if (tmp_offset >= file_size) {
			break;
		}
		/* copy entire block from disk into bounce buffer */
		if (block_read(block_index, bounce_buffer) == EXIT_ERR) {
			return EXIT_ERR;
		}
	
		/* copy desired bytes from bounce buffer into user supplied buffer */
		if (tmp_offset + count > BLOCK_SIZE) {
			if (file_size < BLOCK_SIZE) {
				memcpy(buf + buf_offset, bounce_buffer + tmp_offset, 
						file_size - tmp_offset);
				buf_offset += file_size - tmp_offset;
				offset += file_size - tmp_offset;
				break;
			}
			else {
				memcpy(buf + buf_offset, bounce_buffer + tmp_offset, 
						BLOCK_SIZE - tmp_offset);
				buf_offset += BLOCK_SIZE - tmp_offset;
				count -= BLOCK_SIZE - tmp_offset;
				file_size -= BLOCK_SIZE - tmp_offset;
				offset += BLOCK_SIZE - tmp_offset;
			}
		} else {
			if (tmp_offset + count > file_size) {
				memcpy(buf + buf_offset, bounce_buffer + tmp_offset, 
						file_size - tmp_offset);
				buf_offset += file_size - tmp_offset;
				offset += file_size - tmp_offset;
			}
			else {
				memcpy(buf + buf_offset, bounce_buffer + tmp_offset, count);
				buf_offset += count;
				offset += count;
			}
			break;
		}
	}

	fd_open_list[index].offset += buf_offset;
	
	return buf_offset;
}

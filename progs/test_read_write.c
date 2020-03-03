#include <stdio.h>
#include <string.h>

#include <fs.h>

int main(void) {
	fs_mount("disk.fs");
	char out_buf[32];
	char in_buf[32];
	memset(out_buf, '\0', 32);
	memset(in_buf, '\0', 32);
	strcpy(out_buf, "hello world");

	//printf("+ %s\n", out_buf);

	fs_create("hello.txt");

	int fd = fs_open("hello.txt");

	fs_write(fd, out_buf, 11);

	fs_lseek(fd, 0);
	
	fs_read(fd, in_buf, 11);

	printf("+ %s\n", in_buf);

	fs_close(fd);
	fs_delete("hello.txt");
	fs_umount();
	return 0;
}

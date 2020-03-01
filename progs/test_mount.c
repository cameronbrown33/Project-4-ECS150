#include <stdio.h>

#include <fs.h>

int main(void) {
	fs_mount("disk.fs");
	fs_info();
	fs_create("test.txt");
	fs_create("test2.txt");
	fs_ls();
	printf("%d\n", fs_open("test.txt"));
	printf("%d\n", fs_open("test.txt"));
	fs_umount();
	return 0;
}

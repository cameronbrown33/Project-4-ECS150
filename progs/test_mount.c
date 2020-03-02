#include <stdio.h>

#include <fs.h>

int main(void) {
	fs_mount("disk.fs");
	fs_info();
	fs_create("test.txt");
	fs_create("test2.txt");
	fs_ls();
	int i = fs_open("test.txt");
	int j = fs_open("test.txt");
	printf("fd i: %d\n", i);
	printf("fd j: %d\n", j);
	fs_close(i);
	printf("stat i: %d\n", fs_stat(j));
	fs_close(j);
	printf("delete: %d\n", fs_delete("test.txt"));
	fs_umount();
	return 0;
}

#include <stdio.h>

#include <fs.h>

int main(void) {
	fs_mount("disk.fs");
	fs_info();
	fs_umount();
	return 0;
}

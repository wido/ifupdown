#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/utsname.h>

#include "archlinux.h"

unsigned int mylinuxver(void) {
	static int maj = -1, rev = 0, min = 0;

	if (maj == -1) {
		struct utsname u;
		char *pch;

		uname(&u);
		maj = atoi(u.release);
		pch = strchr(u.release, '.');
		if (pch) {
			rev = atoi(pch + 1);
			pch = strchr(pch + 1, '.');
			if (pch) {
				min = atoi(pch + 1);
			}
		}
	}

	return mylinux(maj, rev, min);
}

unsigned int mylinux(int maj, int rev, int min) {
	return min | rev << 10 | maj << 13;
}

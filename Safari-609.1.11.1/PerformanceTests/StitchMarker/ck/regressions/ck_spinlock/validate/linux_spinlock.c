#ifdef __x86_64__
#include "../linux_spinlock.h"
#include "validate.h"
#else
#include <stdio.h>

int
main(void)
{

	fprintf(stderr, "Unsupported.\n");
	return 0;
}
#endif

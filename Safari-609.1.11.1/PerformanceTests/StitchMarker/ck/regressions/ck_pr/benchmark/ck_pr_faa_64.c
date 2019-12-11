#include <ck_pr.h>

#ifdef CK_F_PR_FAA_64
#define ATOMIC ck_pr_faa_64(object, 1)
#define ATOMIC_STRING "ck_pr_faa_64"
#include "benchmark.h"
#else
#warning Did not find FAA_64 implementation.
#include <stdlib.h>

int
main(void)
{
	exit(EXIT_FAILURE);
}
#endif

#include <ck_pr.h>

#ifdef CK_F_PR_ADD_64
#define ATOMIC ck_pr_add_64(object, 1)
#define ATOMIC_STRING "ck_pr_add_64"
#include "benchmark.h"
#else
#warning Did not find ADD_64 implementation.
#include <stdlib.h>

int
main(void)
{
	exit(EXIT_FAILURE);
}
#endif

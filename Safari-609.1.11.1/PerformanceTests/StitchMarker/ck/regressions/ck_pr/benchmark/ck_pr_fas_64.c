#include <ck_pr.h>

#ifdef CK_F_PR_FAS_64
#define ATOMIC ck_pr_fas_64(object, 1)
#define ATOMIC_STRING "ck_pr_fas_64"
#include "benchmark.h"
#else
#warning Did not find FAS_64 implementation.
#include <stdlib.h>

int
main(void)
{

	return 0;
}
#endif

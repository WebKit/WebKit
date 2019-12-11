#include <ck_pr.h>

#ifdef CK_F_PR_NEG_64
#define ATOMIC ck_pr_neg_64(object)
#define ATOMIC_STRING "ck_pr_neg_64"
#include "benchmark.h"
#else
#warning Did not find NEG_64 implementation.
#include <stdlib.h>

int
main(void)
{
	exit(EXIT_FAILURE);
}
#endif

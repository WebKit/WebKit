#include <ck_pr.h>

#ifdef CK_F_PR_CAS_64_2
#define ATOMIC { uint64_t z[2] = {1, 2}; ck_pr_cas_64_2(object, z, z); }
#define ATOMIC_STRING "ck_pr_cas_64_2"
#include "benchmark.h"
#else
#include <stdio.h>
#include <stdlib.h>

int
main(void)
{
	fprintf(stderr, "Unsupported.\n");
	return 0;
}
#endif

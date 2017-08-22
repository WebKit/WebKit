#include <stdio.h>
#include <inttypes.h>
#include <stdint.h>

#include "../../common.h"

#ifndef IR
#define IR 3000000
#endif /* IR */

static int a CK_CC_CACHELINE;
static int b CK_CC_CACHELINE;

int
main(void)
{
	uint64_t s, e;
	unsigned int i;

	s = rdtsc();
	for (i = 0; i < IR; i++) {
		ck_pr_load_int(&a);
		ck_pr_fence_strict_load();
		ck_pr_load_int(&b);
	}
	e = rdtsc();
	printf("[A] fence_load: %" PRIu64 "\n", (e - s) / IR);

	s = rdtsc();
	for (i = 0; i < IR; i++) {
		if (ck_pr_load_int(&a) == 0)
			ck_pr_barrier();
		ck_pr_fence_strict_lock();
		ck_pr_load_int(&b);
	}
	e = rdtsc();
	printf("[A] fence_lock: %" PRIu64 "\n", (e - s) / IR);

	s = rdtsc();
	for (i = 0; i < IR; i++) {
		ck_pr_store_int(&a, 0);
		ck_pr_fence_strict_store();
		ck_pr_store_int(&b, 0);
	}
	e = rdtsc();
	printf("[B] fence_store: %" PRIu64 "\n", (e - s) / IR);

	s = rdtsc();
	for (i = 0; i < IR; i++) {
		ck_pr_store_int(&a, 0);
		ck_pr_fence_strict_memory();
		ck_pr_load_int(&b);
	}
	e = rdtsc();
	printf("[C] fence_memory: %" PRIu64 "\n", (e - s) / IR);

	s = rdtsc();
	for (i = 0; i < IR; i++) {
		ck_pr_store_int(&a, 0);
		ck_pr_faa_int(&a, 0);
		ck_pr_load_int(&b);
	}
	e = rdtsc();
	printf("[C] atomic: %" PRIu64 "\n", (e - s) / IR);
	return 0;
}

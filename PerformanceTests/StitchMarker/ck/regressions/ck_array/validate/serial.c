#include <ck_array.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#include "../../common.h"

#ifndef ITERATION
#define ITERATION 128
#endif

static void
my_free(void *p, size_t m, bool d)
{

	(void)m;
	(void)d;

	free(p);
	return;
}

static void *
my_malloc(size_t b)
{

	return malloc(b);
}

static void *
my_realloc(void *r, size_t a, size_t b, bool d)
{

	(void)a;
	(void)d;

	return realloc(r, b);
}

int
main(void)
{
	void *r;
	uintptr_t i;
	ck_array_t array;
	ck_array_iterator_t iterator;
	struct ck_malloc m = {
		.malloc = my_malloc,
		.free = NULL,
		.realloc = my_realloc
	};

	if (ck_array_init(&array, CK_ARRAY_MODE_SPMC, &m, 4) == true)
		ck_error("ck_array_init with NULL free succeeded\n");

	m.free = my_free;
	if (ck_array_init(&array, CK_ARRAY_MODE_SPMC, &m, 4) == false)
		ck_error("ck_array_init\n");

	for (i = 0; i < ITERATION; i++) {
		if (ck_array_put(&array, (void *)i) == false)
			ck_error("ck_error_put\n");

		if (ck_array_remove(&array, (void *)i) == false)
			ck_error("ck_error_remove after put\n");
	}

	i = 0; CK_ARRAY_FOREACH(&array, &iterator, &r) i++;
	if (i != 0)
		ck_error("Non-empty array after put -> remove workload.\n");

	ck_array_commit(&array);

	i = 0; CK_ARRAY_FOREACH(&array, &iterator, &r) i++;
	if (i != 0)
		ck_error("Non-empty array after put -> remove -> commit workload.\n");

	for (i = 0; i < ITERATION; i++) {
		if (ck_array_put(&array, (void *)i) == false)
			ck_error("ck_error_put\n");
	}

	i = 0; CK_ARRAY_FOREACH(&array, &iterator, &r) i++;
	if (i != 0)
		ck_error("Non-empty array after put workload.\n");

	for (i = 0; i < ITERATION; i++) {
		if (ck_array_remove(&array, (void *)i) == false)
			ck_error("ck_error_remove after put\n");
	}

	i = 0; CK_ARRAY_FOREACH(&array, &iterator, &r) i++;
	if (i != 0)
		ck_error("Non-empty array after put -> remove workload.\n");

	ck_array_commit(&array);

	i = 0; CK_ARRAY_FOREACH(&array, &iterator, &r) i++;
	if (i != 0)
		ck_error("Non-empty array after put -> remove -> commit workload.\n");

	for (i = 0; i < ITERATION; i++) {
		if (ck_array_put(&array, (void *)i) == false)
			ck_error("ck_error_put\n");
	}

	ck_array_commit(&array);

	i = 0;
	CK_ARRAY_FOREACH(&array, &iterator, &r) {
		i++;
	}

	if (i != ITERATION)
		ck_error("Incorrect item count in iteration\n");

	ck_array_remove(&array, (void *)(uintptr_t)0);
	ck_array_remove(&array, (void *)(uintptr_t)1);
	ck_array_commit(&array);
	i = 0; CK_ARRAY_FOREACH(&array, &iterator, &r) i++;
	if (i != ITERATION - 2 || ck_array_length(&array) != ITERATION - 2)
		ck_error("Incorrect item count in iteration after remove\n");

	if (ck_array_put_unique(&array, (void *)UINTPTR_MAX) != 0)
		ck_error("Unique value put failed.\n");

	if (ck_array_put_unique(&array, (void *)(uintptr_t)4) != 1)
		ck_error("put of 4 not detected as non-unique.\n");

	if (ck_array_put_unique(&array, (void *)UINTPTR_MAX) != 1)
		ck_error("put of UINTPTR_MAX not detected as non-unique.\n");

	ck_array_commit(&array);
	i = 0;
	CK_ARRAY_FOREACH(&array, &iterator, &r) {
		i++;
	}
	if (i != ITERATION - 1 || ck_array_length(&array) != ITERATION - 1)
		ck_error("Incorrect item count in iteration after unique put\n");

	if (ck_array_initialized(&array) == false)
		ck_error("Error, expected array to be initialized.\n");

	for (i = 0; i < ITERATION * 4; i++) {
		ck_array_remove(&array, (void *)i);
	}

	for (i = 0; i < ITERATION * 16; i++) {
		ck_array_put(&array, (void *)i);
	}

	ck_array_commit(&array);

	for (i = 0; i < ITERATION * 128; i++) {
		ck_array_put(&array, (void *)i);
		if (ck_array_put_unique(&array, (void *)i) != 1)
			ck_error("put_unique for non-unique value should fail.\n");
	}

	for (i = 0; i < ITERATION * 64; i++) {
		bool f = ck_array_remove(&array, (void *)i);

		if (f == false && i < ITERATION * 144)
			ck_error("Remove failed for existing entry.\n");

		if (f == true && i > ITERATION * 144)
			ck_error("Remove succeeded for non-existing entry.\n");
	}

	ck_array_commit(&array);
	ck_array_deinit(&array, false);

	if (ck_array_initialized(&array) == true)
		ck_error("Error, expected array to be uninitialized.\n");

	return 0;
}


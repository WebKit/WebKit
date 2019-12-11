/*
 * Copyright 2014 Samy Al Bahra.
 * Copyright 2014 Backtrace I/O, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyrighs
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyrighs
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <ck_hs.h>

#include <assert.h>
#include <ck_malloc.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../../common.h"
#include "../../../src/ck_ht_hash.h"

static ck_hs_t hs;
static char **keys;
static size_t keys_length = 0;
static size_t keys_capacity = 128;
static unsigned long global_seed;

static void *
hs_malloc(size_t r)
{

	return malloc(r);
}

static void
hs_free(void *p, size_t b, bool r)
{

	(void)b;
	(void)r;

	free(p);

	return;
}

static struct ck_malloc my_allocator = {
	.malloc = hs_malloc,
	.free = hs_free
};

static unsigned long
hs_hash(const void *object, unsigned long seed)
{
	const char *c = object;
	unsigned long h;

	h = (unsigned long)MurmurHash64A(c, strlen(c), seed);
	return h;
}

static bool
hs_compare(const void *previous, const void *compare)
{

	return strcmp(previous, compare) == 0;
}

static void
set_destroy(void)
{

	ck_hs_destroy(&hs);
	return;
}

static void
set_init(unsigned int size, unsigned int mode)
{

	if (ck_hs_init(&hs, CK_HS_MODE_OBJECT | CK_HS_MODE_SPMC | mode, hs_hash, hs_compare,
	    &my_allocator, size, global_seed) == false) {
		perror("ck_hs_init");
		exit(EXIT_FAILURE);
	}

	return;
}

static size_t
set_count(void)
{

	return ck_hs_count(&hs);
}

static bool
set_reset(void)
{

	return ck_hs_reset(&hs);
}

static void *
test_apply(void *key, void *closure)
{

	(void)key;

	return closure;
}

static void
run_test(const char *file, size_t r, unsigned int size, unsigned int mode)
{
	FILE *fp;
	char buffer[512];
	size_t i, j;
	unsigned int d = 0;
	uint64_t s, e, a, gp, agp;
	struct ck_hs_stat st;
	char **t;

	keys = malloc(sizeof(char *) * keys_capacity);
	assert(keys != NULL);

	fp = fopen(file, "r");
	assert(fp != NULL);

	while (fgets(buffer, sizeof(buffer), fp) != NULL) {
		buffer[strlen(buffer) - 1] = '\0';
		keys[keys_length++] = strdup(buffer);
		assert(keys[keys_length - 1] != NULL);

		if (keys_length == keys_capacity) {
			t = realloc(keys, sizeof(char *) * (keys_capacity *= 2));
			assert(t != NULL);
			keys = t;
		}
	}

	t = realloc(keys, sizeof(char *) * keys_length);
	assert(t != NULL);
	keys = t;

	set_init(size, mode);
	for (i = 0; i < keys_length; i++) {
		unsigned long h = CK_HS_HASH(&hs, hs_hash, keys[i]);

		if (ck_hs_get(&hs, h, keys[i]) == false) {
			if (ck_hs_put(&hs, h, keys[i]) == false)
				ck_error("ERROR: Failed get to put workload.\n");
		} else {
			d++;
		}
	}
	ck_hs_stat(&hs, &st);

	fprintf(stderr, "# %zu entries stored, %u duplicates, %u probe.\n",
	    set_count(), d, st.probe_maximum);

	a = 0;
	for (j = 0; j < r; j++) {
		if (set_reset() == false)
			ck_error("ERROR: Failed to reset hash table.\n");

		s = rdtsc();
		for (i = 0; i < keys_length; i++) {
			unsigned long h = CK_HS_HASH(&hs, hs_hash, keys[i]);

			if (ck_hs_get(&hs, h, keys[i]) == false &&
			    ck_hs_put(&hs, h, keys[i]) == false) {
				ck_error("ERROR: Failed get to put workload.\n");
			}
		}
		e = rdtsc();
		a += e - s;
	}
	gp = a / (r * keys_length);

	a = 0;
	for (j = 0; j < r; j++) {
		if (set_reset() == false)
			ck_error("ERROR: Failed to reset hash table.\n");

		s = rdtsc();
		for (i = 0; i < keys_length; i++) {
			unsigned long h = CK_HS_HASH(&hs, hs_hash, keys[i]);

			if (ck_hs_apply(&hs, h, keys[i], test_apply, (void *)keys[i]) == false)
				ck_error("ERROR: Failed in apply workload.\n");
		}
		e = rdtsc();
		a += e - s;
	}
	agp = a / (r * keys_length);

	fclose(fp);

	for (i = 0; i < keys_length; i++) {
		free(keys[i]);
	}

	printf("Get to put: %" PRIu64 " ticks\n", gp);
	printf("     Apply: %" PRIu64 " ticks\n", agp);

	free(keys);
	keys_length = 0;
	set_destroy();
	return;
}

int
main(int argc, char *argv[])
{
	unsigned int r, size;

	common_srand48((long int)time(NULL));
	if (argc < 2) {
		ck_error("Usage: ck_hs <dictionary> [<repetitions> <initial size>]\n");
	}

	r = 16;
	if (argc >= 3)
		r = atoi(argv[2]);

	size = 8;
	if (argc >= 4)
		size = atoi(argv[3]);

	global_seed = common_lrand48();
	run_test(argv[1], r, size, 0);

	printf("\n==============================================\n"
	    "Delete mode\n"
	    "==============================================\n");
	run_test(argv[1], r, size, CK_HS_MODE_DELETE);
	return 0;
}


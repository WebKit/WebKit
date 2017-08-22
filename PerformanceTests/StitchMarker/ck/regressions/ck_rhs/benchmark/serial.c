/*
 * Copyright 2012 Samy Al Bahra.
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

#include <ck_rhs.h>

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

static ck_rhs_t hs;
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

	ck_rhs_destroy(&hs);
	return;
}

static void
set_init(unsigned int size, unsigned int mode)
{

	if (ck_rhs_init(&hs, CK_RHS_MODE_OBJECT | CK_RHS_MODE_SPMC | mode, hs_hash, hs_compare,
	    &my_allocator, size, global_seed) == false) {
		perror("ck_rhs_init");
		exit(EXIT_FAILURE);
	}

	return;
}

static bool
set_remove(const char *value)
{
	unsigned long h;

	h = CK_RHS_HASH(&hs, hs_hash, value);
	return ck_rhs_remove(&hs, h, value) != NULL;
}

static bool
set_swap(const char *value)
{
	unsigned long h;
	void *previous;

	h = CK_RHS_HASH(&hs, hs_hash, value);
	return ck_rhs_fas(&hs, h, value, &previous);
}

static bool
set_replace(const char *value)
{
	unsigned long h;
	void *previous;

	h = CK_RHS_HASH(&hs, hs_hash, value);
	ck_rhs_set(&hs, h, value, &previous);
	return previous != NULL;
}

static void *
set_get(const char *value)
{
	unsigned long h;
	void *v;

	h = CK_RHS_HASH(&hs, hs_hash, value);
	v = ck_rhs_get(&hs, h, value);
	return v;
}

static bool
set_insert(const char *value)
{
	unsigned long h;

	h = CK_RHS_HASH(&hs, hs_hash, value);
	return ck_rhs_put(&hs, h, value);
}

static bool
set_insert_unique(const char *value)
{
	unsigned long h;

	h = CK_RHS_HASH(&hs, hs_hash, value);
	return ck_rhs_put_unique(&hs, h, value);
}

static size_t
set_count(void)
{

	return ck_rhs_count(&hs);
}

static bool
set_reset(void)
{

	return ck_rhs_reset(&hs);
}

static void
set_gc(void)
{

	ck_rhs_gc(&hs);
	return;
}

static void
set_rebuild(void)
{

	ck_rhs_rebuild(&hs);
	return;
}

static void
keys_shuffle(char **k)
{
	size_t i, j;
	char *t;

	for (i = keys_length; i > 1; i--) {
		j = rand() % (i - 1);

		if (j != i - 1) {
			t = k[i - 1];
			k[i - 1] = k[j];
			k[j] = t;
		}
	}

	return;
}

static void
run_test(const char *file, size_t r, unsigned int size, unsigned int mode)
{
	FILE *fp;
	char buffer[512];
	size_t i, j;
	unsigned int d = 0;
	uint64_t s, e, a, ri, si, ai, sr, rg, sg, ag, sd, ng, ss, sts, su, sgc, sb;
	struct ck_rhs_stat st;
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
	for (i = 0; i < keys_length; i++)
		d += set_insert(keys[i]) == false;
	ck_rhs_stat(&hs, &st);

	fprintf(stderr, "# %zu entries stored, %u duplicates, %u probe.\n",
	    set_count(), d, st.probe_maximum);

	a = 0;
	for (j = 0; j < r; j++) {
		if (set_reset() == false) {
			ck_error("ERROR: Failed to reset hash table.\n");
		}

		s = rdtsc();
		for (i = keys_length; i > 0; i--)
			d += set_insert(keys[i - 1]) == false;
		e = rdtsc();
		a += e - s;
	}
	ri = a / (r * keys_length);

	a = 0;
	for (j = 0; j < r; j++) {
		if (set_reset() == false) {
			ck_error("ERROR: Failed to reset hash table.\n");
		}

		s = rdtsc();
		for (i = 0; i < keys_length; i++)
			d += set_insert(keys[i]) == false;
		e = rdtsc();
		a += e - s;
	}
	si = a / (r * keys_length);

	a = 0;
	for (j = 0; j < r; j++) {
		keys_shuffle(keys);

		if (set_reset() == false) {
			ck_error("ERROR: Failed to reset hash table.\n");
		}

		s = rdtsc();
		for (i = 0; i < keys_length; i++)
			d += set_insert(keys[i]) == false;
		e = rdtsc();
		a += e - s;
	}
	ai = a / (r * keys_length);

	a = 0;
	for (j = 0; j < r; j++) {
		s = rdtsc();
		for (i = 0; i < keys_length; i++)
			set_swap(keys[i]);
		e = rdtsc();
		a += e - s;
	}
	ss = a / (r * keys_length);

	a = 0;
	for (j = 0; j < r; j++) {
		s = rdtsc();
		for (i = 0; i < keys_length; i++)
			set_replace(keys[i]);
		e = rdtsc();
		a += e - s;
	}
	sr = a / (r * keys_length);

	set_reset();
	for (i = 0; i < keys_length; i++)
		set_insert(keys[i]);

	a = 0;
	for (j = 0; j < r; j++) {
		s = rdtsc();
		for (i = keys_length; i > 0; i--) {
			if (set_get(keys[i - 1]) == NULL) {
				ck_error("ERROR: Unexpected NULL value.\n");
			}
		}
		e = rdtsc();
		a += e - s;
	}
	rg = a / (r * keys_length);

	a = 0;
	for (j = 0; j < r; j++) {
		s = rdtsc();
		for (i = 0; i < keys_length; i++) {
			if (set_get(keys[i]) == NULL) {
				ck_error("ERROR: Unexpected NULL value.\n");
			}
		}
		e = rdtsc();
		a += e - s;
	}
	sg = a / (r * keys_length);

	a = 0;
	for (j = 0; j < r; j++) {
		keys_shuffle(keys);

		s = rdtsc();
		for (i = 0; i < keys_length; i++) {
			if (set_get(keys[i]) == NULL) {
				ck_error("ERROR: Unexpected NULL value.\n");
			}
		}
		e = rdtsc();
		a += e - s;
	}
	ag = a / (r * keys_length);

	a = 0;
	for (j = 0; j < r; j++) {
		s = rdtsc();
		for (i = 0; i < keys_length; i++)
			set_remove(keys[i]);
		e = rdtsc();
		a += e - s;

		for (i = 0; i < keys_length; i++)
			set_insert(keys[i]);
	}
	sd = a / (r * keys_length);

	a = 0;
	for (j = 0; j < r; j++) {
		s = rdtsc();
		for (i = 0; i < keys_length; i++) {
			set_get("\x50\x03\x04\x05\x06\x10");
		}
		e = rdtsc();
		a += e - s;
	}
	ng = a / (r * keys_length);

	set_reset();
	for (i = 0; i < keys_length; i++)
		set_insert(keys[i]);
	for (i = 0; i < keys_length; i++)
		set_remove(keys[i]);

	a = 0;
	for (j = 0; j < r; j++) {
		s = rdtsc();
		for (i = 0; i < keys_length; i++)
			set_insert(keys[i]);
		e = rdtsc();
		a += e - s;

		for (i = 0; i < keys_length; i++)
			set_remove(keys[i]);
	}
	sts = a / (r * keys_length);

	set_reset();

	/* Prune duplicates. */
	for (i = 0; i < keys_length; i++) {
		if (set_insert(keys[i]) == true)
			continue;

		free(keys[i]);
		keys[i] = keys[--keys_length];
	}

	for (i = 0; i < keys_length; i++)
		set_remove(keys[i]);

	a = 0;
	for (j = 0; j < r; j++) {
		s = rdtsc();
		for (i = 0; i < keys_length; i++)
			set_insert_unique(keys[i]);
		e = rdtsc();
		a += e - s;

		for (i = 0; i < keys_length; i++)
			set_remove(keys[i]);
	}
	su = a / (r * keys_length);

	for (i = 0; i < keys_length; i++)
		set_insert_unique(keys[i]);

	for (i = 0; i < keys_length / 2; i++)
		set_remove(keys[i]);

	a = 0;
	for (j = 0; j < r; j++) {
		s = rdtsc();
		set_gc();
		e = rdtsc();
		a += e - s;
	}
	sgc = a / r;

	a = 0;
	for (j = 0; j < r; j++) {
		s = rdtsc();
		set_rebuild();
		e = rdtsc();
		a += e - s;
	}
	sb = a / r;

	printf("%zu "
	    "%" PRIu64 " "
	    "%" PRIu64 " "
	    "%" PRIu64 " "
	    "%" PRIu64 " "
	    "%" PRIu64 " "
	    "%" PRIu64 " "
	    "%" PRIu64 " "
	    "%" PRIu64 " "
	    "%" PRIu64 " "
	    "%" PRIu64 " "
	    "%" PRIu64 " "
	    "%" PRIu64 " "
	    "%" PRIu64 " "
	    "%" PRIu64 "\n",
	    keys_length, ri, si, ai, ss, sr, rg, sg, ag, sd, ng, sts, su, sgc, sb);

	fclose(fp);

	for (i = 0; i < keys_length; i++) {
		free(keys[i]);
	}

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
		ck_error("Usage: ck_rhs <dictionary> [<repetitions> <initial size>]\n");
	}

	r = 16;
	if (argc >= 3)
		r = atoi(argv[2]);

	size = 8;
	if (argc >= 4)
		size = atoi(argv[3]);

	global_seed = common_lrand48();
	run_test(argv[1], r, size, 0);
	run_test(argv[1], r, size, CK_RHS_MODE_READ_MOSTLY);
	fprintf(stderr, "#    reverse_insertion serial_insertion random_insertion serial_swap "
	    "serial_replace reverse_get serial_get random_get serial_remove negative_get tombstone "
	    "set_unique gc rebuild\n\n");

	return 0;
}


/*
 * Copyright 2012-2015 Samy Al Bahra.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
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

#include <ck_ht.h>

#include <assert.h>
#include <ck_malloc.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../../common.h"

static ck_ht_t ht;
static char **keys;
static size_t keys_length = 0;
static size_t keys_capacity = 128;

static void *
ht_malloc(size_t r)
{

	return malloc(r);
}

static void
ht_free(void *p, size_t b, bool r)
{

	(void)b;
	(void)r;

	free(p);

	return;
}

static struct ck_malloc my_allocator = {
	.malloc = ht_malloc,
	.free = ht_free
};

static void
table_init(void)
{
	unsigned int mode = CK_HT_MODE_BYTESTRING;

#ifdef HT_DELETE
	mode |= CK_HT_WORKLOAD_DELETE;
#endif

	common_srand48((long int)time(NULL));
	if (ck_ht_init(&ht, mode, NULL, &my_allocator, 8, common_lrand48()) == false) {
		perror("ck_ht_init");
		exit(EXIT_FAILURE);
	}

	return;
}

static bool
table_remove(const char *value)
{
	ck_ht_entry_t entry;
	ck_ht_hash_t h;
	size_t l = strlen(value);

	ck_ht_hash(&h, &ht, value, l);
	ck_ht_entry_key_set(&entry, value, l);
	return ck_ht_remove_spmc(&ht, h, &entry);
}

static bool
table_replace(const char *value)
{
	ck_ht_entry_t entry;
	ck_ht_hash_t h;
	size_t l = strlen(value);

	ck_ht_hash(&h, &ht, value, l);
	ck_ht_entry_set(&entry, h, value, l, "REPLACED");
	return ck_ht_set_spmc(&ht, h, &entry);
}

static void *
table_get(const char *value)
{
	ck_ht_entry_t entry;
	ck_ht_hash_t h;
	size_t l = strlen(value);
	void *v = NULL;

	ck_ht_hash(&h, &ht, value, l);
	ck_ht_entry_key_set(&entry, value, l);

	if (ck_ht_get_spmc(&ht, h, &entry) == true) {
		v = ck_ht_entry_value(&entry);
	}
	return v;
}

static bool
table_insert(const char *value)
{
	ck_ht_entry_t entry;
	ck_ht_hash_t h;
	size_t l = strlen(value);

	ck_ht_hash(&h, &ht, value, l);
	ck_ht_entry_set(&entry, h, value, l, "VALUE");
	return ck_ht_put_spmc(&ht, h, &entry);
}

static size_t
table_count(void)
{

	return ck_ht_count(&ht);
}

static bool
table_gc(void)
{

	return ck_ht_gc(&ht, 0, common_lrand48());
}

static bool
table_reset(void)
{

	return ck_ht_reset_spmc(&ht);
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

int
main(int argc, char *argv[])
{
	FILE *fp;
	char buffer[512];
	size_t i, j, r;
	unsigned int d = 0;
	uint64_t s, e, a, ri, si, ai, sr, rg, sg, ag, sd, ng, gg;
	char **t;
	struct ck_ht_stat st;

	r = 20;
	s = 8;
	srand(time(NULL));

	if (argc < 2) {
		ck_error("Usage: ck_ht <dictionary> [<repetitions> <initial size>]\n");
	}

	if (argc >= 3)
		r = atoi(argv[2]);

	if (argc >= 4)
		s = (uint64_t)atoi(argv[3]);

	keys = malloc(sizeof(char *) * keys_capacity);
	assert(keys != NULL);

	fp = fopen(argv[1], "r");
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

	table_init();

	for (i = 0; i < keys_length; i++)
		d += table_insert(keys[i]) == false;
	ck_ht_stat(&ht, &st);

	fprintf(stderr, "# %zu entries stored, %u duplicates, %" PRIu64 " probe.\n",
	    table_count(), d, st.probe_maximum);

	fprintf(stderr, "#    reverse_insertion serial_insertion random_insertion serial_replace reverse_get serial_get random_get serial_remove negative_get garbage_collect\n\n");

	a = 0;
	for (j = 0; j < r; j++) {
		if (table_reset() == false) {
			ck_error("ERROR: Failed to reset hash table.\n");
		}

		s = rdtsc();
		for (i = keys_length; i > 0; i--)
			d += table_insert(keys[i - 1]) == false;
		e = rdtsc();
		a += e - s;
	}
	ri = a / (r * keys_length);

	a = 0;
	for (j = 0; j < r; j++) {
		if (table_reset() == false) {
			ck_error("ERROR: Failed to reset hash table.\n");
		}

		s = rdtsc();
		for (i = 0; i < keys_length; i++)
			d += table_insert(keys[i]) == false;
		e = rdtsc();
		a += e - s;
	}
	si = a / (r * keys_length);

	a = 0;
	for (j = 0; j < r; j++) {
		keys_shuffle(keys);

		if (table_reset() == false) {
			ck_error("ERROR: Failed to reset hash table.\n");
		}

		s = rdtsc();
		for (i = 0; i < keys_length; i++)
			d += table_insert(keys[i]) == false;
		e = rdtsc();
		a += e - s;
	}
	ai = a / (r * keys_length);

	a = 0;
	for (j = 0; j < r; j++) {
		s = rdtsc();
		for (i = 0; i < keys_length; i++)
			table_replace(keys[i]);
		e = rdtsc();
		a += e - s;
	}
	sr = a / (r * keys_length);

	table_reset();
	for (i = 0; i < keys_length; i++)
		table_insert(keys[i]);

	a = 0;
	for (j = 0; j < r; j++) {
		s = rdtsc();
		for (i = keys_length; i > 0; i--) {
			if (table_get(keys[i - 1]) == NULL) {
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
			if (table_get(keys[i]) == NULL) {
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
			if (table_get(keys[i]) == NULL) {
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
			table_remove(keys[i]);
		e = rdtsc();
		a += e - s;

		for (i = 0; i < keys_length; i++)
			table_insert(keys[i]);
	}
	sd = a / (r * keys_length);

	for (i = 0; i < keys_length / 2; i++)
		table_remove(keys[i]);

	a = 0;
	for (j = 0; j < r; j++) {
		s = rdtsc();
		table_gc();
		e = rdtsc();
		a += e - s;
	}
	gg = a / r;

	a = 0;
	for (j = 0; j < r; j++) {
		s = rdtsc();
		for (i = 0; i < keys_length; i++) {
			table_get("\x50\x03\x04\x05\x06\x10");
		}
		e = rdtsc();
		a += e - s;
	}
	ng = a / (r * keys_length);

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
	    "%" PRIu64 "\n",
	    keys_length, ri, si, ai, sr, rg, sg, ag, sd, ng, gg);

	return 0;
}

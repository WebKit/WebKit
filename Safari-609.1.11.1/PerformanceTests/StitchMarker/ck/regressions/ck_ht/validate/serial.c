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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../common.h"
#include "../../../src/ck_ht_hash.h"

static size_t hash_times_called = 0;

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

static void
ht_hash_wrapper(struct ck_ht_hash *h,
	const void *key,
	size_t length,
	uint64_t seed)
{
	hash_times_called++;

	h->value = (unsigned long)MurmurHash64A(key, length, seed);
	return;
}

static struct ck_malloc my_allocator = {
	.malloc = ht_malloc,
	.free = ht_free
};

const char *test[] = {"Samy", "Al", "Bahra", "dances", "in", "the", "wind.", "Once",
			"upon", "a", "time", "his", "gypsy", "ate", "one", "itsy",
			    "bitsy", "spider.", "What", "goes", "up", "must",
				"come", "down.", "What", "is", "down", "stays",
				    "down.", "A", "B", "C", "D", "E", "F", "G", "H",
					"I", "J", "K", "L", "M", "N", "O"};

static uintptr_t direct[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 1, 1, 2, 3, 4, 5, 9 };

const char *negative = "negative";

int
main(void)
{
	size_t i, l;
	ck_ht_t ht;
	ck_ht_entry_t entry;
	ck_ht_hash_t h;
	ck_ht_iterator_t iterator = CK_HT_ITERATOR_INITIALIZER;
	ck_ht_entry_t *cursor;
	unsigned int mode = CK_HT_MODE_BYTESTRING;

#ifdef HT_DELETE
	mode |= CK_HT_WORKLOAD_DELETE;
#endif

	if (ck_ht_init(&ht, mode, ht_hash_wrapper, &my_allocator, 2, 6602834) == false) {
		perror("ck_ht_init");
		exit(EXIT_FAILURE);
	}

	for (i = 0; i < sizeof(test) / sizeof(*test); i++) {
		l = strlen(test[i]);
		ck_ht_hash(&h, &ht, test[i], l);
		ck_ht_entry_set(&entry, h, test[i], l, test[i]);
		ck_ht_put_spmc(&ht, h, &entry);
	}

	l = strlen(test[0]);
	ck_ht_hash(&h, &ht, test[0], l);
	ck_ht_entry_set(&entry, h, test[0], l, test[0]);
	ck_ht_put_spmc(&ht, h, &entry);

	for (i = 0; i < sizeof(test) / sizeof(*test); i++) {
		l = strlen(test[i]);
		ck_ht_hash(&h, &ht, test[i], l);
		ck_ht_entry_key_set(&entry, test[i], l);
		if (ck_ht_get_spmc(&ht, h, &entry) == false) {
			ck_error("ERROR (put): Failed to find [%s]\n", test[i]);
		} else {
			void *k, *v;

			k = ck_ht_entry_key(&entry);
			v = ck_ht_entry_value(&entry);

			if (strcmp(k, test[i]) || strcmp(v, test[i])) {
				ck_error("ERROR: Mismatch: (%s, %s) != (%s, %s)\n",
				    (char *)k, (char *)v, test[i], test[i]);
			}
		}
	}

	ck_ht_hash(&h, &ht, negative, strlen(negative));
	ck_ht_entry_key_set(&entry, negative, strlen(negative));
	if (ck_ht_get_spmc(&ht, h, &entry) == true) {
		ck_error("ERROR: Found non-existing entry.\n");
	}

	for (i = 0; i < sizeof(test) / sizeof(*test); i++) {
		l = strlen(test[i]);
		ck_ht_hash(&h, &ht, test[i], l);
		ck_ht_entry_key_set(&entry, test[i], l);

		if (ck_ht_get_spmc(&ht, h, &entry) == false)
			continue;

		if (ck_ht_remove_spmc(&ht, h, &entry) == false) {
			ck_error("ERROR: Failed to delete existing entry\n");
		}

		if (ck_ht_get_spmc(&ht, h, &entry) == true)
			ck_error("ERROR: Able to find [%s] after delete\n", test[i]);

		ck_ht_entry_set(&entry, h, test[i], l, test[i]);
		if (ck_ht_put_spmc(&ht, h, &entry) == false)
			ck_error("ERROR: Failed to insert [%s]\n", test[i]);

		if (ck_ht_remove_spmc(&ht, h, &entry) == false) {
			ck_error("ERROR: Failed to delete existing entry\n");
		}
	}

	ck_ht_reset_spmc(&ht);
	if (ck_ht_count(&ht) != 0) {
		ck_error("ERROR: Map was not reset.\n");
	}

	for (i = 0; i < sizeof(test) / sizeof(*test); i++) {
		l = strlen(test[i]);
		ck_ht_hash(&h, &ht, test[i], l);
		ck_ht_entry_set(&entry, h, test[i], l, test[i]);
		ck_ht_put_spmc(&ht, h, &entry);
	}

	for (i = 0; ck_ht_next(&ht, &iterator, &cursor) == true; i++);
	if (i != 42) {
		ck_error("ERROR: Incorrect number of entries in table.\n");
	}

	for (i = 0; i < sizeof(test) / sizeof(*test); i++) {
		l = strlen(test[i]);
		ck_ht_hash(&h, &ht, test[i], l);
		ck_ht_entry_set(&entry, h, test[i], l, test[i]);
		ck_ht_set_spmc(&ht, h, &entry);
	}

	for (i = 0; i < sizeof(test) / sizeof(*test); i++) {
		l = strlen(test[i]);
		ck_ht_hash(&h, &ht, test[i], l);
		ck_ht_entry_key_set(&entry, test[i], l);
		if (ck_ht_get_spmc(&ht, h, &entry) == false) {
			ck_error("ERROR (set): Failed to find [%s]\n", test[i]);
		} else {
			void *k, *v;

			k = ck_ht_entry_key(&entry);
			v = ck_ht_entry_value(&entry);

			if (strcmp(k, test[i]) || strcmp(v, test[i])) {
				ck_error("ERROR: Mismatch: (%s, %s) != (%s, %s)\n",
				    (char *)k, (char *)v, test[i], test[i]);
			}
		}
	}

	if (ck_ht_gc(&ht, 0, 27) == false) {
		ck_error("ck_ht_gc\n");
	}

	for (i = 0; i < sizeof(test) / sizeof(*test); i++) {
		l = strlen(test[i]);
		ck_ht_hash(&h, &ht, test[i], l);
		ck_ht_entry_set(&entry, h, test[i], l, "REPLACED");
		ck_ht_set_spmc(&ht, h, &entry);

		if (strcmp(test[i], "What") == 0)
			continue;

		if (strcmp(test[i], "down.") == 0)
			continue;

		if (strcmp(ck_ht_entry_value(&entry), test[i]) != 0) {
			ck_error("Mismatch detected: %s, expected %s\n",
				(char *)ck_ht_entry_value(&entry),
				test[i]);
		}
	}

	ck_ht_iterator_init(&iterator);
	while (ck_ht_next(&ht, &iterator, &cursor) == true) {
		if (strcmp(ck_ht_entry_value(cursor), "REPLACED") != 0) {
			ck_error("Mismatch detected: %s, expected REPLACED\n",
				(char *)ck_ht_entry_value(cursor));
		}
	}

	for (i = 0; i < sizeof(test) / sizeof(*test); i++) {
		l = strlen(test[i]);
		ck_ht_hash(&h, &ht, test[i], l);
		ck_ht_entry_key_set(&entry, test[i], l);

		if (ck_ht_get_spmc(&ht, h, &entry) == false)
			continue;

		if (ck_ht_remove_spmc(&ht, h, &entry) == false) {
			ck_error("ERROR: Failed to delete existing entry\n");
		}

		if (ck_ht_get_spmc(&ht, h, &entry) == true)
			ck_error("ERROR: Able to find [%s] after delete\n", test[i]);

		ck_ht_entry_set(&entry, h, test[i], l, test[i]);
		if (ck_ht_put_spmc(&ht, h, &entry) == false)
			ck_error("ERROR: Failed to insert [%s]\n", test[i]);

		if (ck_ht_remove_spmc(&ht, h, &entry) == false) {
			ck_error("ERROR: Failed to delete existing entry\n");
		}
	}

	ck_ht_destroy(&ht);

	if (hash_times_called == 0) {
		ck_error("ERROR: Our hash function was not called!\n");
	}

	hash_times_called = 0;

	if (ck_ht_init(&ht, CK_HT_MODE_DIRECT, ht_hash_wrapper, &my_allocator, 8, 6602834) == false) {
		perror("ck_ht_init");
		exit(EXIT_FAILURE);
	}

	l = 0;
	for (i = 0; i < sizeof(direct) / sizeof(*direct); i++) {
		ck_ht_hash_direct(&h, &ht, direct[i]);
		ck_ht_entry_set_direct(&entry, h, direct[i], (uintptr_t)test[i]);
		l += ck_ht_put_spmc(&ht, h, &entry) == false;
	}

	if (l != 7) {
		ck_error("ERROR: Got %zu failures rather than 7\n", l);
	}

	for (i = 0; i < sizeof(direct) / sizeof(*direct); i++) {
		ck_ht_hash_direct(&h, &ht, direct[i]);
		ck_ht_entry_set_direct(&entry, h, direct[i], (uintptr_t)"REPLACED");
		l += ck_ht_set_spmc(&ht, h, &entry) == false;
	}

	ck_ht_iterator_init(&iterator);
	while (ck_ht_next(&ht, &iterator, &cursor) == true) {
		if (strcmp(ck_ht_entry_value(cursor), "REPLACED") != 0) {
			ck_error("Mismatch detected: %s, expected REPLACED\n",
				(char *)ck_ht_entry_value(cursor));
		}
	}

	ck_ht_destroy(&ht);

	if (hash_times_called == 0) {
		ck_error("ERROR: Our hash function was not called!\n");
	}

	return 0;
}

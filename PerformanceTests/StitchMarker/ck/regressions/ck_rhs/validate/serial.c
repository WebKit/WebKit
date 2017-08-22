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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../common.h"

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

const char *test[] = { "Samy", "Al", "Bahra", "dances", "in", "the", "wind.", "Once",
			"upon", "a", "time", "his", "gypsy", "ate", "one", "itsy",
			    "bitsy", "spider.", "What", "goes", "up", "must",
				"come", "down.", "What", "is", "down", "stays",
				    "down.", "A", "B", "C", "D", "E", "F", "G", "H",
					"I", "J", "K", "L", "M", "N", "O", "P", "Q" };

const char *negative = "negative";

/* Purposefully crappy hash function. */
static unsigned long
hs_hash(const void *object, unsigned long seed)
{
	const char *c = object;
	unsigned long h;

	(void)seed;
	h = c[0];
	return h;
}

static bool
hs_compare(const void *previous, const void *compare)
{

	return strcmp(previous, compare) == 0;
}

static void *
test_ip(void *key, void *closure)
{
	const char *a = key;
	const char *b = closure;

	if (strcmp(a, b) != 0)
		ck_error("Mismatch: %s != %s\n", a, b);

	return closure;
}

static void *
test_negative(void *key, void *closure)
{

	(void)closure;
	if (key != NULL)
		ck_error("ERROR: Apply callback expects NULL argument instead of [%s]\n", key);

	return NULL;
}

static void *
test_unique(void *key, void *closure)
{

	if (key != NULL)
		ck_error("ERROR: Apply callback expects NULL argument instead of [%s]\n", key);

	return closure;
}

static void *
test_remove(void *key, void *closure)
{

	(void)key;
	(void)closure;

	return NULL;
}

static void
run_test(unsigned int is, unsigned int ad)
{
	ck_rhs_t hs[16];
	const size_t size = sizeof(hs) / sizeof(*hs);
	size_t i, j;
	const char *blob = "#blobs";
	unsigned long h;

	if (ck_rhs_init(&hs[0], CK_RHS_MODE_SPMC | CK_RHS_MODE_OBJECT | ad, hs_hash, hs_compare, &my_allocator, is, 6602834) == false)
		ck_error("ck_rhs_init\n");

	for (j = 0; j < size; j++) {
		for (i = 0; i < sizeof(test) / sizeof(*test); i++) {
			h = test[i][0];
			if (ck_rhs_get(&hs[j], h, test[i]) != NULL) {
				continue;
			}

			if (i & 1) {
				if (ck_rhs_put_unique(&hs[j], h, test[i]) == false)
					ck_error("ERROR [%zu]: Failed to insert unique (%s)\n", j, test[i]);
			} else if (ck_rhs_apply(&hs[j], h, test[i], test_unique,
			    (void *)(uintptr_t)test[i]) == false) {
				ck_error("ERROR: Failed to apply for insertion.\n");
			}

			if (i & 1) {
				if (ck_rhs_remove(&hs[j], h, test[i]) == false)
					ck_error("ERROR [%zu]: Failed to remove unique (%s)\n", j, test[i]);
			} else if (ck_rhs_apply(&hs[j], h, test[i], test_remove, NULL) == false) {
				ck_error("ERROR: Failed to remove apply.\n");
			}

			if (ck_rhs_apply(&hs[j], h, test[i], test_negative,
			    (void *)(uintptr_t)test[i]) == false)
				ck_error("ERROR: Failed to apply.\n");

			break;
		}

		for (i = 0; i < sizeof(test) / sizeof(*test); i++) {
			h = test[i][0];
			ck_rhs_put(&hs[j], h, test[i]);
			if (ck_rhs_put(&hs[j], h, test[i]) == true) {
				ck_error("ERROR [%u] [1]: put must fail on collision (%s).\n", is, test[i]);
			}
			if (ck_rhs_get(&hs[j], h, test[i]) == NULL) {
				ck_error("ERROR [%u]: get must not fail after put\n", is);
			}
		}

		/* Test grow semantics. */
		ck_rhs_grow(&hs[j], 128);
		for (i = 0; i < sizeof(test) / sizeof(*test); i++) {
			h = test[i][0];
			if (ck_rhs_put(&hs[j], h, test[i]) == true) {
				ck_error("ERROR [%u] [2]: put must fail on collision.\n", is);
			}

			if (ck_rhs_get(&hs[j], h, test[i]) == NULL) {
				ck_error("ERROR [%u]: get must not fail\n", is);
			}
		}

		h = blob[0];
		if (ck_rhs_get(&hs[j], h, blob) == NULL) {
			if (j > 0)
				ck_error("ERROR [%u]: Blob must always exist after first.\n", is);

			if (ck_rhs_put(&hs[j], h, blob) == false) {
				ck_error("ERROR [%u]: A unique blob put failed.\n", is);
			}
		} else {
			if (ck_rhs_put(&hs[j], h, blob) == true) {
				ck_error("ERROR [%u]: Duplicate blob put succeeded.\n", is);
			}
		}

		/* Grow set and check get semantics. */
		ck_rhs_grow(&hs[j], 512);
		for (i = 0; i < sizeof(test) / sizeof(*test); i++) {
			h = test[i][0];
			if (ck_rhs_get(&hs[j], h, test[i]) == NULL) {
				ck_error("ERROR [%u]: get must not fail\n", is);
			}
		}

		/* Delete and check negative membership. */
		for (i = 0; i < sizeof(test) / sizeof(*test); i++) {
			void *r;

			h = test[i][0];
			if (ck_rhs_get(&hs[j], h, test[i]) == NULL)
				continue;

			if (r = ck_rhs_remove(&hs[j], h, test[i]), r == NULL) {
				ck_error("ERROR [%u]: remove must not fail\n", is);
			}

			if (strcmp(r, test[i]) != 0) {
				ck_error("ERROR [%u]: Removed incorrect node (%s != %s)\n", (char *)r, test[i], is);
			}
		}

		/* Test replacement semantics. */
		for (i = 0; i < sizeof(test) / sizeof(*test); i++) {
			void *r;
			bool d;

			h = test[i][0];
			d = ck_rhs_get(&hs[j], h, test[i]) != NULL;
			if (ck_rhs_set(&hs[j], h, test[i], &r) == false) {
				ck_error("ERROR [%u]: Failed to set\n", is);
			}

			/* Expected replacement. */
			if (d == true && (r == NULL || strcmp(r, test[i]) != 0)) {
				ck_error("ERROR [%u]: Incorrect previous value: %s != %s\n",
				    is, test[i], (char *)r);
			}

			/* Replacement should succeed. */
			if (ck_rhs_fas(&hs[j], h, test[i], &r) == false)
				ck_error("ERROR [%u]: ck_rhs_fas must succeed.\n", is);

			if (strcmp(r, test[i]) != 0) {
				ck_error("ERROR [%u]: Incorrect replaced value: %s != %s\n",
				    is, test[i], (char *)r);
			}

			if (ck_rhs_fas(&hs[j], h, negative, &r) == true)
				ck_error("ERROR [%u]: Replacement of negative should fail.\n", is);

			if (ck_rhs_set(&hs[j], h, test[i], &r) == false) {
				ck_error("ERROR [%u]: Failed to set [1]\n", is);
			}

			if (strcmp(r, test[i]) != 0) {
				ck_error("ERROR [%u]: Invalid &hs[j]: %s != %s\n", (char *)r, test[i], is);
			}
			/* Attempt in-place mutation. */
			if (ck_rhs_apply(&hs[j], h, test[i], test_ip,
			    (void *)(uintptr_t)test[i]) == false) {
				ck_error("ERROR [%u]: Failed to apply: %s != %s\n", is, (char *)r, test[i]);
			}

			d = ck_rhs_get(&hs[j], h, test[i]) != NULL;
			if (d == false)
				ck_error("ERROR [%u]: Expected [%s] to exist.\n", is, test[i]);
		}

		if (j == size - 1)
			break;

		if (ck_rhs_move(&hs[j + 1], &hs[j], hs_hash, hs_compare, &my_allocator) == false)
			ck_error("Failed to move hash table");

		ck_rhs_gc(&hs[j + 1]);

		if (ck_rhs_rebuild(&hs[j + 1]) == false)
			ck_error("Failed to rebuild");
	}

	return;
}

int
main(void)
{
	unsigned int k;

	for (k = 16; k <= 64; k <<= 1) {
		run_test(k, 0);
		break;
	}

	return 0;
}


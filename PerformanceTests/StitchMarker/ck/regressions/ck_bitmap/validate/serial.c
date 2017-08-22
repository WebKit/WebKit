/*
 * Copyright 2012-2015 Samy Al Bahra.
 * Copyright 2012-2014 AppNexus, Inc.
 * Copyright 2012 Shreyas Prasad.
 * Copyright 2014 Paul Khuong.
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

#include <ck_bitmap.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../common.h"

#ifndef STATIC_LENGTH
#define STATIC_LENGTH 256
#endif

static unsigned int length = 256;
static ck_bitmap_t *g_bits;

static void
check_iteration(ck_bitmap_t *bits, unsigned int len, bool initial)
{
	ck_bitmap_iterator_t iter;
	unsigned int i = 0, j;

	len += 1;
	if (initial == true) {
		if (bits == g_bits)
			len = length;
		else
			len = STATIC_LENGTH;
	}

	ck_bitmap_iterator_init(&iter, bits);
	for (j = 0; ck_bitmap_next(bits, &iter, &i) == true; j++) {
		if (i == j)
			continue;

		ck_error("[4] ERROR: Expected bit %u, got bit %u\n", j, i);
	}

	if (j != len) {
		ck_error("[5] ERROR: Expected length %u, got length %u\n", len, j);
	}

	return;
}

static void
test(ck_bitmap_t *bits, unsigned int n_length, bool initial)
{
	bool r;
	unsigned int i;
	CK_BITMAP_INSTANCE(8) u;

	CK_BITMAP_INIT(&u, 8, false);
	CK_BITMAP_SET(&u, 1);
	CK_BITMAP_SET(&u, 4);

	for (i = 0; i < n_length; i++) {
		if (ck_bitmap_test(bits, i) == !initial) {
			ck_error("[0] ERROR [%u]: Expected %u got %u\n", i,
				initial, !initial);
		}
	}

	for (i = 0; i < n_length; i++) {
		ck_bitmap_set(bits, i);
		if (ck_bitmap_test(bits, i) == false) {
			ck_error("[1] ERROR: Expected bit to be set: %u\n", i);
		}

		ck_bitmap_reset(bits, i);
		if (ck_bitmap_test(bits, i) == true) {
			ck_error("[2] ERROR: Expected bit to be cleared: %u\n", i);
		}

		r = ck_bitmap_bts(bits, i);
		if (r == true) {
			ck_error("[3] ERROR: Expected bit to be cleared before 1st bts: %u\n", i);
		}
		if (ck_bitmap_test(bits, i) == false) {
			ck_error("[4] ERROR: Expected bit to be set: %u\n", i);
		}
		r = ck_bitmap_bts(bits, i);
		if (r == false) {
			ck_error("[5] ERROR: Expected bit to be set before 2nd bts: %u\n", i);
		}
		if (ck_bitmap_test(bits, i) == false) {
			ck_error("[6] ERROR: Expected bit to be set: %u\n", i);
		}

		ck_bitmap_reset(bits, i);
		if (ck_bitmap_test(bits, i) == true) {
			ck_error("[7] ERROR: Expected bit to be cleared: %u\n", i);
		}

		ck_bitmap_set(bits, i);
		if (ck_bitmap_test(bits, i) == false) {
			ck_error("[8] ERROR: Expected bit to be set: %u\n", i);
		}

		check_iteration(bits, i, initial);
	}

	for (i = 0; i < n_length; i++) {
		if (ck_bitmap_test(bits, i) == false) {
			ck_error("[9] ERROR: Expected bit to be set: %u\n", i);
		}
	}

	ck_bitmap_clear(bits);

	for (i = 0; i < n_length; i++) {
		if (ck_bitmap_test(bits, i) == true) {
			ck_error("[10] ERROR: Expected bit to be reset: %u\n", i);
		}
	}

	ck_bitmap_union(bits, CK_BITMAP(&u));
	if (ck_bitmap_test(bits, 1) == false ||
	    ck_bitmap_test(bits, 4) == false) {
		ck_error("ERROR: Expected union semantics.\n");
	}

	return;
}

static void
test_init(bool init)
{
	ck_bitmap_t *bitmap;
	size_t bytes;
	unsigned int i;

	bytes = ck_bitmap_size(length);
	bitmap = malloc(bytes);
	memset(bitmap, random(), bytes);

	ck_bitmap_init(bitmap, length, init);

	if (ck_bitmap_bits(bitmap) != length) {
		ck_error("ERROR: Expected length %u got %u\n",
		    length, ck_bitmap_bits(bitmap));
	}

	for (i = 0; i < length; i++) {
		if (ck_bitmap_test(bitmap, i) != init) {
			ck_error("ERROR: Expected bit %i at index %u, got %i\n",
			    (int)init, i, (int)(!init));
		}
	}

	free(bitmap);
}

static ck_bitmap_t *
random_init(void)
{
	ck_bitmap_t *bitmap;
	unsigned int i;

	bitmap = malloc(ck_bitmap_size(length));
	ck_bitmap_init(bitmap, length, false);

	for (i = 0; i < length; i++) {
		if (random() & 1) {
			ck_bitmap_set(bitmap, i);
		}
	}

	return bitmap;
}

static ck_bitmap_t *
copy(const ck_bitmap_t *src)
{
	ck_bitmap_t *bitmap;
	size_t bytes = ck_bitmap_size(ck_bitmap_bits(src));

	bitmap = malloc(bytes);
	memcpy(bitmap, src, bytes);
	return bitmap;
}

static void
test_counts(const ck_bitmap_t *x, const ck_bitmap_t *y)
{
	unsigned int count = 0;
	unsigned int count_intersect = 0;
	unsigned int i;

	for (i = 0; i <= length * 2; i++) {
		unsigned actual_limit = i;
		unsigned int r;
		bool check;

		if (actual_limit > ck_bitmap_bits(x))
			actual_limit = ck_bitmap_bits(x);

		check = ck_bitmap_empty(x, i);
		if (check != (count == 0)) {
			ck_error("ck_bitmap_empty(%u): got %i expected %i\n",
			    i, (int)check, (int)(count == 0));
		}

		check = ck_bitmap_full(x, i);
		if (check != (count == actual_limit)) {
			ck_error("ck_bitmap_full(%u): got %i expected %i\n",
			    i, (int)check, (int)(count == i));
		}

		r = ck_bitmap_count(x, i);
		if (r != count) {
			ck_error("ck_bitmap_count(%u): got %u expected %u\n",
			    i, r, count);
		}

		r = ck_bitmap_count_intersect(x, y, i);
		if (r != count_intersect) {
			ck_error("ck_bitmap_count_intersect(%u): got %u expected %u\n",
			    i, r, count_intersect);
		}

		if (i < length) {
			count += ck_bitmap_test(x, i);
			count_intersect += ck_bitmap_test(x, i) & ck_bitmap_test(y, i);
		}
	}
}

static void
random_test(unsigned int seed)
{
	ck_bitmap_t *x, *x_copy, *y;
	unsigned int i;

	srandom(seed);

	test_init(false);
	test_init(true);

	x = random_init();
	y = random_init();

#define TEST(routine, expected) do {					\
		x_copy = copy(x);					\
		routine(x_copy, y);					\
		for (i = 0; i < length; i++) {				\
			bool xi = ck_bitmap_test(x, i);			\
			bool yi = ck_bitmap_test(y, i);			\
			bool ri = ck_bitmap_test(x_copy, i);		\
			bool wanted = expected(xi, yi);			\
									\
			if (ri != wanted) {				\
				ck_error("In " #routine " at %u: "	\
				    "got %i expected %i\n",		\
				    i, ri, wanted);			\
			}						\
		}							\
		free(x_copy);						\
	} while (0)

#define OR(x, y) (x | y)
#define AND(x, y) (x & y)
#define ANDC2(x, y) (x & (~y))

	TEST(ck_bitmap_union, OR);
	TEST(ck_bitmap_intersection, AND);
	TEST(ck_bitmap_intersection_negate, ANDC2);

#undef ANDC2
#undef AND
#undef OR
#undef TEST

	test_counts(x, y);

	for (i = 0; i < 4; i++) {
		ck_bitmap_init(x, length, i & 1);
		ck_bitmap_init(y, length, i >> 1);
		test_counts(x, y);
	}

	free(y);
	free(x);
}

int
main(int argc, char *argv[])
{
	unsigned int bytes, base;
	size_t i, j;

	if (argc >= 2) {
		length = atoi(argv[1]);
	}

	base = ck_bitmap_base(length);
	bytes = ck_bitmap_size(length);
	fprintf(stderr, "Configuration: %u bytes\n",
	    bytes);

	g_bits = malloc(bytes);
	memset(g_bits->map, 0xFF, base);
	ck_bitmap_init(g_bits, length, false);
	test(g_bits, length, false);

	memset(g_bits->map, 0x00, base);
	ck_bitmap_init(g_bits, length, true);
	test(g_bits, length, true);

	ck_bitmap_test(g_bits, length - 1);

	CK_BITMAP_INSTANCE(STATIC_LENGTH) sb;
	fprintf(stderr, "Static configuration: %zu bytes\n",
	    sizeof(sb));
	memset(CK_BITMAP_BUFFER(&sb), 0xFF, ck_bitmap_base(STATIC_LENGTH));
	CK_BITMAP_INIT(&sb, STATIC_LENGTH, false);
	test(CK_BITMAP(&sb), STATIC_LENGTH, false);
	memset(CK_BITMAP_BUFFER(&sb), 0x00, ck_bitmap_base(STATIC_LENGTH));
	CK_BITMAP_INIT(&sb, STATIC_LENGTH, true);
	test(CK_BITMAP(&sb), STATIC_LENGTH, true);

	CK_BITMAP_CLEAR(&sb);
	if (CK_BITMAP_TEST(&sb, 1) == true) {
		ck_error("ERROR: Expected bit to be reset.\n");
	}

	CK_BITMAP_SET(&sb, 1);
	if (CK_BITMAP_TEST(&sb, 1) == false) {
		ck_error("ERROR: Expected bit to be set.\n");
	}

	CK_BITMAP_RESET(&sb, 1);
	if (CK_BITMAP_TEST(&sb, 1) == true) {
		ck_error("ERROR: Expected bit to be reset.\n");
	}

	for (i = 0; i < 4 * sizeof(unsigned int) * CHAR_BIT; i++) {
		length = i;
		for (j = 0; j < 10; j++) {
			random_test(i * 10 + j);
		}
	}

	return 0;
}

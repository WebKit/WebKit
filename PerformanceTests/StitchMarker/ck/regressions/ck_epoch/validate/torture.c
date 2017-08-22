/*
 * Copyright 2010-2015 Samy Al Bahra.
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

#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ck_cc.h>
#include <ck_pr.h>
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>
#include <ck_epoch.h>
#include <ck_stack.h>

#include "../../common.h"

static unsigned int n_rd;
static unsigned int n_wr;
static unsigned int n_threads;
static unsigned int barrier;
static unsigned int leave;
static unsigned int first;

struct {
	unsigned int value;
} valid CK_CC_CACHELINE = { 1 };

struct {
	unsigned int value;
} invalid CK_CC_CACHELINE;

#ifndef PAIRS_S
#define PAIRS_S 10000
#endif

#ifndef CK_EPOCH_T_DEPTH
#define CK_EPOCH_T_DEPTH	8
#endif

static ck_epoch_t epoch;
static struct affinity a;

static void
test(struct ck_epoch_record *record)
{
	unsigned int j[3];
	unsigned int b, c;
	const unsigned int r = 100;
	size_t i;

	for (i = 0; i < 8; i++) {
		ck_epoch_begin(record, NULL);
		c = ck_pr_load_uint(&invalid.value);
		ck_pr_fence_load();
		b = ck_pr_load_uint(&valid.value);
		ck_test(c > b, "Invalid value: %u > %u\n", c, b);
		ck_epoch_end(record, NULL);
	}

	ck_epoch_begin(record, NULL);

	/* This implies no early load of epoch occurs. */
	j[0] = record->epoch;


	/* We should observe up to one epoch migration. */
	do {
		ck_pr_fence_load();
		j[1] = ck_pr_load_uint(&epoch.epoch);

		if (ck_pr_load_uint(&leave) == 1) {
			ck_epoch_end(record, NULL);
			return;
		}
	} while (j[1] == j[0]);

	/* No more epoch migrations should occur */
	for (i = 0; i < r; i++) {
		ck_pr_fence_strict_load();
		j[2] = ck_pr_load_uint(&epoch.epoch);

		ck_test(j[2] != j[1], "Inconsistency detected: %u %u %u\n",
		    j[0], j[1], j[2]);
	}

	ck_epoch_end(record, NULL);
	return;
}

static void *
read_thread(void *unused CK_CC_UNUSED)
{
	ck_epoch_record_t *record;

	record = malloc(sizeof *record);
	assert(record != NULL);
	ck_epoch_register(&epoch, record, NULL);

	if (aff_iterate(&a)) {
		perror("ERROR: failed to affine thread");
		exit(EXIT_FAILURE);
	}

	ck_pr_inc_uint(&barrier);
	while (ck_pr_load_uint(&barrier) < n_threads);

	do {
		test(record);
		test(record);
		test(record);
		test(record);
	} while (ck_pr_load_uint(&leave) == 0);

	ck_pr_dec_uint(&n_rd);

	return NULL;
}

static void *
write_thread(void *unused CK_CC_UNUSED)
{
	ck_epoch_record_t *record;
	unsigned long iterations = 0;
	bool c = ck_pr_faa_uint(&first, 1);
	uint64_t ac = 0;

	record = malloc(sizeof *record);
	assert(record != NULL);
	ck_epoch_register(&epoch, record, NULL);

	if (aff_iterate(&a)) {
		perror("ERROR: failed to affine thread");
		exit(EXIT_FAILURE);
	}

	ck_pr_inc_uint(&barrier);
	while (ck_pr_load_uint(&barrier) < n_threads);

#define CK_EPOCH_S	do {		\
	uint64_t _s = rdtsc();		\
	ck_epoch_synchronize(record);	\
	ac += rdtsc() - _s;		\
} while (0)

	do {
		/*
		 * A thread should never observe invalid.value > valid.value.
		 * inside a protected section. Only
		 * invalid.value <= valid.value is valid.
		 */
		if (!c) ck_pr_store_uint(&valid.value, 1);
		CK_EPOCH_S;
		if (!c) ck_pr_store_uint(&invalid.value, 1);

		ck_pr_fence_store();
		if (!c) ck_pr_store_uint(&valid.value, 2);
		CK_EPOCH_S;
		if (!c) ck_pr_store_uint(&invalid.value, 2);

		ck_pr_fence_store();
		if (!c) ck_pr_store_uint(&valid.value, 3);
		CK_EPOCH_S;
		if (!c) ck_pr_store_uint(&invalid.value, 3);

		ck_pr_fence_store();
		if (!c) ck_pr_store_uint(&valid.value, 4);
		CK_EPOCH_S;
		if (!c) ck_pr_store_uint(&invalid.value, 4);

		CK_EPOCH_S;
		if (!c) ck_pr_store_uint(&invalid.value, 0);
		CK_EPOCH_S;

		iterations += 6;
	} while (ck_pr_load_uint(&leave) == 0 &&
		 ck_pr_load_uint(&n_rd) > 0);

	fprintf(stderr, "%lu iterations\n", iterations);
	fprintf(stderr, "%" PRIu64 " average latency\n", ac / iterations);
	return NULL;
}

int
main(int argc, char *argv[])
{
	unsigned int i;
	pthread_t *threads;

	if (argc != 4) {
		ck_error("Usage: stack <#readers> <#writers> <affinity delta>\n");
	}

	n_rd = atoi(argv[1]);
	n_wr = atoi(argv[2]);
	n_threads = n_wr + n_rd;

	a.delta = atoi(argv[3]);
	a.request = 0;

	threads = malloc(sizeof(pthread_t) * n_threads);
	ck_epoch_init(&epoch);

	for (i = 0; i < n_rd; i++)
		pthread_create(threads + i, NULL, read_thread, NULL);

	do {
		pthread_create(threads + i, NULL, write_thread, NULL);
	} while (++i < n_wr + n_rd);

	common_sleep(30);
	ck_pr_store_uint(&leave, 1);

	for (i = 0; i < n_threads; i++)
		pthread_join(threads[i], NULL);

	return 0;
}

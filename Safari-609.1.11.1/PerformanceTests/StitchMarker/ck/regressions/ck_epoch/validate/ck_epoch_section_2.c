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
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <ck_epoch.h>
#include <ck_stack.h>

#include "../../common.h"

static unsigned int n_rd;
static unsigned int n_wr;
static unsigned int n_threads;
static unsigned int barrier;
static unsigned int leave;

#ifndef PAIRS_S
#define PAIRS_S 10000
#endif

#ifndef CK_EPOCH_T_DEPTH
#define CK_EPOCH_T_DEPTH	8
#endif

static ck_epoch_t epoch;
static struct affinity a;

static void *
read_thread(void *unused CK_CC_UNUSED)
{
	ck_epoch_record_t *record;
	unsigned long long i = 0;

	record = malloc(sizeof *record);
	assert(record != NULL);
	ck_epoch_register(&epoch, record, NULL);

	if (aff_iterate(&a)) {
		perror("ERROR: failed to affine thread");
		exit(EXIT_FAILURE);
	}

	ck_pr_inc_uint(&barrier);
	while (ck_pr_load_uint(&barrier) < n_threads);

	for (;;) {
		ck_epoch_section_t section[2];
		ck_epoch_section_t junk[CK_EPOCH_T_DEPTH];
		unsigned int j;

		ck_epoch_begin(record, &section[0]);

		for (j = 0; j < CK_EPOCH_T_DEPTH; j++)
			ck_epoch_begin(record, &junk[j]);
		for (j = 0; j < CK_EPOCH_T_DEPTH; j++)
			ck_epoch_end(record, &junk[j]);

		if (i > 0)
			ck_epoch_end(record, &section[1]);

		/* Wait for the next synchronize operation. */
		while ((ck_pr_load_uint(&epoch.epoch) & 1) ==
		    section[0].bucket) {
			i++;

			if (!(i % 10000000)) {
				fprintf(stderr, "%u %u %u\n",
				    ck_pr_load_uint(&epoch.epoch),
				    section[0].bucket, record->epoch);
			}

			while ((ck_pr_load_uint(&epoch.epoch) & 1) ==
			    section[0].bucket) {
				if (ck_pr_load_uint(&leave) == 1)
					break;

				ck_pr_stall();
			}
		}

		ck_epoch_begin(record, &section[1]);

		assert(section[0].bucket != section[1].bucket);
		ck_epoch_end(record, &section[0]);

		assert(ck_pr_load_uint(&record->active) > 0);

		if (ck_pr_load_uint(&leave) == 1) {
			ck_epoch_end(record, &section[1]);
			break;
		}

		i++;
	}

	return NULL;
}

static void *
write_thread(void *unused CK_CC_UNUSED)
{
	ck_epoch_record_t record;
	unsigned long iterations = 0;

	ck_epoch_register(&epoch, &record, NULL);

	if (aff_iterate(&a)) {
		perror("ERROR: failed to affine thread");
		exit(EXIT_FAILURE);
	}

	ck_pr_inc_uint(&barrier);
	while (ck_pr_load_uint(&barrier) < n_threads);

	for (;;) {
		if (!(iterations % 1048575))
			fprintf(stderr, ".");

		ck_epoch_synchronize(&record);
		iterations++;

		if (ck_pr_load_uint(&leave) == 1)
			break;
	}

	fprintf(stderr, "%lu iterations\n", iterations);
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

	common_sleep(10);
	ck_pr_store_uint(&leave, 1);

	for (i = 0; i < n_threads; i++)
		pthread_join(threads[i], NULL);

	return (0);
}

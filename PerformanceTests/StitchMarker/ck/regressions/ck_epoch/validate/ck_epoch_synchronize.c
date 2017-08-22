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

#include <errno.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <sys/time.h>

#include <ck_backoff.h>
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
static unsigned int e_barrier;
static unsigned int readers;
static unsigned int writers;

#ifndef PAIRS_S
#define PAIRS_S 10000
#endif

#ifndef ITERATE_S
#define ITERATE_S 20
#endif

struct node {
	unsigned int value;
	ck_stack_entry_t stack_entry;
	ck_epoch_entry_t epoch_entry;
};
static ck_stack_t stack = CK_STACK_INITIALIZER;
static ck_epoch_t stack_epoch;
CK_STACK_CONTAINER(struct node, stack_entry, stack_container)
CK_EPOCH_CONTAINER(struct node, epoch_entry, epoch_container)
static struct affinity a;
static const char animate[] = "-/|\\";

static void
destructor(ck_epoch_entry_t *p)
{
	struct node *e = epoch_container(p);

	free(e);
	return;
}

static void *
read_thread(void *unused CK_CC_UNUSED)
{
	unsigned int j;
	ck_epoch_record_t record CK_CC_CACHELINE;
	ck_stack_entry_t *cursor;
	ck_stack_entry_t *n;
	unsigned int i;

	ck_epoch_register(&stack_epoch, &record, NULL);

	if (aff_iterate(&a)) {
		perror("ERROR: failed to affine thread");
		exit(EXIT_FAILURE);
	}

	ck_pr_inc_uint(&barrier);
	while (ck_pr_load_uint(&barrier) < n_threads);

	while (CK_STACK_ISEMPTY(&stack) == true) {
		if (ck_pr_load_uint(&readers) != 0)
			break;

		ck_pr_stall();
	}

	j = 0;
	for (;;) {
		i = 0;

		ck_epoch_begin(&record, NULL);
		CK_STACK_FOREACH(&stack, cursor) {
			if (cursor == NULL)
				continue;

			n = CK_STACK_NEXT(cursor);
			j += ck_pr_load_ptr(&n) != NULL;

			if (i++ > 4098)
				break;
		}
		ck_epoch_end(&record, NULL);

		if (j != 0 && ck_pr_load_uint(&readers) == 0)
			ck_pr_store_uint(&readers, 1);

		if (CK_STACK_ISEMPTY(&stack) == true &&
		    ck_pr_load_uint(&e_barrier) != 0)
			break;
	}

	ck_pr_inc_uint(&e_barrier);
	while (ck_pr_load_uint(&e_barrier) < n_threads);

	fprintf(stderr, "[R] Observed entries: %u\n", j);
	return (NULL);
}

static void *
write_thread(void *unused CK_CC_UNUSED)
{
	struct node **entry, *e;
	unsigned int i, j, tid;
	ck_epoch_record_t record;
	ck_stack_entry_t *s;

	ck_epoch_register(&stack_epoch, &record, NULL);

	if (aff_iterate(&a)) {
		perror("ERROR: failed to affine thread");
		exit(EXIT_FAILURE);
	}

	tid = ck_pr_faa_uint(&writers, 1);
	ck_pr_inc_uint(&barrier);
	while (ck_pr_load_uint(&barrier) < n_threads);

	entry = malloc(sizeof(struct node *) * PAIRS_S);
	if (entry == NULL) {
		ck_error("Failed allocation.\n");
	}

	for (j = 0; j < ITERATE_S; j++) {
		for (i = 0; i < PAIRS_S; i++) {
			entry[i] = malloc(sizeof(struct node));
			if (entry == NULL) {
				ck_error("Failed individual allocation\n");
			}
		}

		for (i = 0; i < PAIRS_S; i++) {
			ck_stack_push_upmc(&stack, &entry[i]->stack_entry);
		}

		while (ck_pr_load_uint(&readers) == 0)
			ck_pr_stall();

		for (i = 0; i < PAIRS_S; i++) {
			ck_epoch_begin(&record, NULL);
			s = ck_stack_pop_upmc(&stack);
			e = stack_container(s);
			ck_epoch_end(&record, NULL);

			if (i & 1) {
				ck_epoch_synchronize(&record);
				ck_epoch_reclaim(&record);
				ck_epoch_call(&record, &e->epoch_entry, destructor);
			} else {
				ck_epoch_barrier(&record);
				destructor(&e->epoch_entry);
			}

			if (tid == 0 && (i % 16384) == 0) {
				fprintf(stderr, "[W] %2.2f: %c\n",
				    (double)j / ITERATE_S, animate[i % strlen(animate)]);
			}
		}
	}

	ck_epoch_synchronize(&record);

	if (tid == 0) {
		fprintf(stderr, "[W] Peak: %u (%2.2f%%)\n    Reclamations: %u\n\n",
			record.n_peak,
			(double)record.n_peak / ((double)PAIRS_S * ITERATE_S) * 100,
			record.n_dispatch);
	}

	ck_pr_inc_uint(&e_barrier);
	while (ck_pr_load_uint(&e_barrier) < n_threads);
	return (NULL);
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
	ck_epoch_init(&stack_epoch);

	for (i = 0; i < n_rd; i++)
		pthread_create(threads + i, NULL, read_thread, NULL);

	do {
		pthread_create(threads + i, NULL, write_thread, NULL);
	} while (++i < n_wr + n_rd);

	for (i = 0; i < n_threads; i++)
		pthread_join(threads[i], NULL);

	return (0);
}

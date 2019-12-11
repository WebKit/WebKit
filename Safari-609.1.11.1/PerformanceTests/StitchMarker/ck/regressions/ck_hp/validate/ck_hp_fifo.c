/*
 * Copyright 2011-2015 Samy Al Bahra.
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
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <ck_hp_fifo.h>

#include "../../common.h"

#ifndef ITERATIONS
#define ITERATIONS 128
#endif

struct context {
	unsigned int tid;
	unsigned int previous;
	unsigned int next;
};

struct entry {
	int tid;
	int value;
};

static ck_hp_fifo_t fifo;
static ck_hp_t fifo_hp;
static int nthr;

static struct affinity a;
static int size;
static unsigned int barrier;
static unsigned int e_barrier;

static void *
test(void *c)
{
	struct context *context = c;
	struct entry *entry;
	ck_hp_fifo_entry_t *fifo_entry;
	ck_hp_record_t record;
	int i, j;

        if (aff_iterate(&a)) {
                perror("ERROR: Could not affine thread");
                exit(EXIT_FAILURE);
        }

	ck_hp_register(&fifo_hp, &record, malloc(sizeof(void *) * 2));
	ck_pr_inc_uint(&barrier);
	while (ck_pr_load_uint(&barrier) < (unsigned int)nthr);

	for (i = 0; i < ITERATIONS; i++) {
		for (j = 0; j < size; j++) {
			fifo_entry = malloc(sizeof(ck_hp_fifo_entry_t));
			entry = malloc(sizeof(struct entry));
			entry->tid = context->tid;
			ck_hp_fifo_enqueue_mpmc(&record, &fifo, fifo_entry, entry);

			ck_pr_barrier();

			fifo_entry = ck_hp_fifo_dequeue_mpmc(&record, &fifo, &entry);
			if (fifo_entry == NULL) {
				ck_error("ERROR [%u] Queue should never be empty.\n", context->tid);
			}

			ck_pr_barrier();

			if (entry->tid < 0 || entry->tid >= nthr) {
				ck_error("ERROR [%u] Incorrect value in entry.\n", entry->tid);
			}

			ck_hp_free(&record, &fifo_entry->hazard, fifo_entry, fifo_entry);
		}
	}

	for (i = 0; i < ITERATIONS; i++) {
		for (j = 0; j < size; j++) {
			fifo_entry = malloc(sizeof(ck_hp_fifo_entry_t));
			entry = malloc(sizeof(struct entry));
			entry->tid = context->tid;

			while (ck_hp_fifo_tryenqueue_mpmc(&record, &fifo, fifo_entry, entry) == false)
				ck_pr_stall();

			while (fifo_entry = ck_hp_fifo_trydequeue_mpmc(&record, &fifo, &entry), fifo_entry == NULL)
				ck_pr_stall();

			if (entry->tid < 0 || entry->tid >= nthr) {
				ck_error("ERROR [%u] Incorrect value in entry.\n", entry->tid);
			}

			ck_hp_free(&record, &fifo_entry->hazard, fifo_entry, fifo_entry);
		}
	}

	ck_pr_inc_uint(&e_barrier);
	while (ck_pr_load_uint(&e_barrier) < (unsigned int)nthr);

	return (NULL);
}

static void
destructor(void *p)
{

	free(p);
	return;
}

int
main(int argc, char *argv[])
{
	int i, r;
	struct context *context;
	pthread_t *thread;
	int threshold;

	if (argc != 5) {
		ck_error("Usage: validate <threads> <affinity delta> <size> <threshold>\n");
	}

	a.delta = atoi(argv[2]);

	nthr = atoi(argv[1]);
	assert(nthr >= 1);

	size = atoi(argv[3]);
	assert(size > 0);

	threshold = atoi(argv[4]);
	assert(threshold > 0);

	context = malloc(sizeof(*context) * nthr);
	assert(context);

	thread = malloc(sizeof(pthread_t) * nthr);
	assert(thread);

	ck_hp_init(&fifo_hp, 2, threshold, destructor);
	ck_hp_fifo_init(&fifo, malloc(sizeof(ck_hp_fifo_entry_t)));

	ck_hp_fifo_entry_t *entry;
	ck_hp_fifo_deinit(&fifo, &entry);

	if (entry == NULL)
		ck_error("ERROR: Expected non-NULL stub node.\n");

	free(entry);
	ck_hp_fifo_init(&fifo, malloc(sizeof(ck_hp_fifo_entry_t)));

	for (i = 0; i < nthr; i++) {
		context[i].tid = i;
		r = pthread_create(thread + i, NULL, test, context + i);
		assert(r == 0);
	}

	for (i = 0; i < nthr; i++)
		pthread_join(thread[i], NULL);

	return (0);
}


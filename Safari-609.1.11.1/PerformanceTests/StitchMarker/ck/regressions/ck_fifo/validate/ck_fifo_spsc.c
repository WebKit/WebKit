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

#include <ck_fifo.h>

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

static int nthr;
static ck_fifo_spsc_t *fifo;
static struct affinity a;
static int size;
static unsigned int barrier;

static void *
test(void *c)
{
	struct context *context = c;
	struct entry *entry;
	ck_fifo_spsc_entry_t *fifo_entry;
	int i, j;

        if (aff_iterate(&a)) {
                perror("ERROR: Could not affine thread");
                exit(EXIT_FAILURE);
        }

#ifdef DEBUG
	fprintf(stderr, "%p %u: %u -> %u\n", fifo+context->tid, context->tid, context->previous, context->tid);
#endif

	if (context->tid == 0) {
		struct entry *entries;

		entries = malloc(sizeof(struct entry) * size);
		assert(entries != NULL);

		for (i = 0; i < size; i++) {
			entries[i].value = i;
			entries[i].tid = 0;

			fifo_entry = malloc(sizeof(ck_fifo_spsc_entry_t));
			ck_fifo_spsc_enqueue(fifo + context->tid, fifo_entry, entries + i);
		}
	}

	ck_pr_inc_uint(&barrier);
	while (ck_pr_load_uint(&barrier) < (unsigned int)nthr);

	for (i = 0; i < ITERATIONS; i++) {
		for (j = 0; j < size; j++) {
			while (ck_fifo_spsc_dequeue(fifo + context->previous, &entry) == false);
			if (context->previous != (unsigned int)entry->tid) {
				ck_error("T [%u:%p] %u != %u\n",
					context->tid, (void *)entry, entry->tid, context->previous);
			}

			if (entry->value != j) {
				ck_error("V [%u:%p] %u != %u\n",
					context->tid, (void *)entry, entry->value, j);
			}

			entry->tid = context->tid;
			fifo_entry = ck_fifo_spsc_recycle(fifo + context->tid);
			if (fifo_entry == NULL)
				fifo_entry = malloc(sizeof(ck_fifo_spsc_entry_t));

			ck_fifo_spsc_enqueue(fifo + context->tid, fifo_entry, entry);
		}
	}

	return (NULL);
}

int
main(int argc, char *argv[])
{
	int i, r;
	struct context *context;
	pthread_t *thread;

	if (argc != 4) {
		ck_error("Usage: validate <threads> <affinity delta> <size>\n");
	}

	a.request = 0;
	a.delta = atoi(argv[2]);

	nthr = atoi(argv[1]);
	assert(nthr >= 1);

	size = atoi(argv[3]);
	assert(size > 0);

	fifo = malloc(sizeof(ck_fifo_spsc_t) * nthr);
	assert(fifo);

	context = malloc(sizeof(*context) * nthr);
	assert(context);

	thread = malloc(sizeof(pthread_t) * nthr);
	assert(thread);

	for (i = 0; i < nthr; i++) {
		ck_fifo_spsc_entry_t *garbage;

		context[i].tid = i;
		if (i == 0) {
			context[i].previous = nthr - 1;
			context[i].next = i + 1;
		} else if (i == nthr - 1) {
			context[i].next = 0;
			context[i].previous = i - 1;
		} else {
			context[i].next = i + 1;
			context[i].previous = i - 1;
		}

		ck_fifo_spsc_init(fifo + i, malloc(sizeof(ck_fifo_spsc_entry_t)));
		ck_fifo_spsc_deinit(fifo + i, &garbage);
		if (garbage == NULL)
			ck_error("ERROR: Expected non-NULL stub node on deinit.\n");

		free(garbage);
		ck_fifo_spsc_init(fifo + i, malloc(sizeof(ck_fifo_spsc_entry_t)));
		r = pthread_create(thread + i, NULL, test, context + i);
		assert(r == 0);
	}

	for (i = 0; i < nthr; i++)
		pthread_join(thread[i], NULL);

	return (0);
}


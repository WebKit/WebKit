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
#include <ck_hp.h>
#include <ck_stack.h>
#include <ck_hp_stack.h>

#include "../../common.h"

static unsigned int threshold;
static unsigned int n_threads;
static unsigned int barrier;
static unsigned int e_barrier;

#ifndef PAIRS
#define PAIRS 5000000
#endif

struct node {
	unsigned int value;
	ck_hp_hazard_t hazard;
	ck_stack_entry_t stack_entry;
};
static ck_stack_t stack = {NULL, NULL};
static ck_hp_t stack_hp;
CK_STACK_CONTAINER(struct node, stack_entry, stack_container)
static struct affinity a;

static void *
thread(void *unused CK_CC_UNUSED)
{
	struct node **entry, *e;
	unsigned int i;
	ck_hp_record_t record;
	void **pointers;
	ck_stack_entry_t *s;

	unused = NULL;
	pointers = malloc(sizeof(void *));
	ck_hp_register(&stack_hp, &record, pointers);

	if (aff_iterate(&a)) {
		perror("ERROR: failed to affine thread");
		exit(EXIT_FAILURE);
	}

	entry = malloc(sizeof(struct node *) * PAIRS);
	if (entry == NULL) {
		ck_error("Failed allocation.\n");
	}

	for (i = 0; i < PAIRS; i++) {
		entry[i] = malloc(sizeof(struct node));
		if (entry == NULL) {
			ck_error("Failed individual allocation\n");
		}
	}

	ck_pr_inc_uint(&barrier);
	while (ck_pr_load_uint(&barrier) < n_threads)
		ck_pr_stall();

	for (i = 0; i < PAIRS; i++) {
		ck_hp_stack_push_mpmc(&stack, &entry[i]->stack_entry);
		s = ck_hp_stack_pop_mpmc(&record, &stack);
		e = stack_container(s);
		ck_hp_free(&record, &e->hazard, e, s);
	}

	ck_pr_inc_uint(&e_barrier);
	while (ck_pr_load_uint(&e_barrier) < n_threads)
		ck_pr_stall();

        fprintf(stderr, "Peak: %u (%2.2f%%)\nReclamations: %" PRIu64 "\n\n",
                record.n_peak,
		(double)record.n_peak / PAIRS * 100,
		record.n_reclamations);

	ck_hp_clear(&record);
	ck_hp_purge(&record);

        ck_pr_inc_uint(&e_barrier);
        while (ck_pr_load_uint(&e_barrier) < (n_threads << 1));

        if (record.n_pending != 0) {
                ck_error("ERROR: %u pending, expecting none.\n",
                                record.n_pending);
        }

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
	unsigned int i;
	pthread_t *threads;

	if (argc != 4) {
		ck_error("Usage: stack <threads> <threshold> <delta>\n");
	}

	n_threads = atoi(argv[1]);
	threshold = atoi(argv[2]);
	a.delta = atoi(argv[3]);
	a.request = 0;

	threads = malloc(sizeof(pthread_t) * n_threads);

	ck_hp_init(&stack_hp, 1, threshold, destructor);

	for (i = 0; i < n_threads; i++)
		pthread_create(threads + i, NULL, thread, NULL);

	for (i = 0; i < n_threads; i++)
		pthread_join(threads[i], NULL);

	return (0);
}

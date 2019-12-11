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

/*
 * This is a unit test similar to the implementation in John Dybnis's nbds
 * test.
 */

#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <sys/time.h>

#include <ck_pr.h>

#include <ck_backoff.h>
#include <ck_cc.h>
#include <ck_pr.h>
#include <stdbool.h>
#include <stddef.h>
#include <ck_hp.h>

#include "../../common.h"

#define STACK_CONTAINER(T, M, N) CK_CC_CONTAINER(stack_entry_t, T, M, N)

struct stack_entry {
	struct stack_entry *next;
} CK_CC_ALIGN(8);
typedef struct stack_entry stack_entry_t;

struct stack {
	struct stack_entry *head;
	char *generation;
} CK_CC_PACKED CK_CC_ALIGN(16);
typedef struct stack hp_stack_t;

static unsigned int threshold;
static unsigned int n_threads;
static unsigned int barrier;
static unsigned int e_barrier;
static unsigned int global_tid;
static unsigned int pops;
static unsigned int pushs;

#ifndef PAIRS
#define PAIRS 1000000
#endif

struct node {
	unsigned int value;
	ck_hp_hazard_t hazard;
	stack_entry_t stack_entry;
};
hp_stack_t stack = {NULL, NULL};
ck_hp_t stack_hp;

STACK_CONTAINER(struct node, stack_entry, stack_container)
static struct affinity a;

/*
 * Stack producer operation safe for multiple unique producers and multiple consumers.
 */
CK_CC_INLINE static void
stack_push_mpmc(struct stack *target, struct stack_entry *entry)
{
	struct stack_entry *lstack;
	ck_backoff_t backoff = CK_BACKOFF_INITIALIZER;

	lstack = ck_pr_load_ptr(&target->head);
	ck_pr_store_ptr(&entry->next, lstack);
	ck_pr_fence_store();

	while (ck_pr_cas_ptr_value(&target->head, lstack, entry, &lstack) == false) {
		ck_pr_store_ptr(&entry->next, lstack);
		ck_pr_fence_store();
		ck_backoff_eb(&backoff);
	}

	return;
}

/*
 * Stack consumer operation safe for multiple unique producers and multiple consumers.
 */
CK_CC_INLINE static struct stack_entry *
stack_pop_mpmc(ck_hp_record_t *record, struct stack *target)
{
	struct stack_entry *entry;
	ck_backoff_t backoff = CK_BACKOFF_INITIALIZER;

	do {
		entry = ck_pr_load_ptr(&target->head);
		if (entry == NULL)
			return (NULL);

		ck_hp_set_fence(record, 0, entry);
	} while (entry != ck_pr_load_ptr(&target->head));

	while (ck_pr_cas_ptr_value(&target->head, entry, entry->next, &entry) == false) {
		if (ck_pr_load_ptr(&entry) == NULL)
			break;

		ck_hp_set_fence(record, 0, entry);
		if (entry != ck_pr_load_ptr(&target->head))
			continue;

		ck_backoff_eb(&backoff);
	}

	return (entry);
}

static void *
thread(void *unused CK_CC_UNUSED)
{
	struct node *entry, *e;
	unsigned int i;
	ck_hp_record_t record;
	void **pointers;
	stack_entry_t *s;
	unsigned int tid = ck_pr_faa_uint(&global_tid, 1) + 1;
	unsigned int r = (unsigned int)(tid + 1) * 0x5bd1e995;

	unused = NULL;
	pointers = malloc(sizeof(void *));
	ck_hp_register(&stack_hp, &record, pointers);

	if (aff_iterate(&a)) {
		perror("ERROR: failed to affine thread");
		exit(EXIT_FAILURE);
	}

	ck_pr_inc_uint(&barrier);
	while (ck_pr_load_uint(&barrier) < n_threads)
		ck_pr_stall();

	for (i = 0; i < PAIRS; i++) {
		r ^= r << 6; r ^= r >> 21; r ^= r << 7;

		if (r & 0x1000) {
			entry = malloc(sizeof(struct node));
			assert(entry);
			stack_push_mpmc(&stack, &entry->stack_entry);
			ck_pr_inc_uint(&pushs);
		} else {
			s = stack_pop_mpmc(&record, &stack);
			if (s == NULL)
				continue;

			e = stack_container(s);
			ck_hp_free(&record, &e->hazard, e, s);
			ck_pr_inc_uint(&pops);
		}
	}

	ck_pr_inc_uint(&e_barrier);
	while (ck_pr_load_uint(&e_barrier) < n_threads);

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

	fprintf(stderr, "Push: %u\nPop: %u\n", pushs, pops);
	return (0);
}

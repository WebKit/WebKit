/*
 * Copyright 2009-2015 Samy Al Bahra.
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
#include <ck_pr.h>
#ifdef SPINLOCK
#include <ck_spinlock.h>
#endif
#include <ck_stack.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <sys/time.h>
#include <unistd.h>

#include "../../common.h"

#ifndef ITEMS
#define ITEMS (5765760 * 2)
#endif

#define TVTOD(tv) ((tv).tv_sec+((tv).tv_usec / (double)1000000))

struct entry {
	int value;
#ifdef SPINLOCK
	struct entry *next;
#else
	ck_stack_entry_t next;
#endif
};

#ifdef SPINLOCK
static struct entry *stack CK_CC_CACHELINE;
#else
static ck_stack_t stack CK_CC_CACHELINE;
#endif

CK_STACK_CONTAINER(struct entry, next, getvalue)

static struct affinity affinerator = AFFINITY_INITIALIZER;
static unsigned long long nthr;
static volatile unsigned int barrier = 0;
static unsigned int critical;

#if defined(SPINLOCK)
ck_spinlock_fas_t stack_spinlock = CK_SPINLOCK_FAS_INITIALIZER;
#define UNLOCK ck_spinlock_fas_unlock
#if defined(EB)
#define LOCK ck_spinlock_fas_lock_eb
#else
#define LOCK ck_spinlock_fas_lock
#endif
#elif defined(PTHREAD)
pthread_mutex_t stack_spinlock = PTHREAD_MUTEX_INITIALIZER;
#define LOCK pthread_mutex_lock
#define UNLOCK pthread_mutex_unlock
#endif

static void *
stack_thread(void *buffer)
{
	struct entry *bucket = buffer;
	unsigned long long i, n = ITEMS / nthr;
	unsigned int seed;
	int j;

	if (aff_iterate(&affinerator)) {
		perror("ERROR: failed to affine thread");
		exit(EXIT_FAILURE);
	}

	while (barrier == 0);

	for (i = 0; i < n; i++) {
		bucket[i].value = (i + 1) * 2;

#if   defined(MPNC)
		ck_stack_push_mpnc(&stack, &bucket[i].next);
#elif defined(MPMC)
		ck_stack_push_mpmc(&stack, &bucket[i].next);
#elif defined(TRYMPMC)
		while (ck_stack_trypush_mpmc(&stack, &bucket[i].next) == false)
			ck_pr_stall();
#elif defined(TRYUPMC)
		while (ck_stack_trypush_upmc(&stack, &bucket[i].next) == false)
			ck_pr_stall();
#elif defined(UPMC)
		ck_stack_push_upmc(&stack, &bucket[i].next);
#elif defined(SPINLOCK) || defined(PTHREADS)
		LOCK(&stack_spinlock);
		bucket[i].next = stack;
		stack = bucket + i;
		UNLOCK(&stack_spinlock);
#else
#		error Undefined operation.
#endif

		if (critical) {
			j = common_rand_r(&seed) % critical;
			while (j--)
				__asm__ __volatile__("" ::: "memory");
		}
	}

	return (NULL);
}

static void
stack_assert(void)
{
#ifndef SPINLOCK
	ck_stack_entry_t *n;
#endif
	struct entry *p;
	unsigned long long c = 0;

#ifdef SPINLOCK
	for (p = stack; p; p = p->next)
		c++;
#else
	CK_STACK_FOREACH(&stack, n) {
		p = getvalue(n);
		(void)((volatile struct entry *)p)->value;
		c++;
	}
#endif

	assert(c == ITEMS);
	return;
}

int
main(int argc, char *argv[])
{
	struct entry *bucket;
	unsigned long long i, d, n;
	pthread_t *thread;
	struct timeval stv, etv;

	if (argc != 4) {
		ck_error("Usage: stack <threads> <delta> <critical>\n");
	}

	{
		char *e;

		nthr = strtol(argv[1], &e, 10);
		if (errno == ERANGE) {
			perror("ERROR: too many threads");
			exit(EXIT_FAILURE);
		} else if (*e != '\0') {
			ck_error("ERROR: input format is incorrect\n");
		}

		d = strtol(argv[2], &e, 10);
		if (errno == ERANGE) {
			perror("ERROR: delta is too large");
			exit(EXIT_FAILURE);
		} else if (*e != '\0') {
			ck_error("ERROR: input format is incorrect\n");
		}

		critical = strtoul(argv[3], &e, 10);
		if (errno == ERANGE) {
			perror("ERROR: critical section is too large");
			exit(EXIT_FAILURE);
		} else if (*e != '\0') {
			ck_error("ERROR: input format is incorrect\n");
		}
	}

	srand(getpid());

	affinerator.request = 0;
	affinerator.delta = d;
	n = ITEMS / nthr;

#ifndef SPINLOCK
	ck_stack_init(&stack);
#else
	stack = NULL;
#endif

	bucket = malloc(sizeof(struct entry) * ITEMS);
	assert(bucket != NULL);

	thread = malloc(sizeof(pthread_t) * nthr);
	assert(thread != NULL);

	for (i = 0; i < nthr; i++)
		pthread_create(&thread[i], NULL, stack_thread, bucket + i * n);

	barrier = 1;

	for (i = 0; i < nthr; i++)
		pthread_join(thread[i], NULL);

	barrier = 0;

#ifndef SPINLOCK
	ck_stack_init(&stack);
#else
	stack = NULL;
#endif

	for (i = 0; i < nthr; i++)
		pthread_create(&thread[i], NULL, stack_thread, bucket + i * n);

	common_gettimeofday(&stv, NULL);
	barrier = 1;
	for (i = 0; i < nthr; i++)
		pthread_join(thread[i], NULL);
	common_gettimeofday(&etv, NULL);

	stack_assert();
#ifdef _WIN32
	printf("%3llu %.6f\n", nthr, TVTOD(etv) - TVTOD(stv));
#else
	printf("%3llu %.6lf\n", nthr, TVTOD(etv) - TVTOD(stv));
#endif
	return 0;
}

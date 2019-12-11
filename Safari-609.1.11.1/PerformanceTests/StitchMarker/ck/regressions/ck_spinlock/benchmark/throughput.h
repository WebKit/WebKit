/*
 * Copyright 2008-2012 Samy Al Bahra.
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
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <sys/time.h>

#include <ck_pr.h>
#include <ck_spinlock.h>

#include "../../common.h"

/* 8! = 40320, evenly divide 1 .. 8 processor workload. */
#define WORKLOAD (40320 * 2056)

#ifndef ITERATE
#define ITERATE 65536
#endif

struct block {
	unsigned int tid;
};

static struct affinity a;
static unsigned int ready;

struct counters {
	uint64_t value;
} CK_CC_CACHELINE;

static struct counters *count;
static uint64_t nthr;
static unsigned int barrier;

int critical __attribute__((aligned(64)));

LOCK_DEFINE;

CK_CC_USED static void
gen_lock(void)
{
	CK_CC_UNUSED int core = 0;
#ifdef LOCK_STATE
	LOCK_STATE;
#endif

#ifdef LOCK
	LOCK;
#endif
}

CK_CC_USED static void
gen_unlock(void)
{
#ifdef LOCK_STATE
	LOCK_STATE;
#endif

#ifdef UNLOCK
	UNLOCK;
#endif
}

static void *
fairness(void *null)
{
#ifdef LOCK_STATE
	LOCK_STATE;
#endif
	struct block *context = null;
	unsigned int i = context->tid;
	volatile int j;
	long int base;
	unsigned int core;

        if (aff_iterate_core(&a, &core)) {
                perror("ERROR: Could not affine thread");
                exit(EXIT_FAILURE);
        }

	while (ck_pr_load_uint(&ready) == 0);

	ck_pr_inc_uint(&barrier);
	while (ck_pr_load_uint(&barrier) != nthr);

	while (ready) {
		LOCK;

		count[i].value++;
		if (critical) {
			base = common_lrand48() % critical;
			for (j = 0; j < base; j++);
		}

		UNLOCK;
	}

	return (NULL);
}

int
main(int argc, char *argv[])
{
	uint64_t v, d;
	unsigned int i;
	pthread_t *threads;
	struct block *context;

	if (argc != 4) {
		ck_error("Usage: " LOCK_NAME " <number of threads> <affinity delta> <critical section>\n");
		exit(EXIT_FAILURE);
	}

	nthr = atoi(argv[1]);
	if (nthr <= 0) {
		ck_error("ERROR: Number of threads must be greater than 0\n");
		exit(EXIT_FAILURE);
	}

#ifdef LOCK_INIT
	LOCK_INIT;
#endif

	critical = atoi(argv[3]);
	if (critical < 0) {
		ck_error("ERROR: critical section cannot be negative\n");
		exit(EXIT_FAILURE);
	}

	threads = malloc(sizeof(pthread_t) * nthr);
	if (threads == NULL) {
		ck_error("ERROR: Could not allocate thread structures\n");
		exit(EXIT_FAILURE);
	}

	context = malloc(sizeof(struct block) * nthr);
	if (context == NULL) {
		ck_error("ERROR: Could not allocate thread contexts\n");
		exit(EXIT_FAILURE);
	}

	a.delta = atoi(argv[2]);
	a.request = 0;

	count = malloc(sizeof(*count) * nthr);
	if (count == NULL) {
		ck_error("ERROR: Could not create acquisition buffer\n");
		exit(EXIT_FAILURE);
	}
	memset(count, 0, sizeof(*count) * nthr);

	fprintf(stderr, "Creating threads (fairness)...");
	for (i = 0; i < nthr; i++) {
		context[i].tid = i;
		if (pthread_create(&threads[i], NULL, fairness, context + i)) {
			ck_error("ERROR: Could not create thread %d\n", i);
			exit(EXIT_FAILURE);
		}
	}
	fprintf(stderr, "done\n");

	ck_pr_store_uint(&ready, 1);
	common_sleep(10);
	ck_pr_store_uint(&ready, 0);

	fprintf(stderr, "Waiting for threads to finish acquisition regression...");
	for (i = 0; i < nthr; i++)
		pthread_join(threads[i], NULL);
	fprintf(stderr, "done\n\n");

	for (i = 0, v = 0; i < nthr; i++) {
		printf("%d %15" PRIu64 "\n", i, count[i].value);
		v += count[i].value;
	}

	printf("\n# total       : %15" PRIu64 "\n", v);
	printf("# throughput  : %15" PRIu64 " a/s\n", (v /= nthr) / 10);

	for (i = 0, d = 0; i < nthr; i++)
		d += (count[i].value - v) * (count[i].value - v);

	printf("# average     : %15" PRIu64 "\n", v);
	printf("# deviation   : %.2f (%.2f%%)\n\n", sqrt(d / nthr), (sqrt(d / nthr) / v) * 100.00);

	return (0);
}


/*
 * Copyright 2013-2015 Samy Al Bahra.
 * Copyright 2013 Brendon Scheinman.
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
#include <ck_cohort.h>
#include <ck_md.h>
#include <ck_spinlock.h>

#include "../../common.h"

#define max(x, y) (((x) > (y)) ? (x) : (y))

static struct affinity a;
static unsigned int ready;

struct counters {
	uint64_t value;
} CK_CC_CACHELINE;

static struct counters *count;
static uint64_t nthr;
static unsigned int n_cohorts;
static unsigned int barrier;
static int critical CK_CC_CACHELINE;

static void
ck_spinlock_fas_lock_with_context(ck_spinlock_fas_t *lock, void *context)
{

	(void)context;
	ck_spinlock_fas_lock(lock);
	return;
}

static void
ck_spinlock_fas_unlock_with_context(ck_spinlock_fas_t *lock, void *context)
{

	(void)context;
	ck_spinlock_fas_unlock(lock);
	return;
}

static bool
ck_spinlock_fas_locked_with_context(ck_spinlock_fas_t *lock, void *context)
{

	(void)context;
	return ck_spinlock_fas_locked(lock);
}

CK_COHORT_PROTOTYPE(basic,
    ck_spinlock_fas_lock_with_context, ck_spinlock_fas_unlock_with_context, ck_spinlock_fas_locked_with_context,
    ck_spinlock_fas_lock_with_context, ck_spinlock_fas_unlock_with_context, ck_spinlock_fas_locked_with_context)

struct cohort_record {
	CK_COHORT_INSTANCE(basic) cohort;
} CK_CC_CACHELINE;
static struct cohort_record *cohorts;

static ck_spinlock_t global_lock = CK_SPINLOCK_INITIALIZER;

struct block {
	unsigned int tid;
};

static void *
fairness(void *null)
{
	struct block *context = null;
	unsigned int i = context->tid;
	volatile int j;
	long int base;
	unsigned int core;
	CK_COHORT_INSTANCE(basic) *cohort;


	if (aff_iterate_core(&a, &core)) {
		perror("ERROR: Could not affine thread");
		exit(EXIT_FAILURE);
	}

	cohort = &((cohorts + (core / (int)(a.delta)) % n_cohorts)->cohort);

	while (ck_pr_load_uint(&ready) == 0);

	ck_pr_inc_uint(&barrier);
	while (ck_pr_load_uint(&barrier) != nthr);

	while (ck_pr_load_uint(&ready)) {
		CK_COHORT_LOCK(basic, cohort, NULL, NULL);

		count[i].value++;
		if (critical) {
			base = common_lrand48() % critical;
			for (j = 0; j < base; j++);
		}

		CK_COHORT_UNLOCK(basic, cohort, NULL, NULL);
	}

	return NULL;
}

int
main(int argc, char *argv[])
{
	uint64_t v, d;
	unsigned int i;
	pthread_t *threads;
	struct block *context;
	ck_spinlock_t *local_lock;

	if (argc != 5) {
		ck_error("Usage: ck_cohort <number of cohorts> <threads per cohort> "
			"<affinity delta> <critical section>\n");
	}

	n_cohorts = atoi(argv[1]);
	if (n_cohorts <= 0) {
		ck_error("ERROR: Number of cohorts must be greater than 0\n");
	}

	nthr = n_cohorts * atoi(argv[2]);
	if (nthr <= 0) {
		ck_error("ERROR: Number of threads must be greater than 0\n");
	}

	critical = atoi(argv[4]);
	if (critical < 0) {
		ck_error("ERROR: critical section cannot be negative\n");
	}

	threads = malloc(sizeof(pthread_t) * nthr);
	if (threads == NULL) {
		ck_error("ERROR: Could not allocate thread structures\n");
	}

	cohorts = malloc(sizeof(struct cohort_record) * n_cohorts);
	if (cohorts == NULL) {
		ck_error("ERROR: Could not allocate cohort structures\n");
	}

	context = malloc(sizeof(struct block) * nthr);
	if (context == NULL) {
		ck_error("ERROR: Could not allocate thread contexts\n");
	}

	a.delta = atoi(argv[2]);
	a.request = 0;

	count = malloc(sizeof(*count) * nthr);
	if (count == NULL) {
		ck_error("ERROR: Could not create acquisition buffer\n");
	}
	memset(count, 0, sizeof(*count) * nthr);

	fprintf(stderr, "Creating cohorts...");
	for (i = 0 ; i < n_cohorts ; i++) {
		local_lock = malloc(max(CK_MD_CACHELINE, sizeof(ck_spinlock_t)));
		if (local_lock == NULL) {
			ck_error("ERROR: Could not allocate local lock\n");
		}
		CK_COHORT_INIT(basic, &((cohorts + i)->cohort), &global_lock, local_lock,
		    CK_COHORT_DEFAULT_LOCAL_PASS_LIMIT);
		local_lock = NULL;
	}
	fprintf(stderr, "done\n");

	fprintf(stderr, "Creating threads (fairness)...");
	for (i = 0; i < nthr; i++) {
		context[i].tid = i;
		if (pthread_create(&threads[i], NULL, fairness, context + i)) {
			ck_error("ERROR: Could not create thread %d\n", i);
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

	return 0;
}

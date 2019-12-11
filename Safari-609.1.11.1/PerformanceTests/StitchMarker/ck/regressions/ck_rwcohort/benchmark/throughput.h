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

#include <ck_cohort.h>
#include <ck_rwcohort.h>
#include <ck_spinlock.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../../common.h"

#define max(x, y) (((x) > (y)) ? (x) : (y))

#ifndef STEPS
#define STEPS 1000000
#endif

static unsigned int barrier;
static unsigned int flag CK_CC_CACHELINE;
static struct affinity affinity;
static unsigned int nthr;

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

CK_COHORT_PROTOTYPE(fas_fas,
    ck_spinlock_fas_lock_with_context, ck_spinlock_fas_unlock_with_context, ck_spinlock_fas_locked_with_context,
    ck_spinlock_fas_lock_with_context, ck_spinlock_fas_unlock_with_context, ck_spinlock_fas_locked_with_context)
LOCK_PROTOTYPE(fas_fas)

struct cohort_record {
	CK_COHORT_INSTANCE(fas_fas) cohort;
} CK_CC_CACHELINE;
static struct cohort_record *cohorts;

static ck_spinlock_t global_lock = CK_SPINLOCK_INITIALIZER;
static LOCK_INSTANCE(fas_fas) rw_cohort = LOCK_INITIALIZER;
static unsigned int n_cohorts;

struct block {
	unsigned int tid;
};

static void *
thread_rwlock(void *pun)
{
	uint64_t s_b, e_b, a, i;
	uint64_t *value = pun;
	CK_COHORT_INSTANCE(fas_fas) *cohort;
	unsigned int core;

	if (aff_iterate_core(&affinity, &core) != 0) {
		perror("ERROR: Could not affine thread");
		exit(EXIT_FAILURE);
	}

	cohort = &((cohorts + (core / (int)(affinity.delta)) % n_cohorts)->cohort);

	ck_pr_inc_uint(&barrier);
	while (ck_pr_load_uint(&barrier) != nthr)
		ck_pr_stall();

	for (i = 1, a = 0;; i++) {
		s_b = rdtsc();
		READ_LOCK(fas_fas, &rw_cohort, cohort, NULL, NULL);
		READ_UNLOCK(fas_fas, &rw_cohort, cohort, NULL, NULL);
		READ_LOCK(fas_fas, &rw_cohort, cohort, NULL, NULL);
		READ_UNLOCK(fas_fas, &rw_cohort, cohort, NULL, NULL);
		READ_LOCK(fas_fas, &rw_cohort, cohort, NULL, NULL);
		READ_UNLOCK(fas_fas, &rw_cohort, cohort, NULL, NULL);
		READ_LOCK(fas_fas, &rw_cohort, cohort, NULL, NULL);
		READ_UNLOCK(fas_fas, &rw_cohort, cohort, NULL, NULL);
		READ_LOCK(fas_fas, &rw_cohort, cohort, NULL, NULL);
		READ_UNLOCK(fas_fas, &rw_cohort, cohort, NULL, NULL);
		READ_LOCK(fas_fas, &rw_cohort, cohort, NULL, NULL);
		READ_UNLOCK(fas_fas, &rw_cohort, cohort, NULL, NULL);
		READ_LOCK(fas_fas, &rw_cohort, cohort, NULL, NULL);
		READ_UNLOCK(fas_fas, &rw_cohort, cohort, NULL, NULL);
		READ_LOCK(fas_fas, &rw_cohort, cohort, NULL, NULL);
		READ_UNLOCK(fas_fas, &rw_cohort, cohort, NULL, NULL);
		READ_LOCK(fas_fas, &rw_cohort, cohort, NULL, NULL);
		READ_UNLOCK(fas_fas, &rw_cohort, cohort, NULL, NULL);
		READ_LOCK(fas_fas, &rw_cohort, cohort, NULL, NULL);
		READ_UNLOCK(fas_fas, &rw_cohort, cohort, NULL, NULL);
		READ_LOCK(fas_fas, &rw_cohort, cohort, NULL, NULL);
		READ_UNLOCK(fas_fas, &rw_cohort, cohort, NULL, NULL);
		READ_LOCK(fas_fas, &rw_cohort, cohort, NULL, NULL);
		READ_UNLOCK(fas_fas, &rw_cohort, cohort, NULL, NULL);
		READ_LOCK(fas_fas, &rw_cohort, cohort, NULL, NULL);
		READ_UNLOCK(fas_fas, &rw_cohort, cohort, NULL, NULL);
		READ_LOCK(fas_fas, &rw_cohort, cohort, NULL, NULL);
		READ_UNLOCK(fas_fas, &rw_cohort, cohort, NULL, NULL);
		READ_LOCK(fas_fas, &rw_cohort, cohort, NULL, NULL);
		READ_UNLOCK(fas_fas, &rw_cohort, cohort, NULL, NULL);
		READ_LOCK(fas_fas, &rw_cohort, cohort, NULL, NULL);
		READ_UNLOCK(fas_fas, &rw_cohort, cohort, NULL, NULL);
		e_b = rdtsc();

		a += (e_b - s_b) >> 4;

		if (ck_pr_load_uint(&flag) == 1)
			break;
	}

	ck_pr_inc_uint(&barrier);
	while (ck_pr_load_uint(&barrier) != nthr * 2)
		ck_pr_stall();

	*value = (a / i);
	return NULL;
}

int
main(int argc, char *argv[])
{
	unsigned int i;
	pthread_t *threads;
	uint64_t *latency;
	struct block *context;
	ck_spinlock_fas_t *local_lock;

	if (argc != 4) {
		ck_error("Usage: throughput <number of cohorts> <threads per cohort> <affinity delta>\n");
	}

	n_cohorts = atoi(argv[1]);
	if (n_cohorts <= 0) {
		ck_error("ERROR: Number of cohorts must be greater than 0\n");
	}

	nthr = n_cohorts * atoi(argv[2]);
	if (nthr <= 0) {
		ck_error("ERROR: Number of threads must be greater than 0\n");
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

	affinity.delta = atoi(argv[3]);
	affinity.request = 0;

	latency = malloc(sizeof(*latency) * nthr);
	if (latency == NULL) {
		ck_error("ERROR: Could not create latency buffer\n");
	}
	memset(latency, 0, sizeof(*latency) * nthr);

	fprintf(stderr, "Creating cohorts...");
	for (i = 0 ; i < n_cohorts ; i++) {
		local_lock = malloc(max(CK_MD_CACHELINE, sizeof(ck_spinlock_fas_t)));
		if (local_lock == NULL) {
			ck_error("ERROR: Could not allocate local lock\n");
		}
		CK_COHORT_INIT(fas_fas, &((cohorts + i)->cohort), &global_lock, local_lock,
		    CK_COHORT_DEFAULT_LOCAL_PASS_LIMIT);
		local_lock = NULL;
	}
	fprintf(stderr, "done\n");

	fprintf(stderr, "Creating threads (rwlock)...");
	for (i = 0; i < nthr; i++) {
		if (pthread_create(&threads[i], NULL, thread_rwlock, latency + i) != 0) {
			ck_error("ERROR: Could not create thread %d\n", i);
		}
	}
	fprintf(stderr, "done\n");

	common_sleep(10);
	ck_pr_store_uint(&flag, 1);

	fprintf(stderr, "Waiting for threads to finish acquisition regression...");
	for (i = 0; i < nthr; i++)
		pthread_join(threads[i], NULL);
	fprintf(stderr, "done\n\n");

	for (i = 1; i <= nthr; i++)
		printf("%10u %20" PRIu64 "\n", i, latency[i - 1]);

	return (0);
}


/*
 * Copyright 2013-2015 Samy Al Bahra.
 * Copything 2013 Brendon Scheinman.
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
#include <strings.h>
#include <unistd.h>
#include <sys/time.h>

#include <ck_pr.h>
#include <ck_rwcohort.h>
#include <ck_spinlock.h>

#include "../../common.h"

#ifndef ITERATE
#define ITERATE 1000000
#endif


static struct affinity a;
static unsigned int locked;
static int nthr;
static ck_spinlock_fas_t global_fas_lock = CK_SPINLOCK_FAS_INITIALIZER;

static void
ck_spinlock_fas_lock_with_context(ck_spinlock_fas_t *lock, void *context)
{
	(void)context;
	ck_spinlock_fas_lock(lock);
}

static void
ck_spinlock_fas_unlock_with_context(ck_spinlock_fas_t *lock, void *context)
{
	(void)context;
	ck_spinlock_fas_unlock(lock);
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

static CK_COHORT_INSTANCE(fas_fas) *cohorts;
static LOCK_INSTANCE(fas_fas) rw_cohort = LOCK_INITIALIZER;
static int n_cohorts;

static void *
thread(void *null CK_CC_UNUSED)
{
        int i = ITERATE;
	unsigned int l;
	unsigned int core;
	CK_COHORT_INSTANCE(fas_fas) *cohort;

	if (aff_iterate_core(&a, &core)) {
		perror("ERROR: Could not affine thread");
		exit(EXIT_FAILURE);
	}

	cohort = cohorts + (core / (int)(a.delta)) % n_cohorts;

	while (i--) {
                WRITE_LOCK(fas_fas, &rw_cohort, cohort, NULL, NULL);
		{
			l = ck_pr_load_uint(&locked);
			if (l != 0) {
				ck_error("ERROR [WR:%d]: %u != 0\n", __LINE__, l);
			}

			ck_pr_inc_uint(&locked);
			ck_pr_inc_uint(&locked);
			ck_pr_inc_uint(&locked);
			ck_pr_inc_uint(&locked);
			ck_pr_inc_uint(&locked);
			ck_pr_inc_uint(&locked);
			ck_pr_inc_uint(&locked);
			ck_pr_inc_uint(&locked);

			l = ck_pr_load_uint(&locked);
			if (l != 8) {
				ck_error("ERROR [WR:%d]: %u != 2\n", __LINE__, l);
			}

			ck_pr_dec_uint(&locked);
			ck_pr_dec_uint(&locked);
			ck_pr_dec_uint(&locked);
			ck_pr_dec_uint(&locked);
			ck_pr_dec_uint(&locked);
			ck_pr_dec_uint(&locked);
			ck_pr_dec_uint(&locked);
			ck_pr_dec_uint(&locked);

			l = ck_pr_load_uint(&locked);
			if (l != 0) {
				ck_error("ERROR [WR:%d]: %u != 0\n", __LINE__, l);
			}
		}
		WRITE_UNLOCK(fas_fas, &rw_cohort, cohort, NULL, NULL);

		READ_LOCK(fas_fas, &rw_cohort, cohort, NULL, NULL);
		{
			l = ck_pr_load_uint(&locked);
			if (l != 0) {
				ck_error("ERROR [RD:%d]: %u != 0\n", __LINE__, l);
			}
		}
		READ_UNLOCK(fas_fas, &rw_cohort, cohort, NULL, NULL);
	}

	return (NULL);
}

int
main(int argc, char *argv[])
{
	pthread_t *threads;
	int threads_per_cohort;
	ck_spinlock_fas_t *local_lock;
	int i;

	if (argc != 4) {
		ck_error("Usage: validate <number of cohorts> <threads per cohort> <affinity delta>\n");
	}

	n_cohorts = atoi(argv[1]);
	if (n_cohorts <= 0) {
		ck_error("ERROR: Number of cohorts must be greater than 0\n");
	}

	threads_per_cohort = atoi(argv[2]);
	if (threads_per_cohort <= 0) {
		ck_error("ERROR: Threads per cohort must be greater than 0\n");
	}

	nthr = n_cohorts * threads_per_cohort;

	threads = malloc(sizeof(pthread_t) * nthr);
	if (threads == NULL) {
		ck_error("ERROR: Could not allocate thread structures\n");
	}

	a.delta = atoi(argv[3]);

	fprintf(stderr, "Creating cohorts...");
	cohorts = malloc(sizeof(CK_COHORT_INSTANCE(fas_fas)) * n_cohorts);
	if (cohorts == NULL) {
		ck_error("ERROR: Could not allocate base cohort structures\n");
	}
	for (i = 0 ; i < n_cohorts ; i++) {
		local_lock = malloc(sizeof(ck_spinlock_fas_t));
		CK_COHORT_INIT(fas_fas, cohorts + i, &global_fas_lock, local_lock,
		    CK_COHORT_DEFAULT_LOCAL_PASS_LIMIT);
	}
	fprintf(stderr, "done\n");

	fprintf(stderr, "Creating threads...");
	for (i = 0; i < nthr; i++) {
		if (pthread_create(&threads[i], NULL, thread, NULL)) {
			ck_error("ERROR: Could not create thread %d\n", i);
		}
	}
	fprintf(stderr, "done\n");

	fprintf(stderr, "Waiting for threads to finish correctness regression...");
	for (i = 0; i < nthr; i++)
		pthread_join(threads[i], NULL);
	fprintf(stderr, "done (passed)\n");

	return (0);
}


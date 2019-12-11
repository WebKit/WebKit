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

#include <ck_rwlock.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#include "../../common.h"

#ifndef STEPS
#define STEPS 1000000
#endif

static int barrier;
static int threads;
static unsigned int flag CK_CC_CACHELINE;
static struct {
	ck_rwlock_t lock;
} rw CK_CC_CACHELINE = {
	.lock = CK_RWLOCK_INITIALIZER
};

static struct affinity affinity;

#ifdef CK_F_PR_RTM
static void *
thread_lock_rtm(void *pun)
{
	uint64_t s_b, e_b, a, i;
	uint64_t *value = pun;

	if (aff_iterate(&affinity) != 0) {
		perror("ERROR: Could not affine thread");
		exit(EXIT_FAILURE);
	}

	ck_pr_inc_int(&barrier);
	while (ck_pr_load_int(&barrier) != threads)
		ck_pr_stall();

	for (i = 1, a = 0;; i++) {
		s_b = rdtsc();
		CK_ELIDE_LOCK(ck_rwlock_read, &rw.lock);
		CK_ELIDE_UNLOCK(ck_rwlock_read, &rw.lock);
		CK_ELIDE_LOCK(ck_rwlock_read, &rw.lock);
		CK_ELIDE_UNLOCK(ck_rwlock_read, &rw.lock);
		CK_ELIDE_LOCK(ck_rwlock_read, &rw.lock);
		CK_ELIDE_UNLOCK(ck_rwlock_read, &rw.lock);
		CK_ELIDE_LOCK(ck_rwlock_read, &rw.lock);
		CK_ELIDE_UNLOCK(ck_rwlock_read, &rw.lock);
		CK_ELIDE_LOCK(ck_rwlock_read, &rw.lock);
		CK_ELIDE_UNLOCK(ck_rwlock_read, &rw.lock);
		CK_ELIDE_LOCK(ck_rwlock_read, &rw.lock);
		CK_ELIDE_UNLOCK(ck_rwlock_read, &rw.lock);
		CK_ELIDE_LOCK(ck_rwlock_read, &rw.lock);
		CK_ELIDE_UNLOCK(ck_rwlock_read, &rw.lock);
		CK_ELIDE_LOCK(ck_rwlock_read, &rw.lock);
		CK_ELIDE_UNLOCK(ck_rwlock_read, &rw.lock);
		CK_ELIDE_LOCK(ck_rwlock_read, &rw.lock);
		CK_ELIDE_UNLOCK(ck_rwlock_read, &rw.lock);
		CK_ELIDE_LOCK(ck_rwlock_read, &rw.lock);
		CK_ELIDE_UNLOCK(ck_rwlock_read, &rw.lock);
		CK_ELIDE_LOCK(ck_rwlock_read, &rw.lock);
		CK_ELIDE_UNLOCK(ck_rwlock_read, &rw.lock);
		CK_ELIDE_LOCK(ck_rwlock_read, &rw.lock);
		CK_ELIDE_UNLOCK(ck_rwlock_read, &rw.lock);
		CK_ELIDE_LOCK(ck_rwlock_read, &rw.lock);
		CK_ELIDE_UNLOCK(ck_rwlock_read, &rw.lock);
		CK_ELIDE_LOCK(ck_rwlock_read, &rw.lock);
		CK_ELIDE_UNLOCK(ck_rwlock_read, &rw.lock);
		CK_ELIDE_LOCK(ck_rwlock_read, &rw.lock);
		CK_ELIDE_UNLOCK(ck_rwlock_read, &rw.lock);
		CK_ELIDE_LOCK(ck_rwlock_read, &rw.lock);
		CK_ELIDE_UNLOCK(ck_rwlock_read, &rw.lock);
		e_b = rdtsc();

		a += (e_b - s_b) >> 4;

		if (ck_pr_load_uint(&flag) == 1)
			break;
	}

	ck_pr_inc_int(&barrier);
	while (ck_pr_load_int(&barrier) != threads * 2)
		ck_pr_stall();

	*value = (a / i);
	return NULL;
}
#endif /* CK_F_PR_RTM */

static void *
thread_lock(void *pun)
{
	uint64_t s_b, e_b, a, i;
	uint64_t *value = pun;

	if (aff_iterate(&affinity) != 0) {
		perror("ERROR: Could not affine thread");
		exit(EXIT_FAILURE);
	}

	ck_pr_inc_int(&barrier);
	while (ck_pr_load_int(&barrier) != threads)
		ck_pr_stall();

	for (i = 1, a = 0;; i++) {
		s_b = rdtsc();
		ck_rwlock_read_lock(&rw.lock);
		ck_rwlock_read_unlock(&rw.lock);
		ck_rwlock_read_lock(&rw.lock);
		ck_rwlock_read_unlock(&rw.lock);
		ck_rwlock_read_lock(&rw.lock);
		ck_rwlock_read_unlock(&rw.lock);
		ck_rwlock_read_lock(&rw.lock);
		ck_rwlock_read_unlock(&rw.lock);
		ck_rwlock_read_lock(&rw.lock);
		ck_rwlock_read_unlock(&rw.lock);
		ck_rwlock_read_lock(&rw.lock);
		ck_rwlock_read_unlock(&rw.lock);
		ck_rwlock_read_lock(&rw.lock);
		ck_rwlock_read_unlock(&rw.lock);
		ck_rwlock_read_lock(&rw.lock);
		ck_rwlock_read_unlock(&rw.lock);
		ck_rwlock_read_lock(&rw.lock);
		ck_rwlock_read_unlock(&rw.lock);
		ck_rwlock_read_lock(&rw.lock);
		ck_rwlock_read_unlock(&rw.lock);
		ck_rwlock_read_lock(&rw.lock);
		ck_rwlock_read_unlock(&rw.lock);
		ck_rwlock_read_lock(&rw.lock);
		ck_rwlock_read_unlock(&rw.lock);
		ck_rwlock_read_lock(&rw.lock);
		ck_rwlock_read_unlock(&rw.lock);
		ck_rwlock_read_lock(&rw.lock);
		ck_rwlock_read_unlock(&rw.lock);
		ck_rwlock_read_lock(&rw.lock);
		ck_rwlock_read_unlock(&rw.lock);
		ck_rwlock_read_lock(&rw.lock);
		ck_rwlock_read_unlock(&rw.lock);
		e_b = rdtsc();

		a += (e_b - s_b) >> 4;

		if (ck_pr_load_uint(&flag) == 1)
			break;
	}

	ck_pr_inc_int(&barrier);
	while (ck_pr_load_int(&barrier) != threads * 2)
		ck_pr_stall();

	*value = (a / i);
	return NULL;
}

static void
rwlock_test(pthread_t *p, int d, uint64_t *latency, void *(*f)(void *), const char *label)
{
	int t;

	ck_pr_store_int(&barrier, 0);
	ck_pr_store_uint(&flag, 0);

	affinity.delta = d;
	affinity.request = 0;

	fprintf(stderr, "Creating threads (%s)...", label);
	for (t = 0; t < threads; t++) {
		if (pthread_create(&p[t], NULL, f, latency + t) != 0) {
			ck_error("ERROR: Could not create thread %d\n", t);
		}
	}
	fprintf(stderr, "done\n");

	common_sleep(10);
	ck_pr_store_uint(&flag, 1);

	fprintf(stderr, "Waiting for threads to finish acquisition regression...");
	for (t = 0; t < threads; t++)
		pthread_join(p[t], NULL);
	fprintf(stderr, "done\n\n");

	for (t = 1; t <= threads; t++)
		printf("%10u %20" PRIu64 "\n", t, latency[t - 1]);

	fprintf(stderr, "\n");
	return;
}


int
main(int argc, char *argv[])
{
	int d;
	pthread_t *p;
	uint64_t *latency;

	if (argc != 3) {
		ck_error("Usage: throughput <delta> <threads>\n");
	}

	threads = atoi(argv[2]);
	if (threads <= 0) {
		ck_error("ERROR: Threads must be a value > 0.\n");
	}

	p = malloc(sizeof(pthread_t) * threads);
	if (p == NULL) {
		ck_error("ERROR: Failed to initialize thread.\n");
	}

	latency = malloc(sizeof(uint64_t) * threads);
	if (latency == NULL) {
		ck_error("ERROR: Failed to create latency buffer.\n");
	}

	d = atoi(argv[1]);
	rwlock_test(p, d, latency, thread_lock, "rwlock");

#ifdef CK_F_PR_RTM
	rwlock_test(p, d, latency, thread_lock_rtm, "rwlock, rtm");
#endif /* CK_F_PR_RTM */

	return 0;
}


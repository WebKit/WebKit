/*
 * Copyright 2011-2015 Samy Al Bahra.
 * Copyright 2011 David Joseph.
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
#include <ck_barrier.h>

#include "../../common.h"

#ifndef ITERATE
#define ITERATE 5000000
#endif

#ifndef ENTRIES
#define ENTRIES 512
#endif

static struct affinity a;
static int nthr;
static int counters[ENTRIES];
static int barrier_wait;

static void *
thread(void *b)
{
	ck_barrier_dissemination_t *barrier = b;
	ck_barrier_dissemination_state_t state;
	int j, k, counter;
	int i = 0;

	aff_iterate(&a);
	ck_barrier_dissemination_subscribe(barrier, &state);

	ck_pr_inc_int(&barrier_wait);
	while (ck_pr_load_int(&barrier_wait) != nthr)
		ck_pr_stall();

	for (j = 0, k = 0; j < ITERATE; j++, k++) {
		i = j++ & (ENTRIES - 1);
		ck_pr_inc_int(&counters[i]);
		ck_barrier_dissemination(barrier, &state);
		counter = ck_pr_load_int(&counters[i]);
		if (counter != nthr * (j / ENTRIES + 1)) {
			ck_error("FAILED [%d:%d]: %d != %d\n", i, j - 1, counter, nthr);
		}
	}

	return (NULL);
}

int
main(int argc, char *argv[])
{
	ck_barrier_dissemination_t *barrier;
	ck_barrier_dissemination_flag_t **barrier_internal;
	pthread_t *threads;
	int i, size;

	if (argc < 3) {
		ck_error("Usage: correct <number of threads> <affinity delta>\n");
	}

	nthr = atoi(argv[1]);
	if (nthr <= 0) {
		ck_error("ERROR: Number of threads must be greater than 0\n");
	}

	threads = malloc(sizeof(pthread_t) * nthr);
	if (threads == NULL) {
		ck_error("ERROR: Could not allocate thread structures\n");
	}

	a.delta = atoi(argv[2]);

	barrier = malloc(sizeof(ck_barrier_dissemination_t) * nthr);
	if (barrier == NULL) {
		ck_error("ERROR: Could not allocate barrier structures\n");
	}

	barrier_internal = malloc(sizeof(ck_barrier_dissemination_flag_t *) * nthr);
	if (barrier_internal == NULL) {
		ck_error("ERROR: Could not allocate barrier structures\n");
	}

	size = ck_barrier_dissemination_size(nthr);
	for (i = 0; i < nthr; ++i) {
		barrier_internal[i] = malloc(sizeof(ck_barrier_dissemination_flag_t) * size);
		if (barrier_internal[i] == NULL) {
			ck_error("ERROR: Could not allocate barrier structures\n");
		}
	}
	ck_barrier_dissemination_init(barrier, barrier_internal, nthr);

	fprintf(stderr, "Creating threads (barrier)...");
	for (i = 0; i < nthr; i++) {
		if (pthread_create(&threads[i], NULL, thread, barrier)) {
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


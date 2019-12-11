/*
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

#include <pthread.h>
#include <unistd.h>
#include <ck_stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#include <ck_pr.h>
#include <ck_barrier.h>

#include "../../common.h"

#if defined(CK_F_PR_INC_64) && defined(CK_F_PR_LOAD_64)
static int done = 0;
static struct affinity a;
static int nthr;
static int tid;
static ck_barrier_centralized_t barrier = CK_BARRIER_CENTRALIZED_INITIALIZER;
struct counter {
	uint64_t value;
} CK_CC_CACHELINE;
struct counter *counters;

static void *
thread(void *null CK_CC_UNUSED)
{
	ck_barrier_centralized_state_t state = CK_BARRIER_CENTRALIZED_STATE_INITIALIZER;
	int id;

	id = ck_pr_faa_int(&tid, 1);
	aff_iterate(&a);

	while (ck_pr_load_int(&done) == 0) {
		ck_barrier_centralized(&barrier, &state, nthr);
		ck_pr_inc_64(&counters[id].value);
		ck_barrier_centralized(&barrier, &state, nthr);
		ck_pr_inc_64(&counters[id].value);
		ck_barrier_centralized(&barrier, &state, nthr);
		ck_pr_inc_64(&counters[id].value);
		ck_barrier_centralized(&barrier, &state, nthr);
		ck_pr_inc_64(&counters[id].value);
		ck_barrier_centralized(&barrier, &state, nthr);
		ck_pr_inc_64(&counters[id].value);
		ck_barrier_centralized(&barrier, &state, nthr);
		ck_pr_inc_64(&counters[id].value);
		ck_barrier_centralized(&barrier, &state, nthr);
		ck_pr_inc_64(&counters[id].value);
		ck_barrier_centralized(&barrier, &state, nthr);
		ck_pr_inc_64(&counters[id].value);
	}

	return (NULL);
}

int
main(int argc, char *argv[])
{
	pthread_t *threads;
	uint64_t count;
	int i;

	if (argc != 3) {
		ck_error("Correct usage: <number of threads> <affinity delta>\n");
	}

	nthr = atoi(argv[1]);
        if (nthr <= 0) {
                ck_error("ERROR: Number of threads must be greater than 0\n");
        }

        threads = malloc(sizeof(pthread_t) * nthr);
        if (threads == NULL) {
                ck_error("ERROR: Could not allocate thread structures\n");
        }

	counters = calloc(sizeof(struct counter), nthr);
	if (counters == NULL) {
		ck_error("ERROR: Could not allocate counters\n");
	}

        a.delta = atoi(argv[2]);

        fprintf(stderr, "Creating threads (barrier)...");
        for (i = 0; i < nthr; ++i) {
                if (pthread_create(&threads[i], NULL, thread, NULL)) {
                        ck_error("ERROR: Could not create thread %d\n", i);
                }
        }
        fprintf(stderr, "done\n");

	common_sleep(10);

	count = 0;
	ck_pr_store_int(&done, 1);
	for (i = 0; i < nthr; ++i)
		count += ck_pr_load_64(&counters[i].value);
	printf("%d %16" PRIu64 "\n", nthr, count);

	return (0);
}
#else
int
main(void)
{

	fputs("Unsupported.", stderr);
	return 0;
}
#endif


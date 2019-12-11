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

#include <ck_cc.h>
#include <ck_pr.h>
#include <ck_spinlock.h>

#include "../../common.h"

#ifndef ITERATE
#define ITERATE 1000000
#endif

struct block {
	unsigned int tid;
};

static struct affinity a;
static unsigned int locked = 0;
static uint64_t nthr;

LOCK_DEFINE;

static void *
thread(void *null CK_CC_UNUSED)
{
#ifdef LOCK_STATE
	LOCK_STATE;
#endif
	unsigned int i = ITERATE;
	unsigned int j;
	unsigned int core;

        if (aff_iterate_core(&a, &core)) {
                perror("ERROR: Could not affine thread");
                exit(EXIT_FAILURE);
        }

	while (i--) {
#ifdef TRYLOCK
		if (i & 1) {
			LOCK;
		} else {
			while (TRYLOCK == false)
				ck_pr_stall();
		}
#else
		LOCK;
#endif

#ifdef LOCKED
		if (LOCKED == false)
			ck_error("is_locked operation failed.");
#endif

		ck_pr_store_uint(&locked, locked + 1);
		ck_pr_store_uint(&locked, locked + 1);
		ck_pr_store_uint(&locked, locked + 1);
		ck_pr_store_uint(&locked, locked + 1);
		ck_pr_store_uint(&locked, locked + 1);
		ck_pr_store_uint(&locked, locked + 1);
		ck_pr_store_uint(&locked, locked + 1);
		ck_pr_store_uint(&locked, locked + 1);
		ck_pr_store_uint(&locked, locked + 1);
		ck_pr_store_uint(&locked, locked + 1);

		j = ck_pr_load_uint(&locked);

		if (j != 10) {
			ck_error("ERROR (WR): Race condition (%u)\n", j);
			exit(EXIT_FAILURE);
		}

		ck_pr_store_uint(&locked, locked - 1);
		ck_pr_store_uint(&locked, locked - 1);
		ck_pr_store_uint(&locked, locked - 1);
		ck_pr_store_uint(&locked, locked - 1);
		ck_pr_store_uint(&locked, locked - 1);
		ck_pr_store_uint(&locked, locked - 1);
		ck_pr_store_uint(&locked, locked - 1);
		ck_pr_store_uint(&locked, locked - 1);
		ck_pr_store_uint(&locked, locked - 1);
		ck_pr_store_uint(&locked, locked - 1);

		UNLOCK;
		LOCK;

		j = ck_pr_load_uint(&locked);
		if (j != 0) {
			ck_error("ERROR (RD): Race condition (%u)\n", j);
			exit(EXIT_FAILURE);
		}

		UNLOCK;
	}

	return (NULL);
}

int
main(int argc, char *argv[])
{
	uint64_t i;
	pthread_t *threads;

	if (argc != 3) {
		ck_error("Usage: " LOCK_NAME " <number of threads> <affinity delta>\n");
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

	threads = malloc(sizeof(pthread_t) * nthr);
	if (threads == NULL) {
		ck_error("ERROR: Could not allocate thread structures\n");
		exit(EXIT_FAILURE);
	}

	a.delta = atoi(argv[2]);
	a.request = 0;

	fprintf(stderr, "Creating threads (mutual exclusion)...");
	for (i = 0; i < nthr; i++) {
		if (pthread_create(&threads[i], NULL, thread, NULL)) {
			ck_error("ERROR: Could not create thread %" PRIu64 "\n", i);
			exit(EXIT_FAILURE);
		}
	}
	fprintf(stderr, "done\n");

	fprintf(stderr, "Waiting for threads to finish correctness regression...");
	for (i = 0; i < nthr; i++)
		pthread_join(threads[i], NULL);
	fprintf(stderr, "done (passed)\n");

	return (0);
}


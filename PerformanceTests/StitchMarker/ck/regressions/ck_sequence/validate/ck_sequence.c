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

#include <ck_cc.h>
#include <ck_sequence.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include "../../common.h"

#ifndef STEPS
#define STEPS 1000000
#endif

struct example {
        unsigned int a;
        unsigned int b;
        unsigned int c;
};

static struct example global CK_CC_CACHELINE;
static ck_sequence_t seqlock CK_CC_CACHELINE = CK_SEQUENCE_INITIALIZER;
static unsigned int barrier;
static struct affinity affinerator;

static void
validate(struct example *copy)
{

	if (copy->b != copy->a + 1000) {
		ck_error("ERROR: Failed regression: copy->b (%u != %u + %u / %u)\n",
		    copy->b, copy->a, 1000, copy->a + 1000);
	}

	if (copy->c != copy->a + copy->b) {
		ck_error("ERROR: Failed regression: copy->c (%u != %u + %u / %u)\n",
		    copy->c, copy->a, copy->b, copy->a + copy->b);
	}

	return;
}

static void *
consumer(void *unused CK_CC_UNUSED)
{
        struct example copy;
        uint32_t version;
        unsigned int retries = 0;
        unsigned int i;

	unused = NULL;
	if (aff_iterate(&affinerator)) {
		perror("ERROR: Could not affine thread");
		exit(EXIT_FAILURE);
	}

        while (ck_pr_load_uint(&barrier) == 0);
        for (i = 0; i < STEPS; i++) {
                /*
                 * Attempt a read of the data structure. If the structure
                 * has been modified between ck_sequence_read_begin and
                 * ck_sequence_read_retry then attempt another read since
                 * the data may be in an inconsistent state.
                 */
                do {
                        version = ck_sequence_read_begin(&seqlock);
                        copy.a = ck_pr_load_uint(&global.a);
                        copy.b = ck_pr_load_uint(&global.b);
			copy.c = ck_pr_load_uint(&global.c);
			retries++;
                } while (ck_sequence_read_retry(&seqlock, version) == true);
		validate(&copy);

		CK_SEQUENCE_READ(&seqlock, &version) {
                        copy.a = ck_pr_load_uint(&global.a);
                        copy.b = ck_pr_load_uint(&global.b);
			copy.c = ck_pr_load_uint(&global.c);
			retries++;
		}
		validate(&copy);
        }

        fprintf(stderr, "%u retries.\n", retries - STEPS);
	ck_pr_dec_uint(&barrier);
        return (NULL);
}

int
main(int argc, char *argv[])
{
	pthread_t *threads;
        unsigned int counter = 0;
	bool first = true;
	int n_threads, i;

	if (argc != 3) {
		ck_error("Usage: ck_sequence <number of threads> <affinity delta>\n");
	}

	n_threads = atoi(argv[1]);
	if (n_threads <= 0) {
		ck_error("ERROR: Number of threads must be greater than 0\n");
	}

	threads = malloc(sizeof(pthread_t) * n_threads);
	if (threads == NULL) {
		ck_error("ERROR: Could not allocate memory for threads\n");
	}

	affinerator.delta = atoi(argv[2]);
	affinerator.request = 0;

	for (i = 0; i < n_threads; i++) {
		if (pthread_create(&threads[i], NULL, consumer, NULL)) {
			ck_error("ERROR: Failed to create thread %d\n", i);
		}
	}

        for (;;) {
                /*
                 * Update the shared data in a non-blocking fashion.
		 * If the data is modified by multiple writers then
		 * ck_sequence_write_begin must be called after acquiring
		 * the associated lock and ck_sequence_write_end must be
		 * called before relinquishing the lock.
                 */
                ck_sequence_write_begin(&seqlock);
                global.a = counter++;
		global.b = global.a + 1000;
		global.c = global.b + global.a;
                ck_sequence_write_end(&seqlock);

		if (first == true) {
			ck_pr_store_uint(&barrier, n_threads);
			first = false;
		}

                counter++;
		if (ck_pr_load_uint(&barrier) == 0)
                        break;
        }

        printf("%u updates made.\n", counter);
        return (0);
}


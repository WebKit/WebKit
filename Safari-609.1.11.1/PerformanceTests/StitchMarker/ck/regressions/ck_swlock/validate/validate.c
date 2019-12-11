/*
 * Copyright 2014 Jaidev Sridhar.
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
#include <ck_swlock.h>

#include "../../common.h"

#ifndef ITERATE
#define ITERATE 1000000
#endif

static struct affinity a;
static unsigned int locked;
static int nthr;
static ck_swlock_t lock = CK_SWLOCK_INITIALIZER;
static ck_swlock_t copy;
#ifdef CK_F_PR_RTM
static void *
thread_rtm_adaptive(void *arg)
{
	unsigned int i = ITERATE;
	unsigned int l;
	int tid = ck_pr_load_int(arg);

	struct ck_elide_config config = CK_ELIDE_CONFIG_DEFAULT_INITIALIZER;
	struct ck_elide_stat st = CK_ELIDE_STAT_INITIALIZER;

        if (aff_iterate(&a)) {
                perror("ERROR: Could not affine thread");
                exit(EXIT_FAILURE);
        }

	while (i--) {
		if (tid == 0) {
			CK_ELIDE_LOCK_ADAPTIVE(ck_swlock_write, &st, &config, &lock);
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
			CK_ELIDE_UNLOCK_ADAPTIVE(ck_swlock_write, &st, &lock);
		}

		CK_ELIDE_LOCK(ck_swlock_read, &lock);
		{
			l = ck_pr_load_uint(&locked);
			if (l != 0) {
				ck_error("ERROR [RD:%d]: %u != 0\n", __LINE__, l);
			}
		}
		CK_ELIDE_UNLOCK(ck_swlock_read, &lock);
	}

	return NULL;
}

static void *
thread_rtm_mix(void *arg)
{
	unsigned int i = ITERATE;
	unsigned int l;
	int tid = ck_pr_load_int(arg);

        if (aff_iterate(&a)) {
                perror("ERROR: Could not affine thread");
                exit(EXIT_FAILURE);
        }

	while (i--) {
		if (tid == 0) {
			if (i & 1) {
				CK_ELIDE_LOCK(ck_swlock_write, &lock);
			} else {
				ck_swlock_write_lock(&lock);
			}

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

			if (i & 1) {
				CK_ELIDE_UNLOCK(ck_swlock_write, &lock);
			} else {
				ck_swlock_write_unlock(&lock);
			}
		}
		if (i & 1) {
			CK_ELIDE_LOCK(ck_swlock_read, &lock);
		} else {
			ck_swlock_read_lock(&lock);
		}

		{
			l = ck_pr_load_uint(&locked);
			if (l != 0) {
				ck_error("ERROR [RD:%d]: %u != 0\n", __LINE__, l);
			}
		}

		if (i & 1) {
			CK_ELIDE_UNLOCK(ck_swlock_read, &lock);
		} else {
			ck_swlock_read_unlock(&lock);
		}
	}

	return (NULL);
}

static void *
thread_rtm(void *arg)
{
	unsigned int i = ITERATE;
	unsigned int l;
	int tid = ck_pr_load_int(arg);

        if (aff_iterate(&a)) {
                perror("ERROR: Could not affine thread");
                exit(EXIT_FAILURE);
        }

	while (i--) {
		if (tid == 0) {
			CK_ELIDE_LOCK(ck_swlock_write, &lock);
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
			CK_ELIDE_UNLOCK(ck_swlock_write, &lock);
		}

		CK_ELIDE_LOCK(ck_swlock_read, &lock);
		{
			l = ck_pr_load_uint(&locked);
			if (l != 0) {
				ck_error("ERROR [RD:%d]: %u != 0\n", __LINE__, l);
			}
		}
		CK_ELIDE_UNLOCK(ck_swlock_read, &lock);
	}

	return (NULL);
}
#endif /* CK_F_PR_RTM */

static void *
thread_latch(void *arg)
{
	unsigned int i = ITERATE;
	unsigned int l;
	int tid = ck_pr_load_int(arg);

        if (aff_iterate(&a)) {
                perror("ERROR: Could not affine thread");
                exit(EXIT_FAILURE);
        }

	while (i--) {
		if (tid == 0) {
			/* Writer */
			ck_swlock_write_latch(&lock);
			{
				memcpy(&copy, &lock, sizeof(ck_swlock_t));
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
				memcpy(&lock, &copy, sizeof(ck_swlock_t));
			}
			ck_swlock_write_unlatch(&lock);
		}

		ck_swlock_read_lock(&lock);
		{
			l = ck_pr_load_uint(&locked);
			if (l != 0) {
				ck_error("ERROR [RD:%d]: %u != 0\n", __LINE__, l);
			}
		}
		ck_swlock_read_unlock(&lock);
	}

	return (NULL);
}

static void *
thread(void *arg)
{
	unsigned int i = ITERATE;
	unsigned int l;
	int tid = ck_pr_load_int(arg);

        if (aff_iterate(&a)) {
                perror("ERROR: Could not affine thread");
                exit(EXIT_FAILURE);
        }

	while (i--) {
		if (tid == 0) {
			/* Writer */
			ck_swlock_write_lock(&lock);
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
			ck_swlock_write_unlock(&lock);
		}

		ck_swlock_read_lock(&lock);
		{
			l = ck_pr_load_uint(&locked);
			if (l != 0) {
				ck_error("ERROR [RD:%d]: %u != 0\n", __LINE__, l);
			}
		}
		ck_swlock_read_unlock(&lock);
	}

	return (NULL);
}

static void
swlock_test(pthread_t *threads, void *(*f)(void *), const char *test)
{
	int i, tid[nthr];

	fprintf(stderr, "Creating threads (%s)...", test);
	for (i = 0; i < nthr; i++) {
		ck_pr_store_int(&tid[i], i);
		if (pthread_create(&threads[i], NULL, f, &tid[i])) {
			ck_error("ERROR: Could not create thread %d\n", i);
		}
	}
	fprintf(stderr, ".");

	for (i = 0; i < nthr; i++)
		pthread_join(threads[i], NULL);
	fprintf(stderr, "done (passed)\n");
	return;
}

int
main(int argc, char *argv[])
{
	pthread_t *threads;

	if (argc != 3) {
		ck_error("Usage: validate <number of threads> <affinity delta>\n");
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

	swlock_test(threads, thread, "regular");
	swlock_test(threads, thread_latch, "latch");
#ifdef CK_F_PR_RTM
	swlock_test(threads, thread_rtm, "rtm");
	swlock_test(threads, thread_rtm_mix, "rtm-mix");
	swlock_test(threads, thread_rtm_adaptive, "rtm-adaptive");
#endif
	return 0;
}


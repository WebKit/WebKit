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
#include <stdio.h>

#include "../../common.h"

#define CK_F_PR_RTM

#ifndef STEPS
#define STEPS 2000000
#endif

int
main(void)
{
	uint64_t s_b, e_b, i;
	ck_rwlock_t rwlock = CK_RWLOCK_INITIALIZER;

	for (i = 0; i < STEPS; i++) {
		ck_rwlock_write_lock(&rwlock);
		ck_rwlock_write_unlock(&rwlock);
	}

	s_b = rdtsc();
	for (i = 0; i < STEPS; i++) {
		ck_rwlock_write_lock(&rwlock);
		ck_rwlock_write_unlock(&rwlock);
	}
	e_b = rdtsc();
	printf("                WRITE: rwlock   %15" PRIu64 "\n", (e_b - s_b) / STEPS);

#ifdef CK_F_PR_RTM
	struct ck_elide_config config = CK_ELIDE_CONFIG_DEFAULT_INITIALIZER;
	struct ck_elide_stat st = CK_ELIDE_STAT_INITIALIZER;

	for (i = 0; i < STEPS; i++) {
		CK_ELIDE_LOCK(ck_rwlock_write, &rwlock);
		CK_ELIDE_UNLOCK(ck_rwlock_write, &rwlock);
	}

	s_b = rdtsc();
	for (i = 0; i < STEPS; i++) {
		CK_ELIDE_LOCK(ck_rwlock_write, &rwlock);
		CK_ELIDE_UNLOCK(ck_rwlock_write, &rwlock);
	}
	e_b = rdtsc();
	printf("          (rtm) WRITE: rwlock   %15" PRIu64 "\n", (e_b - s_b) / STEPS);

	for (i = 0; i < STEPS; i++) {
		CK_ELIDE_LOCK_ADAPTIVE(ck_rwlock_write, &st, &config, &rwlock);
		CK_ELIDE_UNLOCK_ADAPTIVE(ck_rwlock_write, &st, &rwlock);
	}

	s_b = rdtsc();
	for (i = 0; i < STEPS; i++) {
		CK_ELIDE_LOCK_ADAPTIVE(ck_rwlock_write, &st, &config, &rwlock);
		CK_ELIDE_UNLOCK_ADAPTIVE(ck_rwlock_write, &st, &rwlock);
	}
	e_b = rdtsc();
	printf(" (rtm-adaptive) WRITE: rwlock   %15" PRIu64 "\n", (e_b - s_b) / STEPS);
#endif /* CK_F_PR_RTM */

	for (i = 0; i < STEPS; i++) {
		ck_rwlock_read_lock(&rwlock);
		ck_rwlock_read_unlock(&rwlock);
	}

	s_b = rdtsc();
	for (i = 0; i < STEPS; i++) {
		ck_rwlock_read_lock(&rwlock);
		ck_rwlock_read_unlock(&rwlock);
	}
	e_b = rdtsc();
	printf("                READ:  rwlock   %15" PRIu64 "\n", (e_b - s_b) / STEPS);

#ifdef CK_F_PR_RTM
	ck_elide_stat_init(&st);

	for (i = 0; i < STEPS; i++) {
		CK_ELIDE_LOCK(ck_rwlock_read, &rwlock);
		CK_ELIDE_UNLOCK(ck_rwlock_read, &rwlock);
	}

	s_b = rdtsc();
	for (i = 0; i < STEPS; i++) {
		CK_ELIDE_LOCK(ck_rwlock_read, &rwlock);
		CK_ELIDE_UNLOCK(ck_rwlock_read, &rwlock);
	}
	e_b = rdtsc();
	printf("          (rtm) READ:  rwlock   %15" PRIu64 "\n", (e_b - s_b) / STEPS);

	for (i = 0; i < STEPS; i++) {
		CK_ELIDE_LOCK_ADAPTIVE(ck_rwlock_read, &st, &config, &rwlock);
		CK_ELIDE_UNLOCK_ADAPTIVE(ck_rwlock_read, &st, &rwlock);
	}

	s_b = rdtsc();
	for (i = 0; i < STEPS; i++) {
		CK_ELIDE_LOCK_ADAPTIVE(ck_rwlock_read, &st, &config, &rwlock);
		CK_ELIDE_UNLOCK_ADAPTIVE(ck_rwlock_read, &st, &rwlock);
	}
	e_b = rdtsc();
	printf(" (rtm-adaptive) READ:  rwlock   %15" PRIu64 "\n", (e_b - s_b) / STEPS);
#endif /* CK_F_PR_RTM */

	return 0;
}


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

#include <ck_rwcohort.h>
#include <ck_spinlock.h>
#include <inttypes.h>
#include <stdio.h>

#include "../../common.h"

#ifndef STEPS
#define STEPS 1000000
#endif

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

int
main(void)
{
	uint64_t s_b, e_b, i;
	ck_spinlock_fas_t global_lock = CK_SPINLOCK_FAS_INITIALIZER;
	ck_spinlock_fas_t local_lock = CK_SPINLOCK_FAS_INITIALIZER;
	CK_COHORT_INSTANCE(fas_fas) cohort = CK_COHORT_INITIALIZER;
	LOCK_INSTANCE(fas_fas) rw_cohort = LOCK_INITIALIZER;

	CK_COHORT_INIT(fas_fas, &cohort, &global_lock, &local_lock,
	    CK_COHORT_DEFAULT_LOCAL_PASS_LIMIT);
	LOCK_INIT(fas_fas, &rw_cohort, CK_RWCOHORT_WP_DEFAULT_WAIT_LIMIT);

	for (i = 0; i < STEPS; i++) {
		WRITE_LOCK(fas_fas, &rw_cohort, &cohort, NULL, NULL);
		WRITE_UNLOCK(fas_fas, &rw_cohort, &cohort, NULL, NULL);
	}

	s_b = rdtsc();
	for (i = 0; i < STEPS; i++) {
		WRITE_LOCK(fas_fas, &rw_cohort, &cohort, NULL, NULL);
		WRITE_UNLOCK(fas_fas, &rw_cohort, &cohort, NULL, NULL);
	}
	e_b = rdtsc();
	printf("WRITE: rwlock   %15" PRIu64 "\n", (e_b - s_b) / STEPS);

	for (i = 0; i < STEPS; i++) {
		READ_LOCK(fas_fas, &rw_cohort, &cohort, NULL, NULL);
		READ_UNLOCK(fas_fas, &rw_cohort, &cohort, NULL, NULL);
	}

	s_b = rdtsc();
	for (i = 0; i < STEPS; i++) {
		READ_LOCK(fas_fas, &rw_cohort, &cohort, NULL, NULL);
		READ_UNLOCK(fas_fas, &rw_cohort, &cohort, NULL, NULL);
	}
	e_b = rdtsc();
	printf("READ:  rwlock   %15" PRIu64 "\n", (e_b - s_b) / STEPS);

	return (0);
}


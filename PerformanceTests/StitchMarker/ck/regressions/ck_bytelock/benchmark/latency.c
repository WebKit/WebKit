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

#include <ck_bytelock.h>
#include <ck_rwlock.h>
#include <inttypes.h>
#include <stdio.h>

#include "../../common.h"

#ifndef STEPS
#define STEPS 1000000
#endif

int
main(void)
{
	uint64_t s_b, e_b, i;
	ck_bytelock_t bytelock = CK_BYTELOCK_INITIALIZER;
	ck_rwlock_t naive;

	for (i = 0; i < STEPS; i++) {
		ck_bytelock_write_lock(&bytelock, 1);
		ck_bytelock_write_unlock(&bytelock);
	}

	s_b = rdtsc();
	for (i = 0; i < STEPS; i++) {
		ck_bytelock_write_lock(&bytelock, 1);
		ck_bytelock_write_unlock(&bytelock);
	}
	e_b = rdtsc();
	printf("WRITE: bytelock %15" PRIu64 "\n", (e_b - s_b) / STEPS);

	ck_rwlock_init(&naive);
	for (i = 0; i < STEPS; i++) {
		ck_rwlock_write_lock(&naive);
		ck_rwlock_write_unlock(&naive);
	}

	s_b = rdtsc();
	for (i = 0; i < STEPS; i++) {
		ck_rwlock_write_lock(&naive);
		ck_rwlock_write_unlock(&naive);
	}
	e_b = rdtsc();
	printf("WRITE: naive    %15" PRIu64 "\n", (e_b - s_b) / STEPS);

	for (i = 0; i < STEPS; i++) {
		ck_bytelock_read_lock(&bytelock, 1);
		ck_bytelock_read_unlock(&bytelock, 1);
	}
	s_b = rdtsc();
	for (i = 0; i < STEPS; i++) {
		ck_bytelock_read_lock(&bytelock, 1);
		ck_bytelock_read_unlock(&bytelock, 1);
	}
	e_b = rdtsc();
	printf("READ:  bytelock %15" PRIu64 "\n", (e_b - s_b) / STEPS);

	for (i = 0; i < STEPS; i++) {
		ck_rwlock_read_lock(&naive);
		ck_rwlock_read_unlock(&naive);
	}

	s_b = rdtsc();
	for (i = 0; i < STEPS; i++) {
		ck_rwlock_read_lock(&naive);
		ck_rwlock_read_unlock(&naive);
	}
	e_b = rdtsc();
	printf("READ:  naive    %15" PRIu64 "\n", (e_b - s_b) / STEPS);

	return (0);
}


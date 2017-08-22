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

#include <ck_bytelock.h>
#include <ck_spinlock.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#include "../../common.h"

#ifndef STEPS
#define STEPS 30000000
#endif

LOCK_DEFINE;

int
main(void)
{
	CK_CC_UNUSED unsigned int nthr = 1;

	#ifdef LOCK_INIT
	LOCK_INIT;
	#endif

	#ifdef LOCK_STATE
	LOCK_STATE;
	#endif

	uint64_t s_b, e_b, i;
	CK_CC_UNUSED int core = 0;

	s_b = rdtsc();
	for (i = 0; i < STEPS; ++i) {
		#ifdef LOCK
		LOCK;
		UNLOCK;
		LOCK;
		UNLOCK;
		LOCK;
		UNLOCK;
		LOCK;
		UNLOCK;
		#endif
	}
	e_b = rdtsc();
	printf("%15" PRIu64 "\n", (e_b - s_b) / 4 / STEPS);

	return (0);
}


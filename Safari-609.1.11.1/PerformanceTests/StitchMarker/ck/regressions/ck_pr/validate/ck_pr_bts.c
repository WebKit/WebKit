/*
 * Copyright 2009 Samy Al Bahra.
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

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#include <ck_pr.h>

#include "../../common.h"
#ifndef R_REPEAT
#define R_REPEAT 200000
#endif

/*
 * Bit selector.
 */
#define BM(v, b) (((v) >> (b)) & 1)

#define CK_PR_BTS_T(w, v)										\
	{												\
		unsigned int j;										\
		uint##w##_t r = v, c = v;								\
		bool t;											\
		for (j = 0; j < (w); j++) {								\
			c |= (uint##w##_t)1 << j;							\
			t = ck_pr_bts_##w(&r, j);							\
			if ((t && !BM(v, j)) || (r != c)) {						\
				printf("FAIL [%" PRIx##w ":%u != %" PRIx##w ":%u]\n", r, j, c, j);	\
				exit(EXIT_FAILURE);							\
			}										\
		}											\
	}

#define CK_PR_BTS_B(w)					\
	{						\
		uint##w##_t o;				\
		unsigned int i;				\
		printf("ck_pr_bts_" #w ": ");		\
		for (i = 0; i < R_REPEAT; i++) {	\
			o = (uint##w##_t)common_rand();	\
			CK_PR_BTS_T(w, o);		\
		}					\
		printf("  SUCCESS\n");			\
	}

int
main(void)
{

	common_srand((unsigned int)getpid());

#ifdef CK_F_PR_BTS_64
	CK_PR_BTS_B(64);
#endif

#ifdef CK_F_PR_BTS_32
	CK_PR_BTS_B(32);
#endif

#ifdef CK_F_PR_BTS_16
	CK_PR_BTS_B(16);
#endif

#ifdef CK_F_PR_BTS_8
	CK_PR_BTS_B(8);
#endif

	return (0);
}


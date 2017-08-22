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

#define BM(m, w) ((uint##m##_t)(uint##w##_t)(-1))

#define CK_PR_FAS_T(w, v, d)							\
	{									\
		uint##w##_t r, t = v;						\
		r = ck_pr_fas_##w(&t, d);					\
		if ((t != d) || (r != v)) {					\
			printf("FAIL [");					\
			printf("%" PRIu##w " (%" PRIu##w ") -> %" PRIu##w	\
					" (%" PRIu##w ")]\n",			\
					(uint##w##_t)v, d, t, r);		\
			exit(EXIT_FAILURE);					\
		}								\
	}

#define CK_PR_FAS_B(w)						\
	{							\
		unsigned int __ck_i = 0;			\
		printf("ck_pr_fas_" #w ": ");			\
		if (w < 10)					\
			printf(" ");				\
		for (__ck_i = 0; __ck_i < R_REPEAT; __ck_i++) {	\
			uint##w##_t a = common_rand();		\
			uint##w##_t b = common_rand();		\
			CK_PR_FAS_T(w, a, b);			\
		}						\
		rg_width(w);					\
		printf("  SUCCESS\n");				\
	}

#define CK_PR_FAS_W(m, w)								\
	{										\
		uint##m##_t t = 0;							\
		ck_pr_fas_##w((uint##w##_t *)(void *)&t, -1);				\
		if (t != BM(m, w)) {							\
			printf("FAIL [%#" PRIx##m " != %#" PRIx##m "]\n", t, BM(m, w));	\
			exit(EXIT_FAILURE);						\
		}									\
	}

static void
rg_width(int m)
{

	/* Other architectures are bi-endian. */
#if !defined(__x86__) && !defined(__x86_64__)
	return;
#endif

#ifdef CK_F_PR_FAS_64
	if (m == 64) {
#if defined(CK_F_PR_FAS_32)
		CK_PR_FAS_W(64, 32);
#endif
#if defined(CK_PR_FAS_16)
		CK_PR_FAS_W(64, 16);
#endif
#if defined(CK_PR_FAS_8)
		CK_PR_FAS_W(64, 8);
#endif
	}
#endif /* CK_PR_FAS_64 */

#ifdef CK_F_PR_FAS_32
	if (m == 32) {
#if defined(CK_F_PR_FAS_16)
		CK_PR_FAS_W(32, 16);
#endif
#if defined(CK_PR_FAS_8)
		CK_PR_FAS_W(32, 8);
#endif
	}
#endif /* CK_PR_FAS_32 */

#if defined(CK_F_PR_FAS_16) && defined(CK_PR_FAS_8)
	if (m == 16) {
		CK_PR_FAS_W(16, 8);
	}
#endif /* CK_PR_FAS_16 && CK_PR_FAS_8 */

	return;
}

int
main(void)
{

	common_srand((unsigned int)getpid());

#ifdef CK_F_PR_FAS_64
	CK_PR_FAS_B(64);
#endif

#ifdef CK_F_PR_FAS_32
	CK_PR_FAS_B(32);
#endif

#ifdef CK_F_PR_FAS_16
	CK_PR_FAS_B(16);
#endif

#ifdef CK_F_PR_FAS_8
	CK_PR_FAS_B(8);
#endif

	return (0);
}


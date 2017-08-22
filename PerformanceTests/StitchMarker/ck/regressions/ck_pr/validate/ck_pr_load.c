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

#define CK_PR_LOAD_B(w)									\
	{										\
		uint##w##_t t = (uint##w##_t)-1, a = 0;					\
		unsigned int i;								\
		printf("ck_pr_load_" #w ": ");						\
		if (w < 10)								\
			printf(" ");							\
		a = ck_pr_load_##w(&t);							\
		if (a != t) {								\
			printf("FAIL [%#" PRIx##w " != %#" PRIx##w "]\n", a, t);	\
			exit(EXIT_FAILURE);						\
		}									\
		for (i = 0; i < R_REPEAT; i++) {					\
			t = (uint##w##_t)common_rand();					\
			a = ck_pr_load_##w(&t);						\
			if (a != t) {							\
				printf("FAIL [%#" PRIx##w " != %#" PRIx##w "]\n", a, t);\
				exit(EXIT_FAILURE);					\
			}								\
		}									\
		rg_width(w);								\
		printf(" SUCCESS\n");							\
	}

#define CK_PR_LOAD_W(m, w)							\
	{									\
		uint##m##_t f = 0;						\
		uint##w##_t j = (uint##w##_t)-1;				\
		f = ck_pr_load_##w(&j);						\
		if (f != j) {							\
			printf("FAIL [%#" PRIx##m " != %#" PRIx##w "]\n", f, j);\
			exit(EXIT_FAILURE);					\
		}								\
	}

static void
rg_width(int m)
{

	/* Other architectures are bi-endian. */
#if !defined(__x86__) && !defined(__x86_64__)
	return;
#endif

#ifdef CK_F_PR_LOAD_64
	if (m == 64) {
#if defined(CK_F_PR_LOAD_32)
		CK_PR_LOAD_W(64, 32);
#endif
#if defined(CK_PR_LOAD_16)
		CK_PR_LOAD_W(64, 16);
#endif
#if defined(CK_PR_LOAD_8)
		CK_PR_LOAD_W(64, 8);
#endif
	}
#endif /* CK_PR_LOAD_64 */

#ifdef CK_F_PR_LOAD_32
	if (m == 32) {
#if defined(CK_F_PR_LOAD_16)
		CK_PR_LOAD_W(32, 16);
#endif
#if defined(CK_PR_LOAD_8)
		CK_PR_LOAD_W(32, 8);
#endif
	}
#endif /* CK_PR_LOAD_32 */

#if defined(CK_F_PR_LOAD_16) && defined(CK_PR_LOAD_8)
	if (m == 16)
		CK_PR_LOAD_W(16, 8);
#endif /* CK_PR_LOAD_16 && CK_PR_LOAD_8 */

	return;
}

int
main(void)
{

	common_srand((unsigned int)getpid());

#ifdef CK_F_PR_LOAD_64
	CK_PR_LOAD_B(64);
#endif

#ifdef CK_F_PR_LOAD_32
	CK_PR_LOAD_B(32);
#endif

#ifdef CK_F_PR_LOAD_16
	CK_PR_LOAD_B(16);
#endif

#ifdef CK_F_PR_LOAD_8
	CK_PR_LOAD_B(8);
#endif

#if 0
	uint64_t a[2] = {0, 0}, b[2] = {0x1111111144444444, 0x2222222266666666};
	printf("%" PRIx64 ":%" PRIx64 " -> ", a[0], a[1]);
	ck_pr_load_64_2(&b, &a);
	printf("%" PRIx64 ":%" PRIx64 "\n", a[0], a[1]);
#endif

	return (0);
}


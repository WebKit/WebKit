/*
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ck_pr.h>

#include "../../common.h"
#define REPEAT 2000000

#define TEST_FAX_FN(S, T, M)								\
	static T									\
	test_faa_##S(M *target, T delta)						\
	{										\
		T previous = (T)*target;						\
		*target = (T)*target + delta;						\
											\
		return (previous);							\
	}										\
	static T									\
	test_fas_##S(M *target, T update)						\
	{										\
		T previous = *target;							\
		*target = update;							\
											\
		return (previous);							\
	}

#define TEST_FAX_FN_S(S, T) TEST_FAX_FN(S, T, T)

TEST_FAX_FN_S(int, int)
TEST_FAX_FN_S(uint, unsigned int)

#undef TEST_FAX_FN_S
#undef TEST_FAX_FN

#define TEST_FAX(K, S, T, D)								\
	static void									\
	run_test_##K##_##S(void)							\
	{										\
		int i, r;								\
		T x = 0, y = 0, x_b, y_b;						\
											\
		puts("***TESTING ck_pr_"#K"_"#S"***");					\
		common_srand((unsigned int)getpid());					\
		for (i = 0; i < REPEAT; ++i) {						\
			r = common_rand();							\
			x_b = test_##K##_##S(&x, r);					\
			y_b = ck_pr_##K##_##S(&y, r);					\
											\
			if (x_b != y_b) {						\
				printf("Serial fetch does not match ck_pr fetch.\n"	\
					"\tSerial: %"#D"\n"				\
					"\tck_pr: %"#D"\n",				\
					x_b, y_b);					\
											\
				return;							\
			}								\
		}									\
											\
		printf("Final result:\n"						\
			"\tSerial: %"#D"\n"						\
			"\tck_pr: %"#D"\n",						\
			x, y);								\
		(x == y) ? puts("SUCCESS.")						\
			 : puts("FAILURE.");						\
											\
		return;									\
	}										\


#define GENERATE_TEST(K)				\
	TEST_FAX(K, int, int, d)			\
	TEST_FAX(K, uint, unsigned int, u)		\
	static void					\
	run_test_##K(void)				\
	{						\
		run_test_##K##_int();			\
		run_test_##K##_uint();			\
	}

GENERATE_TEST(faa)
GENERATE_TEST(fas)

#undef GENERATE_TEST
#undef TEST_FAX

int
main(void)
{
	run_test_faa();
	run_test_fas();

	return (0);
}



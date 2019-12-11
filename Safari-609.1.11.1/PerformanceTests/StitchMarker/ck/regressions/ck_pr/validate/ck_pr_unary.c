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

#define REPEAT 2000000

#define TEST_UNARY(K, S, M, T, P, D, H)									\
	static void											\
	test_##K##_##S(M *target)									\
	{												\
		*target = *target P 1;									\
													\
		return;											\
	}												\
	static void											\
	test_##K##_##S##_zero(M *target, bool *zero)							\
	{												\
		*zero = *target == H;									\
		*target = *target P 1;									\
													\
		return;											\
	}												\
	static void											\
	run_test_##K##_##S(bool use_zero)								\
	{												\
		int i;											\
		T x = 1, y = 1;										\
		bool zero_x = false, zero_y = false;							\
													\
		use_zero ? puts("***TESTING ck_pr_"#K"_"#S"_zero***")					\
			 : puts("***TESTING ck_pr_"#K"_"#S"***");					\
		for (i = 0; i < REPEAT; ++i) {								\
			if (use_zero) {									\
				test_##K##_##S##_zero(&x, &zero_x);					\
				ck_pr_##K##_##S##_zero(&y, &zero_y);					\
			}										\
			else {										\
				test_##K##_##S(&x);							\
				ck_pr_##K##_##S(&y);							\
			}										\
													\
			if (x != y || zero_x != zero_y) {						\
				printf("Serial(%"#D") and ck_pr(%"#D")"					\
					#K"_"#S" do not match.\n"					\
					"FAILURE.\n",							\
					x, y);								\
													\
				return;									\
			}										\
													\
			if (zero_x)									\
				printf("Variables are zero at iteration %d\n", i);			\
		}											\
													\
													\
		printf("\tserial_"#K"_"#S": %"#D"\n"							\
			"\tck_pr_"#K"_"#S": %"#D"\n"							\
			"SUCCESS.\n",									\
			x, y);										\
													\
		return;											\
	}

#define GENERATE_TEST(K, P, Y, Z)					\
	TEST_UNARY(K, int, int, int, P, d, Y)				\
	TEST_UNARY(K, uint, unsigned int, unsigned int, P, u, Z)	\
	static void							\
	run_test_##K(void)						\
	{								\
		run_test_##K##_int(false);				\
		run_test_##K##_int(true);				\
		run_test_##K##_uint(false);				\
		run_test_##K##_uint(true);				\
	}

GENERATE_TEST(inc, +, -1, UINT_MAX)
GENERATE_TEST(dec, -, 1, 1)

#undef GENERATE_TEST
#undef TEST_UNARY

int
main(void)
{
	run_test_inc();
	run_test_dec();

	return (0);
}


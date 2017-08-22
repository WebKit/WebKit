/*
 * Copyright 2009-2015 Samy Al Bahra.
 * Copyright 2017 Neale Ferguson
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

#ifndef CK_PR_S390X_H
#define CK_PR_S390X_H

#ifndef CK_PR_H
#error Do not include this file directly, use ck_pr.h
#endif

#include <ck_cc.h>
#include <ck_md.h>

/*
 * The following represent supported atomic operations.
 * These operations may be emulated.
 */
#include "ck_f_pr.h"

/*
 * Minimum interface requirement met.
 */
#define CK_F_PR

/*
 * This bounces the hardware thread from low to medium
 * priority. I am unsure of the benefits of this approach
 * but it is used by the Linux kernel.
 */
CK_CC_INLINE static void
ck_pr_stall(void)
{
	__sync_synchronize();
	return;
}

#define CK_PR_FENCE(T)					\
	CK_CC_INLINE static void			\
	ck_pr_fence_strict_##T(void)			\
	{						\
		__sync_synchronize();			\
	}

/*
 * These are derived from:
 *     http://www.ibm.com/developerworks/systems/articles/powerpc.html
 */
CK_PR_FENCE(atomic)
CK_PR_FENCE(atomic_store)
CK_PR_FENCE(atomic_load)
CK_PR_FENCE(store_atomic)
CK_PR_FENCE(load_atomic)
CK_PR_FENCE(store)
CK_PR_FENCE(store_load)
CK_PR_FENCE(load)
CK_PR_FENCE(load_store)
CK_PR_FENCE(memory)
CK_PR_FENCE(acquire)
CK_PR_FENCE(release)
CK_PR_FENCE(acqrel)
CK_PR_FENCE(lock)
CK_PR_FENCE(unlock)

#undef CK_PR_FENCE

#define CK_PR_LOAD(S, M, T, C, I)					\
	CK_CC_INLINE static T						\
	ck_pr_md_load_##S(const M *target)				\
	{								\
		T r;							\
		__asm__ __volatile__(I "\t%0, %1\n"			\
					: "=r" (r)			\
					: "Q"  (*(const C *)target)	\
					: "memory");			\
		return (r);						\
	}

CK_PR_LOAD(ptr, void, void *, uint64_t, "lg")

#define CK_PR_LOAD_S(S, T, I) CK_PR_LOAD(S, T, T, T, I)

CK_PR_LOAD_S(64, uint64_t, "lg")
CK_PR_LOAD_S(32, uint32_t, "llgf")
CK_PR_LOAD_S(16, uint16_t, "llgh")
CK_PR_LOAD_S(8, uint8_t, "llgc")
CK_PR_LOAD_S(uint, unsigned int, "llgf")
CK_PR_LOAD_S(int, int, "llgf")
CK_PR_LOAD_S(short, short, "lgh")
CK_PR_LOAD_S(char, char, "lgb")
#ifndef CK_PR_DISABLE_DOUBLE
CK_CC_INLINE static double
ck_pr_md_load_double(const double *target)
{
	double r;
	__asm__ __volatile__("ld  %0, %1\n"
				: "=f" (r)
				: "Q"  (*(const double *)target)   
				: "memory");
	return (r);			
}
#endif

#undef CK_PR_LOAD_S
#undef CK_PR_LOAD

#define CK_PR_STORE(S, M, T, C, I)				\
	CK_CC_INLINE static void				\
	ck_pr_md_store_##S(M *target, T v)			\
	{							\
		__asm__ __volatile__(I "\t%1, %0\n"		\
					: "=Q" (*(C *)target)	\
					: "r" (v)		\
					: "memory");		\
		return;						\
	}

CK_PR_STORE(ptr, void, const void *, uint64_t, "stg")

#define CK_PR_STORE_S(S, T, I) CK_PR_STORE(S, T, T, T, I)

CK_PR_STORE_S(64, uint64_t, "stg")
CK_PR_STORE_S(32, uint32_t, "st")
CK_PR_STORE_S(16, uint16_t, "sth")
CK_PR_STORE_S(8, uint8_t, "stc")
CK_PR_STORE_S(uint, unsigned int, "st")
CK_PR_STORE_S(int, int, "st")
CK_PR_STORE_S(short, short, "sth")
CK_PR_STORE_S(char, char, "stc")
#ifndef CK_PR_DISABLE_DOUBLE
CK_CC_INLINE static void
ck_pr_md_store_double(double *target, double v)
{
	__asm__ __volatile__(" std  %1, %0\n"
				: "=Q" (*(double *)target)   
				: "f"  (v)
				: "0", "memory");
}
#endif

#undef CK_PR_STORE_S
#undef CK_PR_STORE

CK_CC_INLINE static bool
ck_pr_cas_64_value(uint64_t *target, uint64_t compare, uint64_t set, uint64_t *value)
{
	*value = __sync_val_compare_and_swap(target,compare,set);
        return (*value == compare);
}

CK_CC_INLINE static bool
ck_pr_cas_ptr_value(void *target, void *compare, void *set, void *value)
{
	uintptr_t previous;

	previous = __sync_val_compare_and_swap((uintptr_t *) target,
					       (uintptr_t) compare,
					       (uintptr_t) set);
	*((uintptr_t *) value) = previous;
        return (previous == (uintptr_t) compare);
}

CK_CC_INLINE static bool
ck_pr_cas_64(uint64_t *target, uint64_t compare, uint64_t set)
{
	return(__sync_bool_compare_and_swap(target,compare,set));
}

CK_CC_INLINE static bool
ck_pr_cas_ptr(void *target, void *compare, void *set)
{
	return(__sync_bool_compare_and_swap((uintptr_t *) target,
					    (uintptr_t) compare,
					    (uintptr_t) set));
}

#define CK_PR_CAS(N, T)							\
	CK_CC_INLINE static bool					\
	ck_pr_cas_##N##_value(T *target, T compare, T set, T *value)	\
	{								\
		*value = __sync_val_compare_and_swap(target,		\
						     compare,		\
						     set);		\
		return(*value == compare);				\
	}								\
	CK_CC_INLINE static bool					\
	ck_pr_cas_##N(T *target, T compare, T set)			\
	{								\
		return(__sync_bool_compare_and_swap(target,		\
						   compare,		\
						   set));		\
	}

CK_PR_CAS(32, uint32_t)
CK_PR_CAS(uint, unsigned int)
CK_PR_CAS(int, int)

#undef CK_PR_CAS

CK_CC_INLINE static void *
ck_pr_fas_ptr(void *target, void *v)
{
	return((void *)__atomic_exchange_n((uintptr_t *) target, (uintptr_t) v, __ATOMIC_ACQUIRE));
}

#define CK_PR_FAS(N, M, T)							\
	CK_CC_INLINE static T							\
	ck_pr_fas_##N(M *target, T v)						\
	{									\
		return(__atomic_exchange_n(target, v, __ATOMIC_ACQUIRE));	\
	}

CK_PR_FAS(64, uint64_t, uint64_t)
CK_PR_FAS(32, uint32_t, uint32_t)
CK_PR_FAS(int, int, int)
CK_PR_FAS(uint, unsigned int, unsigned int)

#ifndef CK_PR_DISABLE_DOUBLE
CK_CC_INLINE static double
ck_pr_fas_double(double *target, double *v)
{
	double previous;

	__asm__ __volatile__ ("	   lg   1,%2\n"
			      "0:  lg	0,%1\n"
			      "    csg	0,1,%1\n"
			      "    jnz	0b\n"
			      "    ldgr %0,0\n"
			      : "=f" (previous) 
			      : "Q" (target), "Q" (v)
			      : "0", "1", "cc", "memory");
	return (previous);
}
#endif

#undef CK_PR_FAS

/*
 * Atomic store-only binary operations.
 */
#define CK_PR_BINARY(K, S, M, T)				\
	CK_CC_INLINE static void				\
	ck_pr_##K##_##S(M *target, T d)				\
	{							\
		d = __sync_fetch_and_##K((T *)target, d);	\
		return;						\
	}

#define CK_PR_BINARY_S(K, S, T) CK_PR_BINARY(K, S, T, T)

#define CK_PR_GENERATE(K)			\
	CK_PR_BINARY(K, ptr, void, void *)	\
	CK_PR_BINARY_S(K, char, char)		\
	CK_PR_BINARY_S(K, int, int)		\
	CK_PR_BINARY_S(K, uint, unsigned int)	\
	CK_PR_BINARY_S(K, 64, uint64_t)		\
	CK_PR_BINARY_S(K, 32, uint32_t)		\
	CK_PR_BINARY_S(K, 16, uint16_t)		\
	CK_PR_BINARY_S(K, 8, uint8_t)

CK_PR_GENERATE(add)
CK_PR_GENERATE(sub)
CK_PR_GENERATE(and)
CK_PR_GENERATE(or)
CK_PR_GENERATE(xor)

#undef CK_PR_GENERATE
#undef CK_PR_BINARY_S
#undef CK_PR_BINARY

#define CK_PR_UNARY(S, M, T)			\
	CK_CC_INLINE static void		\
	ck_pr_inc_##S(M *target)		\
	{					\
		ck_pr_add_##S(target, (T)1);	\
		return;				\
	}					\
	CK_CC_INLINE static void		\
	ck_pr_dec_##S(M *target)		\
	{					\
		ck_pr_sub_##S(target, (T)1);	\
		return;				\
	}

#define CK_PR_UNARY_X(S, M) 	  					\
	CK_CC_INLINE static void					\
	ck_pr_not_##S(M *target)					\
	{								\
		M newval;						\
		do {							\
			newval = ~(*target);				\
		} while (!__sync_bool_compare_and_swap(target, 		\
						      *target,		\
						      newval));		\
	}								\
	CK_CC_INLINE static void					\
	ck_pr_neg_##S(M *target)					\
	{								\
		M newval;						\
		do {							\
			newval = -(*target);				\
		} while (!__sync_bool_compare_and_swap(target, 		\
						      *target,		\
						      newval));		\
	}								

#define CK_PR_UNARY_S(S, M) CK_PR_UNARY(S, M, M) \
			    CK_PR_UNARY_X(S, M)

CK_PR_UNARY(ptr, void, void *)
CK_PR_UNARY_S(char, char)
CK_PR_UNARY_S(int, int)
CK_PR_UNARY_S(uint, unsigned int)
CK_PR_UNARY_S(64, uint64_t)
CK_PR_UNARY_S(32, uint32_t)
CK_PR_UNARY_S(16, uint16_t)
CK_PR_UNARY_S(8, uint8_t)

#undef CK_PR_UNARY_S
#undef CK_PR_UNARY

CK_CC_INLINE static void *
ck_pr_faa_ptr(void *target, uintptr_t delta)
{
	uintptr_t previous;

	previous = __sync_fetch_and_add((uintptr_t *) target, delta);

	return (void *)(previous);
}

#define CK_PR_FAA(S, T)							\
	CK_CC_INLINE static T						\
	ck_pr_faa_##S(T *target, T delta)				\
	{								\
		T previous;						\
									\
		previous = __sync_fetch_and_add(target, delta);		\
									\
		return (previous);					\
	}

CK_PR_FAA(64, uint64_t)
CK_PR_FAA(32, uint32_t)
CK_PR_FAA(uint, unsigned int)
CK_PR_FAA(int, int)

#undef CK_PR_FAA

#endif /* CK_PR_S390X_H */

/* Copyright (C) 1995-1997 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 *
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 *
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 *
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
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
 *
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.]
 */
/* ====================================================================
 * Copyright (c) 1998-2006 The OpenSSL Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit. (http://www.openssl.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    openssl-core@openssl.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.openssl.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * This product includes cryptographic software written by Eric Young
 * (eay@cryptsoft.com).  This product includes software written by Tim
 * Hudson (tjh@cryptsoft.com).
 *
 */
/* ====================================================================
 * Copyright 2002 Sun Microsystems, Inc. ALL RIGHTS RESERVED.
 *
 * Portions of the attached software ("Contribution") are developed by
 * SUN MICROSYSTEMS, INC., and are contributed to the OpenSSL project.
 *
 * The Contribution is licensed pursuant to the Eric Young open source
 * license provided above.
 *
 * The binary polynomial arithmetic software is originally written by
 * Sheueling Chang Shantz and Douglas Stebila of Sun Microsystems
 * Laboratories. */

#ifndef OPENSSL_HEADER_BN_INTERNAL_H
#define OPENSSL_HEADER_BN_INTERNAL_H

#include <openssl/base.h>

#if defined(OPENSSL_X86_64) && defined(_MSC_VER)
OPENSSL_MSVC_PRAGMA(warning(push, 3))
#include <intrin.h>
OPENSSL_MSVC_PRAGMA(warning(pop))
#pragma intrinsic(__umulh, _umul128)
#endif

#include "../../internal.h"

#if defined(__cplusplus)
extern "C" {
#endif

#if defined(OPENSSL_64_BIT)

#if !defined(_MSC_VER)
/* MSVC doesn't support two-word integers on 64-bit. */
#define BN_ULLONG	uint128_t
#endif

#define BN_BITS2	64
#define BN_BYTES	8
#define BN_BITS4	32
#define BN_MASK2	(0xffffffffffffffffUL)
#define BN_MASK2l	(0xffffffffUL)
#define BN_MASK2h	(0xffffffff00000000UL)
#define BN_MASK2h1	(0xffffffff80000000UL)
#define BN_MONT_CTX_N0_LIMBS 1
#define BN_TBIT		(0x8000000000000000UL)
#define BN_DEC_CONV	(10000000000000000000UL)
#define BN_DEC_NUM	19
#define TOBN(hi, lo) ((BN_ULONG)(hi) << 32 | (lo))

#elif defined(OPENSSL_32_BIT)

#define BN_ULLONG	uint64_t
#define BN_BITS2	32
#define BN_BYTES	4
#define BN_BITS4	16
#define BN_MASK2	(0xffffffffUL)
#define BN_MASK2l	(0xffffUL)
#define BN_MASK2h1	(0xffff8000UL)
#define BN_MASK2h	(0xffff0000UL)
/* On some 32-bit platforms, Montgomery multiplication is done using 64-bit
 * arithmetic with SIMD instructions. On such platforms, |BN_MONT_CTX::n0|
 * needs to be two words long. Only certain 32-bit platforms actually make use
 * of n0[1] and shorter R value would suffice for the others. However,
 * currently only the assembly files know which is which. */
#define BN_MONT_CTX_N0_LIMBS 2
#define BN_TBIT		(0x80000000UL)
#define BN_DEC_CONV	(1000000000UL)
#define BN_DEC_NUM	9
#define TOBN(hi, lo) (lo), (hi)

#else
#error "Must define either OPENSSL_32_BIT or OPENSSL_64_BIT"
#endif


#define STATIC_BIGNUM(x)                                    \
  {                                                         \
    (BN_ULONG *)(x), sizeof(x) / sizeof(BN_ULONG),          \
        sizeof(x) / sizeof(BN_ULONG), 0, BN_FLG_STATIC_DATA \
  }

#if defined(BN_ULLONG)
#define Lw(t) (((BN_ULONG)(t))&BN_MASK2)
#define Hw(t) (((BN_ULONG)((t)>>BN_BITS2))&BN_MASK2)
#endif

/* bn_correct_top decrements |bn->top| until |bn->d[top-1]| is non-zero or
 * until |top| is zero. If |bn| is zero, |bn->neg| is set to zero. */
void bn_correct_top(BIGNUM *bn);

/* bn_wexpand ensures that |bn| has at least |words| works of space without
 * altering its value. It returns one on success or zero on allocation
 * failure. */
int bn_wexpand(BIGNUM *bn, size_t words);

/* bn_expand acts the same as |bn_wexpand|, but takes a number of bits rather
 * than a number of words. */
int bn_expand(BIGNUM *bn, size_t bits);

/* bn_set_words sets |bn| to the value encoded in the |num| words in |words|,
 * least significant word first. */
int bn_set_words(BIGNUM *bn, const BN_ULONG *words, size_t num);

BN_ULONG bn_mul_add_words(BN_ULONG *rp, const BN_ULONG *ap, int num, BN_ULONG w);
BN_ULONG bn_mul_words(BN_ULONG *rp, const BN_ULONG *ap, int num, BN_ULONG w);
void     bn_sqr_words(BN_ULONG *rp, const BN_ULONG *ap, int num);
BN_ULONG bn_add_words(BN_ULONG *rp, const BN_ULONG *ap, const BN_ULONG *bp,int num);
BN_ULONG bn_sub_words(BN_ULONG *rp, const BN_ULONG *ap, const BN_ULONG *bp,int num);

void bn_mul_comba4(BN_ULONG *r, BN_ULONG *a, BN_ULONG *b);
void bn_mul_comba8(BN_ULONG *r, BN_ULONG *a, BN_ULONG *b);
void bn_sqr_comba8(BN_ULONG *r, const BN_ULONG *a);
void bn_sqr_comba4(BN_ULONG *r, const BN_ULONG *a);

/* bn_cmp_words returns a value less than, equal to or greater than zero if
 * the, length |n|, array |a| is less than, equal to or greater than |b|. */
int bn_cmp_words(const BN_ULONG *a, const BN_ULONG *b, int n);

/* bn_cmp_words returns a value less than, equal to or greater than zero if the
 * array |a| is less than, equal to or greater than |b|. The arrays can be of
 * different lengths: |cl| gives the minimum of the two lengths and |dl| gives
 * the length of |a| minus the length of |b|. */
int bn_cmp_part_words(const BN_ULONG *a, const BN_ULONG *b, int cl, int dl);

int bn_mul_mont(BN_ULONG *rp, const BN_ULONG *ap, const BN_ULONG *bp,
                const BN_ULONG *np, const BN_ULONG *n0, int num);

uint64_t bn_mont_n0(const BIGNUM *n);
int bn_mod_exp_base_2_vartime(BIGNUM *r, unsigned p, const BIGNUM *n);

#if defined(OPENSSL_X86_64) && defined(_MSC_VER)
#define BN_UMULT_LOHI(low, high, a, b) ((low) = _umul128((a), (b), &(high)))
#endif

#if !defined(BN_ULLONG) && !defined(BN_UMULT_LOHI)
#error "Either BN_ULLONG or BN_UMULT_LOHI must be defined on every platform."
#endif

/* bn_mod_inverse_prime sets |out| to the modular inverse of |a| modulo |p|,
 * computed with Fermat's Little Theorem. It returns one on success and zero on
 * error. If |mont_p| is NULL, one will be computed temporarily. */
int bn_mod_inverse_prime(BIGNUM *out, const BIGNUM *a, const BIGNUM *p,
                         BN_CTX *ctx, const BN_MONT_CTX *mont_p);

/* bn_mod_inverse_secret_prime behaves like |bn_mod_inverse_prime| but uses
 * |BN_mod_exp_mont_consttime| instead of |BN_mod_exp_mont| in hopes of
 * protecting the exponent. */
int bn_mod_inverse_secret_prime(BIGNUM *out, const BIGNUM *a, const BIGNUM *p,
                                BN_CTX *ctx, const BN_MONT_CTX *mont_p);

/* bn_jacobi returns the Jacobi symbol of |a| and |b| (which is -1, 0 or 1), or
 * -2 on error. */
int bn_jacobi(const BIGNUM *a, const BIGNUM *b, BN_CTX *ctx);


#if defined(__cplusplus)
}  /* extern C */
#endif

#endif  /* OPENSSL_HEADER_BN_INTERNAL_H */

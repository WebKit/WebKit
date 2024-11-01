/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <wtf/Compiler.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

WTF_EXTERN_C_BEGIN

#if USE(APPLE_INTERNAL_SDK)
#include <corecrypto/ccec25519.h>
#include <corecrypto/ccec25519_priv.h>
#include <corecrypto/ccsha2.h>
#else

#define CC_SPTR(_sn_, _n_) _n_

#define CC_WIDE_NULL nullptr

#define CCRNG_STATE_COMMON \
int(*CC_SPTR(ccrng_state, generate))(struct ccrng_state *rng, size_t outlen, void *out);

struct ccrng_state {
    CCRNG_STATE_COMMON
};
struct ccrng_state *ccrng(int *error);

#define ccrng_generate(rng, outlen, out) \
((rng)->generate((struct ccrng_state *)(rng), (outlen), (out)))

typedef uint8_t ccec25519key[32];
typedef ccec25519key ccec25519secretkey;
typedef ccec25519key ccec25519pubkey;
typedef ccec25519key ccec25519base;

typedef uint8_t ccec25519signature[64];

const struct ccdigest_info *ccsha512_di(void);
#define ccoid_sha512 ((unsigned char *)"\x06\x09\x60\x86\x48\x01\x65\x03\x04\x02\x03")
#define ccoid_sha512_len 11
/* SHA512 */
#define CCSHA512_BLOCK_SIZE  128
#define    CCSHA512_OUTPUT_SIZE  64
#define    CCSHA512_STATE_SIZE   64
extern const struct ccdigest_info ccsha512_ltc_di;

#if __has_feature(bounds_attributes)
#define cc_sized_by(x) __attribute__((sized_by(x)))
#else
#define cc_sized_by(x)
#endif // __has_feature(bounds_attributes)

#if HAVE(CORE_CRYPTO_SIGNATURES_INT_RETURN_VALUE)

int cccurve25519(ccec25519key, const ccec25519secretkey, const ccec25519base);
int cccurve25519_make_pub(ccec25519pubkey, const ccec25519secretkey);
int cccurve25519_make_key_pair(struct ccrng_state *, ccec25519pubkey, ccec25519secretkey);
int cced25519_make_key_pair(const struct ccdigest_info *, struct ccrng_state *, ccec25519pubkey pk, ccec25519secretkey sk);
int cced25519_sign(const struct ccdigest_info *, ccec25519signature, size_t len, const void *cc_sized_by(len) msg, const ccec25519pubkey, const ccec25519secretkey);

#else

void cccurve25519(ccec25519key out, const ccec25519secretkey sk, const ccec25519base);

inline void cccurve25519_make_priv(struct ccrng_state *rng, ccec25519secretkey sk)
{
    ccrng_generate(rng, 32, sk);
    sk[0] &= 248;
    sk[31] &= 127;
    sk[31] |= 64;
}

inline void cccurve25519_make_pub(ccec25519pubkey pk, const ccec25519secretkey sk)
{
    cccurve25519(pk, sk, CC_WIDE_NULL);
}

inline void cccurve25519_make_key_pair(struct ccrng_state *rng, ccec25519pubkey pk, ccec25519secretkey sk)
{
    cccurve25519_make_priv(rng, sk);
    cccurve25519_make_pub(pk, sk);
}

void cced25519_make_key_pair(const struct ccdigest_info *, struct ccrng_state *, ccec25519pubkey pk, ccec25519secretkey sk);

void cced25519_sign(const struct ccdigest_info *, ccec25519signature, size_t len, const void *cc_sized_by(len) msg, const ccec25519pubkey pk, const ccec25519secretkey sk);

#endif

int cced25519_make_pub(const struct ccdigest_info *, ccec25519pubkey pk, const ccec25519secretkey sk);


int cced25519_verify(const struct ccdigest_info *, size_t len, const void *cc_sized_by(len) msg, const ccec25519signature, const ccec25519pubkey pk);

#endif // USE(APPLE_INTERNAL_SDK)

WTF_EXTERN_C_END

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

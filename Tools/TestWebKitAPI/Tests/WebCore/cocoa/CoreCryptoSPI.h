/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

extern "C" {

#if USE(APPLE_INTERNAL_SDK)

#include <corecrypto/cc_priv.h>
#include <corecrypto/ccrng.h>
#include <corecrypto/ccrsa.h>
#include <corecrypto/ccrsabssa.h>

#else

typedef size_t cc_size;

#if defined(__arm64__) || defined(__x86_64__)  || defined(_WIN64)
#define CCN_UNIT_SIZE  8
typedef uint64_t cc_unit;
#define CCN_LOG2_BITS_PER_UNIT 6
#elif defined(__arm__) || defined(__i386__) || defined(_WIN32)
#define CCN_UNIT_SIZE  4
typedef uint32_t cc_unit;
#define CCN_LOG2_BITS_PER_UNIT 5
#else
#error undefined architecture
#endif

// Temporary VLA warning opt-outs to help transition away from VLAs.
#define CC_IGNORE_VLA_WARNINGS \
    _Pragma("GCC diagnostic push") \
    _Pragma("GCC diagnostic ignored \"-Wvla\"")
#define CC_RESTORE_VLA_WARNINGS \
    _Pragma("GCC diagnostic pop")

#if __has_feature(ptrauth_calls) && (CC_KERNEL || CC_USE_L4 || CC_USE_SEPROM)
#include <ptrauth.h>
#define CC_SPTR(_sn_, _n_) \
    __ptrauth(ptrauth_key_process_independent_code, 1, ptrauth_string_discriminator("cc_" #_sn_ #_n_)) _n_
#else
#define CC_SPTR(_sn_, _n_) _n_
#endif

#define CCN_UNIT_BITS (sizeof(cc_unit) *8)

#define ccn_sizeof_n(_n_) (sizeof(cc_unit) * (_n_))
#define ccn_nof(_bits_) (((_bits_) + CCN_UNIT_BITS - 1) >> CCN_LOG2_BITS_PER_UNIT)
#define ccn_sizeof(_bits_) (ccn_sizeof_n(ccn_nof(_bits_)))
#define ccn_nof_size(_size_) (((_size_) + sizeof(cc_unit) - 1) / sizeof(cc_unit))
#define ccn_nof_sizeof(_expr_) ccn_nof_size(sizeof(_expr_))
#define ccn_bitsof_n(_n_) ((_n_) * CCN_UNIT_BITS)
#define ccn_bitsof_size(_size_) ((_size_) * 8)
#define ccn_sizeof_size(_size_) ccn_sizeof_n(ccn_nof_size(_size_))

#define cc_ceiling(a, b) (((a) + ((b) - 1)) / (b))
#define cc_ctx_n(_type_, _size_) ((_size_ + sizeof(_type_) - 1) / sizeof(_type_))

struct cczp_funcs;
typedef const struct cczp_funcs *cczp_funcs_t;

#if defined(_MSC_VER)
#if defined(__clang__)
#define CC_ALIGNED(x) __attribute__ ((aligned(x)))
#else
#define CC_ALIGNED(x) __declspec(align(x))
#endif
#else
#if __clang__ || CCN_UNIT_SIZE==8
#define CC_ALIGNED(x) __attribute__ ((aligned(x)))
#else
#define CC_ALIGNED(x) __attribute__ ((aligned((x)>8?8:(x))))
#endif
#endif

#define __CCZP_HEADER_ELEMENTS_DEFINITIONS(pre) \
    cc_size pre##n;                             \
    cc_unit pre##bitlen;                        \
    cczp_funcs_t pre##funcs;

#define __CCZP_ELEMENTS_DEFINITIONS(pre)    \
    __CCZP_HEADER_ELEMENTS_DEFINITIONS(pre) \
    cc_unit pre##ccn[];

struct cczp {
    __CCZP_ELEMENTS_DEFINITIONS()
} CC_ALIGNED(CCN_UNIT_SIZE);

struct ccrsa_full_ctx {
    __CCZP_ELEMENTS_DEFINITIONS(pb_)
} CC_ALIGNED(CCN_UNIT_SIZE);

struct ccrsa_pub_ctx {
    __CCZP_ELEMENTS_DEFINITIONS(pb_)
} CC_ALIGNED(CCN_UNIT_SIZE);

struct ccrsa_priv_ctx {
    __CCZP_ELEMENTS_DEFINITIONS(pv_)
} CC_ALIGNED(CCN_UNIT_SIZE);

typedef struct ccrsa_full_ctx* ccrsa_full_ctx_t;
typedef struct ccrsa_pub_ctx* ccrsa_pub_ctx_t;
typedef struct ccrsa_priv_ctx* ccrsa_priv_ctx_t;

#if defined(_MSC_VER)
#define cc_ctx_decl(_type_, _size_, _name_) _type_ * _name_ = (_type_ *) _alloca(sizeof(_type_) * cc_ctx_n(_type_, _size_) )
#else
#define cc_ctx_decl(_type_, _size_, _name_) \
    CC_IGNORE_VLA_WARNINGS \
    _type_ _name_[cc_ctx_n(_type_, _size_)] \
    CC_RESTORE_VLA_WARNINGS
#endif

#define ccrsa_pub_ctx_size(_nbytes_)   (sizeof(struct cczp) + CCN_UNIT_SIZE + 3 * (ccn_sizeof_size(_nbytes_)))
#define ccrsa_priv_ctx_size(_nbytes_)  ((sizeof(struct cczp) + CCN_UNIT_SIZE) * 2 + 7 * ccn_sizeof(ccn_bitsof_size(_nbytes_) / 2 + 1))
#define ccrsa_full_ctx_size(_nbytes_)  (ccrsa_pub_ctx_size(_nbytes_) + ccn_sizeof_size(_nbytes_) + ccrsa_priv_ctx_size(_nbytes_))

#define ccrsa_full_ctx_decl(_nbytes_, _name_)   cc_ctx_decl(struct ccrsa_full_ctx, ccrsa_full_ctx_size(_nbytes_), _name_)

enum {
    CCERR_OK = 0
};

struct ccrng_state {
    int (*CC_SPTR(ccrng_state, generate))(struct ccrng_state *rng, size_t outlen, void *out);
};
struct ccrng_state *ccrng(int *error);

static inline ccrsa_pub_ctx_t ccrsa_ctx_public(ccrsa_full_ctx_t fk)
{
    return (ccrsa_pub_ctx_t) fk;
}

size_t ccrsa_pubkeylength(ccrsa_pub_ctx_t pubk);
size_t ccder_encode_rsa_pub_size(const ccrsa_pub_ctx_t key);
uint8_t *ccder_encode_rsa_pub(const ccrsa_pub_ctx_t key, uint8_t *der, uint8_t *der_end);
int ccrsa_generate_key(size_t nbits, ccrsa_full_ctx_t fk, size_t e_nbytes, const void *e_bytes, struct ccrng_state *);

extern const struct ccrsabssa_ciphersuite ccrsabssa_ciphersuite_rsa4096_sha384;
int ccrsabssa_sign_blinded_message(
    const struct ccrsabssa_ciphersuite *,
    const ccrsa_full_ctx_t key,
    const uint8_t * blinded_message, const size_t blinded_message_nbytes,
    uint8_t *signature, const size_t signature_nbytes,
    struct ccrng_state *blinding_rng);

#endif

}

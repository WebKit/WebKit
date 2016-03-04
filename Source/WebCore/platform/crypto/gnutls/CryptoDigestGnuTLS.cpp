/*
 * Copyright (C) 2014 Igalia S.L. All rights reserved.
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

#include "config.h"
#include "CryptoDigest.h"

#include <gnutls/gnutls.h>
#include <gnutls/crypto.h>

namespace WebCore {

struct CryptoDigestContext {
    gnutls_digest_algorithm_t algorithm;
    gnutls_hash_hd_t hash;
};

CryptoDigest::CryptoDigest()
    : m_context(new CryptoDigestContext)
{
}

CryptoDigest::~CryptoDigest()
{
    gnutls_hash_deinit(m_context->hash, 0);
}

std::unique_ptr<CryptoDigest> CryptoDigest::create(CryptoDigest::Algorithm algorithm)
{
    gnutls_digest_algorithm_t gnutlsAlgorithm;

    switch (algorithm) {
    case CryptoDigest::Algorithm::SHA_1: {
        gnutlsAlgorithm = GNUTLS_DIG_SHA1;
        break;
    }
    case CryptoDigest::Algorithm::SHA_224: {
        gnutlsAlgorithm = GNUTLS_DIG_SHA224;
        break;
    }
    case CryptoDigest::Algorithm::SHA_256: {
        gnutlsAlgorithm = GNUTLS_DIG_SHA256;
        break;
    }
    case CryptoDigest::Algorithm::SHA_384: {
        gnutlsAlgorithm = GNUTLS_DIG_SHA384;
        break;
    }
    case CryptoDigest::Algorithm::SHA_512: {
        gnutlsAlgorithm = GNUTLS_DIG_SHA512;
        break;
    }
    }

    std::unique_ptr<CryptoDigest> digest(new CryptoDigest);
    digest->m_context->algorithm = gnutlsAlgorithm;

    int ret = gnutls_hash_init(&digest->m_context->hash, gnutlsAlgorithm);
    if (ret != GNUTLS_E_SUCCESS)
        return nullptr;

    return digest;
}

void CryptoDigest::addBytes(const void* input, size_t length)
{
    gnutls_hash(m_context->hash, input, length);
}

Vector<uint8_t> CryptoDigest::computeHash()
{
    Vector<uint8_t> result;
    int digestLen = gnutls_hash_get_len(m_context->algorithm);
    result.resize(digestLen);

    gnutls_hash_output(m_context->hash, result.data());

    return result;
}

} // namespace WebCore

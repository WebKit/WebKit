/*
 * Copyright (C) 2014 Igalia S.L.
 * Copyright (C) 2016 SoftAtHome
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

#include <gcrypt.h>

namespace PAL {

struct CryptoDigestContext {
    int algorithm;
    gcry_md_hd_t md;
};

CryptoDigest::CryptoDigest()
    : m_context(new CryptoDigestContext)
{
}

CryptoDigest::~CryptoDigest() = default;

std::unique_ptr<CryptoDigest> CryptoDigest::create(CryptoDigest::Algorithm algorithm)
{
    int gcryptAlgorithm;

    switch (algorithm) {
    case CryptoDigest::Algorithm::SHA_1:
        gcryptAlgorithm = GCRY_MD_SHA1;
        break;
    case CryptoDigest::Algorithm::SHA_224:
        gcryptAlgorithm = GCRY_MD_SHA224;
        break;
    case CryptoDigest::Algorithm::SHA_256:
        gcryptAlgorithm = GCRY_MD_SHA256;
        break;
    case CryptoDigest::Algorithm::SHA_384:
        gcryptAlgorithm = GCRY_MD_SHA384;
        break;
    case CryptoDigest::Algorithm::SHA_512:
        gcryptAlgorithm = GCRY_MD_SHA512;
        break;
    }

    std::unique_ptr<CryptoDigest> digest(new CryptoDigest);
    digest->m_context->algorithm = gcryptAlgorithm;

    gcry_md_open(&digest->m_context->md, gcryptAlgorithm, 0);
    if (!digest->m_context->md)
        return nullptr;

    return digest;
}

void CryptoDigest::addBytes(const void* input, size_t length)
{
    gcry_md_write(m_context->md, input, length);
}

Vector<uint8_t> CryptoDigest::computeHash()
{
    int digestLen = gcry_md_get_algo_dlen(m_context->algorithm);
    Vector<uint8_t> result(digestLen);

    gcry_md_final(m_context->md);
    memcpy(result.data(), gcry_md_read(m_context->md, 0), digestLen);
    gcry_md_close(m_context->md);

    return result;
}

} // namespace PAL

/*
 * Copyright (C) 2013-2023 Apple Inc. All rights reserved.
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

#include <CommonCrypto/CommonCrypto.h>

namespace PAL {

struct CryptoDigestContext {
    CryptoDigest::Algorithm algorithm;
    void* ccContext;
};

inline CC_SHA1_CTX* toSHA1Context(CryptoDigestContext* context)
{
    ASSERT(context->algorithm == CryptoDigest::Algorithm::SHA_1);
    return static_cast<CC_SHA1_CTX*>(context->ccContext);
}
inline CC_SHA256_CTX* toSHA224Context(CryptoDigestContext* context)
{
    ASSERT(context->algorithm == CryptoDigest::Algorithm::SHA_224);
    return static_cast<CC_SHA256_CTX*>(context->ccContext);
}
inline CC_SHA256_CTX* toSHA256Context(CryptoDigestContext* context)
{
    ASSERT(context->algorithm == CryptoDigest::Algorithm::SHA_256);
    return static_cast<CC_SHA256_CTX*>(context->ccContext);
}
inline CC_SHA512_CTX* toSHA384Context(CryptoDigestContext* context)
{
    ASSERT(context->algorithm == CryptoDigest::Algorithm::SHA_384);
    return static_cast<CC_SHA512_CTX*>(context->ccContext);
}
inline CC_SHA512_CTX* toSHA512Context(CryptoDigestContext* context)
{
    ASSERT(context->algorithm == CryptoDigest::Algorithm::SHA_512);
    return static_cast<CC_SHA512_CTX*>(context->ccContext);
}

CryptoDigest::CryptoDigest()
    : m_context(new CryptoDigestContext)
{
}

CryptoDigest::~CryptoDigest()
{
    switch (m_context->algorithm) {
    case CryptoDigest::Algorithm::SHA_1:
        delete toSHA1Context(m_context.get());
        return;
    case CryptoDigest::Algorithm::SHA_224:
        delete toSHA224Context(m_context.get());
        return;
    case CryptoDigest::Algorithm::SHA_256:
        delete toSHA256Context(m_context.get());
        return;
    case CryptoDigest::Algorithm::SHA_384:
        delete toSHA384Context(m_context.get());
        return;
    case CryptoDigest::Algorithm::SHA_512:
        delete toSHA512Context(m_context.get());
        return;
    }
}


std::unique_ptr<CryptoDigest> CryptoDigest::create(CryptoDigest::Algorithm algorithm)
{
    std::unique_ptr<CryptoDigest> digest(new CryptoDigest);
    digest->m_context->algorithm = algorithm;

    switch (algorithm) {
    case CryptoDigest::Algorithm::SHA_1: {
        CC_SHA1_CTX* context = new CC_SHA1_CTX;
        digest->m_context->ccContext = context;
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        CC_SHA1_Init(context);
ALLOW_DEPRECATED_DECLARATIONS_END
        return digest;
    }
    case CryptoDigest::Algorithm::SHA_224: {
        CC_SHA256_CTX* context = new CC_SHA256_CTX;
        digest->m_context->ccContext = context;
        CC_SHA224_Init(context);
        return digest;
    }
    case CryptoDigest::Algorithm::SHA_256: {
        CC_SHA256_CTX* context = new CC_SHA256_CTX;
        digest->m_context->ccContext = context;
        CC_SHA256_Init(context);
        return digest;
    }
    case CryptoDigest::Algorithm::SHA_384: {
        CC_SHA512_CTX* context = new CC_SHA512_CTX;
        digest->m_context->ccContext = context;
        CC_SHA384_Init(context);
        return digest;
    }
    case CryptoDigest::Algorithm::SHA_512: {
        CC_SHA512_CTX* context = new CC_SHA512_CTX;
        digest->m_context->ccContext = context;
        CC_SHA512_Init(context);
        return digest;
    }
    }
}

void CryptoDigest::addBytes(const void* input, size_t length)
{
    switch (m_context->algorithm) {
    case CryptoDigest::Algorithm::SHA_1:
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        CC_SHA1_Update(toSHA1Context(m_context.get()), input, length);
ALLOW_DEPRECATED_DECLARATIONS_END
        return;
    case CryptoDigest::Algorithm::SHA_224:
        CC_SHA224_Update(toSHA224Context(m_context.get()), input, length);
        return;
    case CryptoDigest::Algorithm::SHA_256:
        CC_SHA256_Update(toSHA256Context(m_context.get()), input, length);
        return;
    case CryptoDigest::Algorithm::SHA_384:
        CC_SHA384_Update(toSHA384Context(m_context.get()), input, length);
        return;
    case CryptoDigest::Algorithm::SHA_512:
        CC_SHA512_Update(toSHA512Context(m_context.get()), input, length);
        return;
    }
}

Vector<uint8_t> CryptoDigest::computeHash()
{
    Vector<uint8_t> result;
    switch (m_context->algorithm) {
    case CryptoDigest::Algorithm::SHA_1:
        result.grow(CC_SHA1_DIGEST_LENGTH);
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        CC_SHA1_Final(result.data(), toSHA1Context(m_context.get()));
ALLOW_DEPRECATED_DECLARATIONS_END
        break;
    case CryptoDigest::Algorithm::SHA_224:
        result.grow(CC_SHA224_DIGEST_LENGTH);
        CC_SHA224_Final(result.data(), toSHA224Context(m_context.get()));
        break;
    case CryptoDigest::Algorithm::SHA_256:
        result.grow(CC_SHA256_DIGEST_LENGTH);
        CC_SHA256_Final(result.data(), toSHA256Context(m_context.get()));
        break;
    case CryptoDigest::Algorithm::SHA_384:
        result.grow(CC_SHA384_DIGEST_LENGTH);
        CC_SHA384_Final(result.data(), toSHA384Context(m_context.get()));
        break;
    case CryptoDigest::Algorithm::SHA_512:
        result.grow(CC_SHA512_DIGEST_LENGTH);
        CC_SHA512_Final(result.data(), toSHA512Context(m_context.get()));
        break;
    }
    return result;
}

String CryptoDigest::toHexString()
{
    auto hash = computeHash();

    char* start = 0;
    unsigned hashLength = hash.size();
    CString result = CString::newUninitialized(hashLength * 2, start);
    char* buffer = start;
    for (size_t i = 0; i < hashLength; ++i) {
        snprintf(buffer, 3, "%02X", hash.at(i));
        buffer += 2;
    }
    return String::fromUTF8(result);
}

} // namespace PAL

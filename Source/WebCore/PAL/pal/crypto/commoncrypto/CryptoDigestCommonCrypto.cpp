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

#if HAVE(SWIFT_CPP_INTEROP)
#include "PALSwift.h"
#endif
#include <CommonCrypto/CommonCrypto.h>
#include <optional>
#include <wtf/TZoneMallocInlines.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace PAL {

struct CryptoDigestContext {
    WTF_MAKE_STRUCT_TZONE_ALLOCATED_INLINE(CryptoDigestContext);
    CryptoDigest::Algorithm algorithm;
    std::variant<
#if HAVE(SWIFT_CPP_INTEROP)
        std::unique_ptr<PAL::Digest>,
#endif
        std::unique_ptr<CC_SHA1_CTX>,
        std::unique_ptr<CC_SHA256_CTX>,
        std::unique_ptr<CC_SHA512_CTX>
    > ccContext;
};

inline CC_SHA1_CTX* toSHA1Context(CryptoDigestContext* context)
{
    ASSERT(context->algorithm == CryptoDigest::Algorithm::SHA_1);
    return static_cast<CC_SHA1_CTX*>(std::get<std::unique_ptr<CC_SHA1_CTX>>(context->ccContext).get());
}
inline CC_SHA256_CTX* toSHA224Context(CryptoDigestContext* context)
{
    ASSERT(context->algorithm == CryptoDigest::Algorithm::SHA_224);
    return static_cast<CC_SHA256_CTX*>(std::get<std::unique_ptr<CC_SHA256_CTX>>(context->ccContext).get());
}
inline CC_SHA256_CTX* toSHA256Context(CryptoDigestContext* context)
{
    ASSERT(context->algorithm == CryptoDigest::Algorithm::SHA_256);
    return static_cast<CC_SHA256_CTX*>(std::get<std::unique_ptr<CC_SHA256_CTX>>(context->ccContext).get());
}
inline CC_SHA512_CTX* toSHA384Context(CryptoDigestContext* context)
{
    ASSERT(context->algorithm == CryptoDigest::Algorithm::SHA_384);
    return static_cast<CC_SHA512_CTX*>(std::get<std::unique_ptr<CC_SHA512_CTX>>(context->ccContext).get());
}
inline CC_SHA512_CTX* toSHA512Context(CryptoDigestContext* context)
{
    ASSERT(context->algorithm == CryptoDigest::Algorithm::SHA_512);
    return static_cast<CC_SHA512_CTX*>(std::get<std::unique_ptr<CC_SHA512_CTX>>(context->ccContext).get());
}

CryptoDigest::CryptoDigest()
    : m_context(WTF::makeUnique<CryptoDigestContext>())
{
}

CryptoDigest::~CryptoDigest()
{
}

#if HAVE(SWIFT_CPP_INTEROP)
static std::variant<std::unique_ptr<PAL::Digest>, std::unique_ptr<CC_SHA1_CTX>, std::unique_ptr<CC_SHA256_CTX>, std::unique_ptr<CC_SHA512_CTX>> createCryptoDigest(CryptoDigest::Algorithm algorithm)
#else
static std::variant<std::unique_ptr<CC_SHA1_CTX>, std::unique_ptr<CC_SHA256_CTX>, std::unique_ptr<CC_SHA512_CTX>> createCryptoDigest(CryptoDigest::Algorithm algorithm)
#endif
{
    switch (algorithm) {
#if !HAVE(SWIFT_CPP_INTEROP)
    case CryptoDigest::Algorithm::SHA_1: {
        std::unique_ptr<CC_SHA1_CTX> context = WTF::makeUniqueWithoutFastMallocCheck<CC_SHA1_CTX>();
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        CC_SHA1_Init(context.get());
        ALLOW_DEPRECATED_DECLARATIONS_END
        return context;
    }
    case CryptoDigest::Algorithm::SHA_256: {
        std::unique_ptr<CC_SHA256_CTX> context = WTF::makeUniqueWithoutFastMallocCheck<CC_SHA256_CTX>();
        CC_SHA256_Init(context.get());
        return context;
    }
    case CryptoDigest::Algorithm::SHA_384: {
        std::unique_ptr<CC_SHA512_CTX> context = WTF::makeUniqueWithoutFastMallocCheck<CC_SHA512_CTX>();
        CC_SHA384_Init(context.get());
        return context;
    }
    case CryptoDigest::Algorithm::SHA_512: {
        std::unique_ptr<CC_SHA512_CTX> context = WTF::makeUniqueWithoutFastMallocCheck<CC_SHA512_CTX>();
        CC_SHA512_Init(context.get());
        return context;
    }
#else
    case CryptoDigest::Algorithm::SHA_1: {
        return WTF::makeUniqueWithoutFastMallocCheck<PAL::Digest>(PAL::Digest::sha1Init());
    }
    case CryptoDigest::Algorithm::SHA_256: {
        return WTF::makeUniqueWithoutFastMallocCheck<PAL::Digest>(PAL::Digest::sha256Init());
    }
    case CryptoDigest::Algorithm::SHA_384: {
        return WTF::makeUniqueWithoutFastMallocCheck<PAL::Digest>(PAL::Digest::sha384Init());
    }
    case CryptoDigest::Algorithm::SHA_512: {
        return WTF::makeUniqueWithoutFastMallocCheck<PAL::Digest>(PAL::Digest::sha512Init());
    }
#endif
    case CryptoDigest::Algorithm::SHA_224: {
        std::unique_ptr<CC_SHA256_CTX> context = WTF::makeUniqueWithoutFastMallocCheck<CC_SHA256_CTX>();
        CC_SHA224_Init(context.get());
        return context;
    }
    }
}

std::unique_ptr<CryptoDigest> CryptoDigest::create(CryptoDigest::Algorithm algorithm)
{
    std::unique_ptr<CryptoDigest> digest = WTF::makeUnique<CryptoDigest>();
    ASSERT(digest->m_context);
    digest->m_context->algorithm = algorithm;
    digest->m_context->ccContext = createCryptoDigest(algorithm);
    return digest;
}

void CryptoDigest::addBytes(std::span<const uint8_t> input)
{
    switch (m_context->algorithm) {
#if !HAVE(SWIFT_CPP_INTEROP)
    case CryptoDigest::Algorithm::SHA_1:
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        CC_SHA1_Update(toSHA1Context(m_context.get()), static_cast<const void*>(input.data()), input.size());
        ALLOW_DEPRECATED_DECLARATIONS_END
        return;
    case CryptoDigest::Algorithm::SHA_256:
        CC_SHA256_Update(toSHA256Context(m_context.get()), static_cast<const void*>(input.data()), input.size());
        return;
    case CryptoDigest::Algorithm::SHA_384:
        CC_SHA384_Update(toSHA384Context(m_context.get()), static_cast<const void*>(input.data()), input.size());
        return;
    case CryptoDigest::Algorithm::SHA_512:
        CC_SHA512_Update(toSHA512Context(m_context.get()), static_cast<const void*>(input.data()), input.size());
        return;
#else
    case CryptoDigest::Algorithm::SHA_1:
    case CryptoDigest::Algorithm::SHA_256:
    case CryptoDigest::Algorithm::SHA_384:
    case CryptoDigest::Algorithm::SHA_512:
        std::get<std::unique_ptr<PAL::Digest>>(m_context->ccContext)->update(input);
        return;
#endif
    case CryptoDigest::Algorithm::SHA_224:
        CC_SHA224_Update(toSHA224Context(m_context.get()), static_cast<const void*>(input.data()), input.size());
        return;
    }
}

Vector<uint8_t> CryptoDigest::computeHash()
{
    Vector<uint8_t> result;
    switch (m_context->algorithm) {
#if !HAVE(SWIFT_CPP_INTEROP)
    case CryptoDigest::Algorithm::SHA_1:
        result.grow(CC_SHA1_DIGEST_LENGTH);
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        CC_SHA1_Final(result.data(), toSHA1Context(m_context.get()));
        ALLOW_DEPRECATED_DECLARATIONS_END
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
#else
    case CryptoDigest::Algorithm::SHA_1:
    case CryptoDigest::Algorithm::SHA_256:
    case CryptoDigest::Algorithm::SHA_384:
    case CryptoDigest::Algorithm::SHA_512:
        return std::get<std::unique_ptr<PAL::Digest>>(m_context->ccContext)->finalize();
#endif
    case CryptoDigest::Algorithm::SHA_224:
        result.grow(CC_SHA224_DIGEST_LENGTH);
        CC_SHA224_Final(result.data(), toSHA224Context(m_context.get()));
        break;

    }
    return result;
}

} // namespace PAL

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

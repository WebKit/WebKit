/*
 * Copyright (C) 2018 Sony Interactive Entertainment Inc.
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

#include <openssl/evp.h>

namespace PAL::Crypto {

struct CryptoDigestContext {
    WTF_DEPRECATED_MAKE_STRUCT_FAST_ALLOCATED(CryptoDigestContext);

    static std::unique_ptr<CryptoDigestContext> create(const EVP_MD* algorithm)
    {
        auto context = makeUnique<CryptoDigestContext>();
        if (!EVP_DigestInit_ex(context->m_context, algorithm, nullptr))
            return nullptr;
        return context;
    }

    CryptoDigestContext()
        : m_context(EVP_MD_CTX_new())
    {
    }

    ~CryptoDigestContext()
    {
        EVP_MD_CTX_free(m_context);
    }

    void addBytes(std::span<const uint8_t> input)
    {
        auto succeeded = EVP_DigestUpdate(m_context, input.data(), input.size());
        ASSERT_UNUSED(succeeded, succeeded);
    }

    Vector<uint8_t> computeHash()
    {
        Vector<uint8_t> result(EVP_MD_CTX_size(m_context));
        unsigned length = 0;
        auto succeeded = EVP_DigestFinal_ex(m_context, result.mutableSpan().data(), &length);
        ASSERT_UNUSED(succeeded, succeeded);
        ASSERT_UNUSED(length, length == result.size());
        return result;
    }

private:
    EVP_MD_CTX* m_context;
};

CryptoDigest::CryptoDigest() = default;

CryptoDigest::~CryptoDigest() = default;

static std::unique_ptr<CryptoDigestContext> createCryptoDigest(CryptoDigest::Algorithm algorithm)
{
    switch (algorithm) {
    case CryptoDigest::Algorithm::SHA_1:
        return CryptoDigestContext::create(EVP_sha1());
    case CryptoDigest::Algorithm::DEPRECATED_SHA_224:
        RELEASE_ASSERT_NOT_REACHED_WITH_MESSAGE("SHA224 is not supported.");
        return CryptoDigestContext::create(EVP_sha224());
    case CryptoDigest::Algorithm::SHA_256:
        return CryptoDigestContext::create(EVP_sha256());
    case CryptoDigest::Algorithm::SHA_384:
        return CryptoDigestContext::create(EVP_sha384());
    case CryptoDigest::Algorithm::SHA_512:
        return CryptoDigestContext::create(EVP_sha512());
    }
    return nullptr;
}

std::unique_ptr<CryptoDigest> CryptoDigest::create(CryptoDigest::Algorithm algorithm)
{
    auto context = createCryptoDigest(algorithm);
    if (!context)
        return nullptr;

    std::unique_ptr<CryptoDigest> digest = WTF::makeUnique<CryptoDigest>();
    digest->m_context = WTF::move(context);
    return digest;
}

void CryptoDigest::addBytes(std::span<const uint8_t> input)
{
    ASSERT(m_context);
    m_context->addBytes(input);
}

Vector<uint8_t> CryptoDigest::computeHash()
{
    ASSERT(m_context);
    return m_context->computeHash();
}

} // namespace PAL::Crypto

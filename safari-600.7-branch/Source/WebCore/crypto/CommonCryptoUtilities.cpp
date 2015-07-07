/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#include "CommonCryptoUtilities.h"

#if ENABLE(SUBTLE_CRYPTO)

#if defined(__has_include)
#if __has_include(<CommonCrypto/CommonBigNum.h>)
#include <CommonCrypto/CommonBigNum.h>
#endif
#endif

typedef CCCryptorStatus CCStatus;
extern "C" CCBigNumRef CCBigNumFromData(CCStatus *status, const void *s, size_t len);
extern "C" size_t CCBigNumToData(CCStatus *status, const CCBigNumRef bn, void *to);
extern "C" uint32_t CCBigNumByteCount(const CCBigNumRef bn);
extern "C" CCBigNumRef CCCreateBigNum(CCStatus *status);
extern "C" void CCBigNumFree(CCBigNumRef bn);
extern "C" CCBigNumRef CCBigNumCopy(CCStatus *status, const CCBigNumRef bn);
extern "C" CCStatus CCBigNumSubI(CCBigNumRef result, const CCBigNumRef a, const uint32_t b);
extern "C" CCStatus CCBigNumMod(CCBigNumRef result, CCBigNumRef dividend, CCBigNumRef modulus);
extern "C" CCStatus CCBigNumInverseMod(CCBigNumRef result, const CCBigNumRef a, const CCBigNumRef modulus);

namespace WebCore {

bool getCommonCryptoDigestAlgorithm(CryptoAlgorithmIdentifier hashFunction, CCDigestAlgorithm& algorithm)
{
    switch (hashFunction) {
    case CryptoAlgorithmIdentifier::SHA_1:
        algorithm = kCCDigestSHA1;
        return true;
    case CryptoAlgorithmIdentifier::SHA_224:
        algorithm = kCCDigestSHA224;
        return true;
    case CryptoAlgorithmIdentifier::SHA_256:
        algorithm = kCCDigestSHA256;
        return true;
    case CryptoAlgorithmIdentifier::SHA_384:
        algorithm = kCCDigestSHA384;
        return true;
    case CryptoAlgorithmIdentifier::SHA_512:
        algorithm = kCCDigestSHA512;
        return true;
    default:
        return false;
    }
}

CCBigNum::CCBigNum(CCBigNumRef number)
    : m_number(number)
{
}

CCBigNum::CCBigNum(const uint8_t* data, size_t size)
{
    CCStatus status = kCCSuccess;
    m_number = CCBigNumFromData(&status, data, size);
    RELEASE_ASSERT(!status);
}

CCBigNum::~CCBigNum()
{
    CCBigNumFree(m_number);
}

CCBigNum::CCBigNum(const CCBigNum& other)
{
    CCStatus status = kCCSuccess;
    m_number = CCBigNumCopy(&status, other.m_number);
    RELEASE_ASSERT(!status);
}

CCBigNum::CCBigNum(CCBigNum&& other)
{
    m_number = other.m_number;
    other.m_number = nullptr;
}

CCBigNum& CCBigNum::operator=(const CCBigNum& other)
{
    if (this == &other)
        return *this;

    CCBigNumFree(m_number);

    CCStatus status = kCCSuccess;
    m_number = CCBigNumCopy(&status, other.m_number);
    RELEASE_ASSERT(!status);
    return *this;
}

CCBigNum& CCBigNum::operator=(CCBigNum&& other)
{
    if (this == &other)
        return *this;

    m_number = other.m_number;
    other.m_number = nullptr;

    return *this;
}

Vector<uint8_t> CCBigNum::data() const
{
    Vector<uint8_t> result(CCBigNumByteCount(m_number));
    CCStatus status = kCCSuccess;
    CCBigNumToData(&status, m_number, result.data());
    RELEASE_ASSERT(!status);

    return result;
}

CCBigNum CCBigNum::operator-(uint32_t b) const
{
    CCStatus status = kCCSuccess;
    CCBigNumRef result = CCCreateBigNum(&status);
    RELEASE_ASSERT(!status);

    status = CCBigNumSubI(result, m_number, b);
    RELEASE_ASSERT(!status);

    return result;
}

CCBigNum CCBigNum::operator%(const CCBigNum& modulus) const
{
    CCStatus status = kCCSuccess;
    CCBigNumRef result = CCCreateBigNum(&status);
    RELEASE_ASSERT(!status);

    status = CCBigNumMod(result, m_number, modulus.m_number);
    RELEASE_ASSERT(!status);

    return result;
}

CCBigNum CCBigNum::inverse(const CCBigNum& modulus) const
{
    CCStatus status = kCCSuccess;
    CCBigNumRef result = CCCreateBigNum(&status);
    RELEASE_ASSERT(!status);

    status = CCBigNumInverseMod(result, m_number, modulus.m_number);
    RELEASE_ASSERT(!status);

    return result;
}

} // namespace WebCore

#endif // ENABLE(SUBTLE_CRYPTO)

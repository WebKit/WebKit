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

#ifndef CryptoKeyDataRSAComponents_h
#define CryptoKeyDataRSAComponents_h

#include "CryptoKeyData.h"
#include <wtf/Vector.h>

#if ENABLE(SUBTLE_CRYPTO)

namespace WebCore {

class CryptoKeyDataRSAComponents FINAL : public CryptoKeyData {
public:
    ENUM_CLASS(Type) {
        Public,
        Private
    };

    struct PrimeInfo {
        Vector<char> primeFactor;
        Vector<char> factorCRTExponent;
        Vector<char> factorCRTCoefficient;
    };

    static std::unique_ptr<CryptoKeyDataRSAComponents> createPublic(const Vector<char>& modulus, const Vector<char>& exponent)
    {
        return std::unique_ptr<CryptoKeyDataRSAComponents>(new CryptoKeyDataRSAComponents(modulus, exponent));
    }

    static std::unique_ptr<CryptoKeyDataRSAComponents> createPrivate(const Vector<char>& modulus, const Vector<char>& exponent, const Vector<char>& privateExponent)
    {
        return std::unique_ptr<CryptoKeyDataRSAComponents>(new CryptoKeyDataRSAComponents(modulus, exponent, privateExponent));
    }

    static std::unique_ptr<CryptoKeyDataRSAComponents> createPrivateWithAdditionalData(const Vector<char>& modulus, const Vector<char>& exponent, const Vector<char>& privateExponent, const PrimeInfo& firstPrimeInfo, const PrimeInfo& secondPrimeInfo, const Vector<PrimeInfo>& otherPrimeInfos)
    {
        return std::unique_ptr<CryptoKeyDataRSAComponents>(new CryptoKeyDataRSAComponents(modulus, exponent, privateExponent, firstPrimeInfo, secondPrimeInfo, otherPrimeInfos));
    }

    virtual ~CryptoKeyDataRSAComponents();

    Type type() const { return m_type; }

    // Private and public keys.
    const Vector<char>& modulus() const { return m_modulus; }
    const Vector<char>& exponent() const { return m_exponent; }

    // Only private keys.
    const Vector<char>& privateExponent() const { return m_privateExponent; }
    bool hasAdditionalPrivateKeyParameters() const { return m_hasAdditionalPrivateKeyParameters; }
    const PrimeInfo& firstPrimeInfo() const { return m_firstPrimeInfo; }
    const PrimeInfo& secondPrimeInfo() const { return m_secondPrimeInfo; }
    const Vector<PrimeInfo>& otherPrimeInfos() const { return m_otherPrimeInfos; }

private:
    CryptoKeyDataRSAComponents(const Vector<char>& modulus, const Vector<char>& exponent);
    CryptoKeyDataRSAComponents(const Vector<char>& modulus, const Vector<char>& exponent, const Vector<char>& privateExponent);
    CryptoKeyDataRSAComponents(const Vector<char>& modulus, const Vector<char>& exponent, const Vector<char>& privateExponent, const PrimeInfo& firstPrimeInfo, const PrimeInfo& secondPrimeInfo, const Vector<PrimeInfo>& otherPrimeInfos);

    Type m_type;

    // Private and public keys.
    Vector<char> m_modulus;
    Vector<char> m_exponent;

    // Only private keys.
    Vector<char> m_privateExponent;
    bool m_hasAdditionalPrivateKeyParameters;
    PrimeInfo m_firstPrimeInfo;
    PrimeInfo m_secondPrimeInfo;
    Vector<PrimeInfo> m_otherPrimeInfos; // When three or more primes have been used, the number of array elements is be the number of primes used minus two.
};

inline const CryptoKeyDataRSAComponents& toCryptoKeyDataRSAComponents(const CryptoKeyData& data)
{
    ASSERT(data.format() == CryptoKeyData::Format::RSAComponents);
    return static_cast<const CryptoKeyDataRSAComponents&>(data);
}

} // namespace WebCore

#endif // ENABLE(SUBTLE_CRYPTO)
#endif // CryptoKeyDataRSAComponents_h

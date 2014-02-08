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

#ifndef CryptoKeyRSA_h
#define CryptoKeyRSA_h

#include "CryptoKey.h"
#include <functional>

#if ENABLE(SUBTLE_CRYPTO)

#if OS(DARWIN) && !PLATFORM(EFL) && !PLATFORM(GTK)
typedef struct _CCRSACryptor *CCRSACryptorRef;
typedef CCRSACryptorRef PlatformRSAKey;
#endif

namespace WebCore {

class CryptoKeyDataRSAComponents;
class CryptoKeyPair;
class PromiseWrapper;

class CryptoKeyRSA final : public CryptoKey {
public:
    static PassRefPtr<CryptoKeyRSA> create(CryptoAlgorithmIdentifier identifier, CryptoKeyType type, PlatformRSAKey platformKey, bool extractable, CryptoKeyUsage usage)
    {
        return adoptRef(new CryptoKeyRSA(identifier, type, platformKey, extractable, usage));
    }
    static PassRefPtr<CryptoKeyRSA> create(CryptoAlgorithmIdentifier, const CryptoKeyDataRSAComponents&, bool extractable, CryptoKeyUsage);
    virtual ~CryptoKeyRSA();

    void restrictToHash(CryptoAlgorithmIdentifier);
    bool isRestrictedToHash(CryptoAlgorithmIdentifier&) const;

    size_t keySizeInBits() const;

    typedef std::function<void(CryptoKeyPair&)> KeyPairCallback;
    typedef std::function<void()> VoidCallback;
    static void generatePair(CryptoAlgorithmIdentifier, unsigned modulusLength, const Vector<uint8_t>& publicExponent, bool extractable, CryptoKeyUsage, KeyPairCallback, VoidCallback failureCallback);

    PlatformRSAKey platformKey() const { return m_platformKey; }

private:
    CryptoKeyRSA(CryptoAlgorithmIdentifier, CryptoKeyType, PlatformRSAKey, bool extractable, CryptoKeyUsage);

    virtual CryptoKeyClass keyClass() const override { return CryptoKeyClass::RSA; }

    virtual void buildAlgorithmDescription(CryptoAlgorithmDescriptionBuilder&) const override;
    virtual std::unique_ptr<CryptoKeyData> exportData() const override;

    PlatformRSAKey m_platformKey;

    bool m_restrictedToSpecificHash;
    CryptoAlgorithmIdentifier m_hash;
};

inline bool isCryptoKeyRSA(const CryptoKey& key)
{
    return key.keyClass() == CryptoKeyClass::RSA;
}

CRYPTO_KEY_TYPE_CASTS(CryptoKeyRSA)

} // namespace WebCore

#endif // ENABLE(SUBTLE_CRYPTO)
#endif // CryptoKeyRSA_h

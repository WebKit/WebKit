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

#if PLATFORM(GTK) || PLATFORM(EFL)
typedef struct _PlatformRSAKeyGnuTLS PlatformRSAKeyGnuTLS;
typedef PlatformRSAKeyGnuTLS *PlatformRSAKey;
#endif

namespace WebCore {

class CryptoKeyDataRSAComponents;
class CryptoKeyPair;
class PromiseWrapper;

class CryptoKeyRSA final : public CryptoKey {
public:
    static Ref<CryptoKeyRSA> create(CryptoAlgorithmIdentifier identifier, CryptoAlgorithmIdentifier hash, bool hasHash, CryptoKeyType type, PlatformRSAKey platformKey, bool extractable, CryptoKeyUsage usage)
    {
        return adoptRef(*new CryptoKeyRSA(identifier, hash, hasHash, type, platformKey, extractable, usage));
    }
    static RefPtr<CryptoKeyRSA> create(CryptoAlgorithmIdentifier, CryptoAlgorithmIdentifier hash, bool hasHash, const CryptoKeyDataRSAComponents&, bool extractable, CryptoKeyUsage);
    virtual ~CryptoKeyRSA();

    bool isRestrictedToHash(CryptoAlgorithmIdentifier&) const;

    size_t keySizeInBits() const;

    typedef std::function<void(CryptoKeyPair&)> KeyPairCallback;
    typedef std::function<void()> VoidCallback;
    static void generatePair(CryptoAlgorithmIdentifier, CryptoAlgorithmIdentifier hash, bool hasHash, unsigned modulusLength, const Vector<uint8_t>& publicExponent, bool extractable, CryptoKeyUsage, KeyPairCallback, VoidCallback failureCallback);

    PlatformRSAKey platformKey() const { return m_platformKey; }

private:
    CryptoKeyRSA(CryptoAlgorithmIdentifier, CryptoAlgorithmIdentifier hash, bool hasHash, CryptoKeyType, PlatformRSAKey, bool extractable, CryptoKeyUsage);

    CryptoKeyClass keyClass() const override { return CryptoKeyClass::RSA; }

    void buildAlgorithmDescription(CryptoAlgorithmDescriptionBuilder&) const override;
    std::unique_ptr<CryptoKeyData> exportData() const override;

    PlatformRSAKey m_platformKey;

    bool m_restrictedToSpecificHash;
    CryptoAlgorithmIdentifier m_hash;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CRYPTO_KEY(CryptoKeyRSA, CryptoKeyClass::RSA)

#endif // ENABLE(SUBTLE_CRYPTO)
#endif // CryptoKeyRSA_h

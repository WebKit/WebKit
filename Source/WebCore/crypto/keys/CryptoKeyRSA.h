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

#pragma once

#include "CryptoKey.h"
#include "ExceptionOr.h"
#include <wtf/Function.h>

#if ENABLE(WEB_CRYPTO)

#if OS(DARWIN) && !PLATFORM(GTK)
typedef struct _CCRSACryptor *CCRSACryptorRef;
typedef CCRSACryptorRef PlatformRSAKey;
#endif

#if PLATFORM(GTK) || PLATFORM(WPE)
// gcry_sexp* equates gcry_sexp_t.
struct gcry_sexp;
typedef gcry_sexp* PlatformRSAKey;
#endif

namespace WebCore {

class CryptoKeyRSAComponents;
class PromiseWrapper;
class ScriptExecutionContext;

struct CryptoKeyPair;
struct JsonWebKey;

class CryptoKeyRSA final : public CryptoKey {
public:
    static Ref<CryptoKeyRSA> create(CryptoAlgorithmIdentifier identifier, CryptoAlgorithmIdentifier hash, bool hasHash, CryptoKeyType type, PlatformRSAKey platformKey, bool extractable, CryptoKeyUsageBitmap usage)
    {
        return adoptRef(*new CryptoKeyRSA(identifier, hash, hasHash, type, platformKey, extractable, usage));
    }
    static RefPtr<CryptoKeyRSA> create(CryptoAlgorithmIdentifier, CryptoAlgorithmIdentifier hash, bool hasHash, const CryptoKeyRSAComponents&, bool extractable, CryptoKeyUsageBitmap);
    virtual ~CryptoKeyRSA();

    bool isRestrictedToHash(CryptoAlgorithmIdentifier&) const;

    size_t keySizeInBits() const;

    using KeyPairCallback = WTF::Function<void(CryptoKeyPair&&)>;
    using VoidCallback = WTF::Function<void()>;
    static void generatePair(CryptoAlgorithmIdentifier, CryptoAlgorithmIdentifier hash, bool hasHash, unsigned modulusLength, const Vector<uint8_t>& publicExponent, bool extractable, CryptoKeyUsageBitmap, KeyPairCallback&&, VoidCallback&& failureCallback, ScriptExecutionContext*);
    static RefPtr<CryptoKeyRSA> importJwk(CryptoAlgorithmIdentifier, Optional<CryptoAlgorithmIdentifier> hash, JsonWebKey&&, bool extractable, CryptoKeyUsageBitmap);
    static RefPtr<CryptoKeyRSA> importSpki(CryptoAlgorithmIdentifier, Optional<CryptoAlgorithmIdentifier> hash, Vector<uint8_t>&&, bool extractable, CryptoKeyUsageBitmap);
    static RefPtr<CryptoKeyRSA> importPkcs8(CryptoAlgorithmIdentifier, Optional<CryptoAlgorithmIdentifier> hash, Vector<uint8_t>&&, bool extractable, CryptoKeyUsageBitmap);

    PlatformRSAKey platformKey() const { return m_platformKey; }
    JsonWebKey exportJwk() const;
    ExceptionOr<Vector<uint8_t>> exportSpki() const;
    ExceptionOr<Vector<uint8_t>> exportPkcs8() const;

    std::unique_ptr<CryptoKeyRSAComponents> exportData() const;

    CryptoAlgorithmIdentifier hashAlgorithmIdentifier() const { return m_hash; }

private:
    CryptoKeyRSA(CryptoAlgorithmIdentifier, CryptoAlgorithmIdentifier hash, bool hasHash, CryptoKeyType, PlatformRSAKey, bool extractable, CryptoKeyUsageBitmap);

    CryptoKeyClass keyClass() const final { return CryptoKeyClass::RSA; }

    KeyAlgorithm algorithm() const final;

    PlatformRSAKey m_platformKey;

    bool m_restrictedToSpecificHash;
    CryptoAlgorithmIdentifier m_hash;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CRYPTO_KEY(CryptoKeyRSA, CryptoKeyClass::RSA)

#endif // ENABLE(WEB_CRYPTO)

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
#include "CryptoKeyRSA.h"

#if ENABLE(SUBTLE_CRYPTO)

#include "CryptoAlgorithmRegistry.h"
#include "CryptoKeyDataRSAComponents.h"
#include "CryptoKeyPair.h"
#include "ExceptionCode.h"
#include "NotImplemented.h"

namespace WebCore {

struct _PlatformRSAKeyGnuTLS {
};

CryptoKeyRSA::CryptoKeyRSA(CryptoAlgorithmIdentifier identifier, CryptoAlgorithmIdentifier hash, bool hasHash, CryptoKeyType type, PlatformRSAKey platformKey, bool extractable, CryptoKeyUsageBitmap usage)
    : CryptoKey(identifier, type, extractable, usage)
    , m_platformKey(platformKey)
    , m_restrictedToSpecificHash(hasHash)
    , m_hash(hash)
{
    notImplemented();
}

RefPtr<CryptoKeyRSA> CryptoKeyRSA::create(CryptoAlgorithmIdentifier identifier, CryptoAlgorithmIdentifier hash, bool hasHash, const CryptoKeyDataRSAComponents& keyData, bool extractable, CryptoKeyUsageBitmap usage)
{
    notImplemented();
    UNUSED_PARAM(identifier);
    UNUSED_PARAM(hash);
    UNUSED_PARAM(hasHash);
    UNUSED_PARAM(keyData);
    UNUSED_PARAM(extractable);
    UNUSED_PARAM(usage);

    return nullptr;
}

CryptoKeyRSA::~CryptoKeyRSA()
{
    notImplemented();
}

bool CryptoKeyRSA::isRestrictedToHash(CryptoAlgorithmIdentifier& identifier) const
{
    if (!m_restrictedToSpecificHash)
        return false;

    identifier = m_hash;
    return true;
}

size_t CryptoKeyRSA::keySizeInBits() const
{
    notImplemented();
    return 0;
}

std::unique_ptr<KeyAlgorithm> CryptoKeyRSA::buildAlgorithm() const
{
    notImplemented();
    Vector<uint8_t> publicExponent;
    return std::make_unique<RsaKeyAlgorithm>(emptyString(), 0, WTFMove(publicExponent));
}

std::unique_ptr<CryptoKeyData> CryptoKeyRSA::exportData() const
{
    ASSERT(extractable());

    notImplemented();
    return nullptr;
}

void CryptoKeyRSA::generatePair(CryptoAlgorithmIdentifier algorithm, CryptoAlgorithmIdentifier hash, bool hasHash, unsigned modulusLength, const Vector<uint8_t>& publicExponent, bool extractable, CryptoKeyUsageBitmap usage, KeyPairCallback&& callback, VoidCallback&& failureCallback, ScriptExecutionContext* context)
{
    notImplemented();
    failureCallback();

    UNUSED_PARAM(algorithm);
    UNUSED_PARAM(hash);
    UNUSED_PARAM(hasHash);
    UNUSED_PARAM(modulusLength);
    UNUSED_PARAM(publicExponent);
    UNUSED_PARAM(extractable);
    UNUSED_PARAM(usage);
    UNUSED_PARAM(callback);
    UNUSED_PARAM(context);
}

RefPtr<CryptoKeyRSA> CryptoKeyRSA::importSpki(CryptoAlgorithmIdentifier, std::optional<CryptoAlgorithmIdentifier>, Vector<uint8_t>&&, bool, CryptoKeyUsageBitmap)
{
    notImplemented();

    return nullptr;
}

ExceptionOr<Vector<uint8_t>> CryptoKeyRSA::exportSpki() const
{
    notImplemented();

    return Exception { NOT_SUPPORTED_ERR };
}

RefPtr<CryptoKeyRSA> CryptoKeyRSA::importPkcs8(CryptoAlgorithmIdentifier, std::optional<CryptoAlgorithmIdentifier>, Vector<uint8_t>&&, bool, CryptoKeyUsageBitmap)
{
    notImplemented();

    return nullptr;
}

ExceptionOr<Vector<uint8_t>> CryptoKeyRSA::exportPkcs8() const
{
    notImplemented();

    return Exception { NOT_SUPPORTED_ERR };
}

} // namespace WebCore

#endif // ENABLE(SUBTLE_CRYPTO)

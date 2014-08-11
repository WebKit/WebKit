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

#include "CryptoAlgorithmDescriptionBuilder.h"
#include "CryptoAlgorithmRegistry.h"
#include "CryptoKeyDataRSAComponents.h"
#include "CryptoKeyPair.h"
#include "NotImplemented.h"

namespace WebCore {

struct _PlatformRSAKeyGtk {
};

CryptoKeyRSA::CryptoKeyRSA(CryptoAlgorithmIdentifier identifier, CryptoKeyType type, PlatformRSAKey platformKey, bool extractable, CryptoKeyUsage usage)
    : CryptoKey(identifier, type, extractable, usage)
    , m_platformKey(platformKey)
    , m_restrictedToSpecificHash(false)
{
    notImplemented();
}

PassRefPtr<CryptoKeyRSA> CryptoKeyRSA::create(CryptoAlgorithmIdentifier identifier, const CryptoKeyDataRSAComponents& keyData, bool extractable, CryptoKeyUsage usage)
{
    notImplemented();
    UNUSED_PARAM(identifier);
    UNUSED_PARAM(keyData);
    UNUSED_PARAM(extractable);
    UNUSED_PARAM(usage);

    return nullptr;
}

CryptoKeyRSA::~CryptoKeyRSA()
{
    notImplemented();
}

void CryptoKeyRSA::restrictToHash(CryptoAlgorithmIdentifier identifier)
{
    m_restrictedToSpecificHash = true;
    m_hash = identifier;
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

void CryptoKeyRSA::buildAlgorithmDescription(CryptoAlgorithmDescriptionBuilder& builder) const
{
    notImplemented();
    UNUSED_PARAM(builder);
}

std::unique_ptr<CryptoKeyData> CryptoKeyRSA::exportData() const
{
    ASSERT(extractable());

    notImplemented();
    return nullptr;
}

void CryptoKeyRSA::generatePair(CryptoAlgorithmIdentifier algorithm, unsigned modulusLength, const Vector<uint8_t>& publicExponent, bool extractable, CryptoKeyUsage usage, KeyPairCallback callback, VoidCallback failureCallback)
{
    notImplemented();
    failureCallback();

    UNUSED_PARAM(algorithm);
    UNUSED_PARAM(modulusLength);
    UNUSED_PARAM(publicExponent);
    UNUSED_PARAM(extractable);
    UNUSED_PARAM(usage);
    UNUSED_PARAM(callback);
}

} // namespace WebCore

#endif // ENABLE(SUBTLE_CRYPTO)

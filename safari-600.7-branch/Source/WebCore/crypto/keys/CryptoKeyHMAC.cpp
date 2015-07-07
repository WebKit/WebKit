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
#include "CryptoKeyHMAC.h"

#if ENABLE(SUBTLE_CRYPTO)

#include "CryptoAlgorithmDescriptionBuilder.h"
#include "CryptoAlgorithmRegistry.h"
#include "CryptoKeyDataOctetSequence.h"
#include <wtf/text/WTFString.h>

namespace WebCore {

CryptoKeyHMAC::CryptoKeyHMAC(const Vector<uint8_t>& key, CryptoAlgorithmIdentifier hash, bool extractable, CryptoKeyUsage usage)
    : CryptoKey(CryptoAlgorithmIdentifier::HMAC, CryptoKeyType::Secret, extractable, usage)
    , m_hash(hash)
    , m_key(key)
{
}

CryptoKeyHMAC::~CryptoKeyHMAC()
{
}

PassRefPtr<CryptoKeyHMAC> CryptoKeyHMAC::generate(size_t lengthBytes, CryptoAlgorithmIdentifier hash, bool extractable, CryptoKeyUsage usages)
{
    if (!lengthBytes) {
        switch (hash) {
        case CryptoAlgorithmIdentifier::SHA_1:
        case CryptoAlgorithmIdentifier::SHA_224:
        case CryptoAlgorithmIdentifier::SHA_256:
            lengthBytes = 64;
            break;
        case CryptoAlgorithmIdentifier::SHA_384:
        case CryptoAlgorithmIdentifier::SHA_512:
            lengthBytes = 128;
            break;
        default:
            return nullptr;
        }
    }

    return adoptRef(new CryptoKeyHMAC(randomData(lengthBytes), hash, extractable, usages));
}

void CryptoKeyHMAC::buildAlgorithmDescription(CryptoAlgorithmDescriptionBuilder& builder) const
{
    CryptoKey::buildAlgorithmDescription(builder);

    auto hashDescriptionBuilder = builder.createEmptyClone();
    hashDescriptionBuilder->add("name", CryptoAlgorithmRegistry::shared().nameForIdentifier(m_hash));
    builder.add("hash", *hashDescriptionBuilder);

    builder.add("length", m_key.size());
}

std::unique_ptr<CryptoKeyData> CryptoKeyHMAC::exportData() const
{
    ASSERT(extractable());
    return CryptoKeyDataOctetSequence::create(m_key);
}

} // namespace WebCore

#endif // ENABLE(SUBTLE_CRYPTO)

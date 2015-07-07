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
#include "CryptoKeyAES.h"

#if ENABLE(SUBTLE_CRYPTO)

#include "CryptoAlgorithmDescriptionBuilder.h"
#include "CryptoAlgorithmRegistry.h"
#include "CryptoKeyDataOctetSequence.h"
#include <wtf/text/WTFString.h>

namespace WebCore {

CryptoKeyAES::CryptoKeyAES(CryptoAlgorithmIdentifier algorithm, const Vector<uint8_t>& key, bool extractable, CryptoKeyUsage usage)
    : CryptoKey(algorithm, CryptoKeyType::Secret, extractable, usage)
    , m_key(key)
{
    ASSERT(isValidAESAlgorithm(algorithm));
}

CryptoKeyAES::~CryptoKeyAES()
{
}

bool CryptoKeyAES::isValidAESAlgorithm(CryptoAlgorithmIdentifier algorithm)
{
    return algorithm == CryptoAlgorithmIdentifier::AES_CTR
        || algorithm == CryptoAlgorithmIdentifier::AES_CBC
        || algorithm == CryptoAlgorithmIdentifier::AES_CMAC
        || algorithm == CryptoAlgorithmIdentifier::AES_GCM
        || algorithm == CryptoAlgorithmIdentifier::AES_CFB
        || algorithm == CryptoAlgorithmIdentifier::AES_KW;
}

PassRefPtr<CryptoKeyAES> CryptoKeyAES::generate(CryptoAlgorithmIdentifier algorithm, size_t lengthBits, bool extractable, CryptoKeyUsage usages)
{
    if (lengthBits % 8)
        return nullptr;
    return adoptRef(new CryptoKeyAES(algorithm, randomData(lengthBits / 8), extractable, usages));
}

void CryptoKeyAES::buildAlgorithmDescription(CryptoAlgorithmDescriptionBuilder& builder) const
{
    CryptoKey::buildAlgorithmDescription(builder);
    builder.add("length", m_key.size() * 8);
}

std::unique_ptr<CryptoKeyData> CryptoKeyAES::exportData() const
{
    ASSERT(extractable());
    return CryptoKeyDataOctetSequence::create(m_key);
}

} // namespace WebCore

#endif // ENABLE(SUBTLE_CRYPTO)

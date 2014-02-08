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
#include "CryptoKey.h"

#if ENABLE(SUBTLE_CRYPTO)

#include "CryptoAlgorithmDescriptionBuilder.h"
#include "CryptoAlgorithmRegistry.h"
#include <wtf/CryptographicallyRandomNumber.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

CryptoKey::CryptoKey(CryptoAlgorithmIdentifier algorithm, CryptoKeyType type, bool extractable, CryptoKeyUsage usages)
    : m_algorithm(algorithm)
    , m_type(type)
    , m_extractable(extractable)
    , m_usages(usages)
{
}

CryptoKey::~CryptoKey()
{
}

String CryptoKey::type() const
{
    switch (m_type) {
    case CryptoKeyType::Secret:
        return ASCIILiteral("secret");
    case CryptoKeyType::Public:
        return ASCIILiteral("public");
    case CryptoKeyType::Private:
        return ASCIILiteral("private");
    }
}

void CryptoKey::buildAlgorithmDescription(CryptoAlgorithmDescriptionBuilder& builder) const
{
    builder.add("name", CryptoAlgorithmRegistry::shared().nameForIdentifier(m_algorithm));
    // Subclasses will add other keys.
}

Vector<String> CryptoKey::usages() const
{
    // The result is ordered alphabetically.
    Vector<String> result;
    if (m_usages & CryptoKeyUsageDecrypt)
        result.append(ASCIILiteral("decrypt"));
    if (m_usages & CryptoKeyUsageDeriveBits)
        result.append(ASCIILiteral("deriveBits"));
    if (m_usages & CryptoKeyUsageDeriveKey)
        result.append(ASCIILiteral("deriveKey"));
    if (m_usages & CryptoKeyUsageEncrypt)
        result.append(ASCIILiteral("encrypt"));
    if (m_usages & CryptoKeyUsageSign)
        result.append(ASCIILiteral("sign"));
    if (m_usages & CryptoKeyUsageUnwrapKey)
        result.append(ASCIILiteral("unwrapKey"));
    if (m_usages & CryptoKeyUsageVerify)
        result.append(ASCIILiteral("verify"));
    if (m_usages & CryptoKeyUsageWrapKey)
        result.append(ASCIILiteral("wrapKey"));

    return result;
}

#if !OS(DARWIN) || PLATFORM(EFL) || PLATFORM(GTK)
Vector<uint8_t> CryptoKey::randomData(size_t size)
{
    Vector<uint8_t> result(size);
    cryptographicallyRandomValues(result.data(), result.size());
    return result;
}
#endif
} // namespace WebCore

#endif // ENABLE(SUBTLE_CRYPTO)

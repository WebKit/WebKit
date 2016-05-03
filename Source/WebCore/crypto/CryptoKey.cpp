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

CryptoKey::CryptoKey(CryptoAlgorithmIdentifier algorithm, Type type, bool extractable, CryptoKeyUsage usages)
    : m_algorithm(algorithm)
    , m_type(type)
    , m_extractable(extractable)
    , m_usages(usages)
{
}

CryptoKey::~CryptoKey()
{
}

void CryptoKey::buildAlgorithmDescription(CryptoAlgorithmDescriptionBuilder& builder) const
{
    builder.add("name", CryptoAlgorithmRegistry::singleton().nameForIdentifier(m_algorithm));
    // Subclasses will add other keys.
}

auto CryptoKey::usages() const -> Vector<Usage>
{
    // The result is ordered alphabetically.
    Vector<Usage> result;
    if (m_usages & CryptoKeyUsageDecrypt)
        result.append(Usage::Decrypt);
    if (m_usages & CryptoKeyUsageDeriveBits)
        result.append(Usage::DeriveBits);
    if (m_usages & CryptoKeyUsageDeriveKey)
        result.append(Usage::DeriveKey);
    if (m_usages & CryptoKeyUsageEncrypt)
        result.append(Usage::Encrypt);
    if (m_usages & CryptoKeyUsageSign)
        result.append(Usage::Sign);
    if (m_usages & CryptoKeyUsageUnwrapKey)
        result.append(Usage::UnwrapKey);
    if (m_usages & CryptoKeyUsageVerify)
        result.append(Usage::Verify);
    if (m_usages & CryptoKeyUsageWrapKey)
        result.append(Usage::WrapKey);
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

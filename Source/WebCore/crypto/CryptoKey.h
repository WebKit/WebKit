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

#if ENABLE(SUBTLE_CRYPTO)

#include "CryptoAesKeyAlgorithm.h"
#include "CryptoAlgorithmIdentifier.h"
#include "CryptoEcKeyAlgorithm.h"
#include "CryptoHmacKeyAlgorithm.h"
#include "CryptoKeyAlgorithm.h"
#include "CryptoKeyType.h"
#include "CryptoKeyUsage.h"
#include "CryptoRsaHashedKeyAlgorithm.h"
#include "CryptoRsaKeyAlgorithm.h"
#include <wtf/Forward.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/TypeCasts.h>
#include <wtf/Variant.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class CryptoAlgorithmDescriptionBuilder;
class CryptoKeyData;

enum class CryptoKeyClass {
    AES,
    EC,
    HMAC,
    RSA,
    Raw,
};

enum class KeyAlgorithmClass {
    AES,
    EC,
    HMAC,
    HRSA,
    RSA,
    Raw,
};

class KeyAlgorithm {
public:
    virtual ~KeyAlgorithm()
    {
    }

    virtual KeyAlgorithmClass keyAlgorithmClass() const = 0;

    const String& name() const { return m_name; }

protected:
    explicit KeyAlgorithm(const String& name)
        : m_name(name)
    {
    }

private:
    String m_name;
};

class CryptoKey : public ThreadSafeRefCounted<CryptoKey> {
public:
    using Type = CryptoKeyType;
    using AlgorithmVariant = Variant<CryptoKeyAlgorithm, CryptoAesKeyAlgorithm, CryptoEcKeyAlgorithm, CryptoHmacKeyAlgorithm, CryptoRsaHashedKeyAlgorithm, CryptoRsaKeyAlgorithm>;

    CryptoKey(CryptoAlgorithmIdentifier, Type, bool extractable, CryptoKeyUsageBitmap);
    virtual ~CryptoKey();

    Type type() const;
    bool extractable() const { return m_extractable; }
    AlgorithmVariant algorithm() const;
    Vector<CryptoKeyUsage> usages() const;

    virtual CryptoKeyClass keyClass() const = 0;
    virtual std::unique_ptr<KeyAlgorithm> buildAlgorithm() const = 0;

    CryptoAlgorithmIdentifier algorithmIdentifier() const { return m_algorithmIdentifier; }
    CryptoKeyUsageBitmap usagesBitmap() const { return m_usages; }
    void setUsagesBitmap(CryptoKeyUsageBitmap usage) { m_usages = usage; };
    bool allows(CryptoKeyUsageBitmap usage) const { return usage == (m_usages & usage); }

    virtual std::unique_ptr<CryptoKeyData> exportData() const = 0;

    static Vector<uint8_t> randomData(size_t);

private:
    CryptoAlgorithmIdentifier m_algorithmIdentifier;
    Type m_type;
    bool m_extractable;
    CryptoKeyUsageBitmap m_usages;
};

inline auto CryptoKey::type() const -> Type
{
    return m_type;
}

} // namespace WebCore

#define SPECIALIZE_TYPE_TRAITS_CRYPTO_KEY(ToClassName, KeyClass) \
SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::ToClassName) \
    static bool isType(const WebCore::CryptoKey& key) { return key.keyClass() == WebCore::KeyClass; } \
SPECIALIZE_TYPE_TRAITS_END()

#define SPECIALIZE_TYPE_TRAITS_KEY_ALGORITHM(ToClassName, KeyAlgorithmClass) \
SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::ToClassName) \
    static bool isType(const WebCore::KeyAlgorithm& algorithm) { return algorithm.keyAlgorithmClass() == WebCore::KeyAlgorithmClass; } \
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(SUBTLE_CRYPTO)

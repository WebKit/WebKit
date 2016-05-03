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

#ifndef CryptoKey_h
#define CryptoKey_h

#if ENABLE(SUBTLE_CRYPTO)

#include "CryptoAlgorithmIdentifier.h"
#include "CryptoKeyType.h"
#include "CryptoKeyUsage.h"
#include <wtf/Forward.h>
#include <wtf/RefCounted.h>
#include <wtf/TypeCasts.h>
#include <wtf/Vector.h>

namespace WebCore {

class CryptoAlgorithmDescriptionBuilder;
class CryptoKeyData;

enum class CryptoKeyClass {
    HMAC,
    AES,
    RSA
};

class CryptoKey : public RefCounted<CryptoKey> {
public:
    using Type = CryptoKeyType;
    CryptoKey(CryptoAlgorithmIdentifier, Type, bool extractable, CryptoKeyUsage);
    virtual ~CryptoKey();

    virtual CryptoKeyClass keyClass() const = 0;

    Type type() const;
    bool extractable() const { return m_extractable; }
    virtual void buildAlgorithmDescription(CryptoAlgorithmDescriptionBuilder&) const;

    // FIXME: Confusing to have CryptoKeyUsage and CryptoKey::Usage named almost the same, but be slightly different.
    // CryptoKeyUsage values are bit masks so they can be combined with "or", while this is a normal enum that must
    // match what is defined in the IDL. Maybe we can rename CryptoKeyUsage to CryptoKey::UsagesBitmap?
    enum class Usage { Encrypt, Decrypt, Sign, Verify, DeriveKey, DeriveBits, WrapKey, UnwrapKey };
    Vector<Usage> usages() const;

    CryptoAlgorithmIdentifier algorithmIdentifier() const { return m_algorithm; }
    CryptoKeyUsage usagesBitmap() const { return m_usages; }
    bool allows(CryptoKeyUsage usage) const { return usage == (m_usages & usage); }

    virtual std::unique_ptr<CryptoKeyData> exportData() const = 0;

    static Vector<uint8_t> randomData(size_t);

private:
    CryptoAlgorithmIdentifier m_algorithm;
    Type m_type;
    bool m_extractable;
    CryptoKeyUsage m_usages;
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

#endif // ENABLE(SUBTLE_CRYPTO)
#endif // CryptoKey_h

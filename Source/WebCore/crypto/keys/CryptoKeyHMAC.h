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

#ifndef CryptoKeyHMAC_h
#define CryptoKeyHMAC_h

#include "CryptoKey.h"
#include <wtf/Vector.h>

#if ENABLE(SUBTLE_CRYPTO)

namespace WebCore {

class CryptoKeyHMAC FINAL : public CryptoKey {
public:
    static PassRefPtr<CryptoKeyHMAC> create(const Vector<char>& key, CryptoAlgorithmIdentifier hash, bool extractable, CryptoKeyUsage usage)
    {
        return adoptRef(new CryptoKeyHMAC(key, hash, extractable, usage));
    }
    virtual ~CryptoKeyHMAC();

    virtual CryptoKeyClass keyClass() const OVERRIDE { return CryptoKeyClass::HMAC; }

    const Vector<char>& key() const { return m_key; }

    virtual void buildAlgorithmDescription(CryptoAlgorithmDescriptionBuilder&) const OVERRIDE;

private:
    CryptoKeyHMAC(const Vector<char>& key, CryptoAlgorithmIdentifier hash, bool extractable, CryptoKeyUsage);

    CryptoAlgorithmIdentifier m_hash;
    Vector<char> m_key;
};

inline const CryptoKeyHMAC* asCryptoKeyHMAC(const CryptoKey& key)
{
    if (key.keyClass() != CryptoKeyClass::HMAC)
        return nullptr;
    return static_cast<const CryptoKeyHMAC*>(&key);
}

inline CryptoKeyHMAC* asCryptoKeyHMAC(CryptoKey& key)
{
    if (key.keyClass() != CryptoKeyClass::HMAC)
        return nullptr;
    return static_cast<CryptoKeyHMAC*>(&key);
}

} // namespace WebCore

#endif // ENABLE(SUBTLE_CRYPTO)
#endif // CryptoKeyHMAC_h

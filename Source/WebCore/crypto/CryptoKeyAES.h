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

#ifndef CryptoKeyAES_h
#define CryptoKeyAES_h

#include "CryptoAlgorithmIdentifier.h"
#include "CryptoKey.h"
#include <wtf/Vector.h>

#if ENABLE(SUBTLE_CRYPTO)

namespace WebCore {

class CryptoKeyAES FINAL : public CryptoKey {
public:
    static PassRefPtr<CryptoKeyAES> create(CryptoAlgorithmIdentifier algorithm, const Vector<char>& key, bool extractable, CryptoKeyUsage usage)
    {
        return adoptRef(new CryptoKeyAES(algorithm, key, extractable, usage));
    }
    virtual ~CryptoKeyAES();

    virtual CryptoKeyClass keyClass() const OVERRIDE { return CryptoKeyClass::AES; }

    const Vector<char>& key() const { return m_key; }

    virtual void buildAlgorithmDescription(CryptoAlgorithmDescriptionBuilder&) const OVERRIDE;

private:
    CryptoKeyAES(CryptoAlgorithmIdentifier, const Vector<char>& key, bool extractable, CryptoKeyUsage);

    Vector<char> m_key;
};

inline const CryptoKeyAES* asCryptoKeyAES(const CryptoKey& key)
{
    if (key.keyClass() != CryptoKeyClass::AES)
        return nullptr;
    return static_cast<const CryptoKeyAES*>(&key);
}

inline CryptoKeyAES* asCryptoKeyAES(CryptoKey& key)
{
    if (key.keyClass() != CryptoKeyClass::AES)
        return nullptr;
    return static_cast<CryptoKeyAES*>(&key);
}

} // namespace WebCore

#endif // ENABLE(SUBTLE_CRYPTO)


#endif // CryptoKeyAES_h

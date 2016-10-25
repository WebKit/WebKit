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

#ifndef CryptoAlgorithm_h
#define CryptoAlgorithm_h

#include "CryptoAlgorithmIdentifier.h"
#include "CryptoKeyUsage.h"
#include <wtf/Function.h>
#include <wtf/Noncopyable.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>

#if ENABLE(SUBTLE_CRYPTO)

namespace WebCore {

typedef int ExceptionCode;

class CryptoAlgorithmParameters;
class CryptoAlgorithmParametersDeprecated;
class CryptoKey;
class CryptoKeyPair;
class CryptoKeyData;
class ScriptExecutionContext;

// Data is mutable, so async operations should copy it first.
typedef std::pair<const uint8_t*, size_t> CryptoOperationData;

class CryptoAlgorithm : public RefCounted<CryptoAlgorithm> {
public:
    virtual ~CryptoAlgorithm();

    virtual CryptoAlgorithmIdentifier identifier() const = 0;

    using BoolCallback = WTF::Function<void(bool)>;
    using KeyCallback = WTF::Function<void(CryptoKey&)>;
    using KeyOrKeyPairCallback = WTF::Function<void(CryptoKey*, CryptoKeyPair*)>;
    using VectorCallback = WTF::Function<void(const Vector<uint8_t>&)>;
    using VoidCallback = WTF::Function<void()>;
    using ExceptionCallback = WTF::Function<void(ExceptionCode)>;

    virtual void generateKey(const std::unique_ptr<CryptoAlgorithmParameters>&&, bool extractable, CryptoKeyUsage, KeyOrKeyPairCallback&&, ExceptionCallback&&, ScriptExecutionContext*);

    // The following will be deprecated.
    virtual void encrypt(const CryptoAlgorithmParametersDeprecated&, const CryptoKey&, const CryptoOperationData&, VectorCallback&&, VoidCallback&& failureCallback, ExceptionCode&);
    virtual void decrypt(const CryptoAlgorithmParametersDeprecated&, const CryptoKey&, const CryptoOperationData&, VectorCallback&&, VoidCallback&& failureCallback, ExceptionCode&);
    virtual void sign(const CryptoAlgorithmParametersDeprecated&, const CryptoKey&, const CryptoOperationData&, VectorCallback&&, VoidCallback&& failureCallback, ExceptionCode&);
    virtual void verify(const CryptoAlgorithmParametersDeprecated&, const CryptoKey&, const CryptoOperationData& signature, const CryptoOperationData&, BoolCallback&&, VoidCallback&& failureCallback, ExceptionCode&);
    virtual void digest(const CryptoAlgorithmParametersDeprecated&, const CryptoOperationData&, VectorCallback&&, VoidCallback&& failureCallback, ExceptionCode&);
    virtual void generateKey(const CryptoAlgorithmParametersDeprecated&, bool extractable, CryptoKeyUsage, KeyOrKeyPairCallback&&, VoidCallback&& failureCallback, ExceptionCode&, ScriptExecutionContext*);
    virtual void deriveKey(const CryptoAlgorithmParametersDeprecated&, const CryptoKey& baseKey, CryptoAlgorithm* derivedKeyType, bool extractable, CryptoKeyUsage, KeyCallback&&, VoidCallback&& failureCallback, ExceptionCode&);
    virtual void deriveBits(const CryptoAlgorithmParametersDeprecated&, const CryptoKey& baseKey, unsigned long length, VectorCallback&&, VoidCallback&& failureCallback, ExceptionCode&);
    virtual void importKey(const CryptoAlgorithmParametersDeprecated&, const CryptoKeyData&, bool extractable, CryptoKeyUsage, KeyCallback&&, VoidCallback&& failureCallback, ExceptionCode&);

    // These are only different from encrypt/decrypt because some algorithms may not expose encrypt/decrypt.
    virtual void encryptForWrapKey(const CryptoAlgorithmParametersDeprecated&, const CryptoKey&, const CryptoOperationData&, VectorCallback&&, VoidCallback&& failureCallback, ExceptionCode&);
    virtual void decryptForUnwrapKey(const CryptoAlgorithmParametersDeprecated&, const CryptoKey&, const CryptoOperationData&, VectorCallback&&, VoidCallback&& failureCallback, ExceptionCode&);

protected:
    CryptoAlgorithm();
};

}

#endif // ENABLE(SUBTLE_CRYPTO)
#endif // CryptoAlgorithm_h

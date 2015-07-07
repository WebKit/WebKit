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

#ifndef JSCryptoKeySerializationJWK_h
#define JSCryptoKeySerializationJWK_h

#include "CryptoKeySerialization.h"
#include <heap/Strong.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

#if ENABLE(SUBTLE_CRYPTO)

namespace JSC {
class ExecState;
class JSObject;
}

namespace WebCore {

class CryptoAlgorithmParameters;
class CryptoKey;
class CryptoKeyDataRSAComponents;

class JSCryptoKeySerializationJWK final : public CryptoKeySerialization {
WTF_MAKE_NONCOPYABLE(JSCryptoKeySerializationJWK);
public:
    static std::unique_ptr<JSCryptoKeySerializationJWK> create(JSC::ExecState* exec, const String& jsonString)
    {
        return std::unique_ptr<JSCryptoKeySerializationJWK>(new JSCryptoKeySerializationJWK(exec, jsonString));
    }

    virtual ~JSCryptoKeySerializationJWK();

    static String serialize(JSC::ExecState* exec, const CryptoKey&);

private:
    JSCryptoKeySerializationJWK(JSC::ExecState*, const String&);

    virtual bool reconcileAlgorithm(std::unique_ptr<CryptoAlgorithm>&, std::unique_ptr<CryptoAlgorithmParameters>&) const override;

    virtual void reconcileUsages(CryptoKeyUsage&) const override;
    virtual void reconcileExtractable(bool&) const override;

    virtual std::unique_ptr<CryptoKeyData> keyData() const override;

    bool keySizeIsValid(size_t sizeInBits) const;
    std::unique_ptr<CryptoKeyData> keyDataOctetSequence() const;
    std::unique_ptr<CryptoKeyData> keyDataRSAComponents() const;

    JSC::ExecState* m_exec;
    JSC::Strong<JSC::JSObject> m_json;

    mutable String m_jwkAlgorithmName; // Stored when reconcileAlgorithm is called, and used later.
};

}

#endif // ENABLE(SUBTLE_CRYPTO)
#endif // JSCryptoKeySerializationJWK_h

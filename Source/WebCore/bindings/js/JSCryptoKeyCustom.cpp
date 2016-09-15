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
#include "JSCryptoKey.h"

#if ENABLE(SUBTLE_CRYPTO)

#include "CryptoKeyAES.h"
#include "CryptoKeyHMAC.h"
#include "CryptoKeyRSA.h"
#include "JSCryptoAlgorithmBuilder.h"
#include <heap/HeapInlines.h>
#include <runtime/JSCJSValueInlines.h>

using namespace JSC;

namespace WebCore {

JSValue JSCryptoKey::algorithm(JSC::ExecState& state) const
{
    if (m_algorithm)
        return m_algorithm.get();

    JSCryptoAlgorithmBuilder builder(&state);

    std::unique_ptr<KeyAlgorithm> algorithm = wrapped().buildAlgorithm();
    switch (algorithm->keyAlgorithmClass()) {
    case KeyAlgorithmClass::AES: {
        auto& aesAlgorithm = downcast<AesKeyAlgorithm>(*algorithm);
        builder.add("name", aesAlgorithm.name());
        builder.add("length", aesAlgorithm.length());
        break;
    }
    case KeyAlgorithmClass::HMAC: {
        auto& hmacAlgorithm = downcast<HmacKeyAlgorithm>(*algorithm);
        builder.add("name", hmacAlgorithm.name());
        JSCryptoAlgorithmBuilder hmacHash(&state);
        hmacHash.add("name", hmacAlgorithm.hash());
        builder.add("hash", hmacHash);
        builder.add("length", hmacAlgorithm.length());
        break;
    }
    case KeyAlgorithmClass::HRSA: {
        auto& rsaAlgorithm = downcast<RsaHashedKeyAlgorithm>(*algorithm);
        builder.add("name", rsaAlgorithm.name());
        builder.add("modulusLength", rsaAlgorithm.modulusLength());
        builder.add("publicExponent", rsaAlgorithm.publicExponent());
        JSCryptoAlgorithmBuilder rsaHash(&state);
        rsaHash.add("name", rsaAlgorithm.hash());
        builder.add("hash", rsaHash);
        break;
    }
    case KeyAlgorithmClass::RSA: {
        auto& rsaAlgorithm = downcast<RsaKeyAlgorithm>(*algorithm);
        builder.add("name", rsaAlgorithm.name());
        builder.add("modulusLength", rsaAlgorithm.modulusLength());
        builder.add("publicExponent", rsaAlgorithm.publicExponent());
        break;
    }
    }

    m_algorithm.set(state.vm(), this, builder.result());
    return m_algorithm.get();
}

} // namespace WebCore

#endif // ENABLE(SUBTLE_CRYPTO)

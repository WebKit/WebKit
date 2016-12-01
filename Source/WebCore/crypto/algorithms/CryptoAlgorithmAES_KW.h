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

#include "CryptoAlgorithm.h"

#if ENABLE(SUBTLE_CRYPTO)

namespace WebCore {

class CryptoKeyAES;

class CryptoAlgorithmAES_KW final : public CryptoAlgorithm {
public:
    static constexpr const char* s_name = "AES-KW";
    static constexpr CryptoAlgorithmIdentifier s_identifier = CryptoAlgorithmIdentifier::AES_KW;
    static Ref<CryptoAlgorithm> create();

private:
    CryptoAlgorithmAES_KW() = default;
    CryptoAlgorithmIdentifier identifier() const final;

    void generateKey(const CryptoAlgorithmParameters&, bool extractable, CryptoKeyUsageBitmap, KeyOrKeyPairCallback&&, ExceptionCallback&&, ScriptExecutionContext&) final;
    void importKey(SubtleCrypto::KeyFormat, KeyData&&, const std::unique_ptr<CryptoAlgorithmParameters>&&, bool extractable, CryptoKeyUsageBitmap, KeyCallback&&, ExceptionCallback&&) final;
    void exportKey(SubtleCrypto::KeyFormat, Ref<CryptoKey>&&, KeyDataCallback&&, ExceptionCallback&&) final;
    void wrapKey(Ref<CryptoKey>&&, Vector<uint8_t>&&, VectorCallback&&, ExceptionCallback&&) final;
    void unwrapKey(Ref<CryptoKey>&&, Vector<uint8_t>&&, VectorCallback&&, ExceptionCallback&&) final;

    ExceptionOr<void> encryptForWrapKey(const CryptoAlgorithmParametersDeprecated&, const CryptoKey&, const CryptoOperationData&, VectorCallback&&, VoidCallback&& failureCallback) final;
    ExceptionOr<void> decryptForUnwrapKey(const CryptoAlgorithmParametersDeprecated&, const CryptoKey&, const CryptoOperationData&, VectorCallback&&, VoidCallback&& failureCallback) final;
    ExceptionOr<void> generateKey(const CryptoAlgorithmParametersDeprecated&, bool extractable, CryptoKeyUsageBitmap, KeyOrKeyPairCallback&&, VoidCallback&& failureCallback, ScriptExecutionContext&) final;
    ExceptionOr<void> importKey(const CryptoAlgorithmParametersDeprecated&, const CryptoKeyData&, bool extractable, CryptoKeyUsageBitmap, KeyCallback&&, VoidCallback&& failureCallback) final;

    bool keyAlgorithmMatches(const CryptoKey&) const;
    void platformWrapKey(Ref<CryptoKey>&&, Vector<uint8_t>&&, VectorCallback&&, ExceptionCallback&&);
    void platformUnwrapKey(Ref<CryptoKey>&&, Vector<uint8_t>&&, VectorCallback&&, ExceptionCallback&&);
    ExceptionOr<void> platformEncrypt(const CryptoKeyAES&, const CryptoOperationData&, VectorCallback&&, VoidCallback&& failureCallback);
    ExceptionOr<void> platformDecrypt(const CryptoKeyAES&, const CryptoOperationData&, VectorCallback&&, VoidCallback&& failureCallback);
};

} // namespace WebCore

#endif // ENABLE(SUBTLE_CRYPTO)

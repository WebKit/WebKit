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

class CryptoAlgorithmHmacParamsDeprecated;
class CryptoKeyHMAC;

class CryptoAlgorithmHMAC final : public CryptoAlgorithm {
public:
    static constexpr const char* s_name = "HMAC";
    static constexpr CryptoAlgorithmIdentifier s_identifier = CryptoAlgorithmIdentifier::HMAC;
    static Ref<CryptoAlgorithm> create();

private:
    CryptoAlgorithmHMAC() = default;
    CryptoAlgorithmIdentifier identifier() const final;

    void sign(Ref<CryptoKey>&&, Vector<uint8_t>&&, VectorCallback&&, ExceptionCallback&&, ScriptExecutionContext&, WorkQueue&) final;
    void verify(Ref<CryptoKey>&&, Vector<uint8_t>&& signature, Vector<uint8_t>&&, BoolCallback&&, ExceptionCallback&&, ScriptExecutionContext&, WorkQueue&) final;
    void generateKey(const CryptoAlgorithmParameters&, bool extractable, CryptoKeyUsageBitmap, KeyOrKeyPairCallback&&, ExceptionCallback&&, ScriptExecutionContext&) final;
    void importKey(SubtleCrypto::KeyFormat, KeyData&&, const std::unique_ptr<CryptoAlgorithmParameters>&&, bool extractable, CryptoKeyUsageBitmap, KeyCallback&&, ExceptionCallback&&) final;
    void exportKey(SubtleCrypto::KeyFormat, Ref<CryptoKey>&&, KeyDataCallback&&, ExceptionCallback&&) final;

    // The following will be deprecated.
    ExceptionOr<void> sign(const CryptoAlgorithmParametersDeprecated&, const CryptoKey&, const CryptoOperationData&, VectorCallback&&, VoidCallback&& failureCallback) final;
    ExceptionOr<void> verify(const CryptoAlgorithmParametersDeprecated&, const CryptoKey&, const CryptoOperationData& signature, const CryptoOperationData&, BoolCallback&&, VoidCallback&& failureCallback) final;
    ExceptionOr<void> generateKey(const CryptoAlgorithmParametersDeprecated&, bool extractable, CryptoKeyUsageBitmap, KeyOrKeyPairCallback&&, VoidCallback&& failureCallback, ScriptExecutionContext&) final;
    ExceptionOr<void> importKey(const CryptoAlgorithmParametersDeprecated&, const CryptoKeyData&, bool extractable, CryptoKeyUsageBitmap, KeyCallback&&, VoidCallback&& failureCallback) final;

    bool keyAlgorithmMatches(const CryptoAlgorithmHmacParamsDeprecated& algorithmParameters, const CryptoKey&) const;
    void platformSign(Ref<CryptoKey>&&, Vector<uint8_t>&&, VectorCallback&&, ExceptionCallback&&, ScriptExecutionContext&, WorkQueue&);
    void platformVerify(Ref<CryptoKey>&&, Vector<uint8_t>&& signature, Vector<uint8_t>&&, BoolCallback&&, ExceptionCallback&&, ScriptExecutionContext&, WorkQueue&);
    ExceptionOr<void> platformSign(const CryptoAlgorithmHmacParamsDeprecated&, const CryptoKeyHMAC&, const CryptoOperationData&, VectorCallback&&, VoidCallback&& failureCallback);
    ExceptionOr<void> platformVerify(const CryptoAlgorithmHmacParamsDeprecated&, const CryptoKeyHMAC&, const CryptoOperationData& signature, const CryptoOperationData&, BoolCallback&&, VoidCallback&& failureCallback);
};

} // namespace WebCore

#endif // ENABLE(SUBTLE_CRYPTO)

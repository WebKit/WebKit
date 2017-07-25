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
#include "CryptoAlgorithm.h"

#if ENABLE(SUBTLE_CRYPTO)

namespace WebCore {

void CryptoAlgorithm::encrypt(std::unique_ptr<CryptoAlgorithmParameters>&&, Ref<CryptoKey>&&, Vector<uint8_t>&&, VectorCallback&&, ExceptionCallback&& exceptionCallback, ScriptExecutionContext&, WorkQueue&)
{
    exceptionCallback(NotSupportedError);
}

void CryptoAlgorithm::decrypt(std::unique_ptr<CryptoAlgorithmParameters>&&, Ref<CryptoKey>&&, Vector<uint8_t>&&, VectorCallback&&, ExceptionCallback&& exceptionCallback, ScriptExecutionContext&, WorkQueue&)
{
    exceptionCallback(NotSupportedError);
}

void CryptoAlgorithm::sign(std::unique_ptr<CryptoAlgorithmParameters>&&, Ref<CryptoKey>&&, Vector<uint8_t>&&, VectorCallback&&, ExceptionCallback&& exceptionCallback, ScriptExecutionContext&, WorkQueue&)
{
    exceptionCallback(NotSupportedError);
}

void CryptoAlgorithm::verify(std::unique_ptr<CryptoAlgorithmParameters>&&, Ref<CryptoKey>&&, Vector<uint8_t>&&, Vector<uint8_t>&&, BoolCallback&&, ExceptionCallback&& exceptionCallback, ScriptExecutionContext&, WorkQueue&)
{
    exceptionCallback(NotSupportedError);
}

void CryptoAlgorithm::digest(Vector<uint8_t>&&, VectorCallback&&, ExceptionCallback&& exceptionCallback, ScriptExecutionContext&, WorkQueue&)
{
    exceptionCallback(NotSupportedError);
}

void CryptoAlgorithm::generateKey(const CryptoAlgorithmParameters&, bool, CryptoKeyUsageBitmap, KeyOrKeyPairCallback&&, ExceptionCallback&& exceptionCallback, ScriptExecutionContext&)
{
    exceptionCallback(NotSupportedError);
}

void CryptoAlgorithm::deriveBits(std::unique_ptr<CryptoAlgorithmParameters>&&, Ref<CryptoKey>&&, size_t, VectorCallback&&, ExceptionCallback&& exceptionCallback, ScriptExecutionContext&, WorkQueue&)
{
    exceptionCallback(NotSupportedError);
}

void CryptoAlgorithm::importKey(SubtleCrypto::KeyFormat, KeyData&&, const std::unique_ptr<CryptoAlgorithmParameters>&&, bool, CryptoKeyUsageBitmap, KeyCallback&&, ExceptionCallback&& exceptionCallback)
{
    exceptionCallback(NotSupportedError);
}

void CryptoAlgorithm::exportKey(SubtleCrypto::KeyFormat, Ref<CryptoKey>&&, KeyDataCallback&&, ExceptionCallback&& exceptionCallback)
{
    exceptionCallback(NotSupportedError);
}

void CryptoAlgorithm::wrapKey(Ref<CryptoKey>&&, Vector<uint8_t>&&, VectorCallback&&, ExceptionCallback&& exceptionCallback)
{
    exceptionCallback(NotSupportedError);
}

void CryptoAlgorithm::unwrapKey(Ref<CryptoKey>&&, Vector<uint8_t>&&, VectorCallback&&, ExceptionCallback&& exceptionCallback)
{
    exceptionCallback(NotSupportedError);
}

ExceptionOr<size_t> CryptoAlgorithm::getKeyLength(const CryptoAlgorithmParameters&)
{
    return Exception { NotSupportedError };
}

ExceptionOr<void> CryptoAlgorithm::encrypt(const CryptoAlgorithmParametersDeprecated&, const CryptoKey&, const CryptoOperationData&, VectorCallback&&, VoidCallback&&)
{
    return Exception { NotSupportedError };
}

ExceptionOr<void> CryptoAlgorithm::decrypt(const CryptoAlgorithmParametersDeprecated&, const CryptoKey&, const CryptoOperationData&, VectorCallback&&, VoidCallback&&)
{
    return Exception { NotSupportedError };
}

ExceptionOr<void> CryptoAlgorithm::sign(const CryptoAlgorithmParametersDeprecated&, const CryptoKey&, const CryptoOperationData&, VectorCallback&&, VoidCallback&&)
{
    return Exception { NotSupportedError };
}

ExceptionOr<void> CryptoAlgorithm::verify(const CryptoAlgorithmParametersDeprecated&, const CryptoKey&, const CryptoOperationData&, const CryptoOperationData&, BoolCallback&&, VoidCallback&&)
{
    return Exception { NotSupportedError };
}

ExceptionOr<void> CryptoAlgorithm::digest(const CryptoAlgorithmParametersDeprecated&, const CryptoOperationData&, VectorCallback&&, VoidCallback&&)
{
    return Exception { NotSupportedError };
}

ExceptionOr<void> CryptoAlgorithm::generateKey(const CryptoAlgorithmParametersDeprecated&, bool, CryptoKeyUsageBitmap, KeyOrKeyPairCallback&&, VoidCallback&&, ScriptExecutionContext&)
{
    return Exception { NotSupportedError };
}

ExceptionOr<void> CryptoAlgorithm::deriveKey(const CryptoAlgorithmParametersDeprecated&, const CryptoKey&, CryptoAlgorithm*, bool, CryptoKeyUsageBitmap, KeyCallback&&, VoidCallback&&)
{
    return Exception { NotSupportedError };
}

ExceptionOr<void> CryptoAlgorithm::deriveBits(const CryptoAlgorithmParametersDeprecated&, const CryptoKey&, size_t, VectorCallback&&, VoidCallback&&)
{
    return Exception { NotSupportedError };
}

ExceptionOr<void> CryptoAlgorithm::importKey(const CryptoAlgorithmParametersDeprecated&, const CryptoKeyData&, bool, CryptoKeyUsageBitmap, KeyCallback&&, VoidCallback&&)
{
    return Exception { NotSupportedError };
}

ExceptionOr<void> CryptoAlgorithm::encryptForWrapKey(const CryptoAlgorithmParametersDeprecated& parameters, const CryptoKey& key, const CryptoOperationData& data, VectorCallback&& callback, VoidCallback&& failureCallback)
{
    return encrypt(parameters, key, data, WTFMove(callback), WTFMove(failureCallback));
}

ExceptionOr<void> CryptoAlgorithm::decryptForUnwrapKey(const CryptoAlgorithmParametersDeprecated& parameters, const CryptoKey& key, const CryptoOperationData& data, VectorCallback&& callback, VoidCallback&& failureCallback)
{
    return decrypt(parameters, key, data, WTFMove(callback), WTFMove(failureCallback));
}

}

#endif

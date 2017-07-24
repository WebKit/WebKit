/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
 * Copyright (C) 2017 Metrological Group B.V.
 * Copyright (C) 2017 Igalia S.L.
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
#include "CryptoAlgorithmPBKDF2.h"

#if ENABLE(SUBTLE_CRYPTO)

#include "CryptoAlgorithmPbkdf2Params.h"
#include "CryptoKeyRaw.h"
#include "GCryptUtilities.h"
#include "ScriptExecutionContext.h"

namespace WebCore {

static std::optional<Vector<uint8_t>> gcryptDeriveBits(const Vector<uint8_t>& keyData, const Vector<uint8_t>& saltData, CryptoAlgorithmIdentifier hashIdentifier, size_t iterations, size_t length)
{
    // Determine the hash algorithm that will be used for derivation.
    auto hashAlgorithm = digestAlgorithm(hashIdentifier);
    if (!hashAlgorithm)
        return std::nullopt;

    // Length, in bits, is a multiple of 8, as guaranteed by CryptoAlgorithmPBKDF2::deriveBits().
    ASSERT(!(length % 8));

    // Derive bits using PBKDF2, the specified hash algorithm, salt data and iteration count.
    Vector<uint8_t> result(length / 8);
    gcry_error_t error = gcry_kdf_derive(keyData.data(), keyData.size(), GCRY_KDF_PBKDF2, *hashAlgorithm, saltData.data(), saltData.size(), iterations, result.size(), result.data());
    if (error != GPG_ERR_NO_ERROR) {
        PAL::GCrypt::logError(error);
        return std::nullopt;
    }

    return result;
}

void CryptoAlgorithmPBKDF2::platformDeriveBits(std::unique_ptr<CryptoAlgorithmParameters>&& parameters, Ref<CryptoKey>&& key, size_t length, VectorCallback&& callback, ExceptionCallback&& exceptionCallback, ScriptExecutionContext& context, WorkQueue& workQueue)
{
    context.ref();
    workQueue.dispatch(
        [parameters = WTFMove(parameters), key = WTFMove(key), length, callback = WTFMove(callback), exceptionCallback = WTFMove(exceptionCallback), &context]() mutable {
            auto& pbkdf2Parameters = downcast<CryptoAlgorithmPbkdf2Params>(*parameters);
            auto& rawKey = downcast<CryptoKeyRaw>(key.get());

            auto output = gcryptDeriveBits(rawKey.key(), pbkdf2Parameters.saltVector(), pbkdf2Parameters.hashIdentifier, pbkdf2Parameters.iterations, length);
            if (!output) {
                // We should only dereference callbacks after being back to the Document/Worker threads.
                context.postTask(
                    [callback = WTFMove(callback), exceptionCallback = WTFMove(exceptionCallback)](ScriptExecutionContext& context) {
                        exceptionCallback(OperationError);
                        context.deref();
                    });
                return;
            }

            // We should only dereference callbacks after being back to the Document/Worker threads.
            context.postTask(
                [output = WTFMove(*output), callback = WTFMove(callback), exceptionCallback = WTFMove(exceptionCallback)](ScriptExecutionContext& context) {
                    callback(output);
                    context.deref();
                });
        });
}

} // namespace WebCore

#endif // ENABLE(SUBTLE_CRYPTO)

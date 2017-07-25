/*
 * Copyright (C) 2014 Igalia S.L. All rights reserved.
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
#include "CryptoAlgorithmRSA_OAEP.h"

#if ENABLE(SUBTLE_CRYPTO)

#include "CryptoAlgorithmRsaOaepParams.h"
#include "CryptoKeyRSA.h"
#include "GCryptUtilities.h"
#include "NotImplemented.h"
#include "ScriptExecutionContext.h"

namespace WebCore {

static std::optional<Vector<uint8_t>> gcryptEncrypt(CryptoAlgorithmIdentifier hashAlgorithmIdentifier, gcry_sexp_t keySexp, const Vector<uint8_t>& labelVector, const Vector<uint8_t>& plainText)
{
    // Embed the plain-text data in a data s-expression using OAEP padding.
    // Empty label data is properly handled by gcry_sexp_build().
    PAL::GCrypt::Handle<gcry_sexp_t> dataSexp;
    {
        auto shaAlgorithm = hashAlgorithmName(hashAlgorithmIdentifier);
        if (!shaAlgorithm)
            return std::nullopt;

        gcry_error_t error = gcry_sexp_build(&dataSexp, nullptr, "(data(flags oaep)(hash-algo %s)(label %b)(value %b))",
            *shaAlgorithm, labelVector.size(), labelVector.data(), plainText.size(), plainText.data());
        if (error != GPG_ERR_NO_ERROR) {
            PAL::GCrypt::logError(error);
            return std::nullopt;
        }
    }

    // Encrypt data with the provided key. The returned s-expression is of this form:
    // (enc-val
    //   (flags oaep)
    //   (rsa
    //     (a a-mpi)))
    PAL::GCrypt::Handle<gcry_sexp_t> cipherSexp;
    gcry_error_t error = gcry_pk_encrypt(&cipherSexp, dataSexp, keySexp);
    if (error != GPG_ERR_NO_ERROR) {
        PAL::GCrypt::logError(error);
        return std::nullopt;
    }

    // Return MPI data of the embedded a integer.
    PAL::GCrypt::Handle<gcry_sexp_t> aSexp(gcry_sexp_find_token(cipherSexp, "a", 0));
    if (!aSexp)
        return std::nullopt;

    return mpiData(aSexp);
}

static std::optional<Vector<uint8_t>> gcryptDecrypt(CryptoAlgorithmIdentifier hashAlgorithmIdentifier, gcry_sexp_t keySexp, const Vector<uint8_t>& labelVector, const Vector<uint8_t>& cipherText)
{
    // Embed the cipher-text data in an enc-val s-expression using OAEP padding.
    // Empty label data is properly handled by gcry_sexp_build().
    PAL::GCrypt::Handle<gcry_sexp_t> encValSexp;
    {
        auto shaAlgorithm = hashAlgorithmName(hashAlgorithmIdentifier);
        if (!shaAlgorithm)
            return std::nullopt;

        gcry_error_t error = gcry_sexp_build(&encValSexp, nullptr, "(enc-val(flags oaep)(hash-algo %s)(label %b)(rsa(a %b)))",
            *shaAlgorithm, labelVector.size(), labelVector.data(), cipherText.size(), cipherText.data());
        if (error != GPG_ERR_NO_ERROR) {
            PAL::GCrypt::logError(error);
            return std::nullopt;
        }
    }

    // Decrypt data with the provided key. The returned s-expression is of this form:
    // (data
    //   (flags oaep)
    //   (value block))
    PAL::GCrypt::Handle<gcry_sexp_t> plainSexp;
    gcry_error_t error = gcry_pk_decrypt(&plainSexp, encValSexp, keySexp);
    if (error != GPG_ERR_NO_ERROR) {
        PAL::GCrypt::logError(error);
        return std::nullopt;
    }

    // Return MPI data of the embedded value integer.
    PAL::GCrypt::Handle<gcry_sexp_t> valueSexp(gcry_sexp_find_token(plainSexp, "value", 0));
    if (!valueSexp)
        return std::nullopt;

    return mpiData(valueSexp);
}

void CryptoAlgorithmRSA_OAEP::platformEncrypt(std::unique_ptr<CryptoAlgorithmParameters>&& parameters, Ref<CryptoKey>&& key, Vector<uint8_t>&& plainText, VectorCallback&& callback, ExceptionCallback&& exceptionCallback, ScriptExecutionContext& context, WorkQueue& workQueue)
{
    context.ref();
    workQueue.dispatch(
        [parameters = WTFMove(parameters), key = WTFMove(key), plainText = WTFMove(plainText), callback = WTFMove(callback), exceptionCallback = WTFMove(exceptionCallback), &context]() mutable {
            auto& rsaParameters = downcast<CryptoAlgorithmRsaOaepParams>(*parameters);
            auto& rsaKey = downcast<CryptoKeyRSA>(key.get());

            auto output = gcryptEncrypt(rsaKey.hashAlgorithmIdentifier(), rsaKey.platformKey(), rsaParameters.labelVector(), plainText);
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

void CryptoAlgorithmRSA_OAEP::platformDecrypt(std::unique_ptr<CryptoAlgorithmParameters>&& parameters, Ref<CryptoKey>&& key, Vector<uint8_t>&& cipherText, VectorCallback&& callback, ExceptionCallback&& exceptionCallback, ScriptExecutionContext& context, WorkQueue& workQueue)
{
    context.ref();
    workQueue.dispatch(
        [parameters = WTFMove(parameters), key = WTFMove(key), cipherText = WTFMove(cipherText), callback = WTFMove(callback), exceptionCallback = WTFMove(exceptionCallback), &context]() mutable {
            auto& rsaParameters = downcast<CryptoAlgorithmRsaOaepParams>(*parameters);
            auto& rsaKey = downcast<CryptoKeyRSA>(key.get());

            auto output = gcryptDecrypt(rsaKey.hashAlgorithmIdentifier(), rsaKey.platformKey(), rsaParameters.labelVector(), cipherText);
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

ExceptionOr<void> CryptoAlgorithmRSA_OAEP::platformEncrypt(const CryptoAlgorithmRsaOaepParamsDeprecated&, const CryptoKeyRSA&, const CryptoOperationData&, VectorCallback&&, VoidCallback&&)
{
    notImplemented();
    return Exception { NotSupportedError };
}

ExceptionOr<void> CryptoAlgorithmRSA_OAEP::platformDecrypt(const CryptoAlgorithmRsaOaepParamsDeprecated&, const CryptoKeyRSA&, const CryptoOperationData&, VectorCallback&&, VoidCallback&&)
{
    notImplemented();
    return Exception { NotSupportedError };
}

} // namespace WebCore

#endif // ENABLE(SUBTLE_CRYPTO)

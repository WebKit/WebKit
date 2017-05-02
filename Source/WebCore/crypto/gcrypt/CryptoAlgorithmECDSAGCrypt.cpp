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
#include "CryptoAlgorithmECDSA.h"

#if ENABLE(SUBTLE_CRYPTO)

#include "CryptoAlgorithmEcdsaParams.h"
#include "CryptoKeyEC.h"
#include "ExceptionCode.h"
#include "ScriptExecutionContext.h"
#include <pal/crypto/CryptoDigest.h>
#include <pal/crypto/gcrypt/Handle.h>
#include <pal/crypto/gcrypt/Utilities.h>

namespace WebCore {

static std::optional<PAL::CryptoDigest::Algorithm> hashCryptoDigestAlgorithm(CryptoAlgorithmIdentifier identifier)
{
    switch (identifier) {
    case CryptoAlgorithmIdentifier::SHA_1:
        return PAL::CryptoDigest::Algorithm::SHA_1;
    case CryptoAlgorithmIdentifier::SHA_224:
        return PAL::CryptoDigest::Algorithm::SHA_224;
    case CryptoAlgorithmIdentifier::SHA_256:
        return PAL::CryptoDigest::Algorithm::SHA_256;
    case CryptoAlgorithmIdentifier::SHA_384:
        return PAL::CryptoDigest::Algorithm::SHA_384;
    case CryptoAlgorithmIdentifier::SHA_512:
        return PAL::CryptoDigest::Algorithm::SHA_512;
    default:
        return std::nullopt;
    }
}

static std::optional<const char*> hashAlgorithmName(CryptoAlgorithmIdentifier identifier)
{
    switch (identifier) {
    case CryptoAlgorithmIdentifier::SHA_1:
        return "sha1";
    case CryptoAlgorithmIdentifier::SHA_224:
        return "sha224";
    case CryptoAlgorithmIdentifier::SHA_256:
        return "sha256";
    case CryptoAlgorithmIdentifier::SHA_384:
        return "sha384";
    case CryptoAlgorithmIdentifier::SHA_512:
        return "sha512";
    default:
        return std::nullopt;
    }
}

std::optional<Vector<uint8_t>> mpiData(gcry_sexp_t paramSexp)
{
    // Retrieve the MPI value stored in the s-expression: (name mpi-data)
    PAL::GCrypt::Handle<gcry_mpi_t> paramMPI(gcry_sexp_nth_mpi(paramSexp, 1, GCRYMPI_FMT_USG));
    if (!paramMPI)
        return std::nullopt;

    // Query the data length first to properly prepare the buffer.
    size_t dataLength = 0;
    gcry_error_t error = gcry_mpi_print(GCRYMPI_FMT_USG, nullptr, 0, &dataLength, paramMPI);
    if (error != GPG_ERR_NO_ERROR) {
        PAL::GCrypt::logError(error);
        return std::nullopt;
    }

    // Finally, copy the MPI data into a properly-sized buffer.
    Vector<uint8_t> output(dataLength);
    error = gcry_mpi_print(GCRYMPI_FMT_USG, output.data(), output.size(), nullptr, paramMPI);
    if (error != GPG_ERR_NO_ERROR) {
        PAL::GCrypt::logError(error);
        return std::nullopt;
    }

    return output;
}

static std::optional<Vector<uint8_t>> gcryptSign(gcry_sexp_t keySexp, const Vector<uint8_t>& data, CryptoAlgorithmIdentifier hashAlgorithmIdentifier, size_t keySizeInBytes)
{
    // Perform digest operation with the specified algorithm on the given data.
    Vector<uint8_t> dataHash;
    {
        auto digestAlgorithm = hashCryptoDigestAlgorithm(hashAlgorithmIdentifier);
        if (!digestAlgorithm)
            return std::nullopt;

        auto digest = PAL::CryptoDigest::create(*digestAlgorithm);
        if (!digest)
            return std::nullopt;

        digest->addBytes(data.data(), data.size());
        dataHash = digest->computeHash();
    }

    // Construct the data s-expression that contains raw hashed data.
    PAL::GCrypt::Handle<gcry_sexp_t> dataSexp;
    {
        auto shaAlgorithm = hashAlgorithmName(hashAlgorithmIdentifier);
        if (!shaAlgorithm)
            return std::nullopt;

        gcry_error_t error = gcry_sexp_build(&dataSexp, nullptr, "(data(flags raw)(hash %s %b))",
            *shaAlgorithm, dataHash.size(), dataHash.data());
        if (error != GPG_ERR_NO_ERROR) {
            PAL::GCrypt::logError(error);
            return std::nullopt;
        }
    }

    // Perform the PK signing, retrieving a sig-val s-expression of the following form:
    // (sig-val
    //   (dsa
    //     (r r-mpi)
    //     (s s-mpi)))
    PAL::GCrypt::Handle<gcry_sexp_t> signatureSexp;
    gcry_error_t error = gcry_pk_sign(&signatureSexp, dataSexp, keySexp);
    if (error != GPG_ERR_NO_ERROR) {
        PAL::GCrypt::logError(error);
        return std::nullopt;
    }

    // Retrieve MPI data of the resulting r and s integers. They are concatenated into
    // a single buffer after checking that the data length matches the key size.
    // FIXME: But r and s integers can still be valid even if they're of shorter size.
    // https://bugs.webkit.org/show_bug.cgi?id=171535
    Vector<uint8_t> signature;
    {
        PAL::GCrypt::Handle<gcry_sexp_t> rSexp(gcry_sexp_find_token(signatureSexp, "r", 0));
        if (!rSexp)
            return std::nullopt;

        auto rData = mpiData(rSexp);
        if (!rData || rData->size() != keySizeInBytes)
            return std::nullopt;

        signature.appendVector(*rData);
    }
    {
        PAL::GCrypt::Handle<gcry_sexp_t> sSexp(gcry_sexp_find_token(signatureSexp, "s", 0));
        if (!sSexp)
            return std::nullopt;

        auto sData = mpiData(sSexp);
        if (!sData || sData->size() != keySizeInBytes)
            return std::nullopt;

        signature.appendVector(*sData);
    }

    return signature;
}

static std::optional<bool> gcryptVerify(gcry_sexp_t keySexp, const Vector<uint8_t>& signature, const Vector<uint8_t>& data, CryptoAlgorithmIdentifier hashAlgorithmIdentifier, size_t keySizeInBytes)
{
    // Bail if the signature size isn't double the key size (i.e. concatenated r and s components).
    if (signature.size() != keySizeInBytes * 2)
        return false;

    // Perform digest operation with the specified algorithm on the given data.
    Vector<uint8_t> dataHash;
    {
        auto digestAlgorithm = hashCryptoDigestAlgorithm(hashAlgorithmIdentifier);
        if (!digestAlgorithm)
            return std::nullopt;

        auto digest = PAL::CryptoDigest::create(*digestAlgorithm);
        if (!digest)
            return std::nullopt;

        digest->addBytes(data.data(), data.size());
        dataHash = digest->computeHash();
    }

    // Construct the sig-val s-expression, extracting the r and s components from the signature vector.
    PAL::GCrypt::Handle<gcry_sexp_t> signatureSexp;
    gcry_error_t error = gcry_sexp_build(&signatureSexp, nullptr, "(sig-val(ecdsa(r %b)(s %b)))",
        keySizeInBytes, signature.data(), keySizeInBytes, signature.data() + keySizeInBytes);
    if (error != GPG_ERR_NO_ERROR) {
        PAL::GCrypt::logError(error);
        return std::nullopt;
    }

    // Construct the data s-expression that contains raw hashed data.
    PAL::GCrypt::Handle<gcry_sexp_t> dataSexp;
    {
        auto shaAlgorithm = hashAlgorithmName(hashAlgorithmIdentifier);
        if (!shaAlgorithm)
            return std::nullopt;

        error = gcry_sexp_build(&dataSexp, nullptr, "(data(flags raw)(hash %s %b))",
            *shaAlgorithm, dataHash.size(), dataHash.data());
        if (error != GPG_ERR_NO_ERROR) {
            PAL::GCrypt::logError(error);
            return std::nullopt;
        }
    }

    // Perform the PK verification. We report success if there's no error returned, failure
    // if the returned error is GPG_ERR_BAD_SIGNATURE, or an OperationError otherwise.
    error = gcry_pk_verify(signatureSexp, dataSexp, keySexp);
    if (error != GPG_ERR_NO_ERROR && gcry_err_code(error) != GPG_ERR_BAD_SIGNATURE) {
        PAL::GCrypt::logError(error);
        return std::nullopt;
    }

    return error == GPG_ERR_NO_ERROR;
}

void CryptoAlgorithmECDSA::platformSign(std::unique_ptr<CryptoAlgorithmParameters>&& parameters, Ref<CryptoKey>&& key, Vector<uint8_t>&& data, VectorCallback&& callback, ExceptionCallback&& exceptionCallback, ScriptExecutionContext& context, WorkQueue& workQueue)
{
    context.ref();
    workQueue.dispatch(
        [parameters = WTFMove(parameters), key = WTFMove(key), data = WTFMove(data), callback = WTFMove(callback), exceptionCallback = WTFMove(exceptionCallback), &context]() mutable {
            auto& ecKey = downcast<CryptoKeyEC>(key.get());
            auto& ecParameters = downcast<CryptoAlgorithmEcdsaParams>(*parameters);

            auto output = gcryptSign(ecKey.platformKey(), data, ecParameters.hashIdentifier, ecKey.keySizeInBits() / 8);
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

void CryptoAlgorithmECDSA::platformVerify(std::unique_ptr<CryptoAlgorithmParameters>&& parameters, Ref<CryptoKey>&& key, Vector<uint8_t>&& signature, Vector<uint8_t>&& data, BoolCallback&& callback, ExceptionCallback&& exceptionCallback, ScriptExecutionContext& context, WorkQueue& workQueue)
{
    context.ref();
    workQueue.dispatch(
        [parameters = WTFMove(parameters), key = WTFMove(key), signature = WTFMove(signature), data = WTFMove(data), callback = WTFMove(callback), exceptionCallback = WTFMove(exceptionCallback), &context]() mutable {
            auto& ecKey = downcast<CryptoKeyEC>(key.get());
            auto& ecParameters = downcast<CryptoAlgorithmEcdsaParams>(*parameters);

            auto output = gcryptVerify(ecKey.platformKey(), signature, data, ecParameters.hashIdentifier, ecKey.keySizeInBits() / 8);
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

/*
 * Copyright (C) 2014 Igalia S.L. All rights reserved.
 * Copyright (C) 2016 Yusuke Suzuki <utatane.tea@gmail.com>.
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
#include "CryptoAlgorithmRSASSA_PKCS1_v1_5.h"

#if ENABLE(WEB_CRYPTO)

#include "CryptoKeyRSA.h"
#include "GCryptUtilities.h"
#include "NotImplemented.h"

namespace WebCore {

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

    // Construct the data s-expression that contains PKCS#1-padded hashed data.
    PAL::GCrypt::Handle<gcry_sexp_t> dataSexp;
    {
        auto shaAlgorithm = hashAlgorithmName(hashAlgorithmIdentifier);
        if (!shaAlgorithm)
            return std::nullopt;

        gcry_error_t error = gcry_sexp_build(&dataSexp, nullptr, "(data(flags pkcs1)(hash %s %b))",
            *shaAlgorithm, dataHash.size(), dataHash.data());
        if (error != GPG_ERR_NO_ERROR) {
            PAL::GCrypt::logError(error);
            return std::nullopt;
        }
    }

    // Perform the PK signing, retrieving a sig-val s-expression of the following form:
    // (sig-val
    //   (rsa
    //     (s s-mpi)))
    PAL::GCrypt::Handle<gcry_sexp_t> signatureSexp;
    gcry_error_t error = gcry_pk_sign(&signatureSexp, dataSexp, keySexp);
    if (error != GPG_ERR_NO_ERROR) {
        PAL::GCrypt::logError(error);
        return std::nullopt;
    }

    // Return MPI data of the embedded s integer.
    PAL::GCrypt::Handle<gcry_sexp_t> sSexp(gcry_sexp_find_token(signatureSexp, "s", 0));
    if (!sSexp)
        return std::nullopt;

    return mpiZeroPrefixedData(sSexp, keySizeInBytes);
}

static std::optional<bool> gcryptVerify(gcry_sexp_t keySexp, const Vector<uint8_t>& signature, const Vector<uint8_t>& data, CryptoAlgorithmIdentifier hashAlgorithmIdentifier)
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

    // Construct the sig-val s-expression that contains the signature data.
    PAL::GCrypt::Handle<gcry_sexp_t> signatureSexp;
    gcry_error_t error = gcry_sexp_build(&signatureSexp, nullptr, "(sig-val(rsa(s %b)))",
        signature.size(), signature.data());
    if (error != GPG_ERR_NO_ERROR) {
        PAL::GCrypt::logError(error);
        return std::nullopt;
    }

    // Construct the data s-expression that contains PKCS#1-padded hashed data.
    PAL::GCrypt::Handle<gcry_sexp_t> dataSexp;
    {
        auto shaAlgorithm = hashAlgorithmName(hashAlgorithmIdentifier);
        if (!shaAlgorithm)
            return std::nullopt;

        error = gcry_sexp_build(&dataSexp, nullptr, "(data(flags pkcs1)(hash %s %b))",
            *shaAlgorithm, dataHash.size(), dataHash.data());
        if (error != GPG_ERR_NO_ERROR) {
            PAL::GCrypt::logError(error);
            return std::nullopt;
        }
    }

    // Perform the PK verification. We report success if there's no error returned, or
    // a failure in any other case. OperationError should not be returned at this point,
    // avoiding spilling information about the exact cause of verification failure.
    error = gcry_pk_verify(signatureSexp, dataSexp, keySexp);
    return { error == GPG_ERR_NO_ERROR };
}

ExceptionOr<Vector<uint8_t>> CryptoAlgorithmRSASSA_PKCS1_v1_5::platformSign(const CryptoKeyRSA& key, const Vector<uint8_t>& data)
{
    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(!(key.keySizeInBits() % 8));
    auto output = gcryptSign(key.platformKey(), data, key.hashAlgorithmIdentifier(), key.keySizeInBits() / 8);
    if (!output)
        return Exception { OperationError };
    return WTFMove(*output);
}

ExceptionOr<bool> CryptoAlgorithmRSASSA_PKCS1_v1_5::platformVerify(const CryptoKeyRSA& key, const Vector<uint8_t>& signature, const Vector<uint8_t>& data)
{
    auto output = gcryptVerify(key.platformKey(), signature, data, key.hashAlgorithmIdentifier());
    if (!output)
        return Exception { OperationError };
    return *output;
}

} // namespace WebCore

#endif // ENABLE(WEB_CRYPTO)

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

#if ENABLE(WEB_CRYPTO)

#include "CryptoAlgorithmRsaOaepParams.h"
#include "CryptoKeyRSA.h"
#include "GCryptUtilities.h"
#include "NotImplemented.h"

namespace WebCore {

static Optional<Vector<uint8_t>> gcryptEncrypt(CryptoAlgorithmIdentifier hashAlgorithmIdentifier, gcry_sexp_t keySexp, const Vector<uint8_t>& labelVector, const Vector<uint8_t>& plainText, size_t keySizeInBytes)
{
    // Embed the plain-text data in a data s-expression using OAEP padding.
    // Empty label data is properly handled by gcry_sexp_build().
    PAL::GCrypt::Handle<gcry_sexp_t> dataSexp;
    {
        auto shaAlgorithm = hashAlgorithmName(hashAlgorithmIdentifier);
        if (!shaAlgorithm)
            return WTF::nullopt;

        gcry_error_t error = gcry_sexp_build(&dataSexp, nullptr, "(data(flags oaep)(hash-algo %s)(label %b)(value %b))",
            *shaAlgorithm, labelVector.size(), labelVector.data(), plainText.size(), plainText.data());
        if (error != GPG_ERR_NO_ERROR) {
            PAL::GCrypt::logError(error);
            return WTF::nullopt;
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
        return WTF::nullopt;
    }

    // Return MPI data of the embedded a integer.
    PAL::GCrypt::Handle<gcry_sexp_t> aSexp(gcry_sexp_find_token(cipherSexp, "a", 0));
    if (!aSexp)
        return WTF::nullopt;

    return mpiZeroPrefixedData(aSexp, keySizeInBytes);
}

static Optional<Vector<uint8_t>> gcryptDecrypt(CryptoAlgorithmIdentifier hashAlgorithmIdentifier, gcry_sexp_t keySexp, const Vector<uint8_t>& labelVector, const Vector<uint8_t>& cipherText)
{
    // Embed the cipher-text data in an enc-val s-expression using OAEP padding.
    // Empty label data is properly handled by gcry_sexp_build().
    PAL::GCrypt::Handle<gcry_sexp_t> encValSexp;
    {
        auto shaAlgorithm = hashAlgorithmName(hashAlgorithmIdentifier);
        if (!shaAlgorithm)
            return WTF::nullopt;

        gcry_error_t error = gcry_sexp_build(&encValSexp, nullptr, "(enc-val(flags oaep)(hash-algo %s)(label %b)(rsa(a %b)))",
            *shaAlgorithm, labelVector.size(), labelVector.data(), cipherText.size(), cipherText.data());
        if (error != GPG_ERR_NO_ERROR) {
            PAL::GCrypt::logError(error);
            return WTF::nullopt;
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
        return WTF::nullopt;
    }

    // Return MPI data of the embedded value integer.
    PAL::GCrypt::Handle<gcry_sexp_t> valueSexp(gcry_sexp_find_token(plainSexp, "value", 0));
    if (!valueSexp)
        return WTF::nullopt;

    return mpiData(valueSexp);
}

ExceptionOr<Vector<uint8_t>> CryptoAlgorithmRSA_OAEP::platformEncrypt(const CryptoAlgorithmRsaOaepParams& parameters, const CryptoKeyRSA& key, const Vector<uint8_t>& plainText)
{
    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(!(key.keySizeInBits() % 8));
    auto output = gcryptEncrypt(key.hashAlgorithmIdentifier(), key.platformKey(), parameters.labelVector(), plainText, key.keySizeInBits() / 8);
    if (!output)
        return Exception { OperationError };
    return WTFMove(*output);
}

ExceptionOr<Vector<uint8_t>> CryptoAlgorithmRSA_OAEP::platformDecrypt(const CryptoAlgorithmRsaOaepParams& parameters, const CryptoKeyRSA& key, const Vector<uint8_t>& cipherText)
{
    auto output = gcryptDecrypt(key.hashAlgorithmIdentifier(), key.platformKey(), parameters.labelVector(), cipherText);
    if (!output)
        return Exception { OperationError };
    return WTFMove(*output);
}

} // namespace WebCore

#endif // ENABLE(WEB_CRYPTO)

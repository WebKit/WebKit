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
#include "CryptoAlgorithmRSAES_PKCS1_v1_5.h"

#if ENABLE(WEB_CRYPTO)

#include "CryptoKeyRSA.h"
#include "GCryptUtilities.h"

namespace WebCore {

static std::optional<Vector<uint8_t>> gcryptEncrypt(gcry_sexp_t keySexp, const Vector<uint8_t>& plainText, size_t keySizeInBytes)
{
    // Embed the plain-text data in a `data` s-expression using PKCS#1 padding.
    PAL::GCrypt::Handle<gcry_sexp_t> dataSexp;
    gcry_error_t error = gcry_sexp_build(&dataSexp, nullptr, "(data(flags pkcs1)(value %b))",
        plainText.size(), plainText.data());
    if (error != GPG_ERR_NO_ERROR) {
        PAL::GCrypt::logError(error);
        return std::nullopt;
    }

    // Encrypt data with the provided key. The returned s-expression is of this form:
    // (enc-val
    //   (rsa
    //     (a a-mpi)))
    PAL::GCrypt::Handle<gcry_sexp_t> cipherSexp;
    error = gcry_pk_encrypt(&cipherSexp, dataSexp, keySexp);
    if (error != GPG_ERR_NO_ERROR) {
        PAL::GCrypt::logError(error);
        return std::nullopt;
    }

    // Return MPI data of the embedded `a` integer.
    PAL::GCrypt::Handle<gcry_sexp_t> aSexp(gcry_sexp_find_token(cipherSexp, "a", 0));
    if (!aSexp)
        return std::nullopt;

    return mpiZeroPrefixedData(aSexp, keySizeInBytes);
}

static std::optional<Vector<uint8_t>> gcryptDecrypt(gcry_sexp_t keySexp, const Vector<uint8_t>& cipherText)
{
    // Embed the cipher-text data in an `enc-val` s-expression using PKCS#1 padding.
    PAL::GCrypt::Handle<gcry_sexp_t> encValSexp;
    gcry_error_t error = gcry_sexp_build(&encValSexp, nullptr, "(enc-val(flags pkcs1)(rsa(a %b)))",
        cipherText.size(), cipherText.data());
    if (error != GPG_ERR_NO_ERROR) {
        PAL::GCrypt::logError(error);
        return std::nullopt;
    }

    // Decrypt data with the provided key. The returned s-expression is of this form:
    // (data
    //   (flags pkcs1)
    //   (value block))
    PAL::GCrypt::Handle<gcry_sexp_t> plainSexp;
    error = gcry_pk_decrypt(&plainSexp, encValSexp, keySexp);
    if (error != GPG_ERR_NO_ERROR) {
        PAL::GCrypt::logError(error);
        return std::nullopt;
    }

    // Return MPI data of the embedded `value` integer.
    PAL::GCrypt::Handle<gcry_sexp_t> valueSexp(gcry_sexp_find_token(plainSexp, "value", 0));
    if (!valueSexp)
        return std::nullopt;

    return mpiData(valueSexp);
}

ExceptionOr<Vector<uint8_t>> CryptoAlgorithmRSAES_PKCS1_v1_5::platformEncrypt(const CryptoKeyRSA& key, const Vector<uint8_t>& plainText)
{
    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(!(key.keySizeInBits() % 8));
    auto output = gcryptEncrypt(key.platformKey(), plainText, key.keySizeInBits() / 8);
    if (!output)
        return Exception { ExceptionCode::OperationError };
    return WTFMove(*output);
}

ExceptionOr<Vector<uint8_t>> CryptoAlgorithmRSAES_PKCS1_v1_5::platformDecrypt(const CryptoKeyRSA& key, const Vector<uint8_t>& cipherText)
{
    auto output = gcryptDecrypt(key.platformKey(), cipherText);
    if (!output)
        return Exception { ExceptionCode::OperationError };
    return WTFMove(*output);
}

} // namespace WebCore

#endif // ENABLE(WEB_CRYPTO)

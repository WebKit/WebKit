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
#include "CryptoAlgorithmAES_GCM.h"

#if ENABLE(WEB_CRYPTO)

#include "CryptoAlgorithmAesGcmParams.h"
#include "CryptoKeyAES.h"
#include "NotImplemented.h"
#include <pal/crypto/gcrypt/Handle.h>
#include <pal/crypto/gcrypt/Utilities.h>
#include <wtf/CryptographicUtilities.h>

namespace WebCore {

static Optional<Vector<uint8_t>> gcryptEncrypt(const Vector<uint8_t>& key, const Vector<uint8_t>& iv, const Vector<uint8_t>& plainText, const Vector<uint8_t>& additionalData, uint8_t tagLength)
{
    // Determine the AES algorithm for the given key size.
    auto algorithm = PAL::GCrypt::aesAlgorithmForKeySize(key.size() * 8);
    if (!algorithm)
        return WTF::nullopt;

    // Create a new GCrypt cipher object for the AES algorithm and the GCM cipher mode.
    PAL::GCrypt::Handle<gcry_cipher_hd_t> handle;
    gcry_error_t error = gcry_cipher_open(&handle, *algorithm, GCRY_CIPHER_MODE_GCM, GCRY_CIPHER_SECURE);
    if (error != GPG_ERR_NO_ERROR) {
        PAL::GCrypt::logError(error);
        return WTF::nullopt;
    }

    // Use the given key for this cipher object.
    error = gcry_cipher_setkey(handle, key.data(), key.size());
    if (error != GPG_ERR_NO_ERROR) {
        PAL::GCrypt::logError(error);
        return WTF::nullopt;
    }

    // Use the given IV for this cipher object.
    error = gcry_cipher_setiv(handle, iv.data(), iv.size());
    if (error != GPG_ERR_NO_ERROR) {
        PAL::GCrypt::logError(error);
        return WTF::nullopt;
    }

    // Use the given additonal data, if any, as the authentication data for this cipher object.
    if (!additionalData.isEmpty()) {
        error = gcry_cipher_authenticate(handle, additionalData.data(), additionalData.size());
        if (error != GPG_ERR_NO_ERROR) {
            PAL::GCrypt::logError(error);
            return WTF::nullopt;
        }
    }

    // Finalize the cipher object before performing the encryption.
    error = gcry_cipher_final(handle);
    if (error != GPG_ERR_NO_ERROR) {
        PAL::GCrypt::logError(error);
        return WTF::nullopt;
    }

    // Perform the encryption and retrieve the encrypted output.
    Vector<uint8_t> output(plainText.size());
    error = gcry_cipher_encrypt(handle, output.data(), output.size(), plainText.data(), plainText.size());
    if (error != GPG_ERR_NO_ERROR) {
        PAL::GCrypt::logError(error);
        return WTF::nullopt;
    }

    // If tag length was specified, retrieve the tag data and append it to the output vector.
    if (tagLength) {
        Vector<uint8_t> tag(tagLength);
        error = gcry_cipher_gettag(handle, tag.data(), tag.size());
        if (error != GPG_ERR_NO_ERROR) {
            PAL::GCrypt::logError(error);
            return WTF::nullopt;
        }

        output.appendVector(tag);
    }

    return output;
}

static Optional<Vector<uint8_t>> gcryptDecrypt(const Vector<uint8_t>& key, const Vector<uint8_t>& iv, const Vector<uint8_t>& cipherText, const Vector<uint8_t>& additionalData, uint8_t tagLength)
{
    // Determine the AES algorithm for the given key size.
    auto algorithm = PAL::GCrypt::aesAlgorithmForKeySize(key.size() * 8);
    if (!algorithm)
        return WTF::nullopt;

    // Create a new GCrypt cipher object for the AES algorithm and the GCM cipher mode.
    PAL::GCrypt::Handle<gcry_cipher_hd_t> handle;
    gcry_error_t error = gcry_cipher_open(&handle, *algorithm, GCRY_CIPHER_MODE_GCM, 0);
    if (error != GPG_ERR_NO_ERROR) {
        PAL::GCrypt::logError(error);
        return WTF::nullopt;
    }

    // Use the given key for this cipher object.
    error = gcry_cipher_setkey(handle, key.data(), key.size());
    if (error != GPG_ERR_NO_ERROR) {
        PAL::GCrypt::logError(error);
        return WTF::nullopt;
    }

    // Use the given IV for this cipher object.
    error = gcry_cipher_setiv(handle, iv.data(), iv.size());
    if (error != GPG_ERR_NO_ERROR) {
        PAL::GCrypt::logError(error);
        return WTF::nullopt;
    }

    // Use the given additonal data, if any, as the authentication data for this cipher object.
    if (!additionalData.isEmpty()) {
        error = gcry_cipher_authenticate(handle, additionalData.data(), additionalData.size());
        if (error != GPG_ERR_NO_ERROR) {
            PAL::GCrypt::logError(error);
            return WTF::nullopt;
        }
    }

    // Finalize the cipher object before performing the encryption.
    error = gcry_cipher_final(handle);
    if (error != GPG_ERR_NO_ERROR) {
        PAL::GCrypt::logError(error);
        return WTF::nullopt;
    }

    // Account for the specified tag length when performing the decryption and retrieving the decrypted output.
    size_t cipherLength = cipherText.size() - tagLength;
    Vector<uint8_t> output(cipherLength);
    error = gcry_cipher_decrypt(handle, output.data(), output.size(), cipherText.data(), cipherLength);
    if (error != GPG_ERR_NO_ERROR) {
        PAL::GCrypt::logError(error);
        return WTF::nullopt;
    }

    // If tag length was indeed specified, retrieve the tag data and compare it securely to the tag data that
    // is in the passed-in cipher text Vector, bailing if there is a mismatch and returning the decrypted
    // plaintext otherwise.
    if (tagLength) {
        Vector<uint8_t> tag(tagLength);
        error = gcry_cipher_gettag(handle, tag.data(), tagLength);
        if (error != GPG_ERR_NO_ERROR) {
            PAL::GCrypt::logError(error);
            return WTF::nullopt;
        }

        if (constantTimeMemcmp(tag.data(), cipherText.data() + cipherLength, tagLength))
            return WTF::nullopt;
    }

    return output;
}

ExceptionOr<Vector<uint8_t>> CryptoAlgorithmAES_GCM::platformEncrypt(const CryptoAlgorithmAesGcmParams& parameters, const CryptoKeyAES& key, const Vector<uint8_t>& plainText)
{
    auto output = gcryptEncrypt(key.key(), parameters.ivVector(), plainText, parameters.additionalDataVector(), parameters.tagLength.valueOr(0) / 8);
    if (!output)
        return Exception { OperationError };
    return WTFMove(*output);
}

ExceptionOr<Vector<uint8_t>> CryptoAlgorithmAES_GCM::platformDecrypt(const CryptoAlgorithmAesGcmParams& parameters, const CryptoKeyAES& key, const Vector<uint8_t>& cipherText)
{
    auto output = gcryptDecrypt(key.key(), parameters.ivVector(), cipherText, parameters.additionalDataVector(), parameters.tagLength.valueOr(0) / 8);
    if (!output)
        return Exception { OperationError };
    return WTFMove(*output);
}

} // namespace WebCore

#endif // ENABLE(WEB_CRYPTO)

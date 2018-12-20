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
#include "CryptoAlgorithmAES_CBC.h"

#if ENABLE(WEB_CRYPTO)

#include "CryptoAlgorithmAesCbcCfbParams.h"
#include "CryptoKeyAES.h"
#include "NotImplemented.h"
#include <pal/crypto/gcrypt/Handle.h>
#include <pal/crypto/gcrypt/Utilities.h>

namespace WebCore {

static Optional<Vector<uint8_t>> gcryptEncrypt(const Vector<uint8_t>& key, const Vector<uint8_t>& iv, Vector<uint8_t>&& plainText)
{
    // Determine the AES algorithm for the given key size.
    auto algorithm = PAL::GCrypt::aesAlgorithmForKeySize(key.size() * 8);
    if (!algorithm)
        return WTF::nullopt;

    // Create a new GCrypt cipher object for the AES algorithm and the CBC cipher mode.
    PAL::GCrypt::Handle<gcry_cipher_hd_t> handle;
    gcry_error_t error = gcry_cipher_open(&handle, *algorithm, GCRY_CIPHER_MODE_CBC, 0);
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

    // Use the PKCS#7 padding.
    {
        // Round up the size value to the next multiple of the cipher's block length to get the padded size.
        size_t size = plainText.size();
        size_t paddedSize = roundUpToMultipleOf(gcry_cipher_get_algo_blklen(*algorithm), size + 1);

        // Padded size should be bigger than size, but bail if the value doesn't fit into a byte.
        ASSERT(paddedSize > size);
        if (paddedSize - size > 255)
            return WTF::nullopt;
        uint8_t paddingValue = paddedSize - size;

        plainText.grow(paddedSize);
        std::memset(plainText.data() + size, paddingValue, paddingValue);
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

    return output;
}

static Optional<Vector<uint8_t>> gcryptDecrypt(const Vector<uint8_t>& key, const Vector<uint8_t>& iv, const Vector<uint8_t>& cipherText)
{
    // Determine the AES algorithm for the given key size.
    auto algorithm = PAL::GCrypt::aesAlgorithmForKeySize(key.size() * 8);
    if (!algorithm)
        return WTF::nullopt;

    // Create a new GCrypt cipher object for the AES algorithm and the CBC cipher mode.
    PAL::GCrypt::Handle<gcry_cipher_hd_t> handle;
    gcry_error_t error = gcry_cipher_open(&handle, *algorithm, GCRY_CIPHER_MODE_CBC, 0);
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

    // Finalize the cipher object before performing the decryption.
    error = gcry_cipher_final(handle);
    if (error != GPG_ERR_NO_ERROR) {
        PAL::GCrypt::logError(error);
        return WTF::nullopt;
    }

    // Perform the decryption and retrieve the decrypted output.
    Vector<uint8_t> output(cipherText.size());
    error = gcry_cipher_decrypt(handle, output.data(), output.size(), cipherText.data(), cipherText.size());
    if (error != GPG_ERR_NO_ERROR) {
        PAL::GCrypt::logError(error);
        return WTF::nullopt;
    }

    // Remove the PKCS#7 padding from the decrypted output.
    {
        // The padding value can be retrieved from the last byte.
        uint8_t paddingValue = output.last();
        if (paddingValue > gcry_cipher_get_algo_blklen(*algorithm))
            return WTF::nullopt;

        // Padding value mustn't be greater than the size of the padded output.
        size_t size = output.size();
        if (paddingValue > size)
            return WTF::nullopt;

        // Bail if the last `paddingValue` bytes don't have the value of `paddingValue`.
        if (std::count(output.end() - paddingValue, output.end(), paddingValue) != paddingValue)
            return WTF::nullopt;

        // Shrink the output Vector object to drop the PKCS#7 padding.
        output.shrink(size - paddingValue);
    }

    return output;
}

ExceptionOr<Vector<uint8_t>> CryptoAlgorithmAES_CBC::platformEncrypt(const CryptoAlgorithmAesCbcCfbParams& parameters, const CryptoKeyAES& key, const Vector<uint8_t>& plainText)
{
    auto output = gcryptEncrypt(key.key(), parameters.ivVector(), Vector<uint8_t>(plainText));
    if (!output)
        return Exception { OperationError };
    return WTFMove(*output);
}

ExceptionOr<Vector<uint8_t>> CryptoAlgorithmAES_CBC::platformDecrypt(const CryptoAlgorithmAesCbcCfbParams& parameters, const CryptoKeyAES& key, const Vector<uint8_t>& cipherText)
{
    auto output = gcryptDecrypt(key.key(), parameters.ivVector(), cipherText);
    if (!output)
        return Exception { OperationError };
    return WTFMove(*output);
}

} // namespace WebCore

#endif // ENABLE(WEB_CRYPTO)

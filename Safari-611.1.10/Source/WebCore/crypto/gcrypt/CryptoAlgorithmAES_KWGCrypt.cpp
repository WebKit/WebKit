/*
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
#include "CryptoAlgorithmAES_KW.h"

#if ENABLE(WEB_CRYPTO)

#include "CryptoKeyAES.h"
#include <pal/crypto/gcrypt/Handle.h>
#include <pal/crypto/gcrypt/Utilities.h>

namespace WebCore {

static Optional<Vector<uint8_t>> gcryptWrapKey(const Vector<uint8_t>& key, const Vector<uint8_t>& data)
{
    // Determine the AES algorithm for the given key size.
    auto algorithm = PAL::GCrypt::aesAlgorithmForKeySize(key.size() * 8);
    if (!algorithm)
        return WTF::nullopt;

    // Create a new GCrypt cipher object for the AES algorithm and the AES-Wrap cipher mode.
    PAL::GCrypt::Handle<gcry_cipher_hd_t> handle;
    gcry_error_t error = gcry_cipher_open(&handle, *algorithm, GCRY_CIPHER_MODE_AESWRAP, 0);
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

    // Finalize the cipher object before performing the encryption.
    error = gcry_cipher_final(handle);
    if (error != GPG_ERR_NO_ERROR) {
        PAL::GCrypt::logError(error);
        return WTF::nullopt;
    }

    // Perform the encryption. The provided output buffer must be 64 bits larger than the input buffer.
    Vector<uint8_t> output(data.size() + 8);
    error = gcry_cipher_encrypt(handle, output.data(), output.size(), data.data(), data.size());
    if (error != GPG_ERR_NO_ERROR) {
        PAL::GCrypt::logError(error);
        return WTF::nullopt;
    }

    return output;
}

static Optional<Vector<uint8_t>> gcryptUnwrapKey(const Vector<uint8_t>& key, const Vector<uint8_t>& data)
{
    // Determine the AES algorithm for the given key size.
    auto algorithm = PAL::GCrypt::aesAlgorithmForKeySize(key.size() * 8);
    if (!algorithm)
        return WTF::nullopt;

    // Create a new GCrypt cipher object for the AES algorithm and the AES-Wrap cipher mode.
    PAL::GCrypt::Handle<gcry_cipher_hd_t> handle;
    gcry_error_t error = gcry_cipher_open(&handle, *algorithm, GCRY_CIPHER_MODE_AESWRAP, 0);
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

    // Finalize the cipher object before performing the encryption.
    error = gcry_cipher_final(handle);
    if (error != GPG_ERR_NO_ERROR) {
        PAL::GCrypt::logError(error);
        return WTF::nullopt;
    }

    // Perform the decryption. The output buffer may be specified 64 bits shorter than the input buffer.
    Vector<uint8_t> output(data.size() - 8);
    error = gcry_cipher_decrypt(handle, output.data(), output.size(), data.data(), data.size());
    if (error != GPG_ERR_NO_ERROR) {
        PAL::GCrypt::logError(error);
        return WTF::nullopt;
    }

    return output;
}

ExceptionOr<Vector<uint8_t>> CryptoAlgorithmAES_KW::platformWrapKey(const CryptoKeyAES& key, const Vector<uint8_t>& data)
{
    auto output = gcryptWrapKey(key.key(), data);
    if (!output)
        return Exception { OperationError };
    return WTFMove(*output);
}

ExceptionOr<Vector<uint8_t>> CryptoAlgorithmAES_KW::platformUnwrapKey(const CryptoKeyAES& key, const Vector<uint8_t>& data)
{
    auto output = gcryptUnwrapKey(key.key(), data);
    if (!output)
        return Exception { OperationError };
    return WTFMove(*output);
}

} // namespace WebCore

#endif // ENABLE(WEB_CRYPTO)

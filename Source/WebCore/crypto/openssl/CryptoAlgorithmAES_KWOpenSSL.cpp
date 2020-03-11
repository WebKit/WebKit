/*
 * Copyright (C) 2020 Sony Interactive Entertainment Inc.
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
#include "OpenSSLCryptoUniquePtr.h"
#include <openssl/evp.h>

namespace WebCore {

static const EVP_CIPHER* aesAlgorithm(size_t keySize)
{
    if (keySize * 8 == 128)
        return EVP_aes_128_wrap();

    if (keySize * 8 == 192)
        return EVP_aes_192_wrap();

    if (keySize * 8 == 256)
        return EVP_aes_256_wrap();

    return nullptr;
}

static Optional<Vector<uint8_t>> cryptWrapKey(const Vector<uint8_t>& key, const Vector<uint8_t>& data)
{
    const EVP_CIPHER* algorithm = aesAlgorithm(key.size());
    if (!algorithm)
        return WTF::nullopt;

    EvpCipherCtxPtr ctx;
    Vector<uint8_t> cipherText(data.size() + 8);
    int len;

    // Create and initialize the context
    if (!(ctx = EvpCipherCtxPtr(EVP_CIPHER_CTX_new())))
        return WTF::nullopt;

    EVP_CIPHER_CTX_set_flags(ctx.get(), EVP_CIPHER_CTX_FLAG_WRAP_ALLOW);

    // Initialize the encryption operation
    if (1 != EVP_EncryptInit_ex(ctx.get(), algorithm, nullptr, key.data(), nullptr))
        return WTF::nullopt;

    // Provide the message to be encrypted, and obtain the encrypted output
    if (1 != EVP_EncryptUpdate(ctx.get(), cipherText.data(), &len, data.data(), data.size()))
        return WTF::nullopt;

    // Finalize the encryption. Further ciphertext bytes may be written at this stage
    if (1 != EVP_EncryptFinal_ex(ctx.get(), cipherText.data() + len, &len))
        return WTF::nullopt;

    return cipherText;
}

static Optional<Vector<uint8_t>> cryptUnwrapKey(const Vector<uint8_t>& key, const Vector<uint8_t>& data)
{
    const EVP_CIPHER* algorithm = aesAlgorithm(key.size());
    if (!algorithm)
        return WTF::nullopt;

    EvpCipherCtxPtr ctx;
    Vector<uint8_t> plainText(data.size());
    int len;
    int plainTextLen;

    // Create and initialize the context
    if (!(ctx = EvpCipherCtxPtr(EVP_CIPHER_CTX_new())))
        return WTF::nullopt;

    EVP_CIPHER_CTX_set_flags(ctx.get(), EVP_CIPHER_CTX_FLAG_WRAP_ALLOW);

    // Initialize the decryption operation
    if (1 != EVP_DecryptInit_ex(ctx.get(), algorithm, nullptr, key.data(), nullptr))
        return WTF::nullopt;

    // Provide the message to be decrypted, and obtain the plaintext output
    if (1 != EVP_DecryptUpdate(ctx.get(), plainText.data(), &len, data.data(), data.size()))
        return WTF::nullopt;
    plainTextLen = len;

    // Finalize the decryption. Further plaintext bytes may be written at this stage
    if (1 != EVP_DecryptFinal_ex(ctx.get(), plainText.data() + len, &len))
        return WTF::nullopt;
    plainTextLen += len;

    plainText.shrink(plainTextLen);

    return plainText;
}

ExceptionOr<Vector<uint8_t>> CryptoAlgorithmAES_KW::platformWrapKey(const CryptoKeyAES& key, const Vector<uint8_t>& data)
{
    auto output = cryptWrapKey(key.key(), data);
    if (!output)
        return Exception { OperationError };
    return WTFMove(*output);
}

ExceptionOr<Vector<uint8_t>> CryptoAlgorithmAES_KW::platformUnwrapKey(const CryptoKeyAES& key, const Vector<uint8_t>& data)
{
    auto output = cryptUnwrapKey(key.key(), data);
    if (!output)
        return Exception { OperationError };
    return WTFMove(*output);
}

} // namespace WebCore

#endif // ENABLE(WEB_CRYPTO)

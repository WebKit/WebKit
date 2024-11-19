/*
 *  Copyright (C) 2024 Igalia S.L. All rights reserved.
 *  Copyright (C) 2024 Metrological Group B.V.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "RTCRtpSFrameTransformer.h"

#if USE(GSTREAMER_WEBRTC)

#include "CryptoAlgorithmAESCTR.h"
#include "OpenSSLCryptoUniquePtr.h"
#include "SFrameUtils.h"
#include <openssl/aes.h>
#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/kdf.h>

namespace WebCore {

static ExceptionOr<Vector<uint8_t>> deriveHKDFBits(const Vector<uint8_t>& key, const Vector<uint8_t>& salt, const Vector<uint8_t>& info, size_t lengthInBytes)
{
    auto paramsBuilder = OsslParamBldPtr(OSSL_PARAM_BLD_new());
    if (!paramsBuilder)
        return Exception { ExceptionCode::OperationError };

    EVPKDFPtr kdf(EVP_KDF_fetch(nullptr, "HKDF", nullptr));
    EVPKDFCtxPtr kdfCtx(EVP_KDF_CTX_new(kdf.get()));

    if (!OSSL_PARAM_BLD_push_utf8_string(paramsBuilder.get(), OSSL_KDF_PARAM_DIGEST, SN_sha256, strlen(SN_sha256)))
        return Exception { ExceptionCode::OperationError };
    if (!OSSL_PARAM_BLD_push_octet_string(paramsBuilder.get(), OSSL_KDF_PARAM_KEY, key.data(), key.size()))
        return Exception { ExceptionCode::OperationError };
    if (!OSSL_PARAM_BLD_push_octet_string(paramsBuilder.get(), OSSL_KDF_PARAM_INFO, info.data(), info.size()))
        return Exception { ExceptionCode::OperationError };
    if (!OSSL_PARAM_BLD_push_octet_string(paramsBuilder.get(), OSSL_KDF_PARAM_SALT, salt.data(), salt.size()))
        return Exception { ExceptionCode::OperationError };

    auto params = OsslParamPtr(OSSL_PARAM_BLD_to_param(paramsBuilder.get()));
    if (!params)
        return Exception { ExceptionCode::OperationError };

    Vector<uint8_t> output(lengthInBytes / 8);
    if (!EVP_KDF_derive(kdfCtx.get(), output.data(), output.sizeInBytes(), params.get()))
        return Exception { ExceptionCode::OperationError };

    return output;
}

ExceptionOr<Vector<uint8_t>> RTCRtpSFrameTransformer::computeSaltKey(const Vector<uint8_t>& rawKey)
{
    return deriveHKDFBits(rawKey, "SFrame10"_span, "salt"_span, 96);
}

static ExceptionOr<Vector<uint8_t>> createBaseSFrameKey(const Vector<uint8_t>& rawKey)
{
    return deriveHKDFBits(rawKey, "SFrame10"_span, "key"_span, 128);
}

ExceptionOr<Vector<uint8_t>> RTCRtpSFrameTransformer::computeAuthenticationKey(const Vector<uint8_t>& rawKey)
{
    auto key = createBaseSFrameKey(rawKey);
    if (key.hasException())
        return key;

    return deriveHKDFBits(key.returnValue().subspan(0, 16), "SFrame10 AES CM AEAD"_span, "auth"_span, 256);
}

ExceptionOr<Vector<uint8_t>> RTCRtpSFrameTransformer::computeEncryptionKey(const Vector<uint8_t>& rawKey)
{
    auto key = createBaseSFrameKey(rawKey);
    if (key.hasException())
        return key;

    return deriveHKDFBits(key.returnValue().subspan(0, 16), "SFrame10 AES CM AEAD"_span, "enc"_span, 128);
}

// FIXME: This is copied from CryptoAlgorithmAESCTROpenSSL. Remove when the GTK/WPE ports include
// OpenSSL.cmake in their build.
static std::optional<Vector<uint8_t>> crypt(int operation, const Vector<uint8_t>& key, const Vector<uint8_t>& counter, size_t counterLength, std::span<const uint8_t> inputText)
{
    constexpr size_t blockSize = 16;
    const EVP_CIPHER* algorithm = EVP_aes_128_ctr();
    if (!algorithm)
        return std::nullopt;

    EvpCipherCtxPtr ctx;
    int len;

    // Create and initialize the context
    if (!(ctx = EvpCipherCtxPtr(EVP_CIPHER_CTX_new())))
        return std::nullopt;

    const size_t blocks = roundUpToMultipleOf(blockSize, inputText.size()) / blockSize;

    // Detect loop
    if (counterLength < sizeof(size_t) * 8 && blocks > ((size_t)1 << counterLength))
        return std::nullopt;

    // Calculate capacity before overflow
    CryptoAlgorithmAESCTR::CounterBlockHelper counterBlockHelper(counter, counterLength);
    size_t capacity = counterBlockHelper.countToOverflowSaturating();

    // Divide data into two parts if necessary
    size_t headSize = inputText.size();
    if (capacity < blocks)
        headSize = capacity * blockSize;

    Vector<uint8_t> outputText(inputText.size());
    // First part
    {
        // Initialize the encryption(decryption) operation
        if (1 != EVP_CipherInit_ex(ctx.get(), algorithm, nullptr, key.data(), counter.data(), operation))
            return std::nullopt;

        // Disable padding
        if (1 != EVP_CIPHER_CTX_set_padding(ctx.get(), 0))
            return std::nullopt;

        // Provide the message to be encrypted(decrypted), and obtain the encrypted(decrypted) output
        if (1 != EVP_CipherUpdate(ctx.get(), outputText.data(), &len, inputText.data(), headSize))
            return std::nullopt;

        // Finalize the encryption(decryption)
        WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN // GLib port
        if (1 != EVP_CipherFinal_ex(ctx.get(), outputText.data() + len, &len))
            return std::nullopt;
        WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
    }

    // Second part
    if (capacity < blocks) {
        size_t tailSize = inputText.size() - headSize;

        Vector<uint8_t> remainingCounter = counterBlockHelper.counterVectorAfterOverflow();

        // Initialize the encryption(decryption) operation
        if (1 != EVP_CipherInit_ex(ctx.get(), algorithm, nullptr, key.data(), remainingCounter.data(), operation))
            return std::nullopt;

        // Disable padding
        if (1 != EVP_CIPHER_CTX_set_padding(ctx.get(), 0))
            return std::nullopt;

        // Provide the message to be encrypted(decrypted), and obtain the encrypted(decrypted) output
        WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN // GLib
        if (1 != EVP_CipherUpdate(ctx.get(), outputText.data() + headSize, &len, inputText.data() + headSize, tailSize))
            return std::nullopt;

        // Finalize the encryption(decryption)
        if (1 != EVP_CipherFinal_ex(ctx.get(), outputText.data() + headSize + len, &len))
            return std::nullopt;
        WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
    }

    return outputText;
}

ExceptionOr<Vector<uint8_t>> RTCRtpSFrameTransformer::decryptData(std::span<const uint8_t> data, const Vector<uint8_t>& iv, const Vector<uint8_t>& key)
{
    auto output = crypt(0, key, iv, iv.sizeInBytes(), data);
    if (!output)
        return Exception { ExceptionCode::OperationError };

    return WTFMove(*output);
}

ExceptionOr<Vector<uint8_t>> RTCRtpSFrameTransformer::encryptData(std::span<const uint8_t> data, const Vector<uint8_t>& iv, const Vector<uint8_t>& key)
{
    auto output = crypt(1, key, iv, iv.sizeInBytes(), data);
    if (!output)
        return Exception { ExceptionCode::OperationError };

    return WTFMove(*output);
}

Vector<uint8_t> RTCRtpSFrameTransformer::computeEncryptedDataSignature(const Vector<uint8_t>& nonce, std::span<const uint8_t> header, std::span<const uint8_t> data, const Vector<uint8_t>& key)
{
    Vector<uint8_t> signature(32);
    Vector<uint8_t> payload;
    payload.appendVector(encodeBigEndian(header.size()));
    payload.appendVector(encodeBigEndian(data.size()));

    auto iv = nonce.span();
    payload.append(iv.subspan(0, 12));
    payload.append(header);
    payload.append(data);

    HMAC(EVP_sha256(), key.data(), key.size(), payload.data(), payload.size(), signature.data(), nullptr);
    return signature;
}

void RTCRtpSFrameTransformer::updateAuthenticationSize()
{
    size_t digestLength = 32;
    if (m_authenticationSize > digestLength)
        m_authenticationSize = digestLength;
}

} // namespace WebCore

#endif // USE(GSTREAMER_WEBRTC)

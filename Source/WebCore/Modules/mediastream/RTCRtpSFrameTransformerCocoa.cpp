/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "RTCRtpSFrameTransformer.h"

#if ENABLE(WEB_RTC)

#include "CryptoUtilitiesCocoa.h"
#include <CommonCrypto/CommonCrypto.h>

namespace WebCore {

ExceptionOr<Vector<uint8_t>> RTCRtpSFrameTransformer::computeSaltKey(const Vector<uint8_t>& rawKey)
{
    uint8_t usage[] = "SFrame10";
    uint8_t info[] = "salt";
    return deriveHDKFSHA256Bits(rawKey.data(), 16, usage, sizeof(usage) - 1, info, sizeof(info) - 1, 96);
}

static ExceptionOr<Vector<uint8_t>> createBaseSFrameKey(const Vector<uint8_t>& rawKey)
{
    uint8_t usage[] = "SFrame10";
    uint8_t info[] = "key";
    return deriveHDKFSHA256Bits(rawKey.data(), 16, usage, sizeof(usage) - 1, info, sizeof(info) - 1, 128);
}

ExceptionOr<Vector<uint8_t>> RTCRtpSFrameTransformer::computeAuthenticationKey(const Vector<uint8_t>& rawKey)
{
    auto key = createBaseSFrameKey(rawKey);
    if (key.hasException())
        return key;

    uint8_t usage[] = "SFrame10 AES CM AEAD";
    uint8_t info[] = "auth";
    return deriveHDKFSHA256Bits(key.returnValue().data(), 16, usage, sizeof(usage) - 1, info, sizeof(info) - 1, 256);
}

ExceptionOr<Vector<uint8_t>> RTCRtpSFrameTransformer::computeEncryptionKey(const Vector<uint8_t>& rawKey)
{
    auto key = createBaseSFrameKey(rawKey);
    if (key.hasException())
        return key;

    uint8_t usage[] = "SFrame10 AES CM AEAD";
    uint8_t info[] = "enc";
    return deriveHDKFSHA256Bits(key.returnValue().data(), 16, usage, sizeof(usage) - 1, info, sizeof(info) - 1, 128);
}

ExceptionOr<Vector<uint8_t>> RTCRtpSFrameTransformer::decryptData(const uint8_t* data, size_t size, const Vector<uint8_t>& iv, const Vector<uint8_t>& key)
{
    return transformAESCTR(kCCDecrypt, iv, iv.size(), key, data, size);
}

ExceptionOr<Vector<uint8_t>> RTCRtpSFrameTransformer::encryptData(const uint8_t* data, size_t size, const Vector<uint8_t>& iv, const Vector<uint8_t>& key)
{
    return transformAESCTR(kCCEncrypt, iv, iv.size(), key, data, size);
}

static inline Vector<uint8_t, 8> encodeBigEndian(uint64_t value)
{
    Vector<uint8_t, 8> result(8);
    for (int i = 7; i >= 0; --i) {
        result.data()[i] = value & 0xff;
        value = value >> 8;
    }
    return result;
}

Vector<uint8_t> RTCRtpSFrameTransformer::computeEncryptedDataSignature(const Vector<uint8_t>& nonce, const uint8_t* header, size_t headerSize, const uint8_t* data, size_t dataSize, const Vector<uint8_t>& key)
{
    auto headerLength = encodeBigEndian(headerSize);
    auto dataLength = encodeBigEndian(dataSize);

    Vector<uint8_t> result(CC_SHA256_DIGEST_LENGTH);
    CCHmacContext context;
    CCHmacInit(&context, kCCHmacAlgSHA256, key.data(), key.size());
    CCHmacUpdate(&context, headerLength.data(), headerLength.size());
    CCHmacUpdate(&context, dataLength.data(), dataLength.size());
    CCHmacUpdate(&context, nonce.data(), 12);
    CCHmacUpdate(&context, header, headerSize);
    CCHmacUpdate(&context, data, dataSize);
    CCHmacFinal(&context, result.data());

    return result;
}

void RTCRtpSFrameTransformer::updateAuthenticationSize()
{
    if (m_authenticationSize > CC_SHA256_DIGEST_LENGTH)
        m_authenticationSize = CC_SHA256_DIGEST_LENGTH;
}

} // namespace WebCore

#endif // ENABLE(WEB_RTC)

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
    return deriveHDKFSHA256Bits(rawKey.subspan(0, 16), "SFrame10"_span, "salt"_span, 96);
}

static ExceptionOr<Vector<uint8_t>> createBaseSFrameKey(const Vector<uint8_t>& rawKey)
{
    return deriveHDKFSHA256Bits(rawKey.subspan(0, 16), "SFrame10"_span, "key"_span, 128);
}

ExceptionOr<Vector<uint8_t>> RTCRtpSFrameTransformer::computeAuthenticationKey(const Vector<uint8_t>& rawKey)
{
    auto key = createBaseSFrameKey(rawKey);
    if (key.hasException())
        return key;

    return deriveHDKFSHA256Bits(key.returnValue().subspan(0, 16), "SFrame10 AES CM AEAD"_span, "auth"_span, 256);
}

ExceptionOr<Vector<uint8_t>> RTCRtpSFrameTransformer::computeEncryptionKey(const Vector<uint8_t>& rawKey)
{
    auto key = createBaseSFrameKey(rawKey);
    if (key.hasException())
        return key;

    return deriveHDKFSHA256Bits(key.returnValue().subspan(0, 16), "SFrame10 AES CM AEAD"_span, "enc"_span, 128);
}

ExceptionOr<Vector<uint8_t>> RTCRtpSFrameTransformer::decryptData(std::span<const uint8_t> data, const Vector<uint8_t>& iv, const Vector<uint8_t>& key)
{
    return transformAESCTR(kCCDecrypt, iv, iv.size(), key, data);
}

ExceptionOr<Vector<uint8_t>> RTCRtpSFrameTransformer::encryptData(std::span<const uint8_t> data, const Vector<uint8_t>& iv, const Vector<uint8_t>& key)
{
    return transformAESCTR(kCCEncrypt, iv, iv.size(), key, data);
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

Vector<uint8_t> RTCRtpSFrameTransformer::computeEncryptedDataSignature(const Vector<uint8_t>& nonce, std::span<const uint8_t> header, std::span<const uint8_t> data, const Vector<uint8_t>& key)
{
    auto headerLength = encodeBigEndian(header.size());
    auto dataLength = encodeBigEndian(data.size());

    Vector<uint8_t> result(CC_SHA256_DIGEST_LENGTH);
    CCHmacContext context;
    CCHmacInit(&context, kCCHmacAlgSHA256, key.data(), key.size());
    CCHmacUpdate(&context, headerLength.data(), headerLength.size());
    CCHmacUpdate(&context, dataLength.data(), dataLength.size());
    CCHmacUpdate(&context, nonce.data(), 12);
    CCHmacUpdate(&context, header.data(), header.size());
    CCHmacUpdate(&context, data.data(), data.size());
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

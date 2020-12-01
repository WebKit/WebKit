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
    uint8_t usage[] = "SFrameSaltKey";
    return deriveHDKFSHA256Bits(rawKey.data(), 16, rawKey.data() + 16, rawKey.size() - 16, usage, sizeof(usage) - 1, 128);
}

ExceptionOr<Vector<uint8_t>> RTCRtpSFrameTransformer::computeAuthenticationKey(const Vector<uint8_t>& rawKey)
{
    uint8_t usage[] = "SFrameAuthenticationKey";
    return deriveHDKFSHA256Bits(rawKey.data(), 16, rawKey.data() + 16, rawKey.size() - 16, usage, sizeof(usage) - 1, 256);
}

ExceptionOr<Vector<uint8_t>> RTCRtpSFrameTransformer::computeEncryptionKey(const Vector<uint8_t>& rawKey)
{
    uint8_t usage[] = "SFrameEncryptionKey";
    return deriveHDKFSHA256Bits(rawKey.data(), 16, rawKey.data() + 16, rawKey.size() - 16, usage, sizeof(usage) - 1, 128);
}

ExceptionOr<Vector<uint8_t>> RTCRtpSFrameTransformer::decryptData(const uint8_t* data, size_t size, const Vector<uint8_t>& iv, const Vector<uint8_t>& key)
{
    return transformAES_CTR(kCCDecrypt, iv, iv.size(), key, data, size);
}

ExceptionOr<Vector<uint8_t>> RTCRtpSFrameTransformer::encryptData(const uint8_t* data, size_t size, const Vector<uint8_t>& iv, const Vector<uint8_t>& key)
{
    return transformAES_CTR(kCCEncrypt, iv, iv.size(), key, data, size);
}

Vector<uint8_t> RTCRtpSFrameTransformer::computeEncryptedDataSignature(const uint8_t* data, size_t size, const Vector<uint8_t>& key)
{
    return calculateSHA256Signature(key, data, size);
}

void RTCRtpSFrameTransformer::updateAuthenticationSize()
{
    if (m_authenticationSize > CC_SHA256_DIGEST_LENGTH)
        m_authenticationSize = CC_SHA256_DIGEST_LENGTH;
}

} // namespace WebCore

#endif // ENABLE(WEB_RTC)

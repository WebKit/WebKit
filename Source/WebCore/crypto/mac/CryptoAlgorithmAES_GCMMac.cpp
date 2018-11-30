/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include "CommonCryptoUtilities.h"
#include "CryptoAlgorithmAesGcmParams.h"
#include "CryptoKeyAES.h"
#include <wtf/CryptographicUtilities.h>

namespace WebCore {

static ExceptionOr<Vector<uint8_t>> encryptAES_GCM(const Vector<uint8_t>& iv, const Vector<uint8_t>& key, const Vector<uint8_t>& plainText, const Vector<uint8_t>& additionalData, size_t desiredTagLengthInBytes)
{
    Vector<uint8_t> cipherText(plainText.size()); // Per section 5.2.1.2: http://nvlpubs.nist.gov/nistpubs/Legacy/SP/nistspecialpublication800-38d.pdf
    Vector<uint8_t> tag(desiredTagLengthInBytes);
    // tagLength is actual an input <rdar://problem/30660074>
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    CCCryptorStatus status = CCCryptorGCM(kCCEncrypt, kCCAlgorithmAES, key.data(), key.size(), iv.data(), iv.size(), additionalData.data(), additionalData.size(), plainText.data(), plainText.size(), cipherText.data(), tag.data(), &desiredTagLengthInBytes);
    ALLOW_DEPRECATED_DECLARATIONS_END
    if (status)
        return Exception { OperationError };

    cipherText.append(tag.data(), desiredTagLengthInBytes);

    return WTFMove(cipherText);
}

static ExceptionOr<Vector<uint8_t>> decyptAES_GCM(const Vector<uint8_t>& iv, const Vector<uint8_t>& key, const Vector<uint8_t>& cipherText, const Vector<uint8_t>& additionalData, size_t desiredTagLengthInBytes)
{
    Vector<uint8_t> plainText(cipherText.size() - desiredTagLengthInBytes); // Per section 5.2.1.2: http://nvlpubs.nist.gov/nistpubs/Legacy/SP/nistspecialpublication800-38d.pdf
    Vector<uint8_t> tag(desiredTagLengthInBytes);
    size_t offset = cipherText.size() - desiredTagLengthInBytes;
    // tagLength is actual an input <rdar://problem/30660074>
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    CCCryptorStatus status = CCCryptorGCM(kCCDecrypt, kCCAlgorithmAES, key.data(), key.size(), iv.data(), iv.size(), additionalData.data(), additionalData.size(), cipherText.data(), offset, plainText.data(), tag.data(), &desiredTagLengthInBytes);
    ALLOW_DEPRECATED_DECLARATIONS_END
    if (status)
        return Exception { OperationError };

    // Using a constant time comparison to prevent timing attacks.
    if (constantTimeMemcmp(tag.data(), cipherText.data() + offset, desiredTagLengthInBytes))
        return Exception { OperationError };

    return WTFMove(plainText);
}

ExceptionOr<Vector<uint8_t>> CryptoAlgorithmAES_GCM::platformEncrypt(const CryptoAlgorithmAesGcmParams& parameters, const CryptoKeyAES& key, const Vector<uint8_t>& plainText)
{
    return encryptAES_GCM(parameters.ivVector(), key.key(), plainText, parameters.additionalDataVector(), parameters.tagLength.value_or(0) / 8);
}

ExceptionOr<Vector<uint8_t>> CryptoAlgorithmAES_GCM::platformDecrypt(const CryptoAlgorithmAesGcmParams& parameters, const CryptoKeyAES& key, const Vector<uint8_t>& cipherText)
{
    return decyptAES_GCM(parameters.ivVector(), key.key(), cipherText, parameters.additionalDataVector(), parameters.tagLength.value_or(0) / 8);
}

} // namespace WebCore

#endif // ENABLE(WEB_CRYPTO)

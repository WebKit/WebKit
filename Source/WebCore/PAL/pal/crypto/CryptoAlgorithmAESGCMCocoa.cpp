/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "CryptoAlgorithmAESGCMCocoa.h"

#include "CommonCryptoSPI.h"
#include "PALSwift-Generated.h"
#include <wtf/CryptographicUtilities.h>

namespace PAL::Crypto {

Expected<VectorUInt8, Error> encryptAESGCM(const VectorUInt8& iv, const VectorUInt8& key, const VectorUInt8& plainText, const VectorUInt8& additionalData, size_t desiredTagLengthInBytes)
{
    Vector<uint8_t> cipherText(plainText.size() + desiredTagLengthInBytes); // Per section 5.2.1.2: http://nvlpubs.nist.gov/nistpubs/Legacy/SP/nistspecialpublication800-38d.pdf
    Vector<uint8_t> tag(desiredTagLengthInBytes);
    // tagLength is actual an input <rdar://problem/30660074>
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    CCCryptorStatus status = CCCryptorGCM(kCCEncrypt, kCCAlgorithmAES, key.span().data(), key.size(), iv.span().data(), iv.size(), additionalData.span().data(), additionalData.size(), plainText.span().data(), plainText.size(), cipherText.mutableSpan().data(), tag.mutableSpan().data(), &desiredTagLengthInBytes);
    ALLOW_DEPRECATED_DECLARATIONS_END
    if (status)
        return makeUnexpected(Error::EncryptionFailed);
    memcpySpan(cipherText.mutableSpan().subspan(plainText.size()), tag.span());

    return WTF::move(cipherText);
}

Expected<VectorUInt8, Error> encryptCryptoKitAESGCM(const VectorUInt8& iv, const VectorUInt8& key, const VectorUInt8& plainText, const VectorUInt8& additionalData, size_t desiredTagLengthInBytes)
{
    auto rv = pal::AesGcm::encrypt(key.span(), iv.span(), additionalData.span(), plainText.span(), desiredTagLengthInBytes);
    if (rv.errorCode != Error::Success)
        return makeUnexpected(rv.errorCode);
    return WTF::move(rv.result);
}

Expected<VectorUInt8, Error> decyptAESGCM(const VectorUInt8& iv, const VectorUInt8& key, const VectorUInt8& cipherText, const VectorUInt8& additionalData, size_t desiredTagLengthInBytes)
{
    Vector<uint8_t> plainText(cipherText.size()); // Per section 5.2.1.2: http://nvlpubs.nist.gov/nistpubs/Legacy/SP/nistspecialpublication800-38d.pdf
    Vector<uint8_t> tag(desiredTagLengthInBytes);
    size_t offset = cipherText.size() - desiredTagLengthInBytes;
    // tagLength is actual an input <rdar://problem/30660074>
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    CCCryptorStatus status = CCCryptorGCM(kCCDecrypt, kCCAlgorithmAES, key.span().data(), key.size(), iv.span().data(), iv.size(), additionalData.span().data(), additionalData.size(), cipherText.span().data(), offset, plainText.mutableSpan().data(), tag.mutableSpan().data(), &desiredTagLengthInBytes);
ALLOW_DEPRECATED_DECLARATIONS_END
    if (status)
        return makeUnexpected(Error::DecryptionFailed);

    // Using a constant time comparison to prevent timing attacks.
    if (constantTimeMemcmp(tag.span(), cipherText.subspan(offset)))
        return makeUnexpected(Error::DecryptionFailed);

    plainText.shrink(offset);
    return WTF::move(plainText);
}

}

/*
 * Copyright (C) 2017-2023 Apple Inc. All rights reserved.
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
#include "CryptoAlgorithmAESGCM.h"

#include "CommonCryptoUtilities.h"
#include "CryptoAlgorithmAesGcmParams.h"
#include "CryptoKeyAES.h"
#include <pal/PALSwift.h>
#include <wtf/CryptographicUtilities.h>
#include <wtf/StdLibExtras.h>

namespace WebCore {

static ExceptionOr<Vector<uint8_t>> encryptAESGCM(const Vector<uint8_t>& iv, const Vector<uint8_t>& key, const Vector<uint8_t>& plainText, const Vector<uint8_t>& additionalData, size_t desiredTagLengthInBytes)
{
    Vector<uint8_t> cipherText(plainText.size() + desiredTagLengthInBytes); // Per section 5.2.1.2: http://nvlpubs.nist.gov/nistpubs/Legacy/SP/nistspecialpublication800-38d.pdf
    Vector<uint8_t> tag(desiredTagLengthInBytes);
    // tagLength is actual an input <rdar://problem/30660074>
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    CCCryptorStatus status = CCCryptorGCM(kCCEncrypt, kCCAlgorithmAES, key.span().data(), key.size(), iv.span().data(), iv.size(), additionalData.span().data(), additionalData.size(), plainText.span().data(), plainText.size(), cipherText.mutableSpan().data(), tag.mutableSpan().data(), &desiredTagLengthInBytes);
ALLOW_DEPRECATED_DECLARATIONS_END
    if (status)
        return Exception { ExceptionCode::OperationError };
    memcpySpan(cipherText.mutableSpan().subspan(plainText.size()), tag.span());

    return WTFMove(cipherText);
}

static ExceptionOr<Vector<uint8_t>> encryptCryptoKitAESGCM(const Vector<uint8_t>& iv, const Vector<uint8_t>& key, const Vector<uint8_t>& plainText, const Vector<uint8_t>& additionalData, size_t desiredTagLengthInBytes)
{
#if !defined(CLANG_WEBKIT_BRANCH)
    auto rv = PAL::AesGcm::encrypt(key.span(), iv.span(), additionalData.span(), plainText.span(), desiredTagLengthInBytes);
    if (rv.errorCode != Cpp::ErrorCodes::Success)
        return Exception { ExceptionCode::OperationError };
    return WTFMove(rv.result);
#else
    UNUSED_PARAM(iv);
    UNUSED_PARAM(key);
    UNUSED_PARAM(plainText);
    UNUSED_PARAM(additionalData);
    UNUSED_PARAM(desiredTagLengthInBytes);
    RELEASE_ASSERT_NOT_REACHED_WITH_MESSAGE("CLANG_WEBKIT_BRANCH");
#endif
}

static ExceptionOr<Vector<uint8_t>> decyptAESGCM(const Vector<uint8_t>& iv, const Vector<uint8_t>& key, const Vector<uint8_t>& cipherText, const Vector<uint8_t>& additionalData, size_t desiredTagLengthInBytes)
{
    Vector<uint8_t> plainText(cipherText.size()); // Per section 5.2.1.2: http://nvlpubs.nist.gov/nistpubs/Legacy/SP/nistspecialpublication800-38d.pdf
    Vector<uint8_t> tag(desiredTagLengthInBytes);
    size_t offset = cipherText.size() - desiredTagLengthInBytes;
    // tagLength is actual an input <rdar://problem/30660074>
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    CCCryptorStatus status = CCCryptorGCM(kCCDecrypt, kCCAlgorithmAES, key.span().data(), key.size(), iv.span().data(), iv.size(), additionalData.span().data(), additionalData.size(), cipherText.span().data(), offset, plainText.mutableSpan().data(), tag.mutableSpan().data(), &desiredTagLengthInBytes);
ALLOW_DEPRECATED_DECLARATIONS_END
    if (status)
        return Exception { ExceptionCode::OperationError };

    // Using a constant time comparison to prevent timing attacks.
    if (constantTimeMemcmp(tag.span(), cipherText.subspan(offset)))
        return Exception { ExceptionCode::OperationError };

    plainText.shrink(offset);
    return WTFMove(plainText);
}

ExceptionOr<Vector<uint8_t>> CryptoAlgorithmAESGCM::platformEncrypt(const CryptoAlgorithmAesGcmParams& parameters, const CryptoKeyAES& key, const Vector<uint8_t>& plainText)
{
    if (parameters.ivVector().size() >= 12)
        return encryptCryptoKitAESGCM(parameters.ivVector(), key.key(), plainText, parameters.additionalDataVector(), parameters.tagLength.value_or(0) / 8);
    return encryptAESGCM(parameters.ivVector(), key.key(), plainText, parameters.additionalDataVector(), parameters.tagLength.value_or(0) / 8);
}

ExceptionOr<Vector<uint8_t>> CryptoAlgorithmAESGCM::platformDecrypt(const CryptoAlgorithmAesGcmParams& parameters, const CryptoKeyAES& key, const Vector<uint8_t>& cipherText)
{
    // FIXME: Add decrypt with CryptoKit once rdar://92701544 is resolved.
    return decyptAESGCM(parameters.ivVector(), key.key(), cipherText, parameters.additionalDataVector(), parameters.tagLength.value_or(0) / 8);
}

} // namespace WebCore

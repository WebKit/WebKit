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
#include "CryptoUtilitiesCocoa.h"

#if ENABLE(WEB_CRYPTO) || ENABLE(WEB_RTC)

#include "CryptoAlgorithmAES_CTR.h"
#include <CommonCrypto/CommonCrypto.h>

namespace WebCore {

ExceptionOr<Vector<uint8_t>> transformAES_CTR(CCOperation operation, const Vector<uint8_t>& counter, size_t counterLength, const Vector<uint8_t>& key, const uint8_t* data, size_t dataSize)
{
    // FIXME: We should remove the following hack once <rdar://problem/31361050> is fixed.
    // counter = nonce + counter
    // CommonCrypto currently can neither reset the counter nor detect overflow once the counter reaches its max value restricted
    // by the counterLength. It then increments the nonce which should stay same for the whole operation. To remedy this issue,
    // we detect the overflow ahead and divide the operation into two parts.
    size_t numberOfBlocks = dataSize % kCCBlockSizeAES128 ? dataSize / kCCBlockSizeAES128 + 1 : dataSize / kCCBlockSizeAES128;

    // Detect loop
    if (counterLength < sizeof(size_t) * 8 && numberOfBlocks > (static_cast<size_t>(1) << counterLength))
        return Exception { OperationError };

    // Calculate capacity before overflow
    CryptoAlgorithmAES_CTR::CounterBlockHelper counterBlockHelper(counter, counterLength);
    size_t capacity = counterBlockHelper.countToOverflowSaturating();

    // Divide data into two parts if necessary.
    size_t headSize = dataSize;
    if (capacity < numberOfBlocks)
        headSize = capacity * kCCBlockSizeAES128;

    // first part: compute the first n=capacity blocks of data if capacity is insufficient. Otherwise, return the result.
    CCCryptorRef cryptor;
    CCCryptorStatus status = CCCryptorCreateWithMode(operation, kCCModeCTR, kCCAlgorithmAES128, ccNoPadding, counter.data(), key.data(), key.size(), 0, 0, 0, kCCModeOptionCTR_BE, &cryptor);
    if (status)
        return Exception { OperationError };

    Vector<uint8_t> head(CCCryptorGetOutputLength(cryptor, headSize, true));

    size_t bytesWritten;
    status = CCCryptorUpdate(cryptor, data, headSize, head.data(), head.size(), &bytesWritten);
    if (status)
        return Exception { OperationError };

    uint8_t* p = head.data() + bytesWritten;
    status = CCCryptorFinal(cryptor, p, head.end() - p, &bytesWritten);
    p += bytesWritten;
    if (status)
        return Exception { OperationError };

    ASSERT_WITH_SECURITY_IMPLICATION(p <= head.end());
    head.shrink(p - head.begin());

    CCCryptorRelease(cryptor);

    if (capacity >= numberOfBlocks)
        return WTFMove(head);

    // second part: compute the remaining data and append them to the head.
    // reset counter
    Vector<uint8_t> remainingCounter = counterBlockHelper.counterVectorAfterOverflow();
    status = CCCryptorCreateWithMode(operation, kCCModeCTR, kCCAlgorithmAES128, ccNoPadding, remainingCounter.data(), key.data(), key.size(), 0, 0, 0, kCCModeOptionCTR_BE, &cryptor);
    if (status)
        return Exception { OperationError };

    size_t tailSize = dataSize - headSize;
    Vector<uint8_t> tail(CCCryptorGetOutputLength(cryptor, tailSize, true));

    status = CCCryptorUpdate(cryptor, data + headSize, tailSize, tail.data(), tail.size(), &bytesWritten);
    if (status)
        return Exception { OperationError };

    p = tail.data() + bytesWritten;
    status = CCCryptorFinal(cryptor, p, tail.end() - p, &bytesWritten);
    p += bytesWritten;
    if (status)
        return Exception { OperationError };

    ASSERT_WITH_SECURITY_IMPLICATION(p <= tail.end());
    tail.shrink(p - tail.begin());

    CCCryptorRelease(cryptor);

    head.appendVector(tail);
    return WTFMove(head);
}

ExceptionOr<Vector<uint8_t>> deriveHDKFBits(CCDigestAlgorithm digestAlgorithm, const uint8_t* key, size_t keySize, const uint8_t* salt, size_t saltSize, const uint8_t* info, size_t infoSize, size_t length)
{
    Vector<uint8_t> result(length / 8);
    Vector<uint8_t> infoVector;

    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    // <rdar://problem/32439455> Currently, when key data is empty, CCKeyDerivationHMac will bail out.
    // <rdar://problem/48896021> Reminder: Switch to CCDeriveKey now that CCKeyDerivationHMac is deprecated.
    if (CCKeyDerivationHMac(kCCKDFAlgorithmHKDF, digestAlgorithm, 0, key, keySize, 0, 0, info, infoSize, 0, 0, salt, saltSize, result.data(), result.size()))
        return Exception { OperationError };
    ALLOW_DEPRECATED_DECLARATIONS_END
    return WTFMove(result);
}

ExceptionOr<Vector<uint8_t>> deriveHDKFSHA256Bits(const uint8_t* key, size_t keySize, const uint8_t* salt, size_t saltSize, const uint8_t* info, size_t infoSize, size_t length)
{
    return deriveHDKFBits(kCCDigestSHA256, key, keySize, salt, saltSize, info, infoSize, length);
}

Vector<uint8_t> calculateHMACSignature(CCHmacAlgorithm algorithm, const Vector<uint8_t>& key, const uint8_t* data, size_t size)
{
    size_t digestLength;
    switch (algorithm) {
    case kCCHmacAlgSHA1:
        digestLength = CC_SHA1_DIGEST_LENGTH;
        break;
    case kCCHmacAlgSHA224:
        digestLength = CC_SHA224_DIGEST_LENGTH;
        break;
    case kCCHmacAlgSHA256:
        digestLength = CC_SHA256_DIGEST_LENGTH;
        break;
    case kCCHmacAlgSHA384:
        digestLength = CC_SHA384_DIGEST_LENGTH;
        break;
    case kCCHmacAlgSHA512:
        digestLength = CC_SHA512_DIGEST_LENGTH;
        break;
    default:
        ASSERT_NOT_REACHED();
        return Vector<uint8_t>();
    }

    Vector<uint8_t> result(digestLength);
    CCHmac(algorithm, key.data(), key.size(), data, size, result.data());
    return result;
}

Vector<uint8_t> calculateSHA256Signature(const Vector<uint8_t>& key, const uint8_t* data, size_t size)
{
    return calculateHMACSignature(kCCHmacAlgSHA256, key, data, size);
}

} // namespace WebCore

#endif // ENABLE(WEB_CRYPTO)

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
#include "CryptoAlgorithmAES_CTR.h"

#if ENABLE(WEB_CRYPTO)

#include "CryptoAlgorithmAesCtrParams.h"
#include "CryptoKeyAES.h"
#include <CommonCrypto/CommonCrypto.h>

namespace WebCore {

// It takes the last WORDSIZE/8 bytes of the bigInteger and then convert them into a size_t value
static size_t bigIntegerToSizeT(const Vector<uint8_t>& bigInteger)
{
    size_t result = 0;
    for (size_t i = bigInteger.size() - (__WORDSIZE / 8); i < bigInteger.size(); ++i) {
        result <<= 8;
        result += bigInteger[i];
    }
    return result;
}

static ExceptionOr<Vector<uint8_t>> transformAES_CTR(CCOperation operation, const Vector<uint8_t>& counter, size_t counterLength, const Vector<uint8_t>& key, const Vector<uint8_t>& data)
{
    // FIXME: We should remove the following hack once <rdar://problem/31361050> is fixed.
    // counter = nonce + counter
    // CommonCrypto currently can neither reset the counter nor detect overflow once the counter reaches its max value restricted
    // by the counterLength. It then increments the nonce which should stay same for the whole operation. To remedy this issue,
    // we detect the overflow ahead and divide the operation into two parts.
    // Ignore the case: counterLength > __WORDSIZE.
    size_t numberOfBlocks = data.size() % kCCBlockSizeAES128 ? data.size() / kCCBlockSizeAES128 + 1 : data.size() / kCCBlockSizeAES128;
    // Detect loop
    if (counterLength < __WORDSIZE && numberOfBlocks > (1 << counterLength))
        return Exception { OperationError };
    // Calculate capacity before overflow
    size_t rightMost = bigIntegerToSizeT(counter); // convert right most __WORDSIZE bits into a size_t value, which is the longest counter we could possibly use.
    size_t capacity = numberOfBlocks; // SIZE_MAX - counter
    if (counterLength < __WORDSIZE) {
        size_t mask = SIZE_MAX << counterLength; // Used to set nonce in rightMost(nonce + counter) to 1s
        capacity = SIZE_MAX - (rightMost| mask) + 1;
    }
    if (counterLength == __WORDSIZE)
        capacity = SIZE_MAX - rightMost + 1;

    // Divide data into two parts if necessary.
    size_t headSize = data.size();
    if (capacity < numberOfBlocks)
        headSize = capacity * kCCBlockSizeAES128;

    // first part: compute the first n=capacity blocks of data if capacity is insufficient. Otherwise, return the result.
    CCCryptorRef cryptor;
    CCCryptorStatus status = CCCryptorCreateWithMode(operation, kCCModeCTR, kCCAlgorithmAES128, ccNoPadding, counter.data(), key.data(), key.size(), 0, 0, 0, kCCModeOptionCTR_BE, &cryptor);
    if (status)
        return Exception { OperationError };

    Vector<uint8_t> head(CCCryptorGetOutputLength(cryptor, headSize, true));

    size_t bytesWritten;
    status = CCCryptorUpdate(cryptor, data.data(), headSize, head.data(), head.size(), &bytesWritten);
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
    Vector<uint8_t> remainingCounter(counter.size());
    size_t counterOffset = counterLength % 8;
    size_t nonceOffset = counter.size() - counterLength / 8 - !!counterOffset;
    memcpy(remainingCounter.data(), counter.data(), nonceOffset); // copy the nonce
    // set the middle byte
    if (!!counterOffset) {
        size_t mask = SIZE_MAX << counterOffset;
        remainingCounter[nonceOffset] = counter[nonceOffset] & mask;
    }
    memset(remainingCounter.data() + nonceOffset + !!counterOffset, 0, counterLength / 8); // reset the counter

    status = CCCryptorCreateWithMode(operation, kCCModeCTR, kCCAlgorithmAES128, ccNoPadding, remainingCounter.data(), key.data(), key.size(), 0, 0, 0, kCCModeOptionCTR_BE, &cryptor);
    if (status)
        return Exception { OperationError };

    size_t tailSize = data.size() - headSize;
    Vector<uint8_t> tail(CCCryptorGetOutputLength(cryptor, tailSize, true));

    status = CCCryptorUpdate(cryptor, data.data() + headSize, tailSize, tail.data(), tail.size(), &bytesWritten);
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

ExceptionOr<Vector<uint8_t>> CryptoAlgorithmAES_CTR::platformEncrypt(const CryptoAlgorithmAesCtrParams& parameters, const CryptoKeyAES& key, const Vector<uint8_t>& plainText)
{
    ASSERT(parameters.counterVector().size() == kCCBlockSizeAES128);
    return transformAES_CTR(kCCEncrypt, parameters.counterVector(), parameters.length, key.key(), plainText);
}

ExceptionOr<Vector<uint8_t>> CryptoAlgorithmAES_CTR::platformDecrypt(const CryptoAlgorithmAesCtrParams& parameters, const CryptoKeyAES& key, const Vector<uint8_t>& cipherText)
{
      ASSERT(parameters.counterVector().size() == kCCBlockSizeAES128);
    return transformAES_CTR(kCCDecrypt, parameters.counterVector(), parameters.length, key.key(), cipherText);
}

} // namespace WebCore

#endif // ENABLE(WEB_CRYPTO)

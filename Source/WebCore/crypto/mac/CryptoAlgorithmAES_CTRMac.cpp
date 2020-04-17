/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "CryptoAlgorithmAES_CTR.h"

#if ENABLE(WEB_CRYPTO)

#include "CryptoAlgorithmAesCtrParams.h"
#include "CryptoKeyAES.h"
#include <CommonCrypto/CommonCrypto.h>

namespace WebCore {

static ExceptionOr<Vector<uint8_t>> transformAES_CTR(CCOperation operation, const Vector<uint8_t>& counter, size_t counterLength, const Vector<uint8_t>& key, const Vector<uint8_t>& data)
{
    // FIXME: We should remove the following hack once <rdar://problem/31361050> is fixed.
    // counter = nonce + counter
    // CommonCrypto currently can neither reset the counter nor detect overflow once the counter reaches its max value restricted
    // by the counterLength. It then increments the nonce which should stay same for the whole operation. To remedy this issue,
    // we detect the overflow ahead and divide the operation into two parts.
    size_t numberOfBlocks = data.size() % kCCBlockSizeAES128 ? data.size() / kCCBlockSizeAES128 + 1 : data.size() / kCCBlockSizeAES128;

    // Detect loop
    if (counterLength < sizeof(size_t) * 8 && numberOfBlocks > (static_cast<size_t>(1) << counterLength))
        return Exception { OperationError };

    // Calculate capacity before overflow
    CryptoAlgorithmAES_CTR::CounterBlockHelper counterBlockHelper(counter, counterLength);
    size_t capacity = counterBlockHelper.countToOverflowSaturating();

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
    Vector<uint8_t> remainingCounter = counterBlockHelper.counterVectorAfterOverflow();
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

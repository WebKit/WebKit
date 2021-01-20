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
#include "CryptoAlgorithmAES_CFB.h"

#if ENABLE(WEB_CRYPTO)

#include "CryptoAlgorithmAesCbcCfbParams.h"
#include "CryptoKeyAES.h"
#include <CommonCrypto/CommonCrypto.h>

namespace WebCore {

static ExceptionOr<Vector<uint8_t>> transformAES_CFB(CCOperation operation, const Vector<uint8_t>& iv, const Vector<uint8_t>& key, const Vector<uint8_t>& data)
{
    CCCryptorRef cryptor;
    CCCryptorStatus status = CCCryptorCreateWithMode(operation, kCCModeCFB8, kCCAlgorithmAES, ccNoPadding, iv.data(), key.data(), key.size(), 0, 0, 0, 0, &cryptor);
    if (status)
        return Exception { OperationError };

    Vector<uint8_t> result(CCCryptorGetOutputLength(cryptor, data.size(), true));

    size_t bytesWritten;
    status = CCCryptorUpdate(cryptor, data.data(), data.size(), result.data(), result.size(), &bytesWritten);
    if (status)
        return Exception { OperationError };

    uint8_t* p = result.data() + bytesWritten;
    status = CCCryptorFinal(cryptor, p, result.end() - p, &bytesWritten);
    p += bytesWritten;
    if (status)
        return Exception { OperationError };

    ASSERT(p <= result.end());
    result.shrink(p - result.begin());

    CCCryptorRelease(cryptor);

    return WTFMove(result);
}

ExceptionOr<Vector<uint8_t>> CryptoAlgorithmAES_CFB::platformEncrypt(const CryptoAlgorithmAesCbcCfbParams& parameters, const CryptoKeyAES& key, const Vector<uint8_t>& plainText)
{
    ASSERT(parameters.ivVector().size() == kCCBlockSizeAES128);
    return transformAES_CFB(kCCEncrypt, parameters.ivVector(), key.key(), plainText);
}

ExceptionOr<Vector<uint8_t>> CryptoAlgorithmAES_CFB::platformDecrypt(const CryptoAlgorithmAesCbcCfbParams& parameters, const CryptoKeyAES& key, const Vector<uint8_t>& cipherText)
{
    ASSERT(parameters.ivVector().size() == kCCBlockSizeAES128);
    return transformAES_CFB(kCCDecrypt, parameters.ivVector(), key.key(), cipherText);
}

} // namespace WebCore

#endif // ENABLE(WEB_CRYPTO)

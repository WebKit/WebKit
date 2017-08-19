/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#include "CryptoAlgorithmAES_KW.h"

#if ENABLE(SUBTLE_CRYPTO)

#include "CryptoKeyAES.h"
#include <CommonCrypto/CommonCrypto.h>

namespace WebCore {

static ExceptionOr<Vector<uint8_t>> wrapKeyAES_KW(const Vector<uint8_t>& key, const Vector<uint8_t>& data)
{
    Vector<uint8_t> result(CCSymmetricWrappedSize(kCCWRAPAES, data.size()));
    size_t resultSize = result.size();
    if (CCSymmetricKeyWrap(kCCWRAPAES, CCrfc3394_iv, CCrfc3394_ivLen, key.data(), key.size(), data.data(), data.size(), result.data(), &resultSize))
        return Exception { OperationError };

    result.shrink(resultSize);
    return WTFMove(result);
}

static ExceptionOr<Vector<uint8_t>> unwrapKeyAES_KW(const Vector<uint8_t>& key, const Vector<uint8_t>& data)
{
    Vector<uint8_t> result(CCSymmetricUnwrappedSize(kCCWRAPAES, data.size()));
    size_t resultSize = result.size();

    if (resultSize % 8)
        return Exception { OperationError };

    if (CCSymmetricKeyUnwrap(kCCWRAPAES, CCrfc3394_iv, CCrfc3394_ivLen, key.data(), key.size(), data.data(), data.size(), result.data(), &resultSize))
        return Exception { OperationError };

    result.shrink(resultSize);
    return WTFMove(result);
}

void CryptoAlgorithmAES_KW::platformWrapKey(Ref<CryptoKey>&& key, Vector<uint8_t>&& data, VectorCallback&& callback, ExceptionCallback&& exceptionCallback)
{
    auto& aesKey = downcast<CryptoKeyAES>(key.get());
    auto result = wrapKeyAES_KW(aesKey.key(), data);
    if (result.hasException()) {
        exceptionCallback(result.releaseException().code());
        return;
    }
    callback(result.releaseReturnValue());
}

void CryptoAlgorithmAES_KW::platformUnwrapKey(Ref<CryptoKey>&& key, Vector<uint8_t>&& data, VectorCallback&& callback, ExceptionCallback&& exceptionCallback)
{
    auto& aesKey = downcast<CryptoKeyAES>(key.get());
    auto result = unwrapKeyAES_KW(aesKey.key(), data);
    if (result.hasException()) {
        exceptionCallback(result.releaseException().code());
        return;
    }
    callback(result.releaseReturnValue());
}

} // namespace WebCore

#endif // ENABLE(SUBTLE_CRYPTO)

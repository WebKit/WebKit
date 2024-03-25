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
#include "CryptoAlgorithmAESKW.h"

#include "CryptoKeyAES.h"
#include "PALSwift.h"
#include <CommonCrypto/CommonCrypto.h>

namespace WebCore {

static ExceptionOr<Vector<uint8_t>> wrapKeyAESKW(const Vector<uint8_t>& key, const Vector<uint8_t>& data)
{
    Vector<uint8_t> result(CCSymmetricWrappedSize(kCCWRAPAES, data.size()));
    size_t resultSize = result.size();
    if (CCSymmetricKeyWrap(kCCWRAPAES, CCrfc3394_iv, CCrfc3394_ivLen, key.data(), key.size(), data.data(), data.size(), result.data(), &resultSize))
        return Exception { ExceptionCode::OperationError };

    result.shrink(resultSize);
    return WTFMove(result);
}

static ExceptionOr<Vector<uint8_t>> wrapKeyAESKWCryptoKit(const Vector<uint8_t>& key, const Vector<uint8_t>& data)
{
    Vector<uint8_t> result(data.size() + CryptoAlgorithmAESKW::s_extraSize);
    uint64_t resultSize = result.size();
    const WebCryptoAesKwReturnValue* rv = [WebCryptoAesKw wrap:key.data() keySize:key.size() data:data.data() dataSize:data.size() cipherText:result.data() cipherTextSize:resultSize];
    if (WebCryptoErrorCodesSuccess != rv.errCode)
        return Exception { ExceptionCode::OperationError };
    result.shrink(rv.outputSize);
    return WTFMove(result);
}

static ExceptionOr<Vector<uint8_t>> unwrapKeyAESKW(const Vector<uint8_t>& key, const Vector<uint8_t>& data)
{
    Vector<uint8_t> result(CCSymmetricUnwrappedSize(kCCWRAPAES, data.size()));
    size_t resultSize = result.size();

    if (resultSize % 8)
        return Exception { ExceptionCode::OperationError };

    if (CCSymmetricKeyUnwrap(kCCWRAPAES, CCrfc3394_iv, CCrfc3394_ivLen, key.data(), key.size(), data.data(), data.size(), result.data(), &resultSize))
        return Exception { ExceptionCode::OperationError };

    result.shrink(resultSize);
    return WTFMove(result);
}

static ExceptionOr<Vector<uint8_t>> unwrapKeyAESKWCryptoKit(const Vector<uint8_t>& key, const Vector<uint8_t>& data)
{
    Vector<uint8_t> dataOut(data.size());
    uint64_t dataOutSize = dataOut.size();
    const WebCryptoAesKwReturnValue* rv = [WebCryptoAesKw unwrap:key.data() keySize:key.size() cipherText:data.data() cipherTextSize:data.size() data:dataOut.data() dataSize:dataOutSize];
    if (WebCryptoErrorCodesSuccess != rv.errCode)
        return Exception { ExceptionCode::OperationError };
    if (rv.outputSize % 8)
        return Exception { ExceptionCode::OperationError };
    dataOut.shrink(rv.outputSize);
    return WTFMove(dataOut);
}

ExceptionOr<Vector<uint8_t>> CryptoAlgorithmAESKW::platformWrapKey(const CryptoKeyAES& key, const Vector<uint8_t>& data, bool useCryptoKit)
{
    if (useCryptoKit)
        return wrapKeyAESKWCryptoKit(key.key(), data);
    return wrapKeyAESKW(key.key(), data);
}

ExceptionOr<Vector<uint8_t>> CryptoAlgorithmAESKW::platformUnwrapKey(const CryptoKeyAES& key, const Vector<uint8_t>& data, bool useCryptoKit)
{
    if (useCryptoKit)
        return unwrapKeyAESKWCryptoKit(key.key(), data);
    return unwrapKeyAESKW(key.key(), data);
}

} // namespace WebCore

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
#include "CryptoAlgorithmHMAC.h"

#include "CryptoKeyHMAC.h"
#include "CryptoUtilitiesCocoa.h"
#include <CommonCrypto/CommonHMAC.h>

#if HAVE(SWIFT_CPP_INTEROP)
#include <pal/PALSwift.h>
#endif

#include <wtf/CryptographicUtilities.h>

namespace WebCore {

static ExceptionOr<Vector<uint8_t>> platformSignCryptoKit(const CryptoKeyHMAC& key, const Vector<uint8_t>& data)
{
#if !defined(CLANG_WEBKIT_BRANCH)
    if (!isValidHashParameter(key.hashAlgorithmIdentifier()))
        return Exception { ExceptionCode::OperationError };
    return PAL::HMAC::sign(key.key().span(), data.span(), std::to_underlying(toCKHashFunction(key.hashAlgorithmIdentifier())));
#else
    UNUSED_PARAM(key);
    UNUSED_PARAM(data);
    RELEASE_ASSERT_NOT_REACHED_WITH_MESSAGE("CLANG_WEBKIT_BRANCH");
#endif
}

static ExceptionOr<bool> platformVerifyCryptoKit(const CryptoKeyHMAC& key, const Vector<uint8_t>& signature, const Vector<uint8_t>& data)
{
#if !defined(CLANG_WEBKIT_BRANCH)
    if (!isValidHashParameter(key.hashAlgorithmIdentifier()))
        return Exception { ExceptionCode::OperationError };
    return PAL::HMAC::verify(signature.span(), key.key().span(), data.span(), std::to_underlying(toCKHashFunction(key.hashAlgorithmIdentifier())));
#else
    UNUSED_PARAM(key);
    UNUSED_PARAM(signature);
    UNUSED_PARAM(data);
    RELEASE_ASSERT_NOT_REACHED_WITH_MESSAGE("CLANG_WEBKIT_BRANCH");
#endif
}

ExceptionOr<Vector<uint8_t>> CryptoAlgorithmHMAC::platformSign(const CryptoKeyHMAC& key, const Vector<uint8_t>& data)
{
    return platformSignCryptoKit(key, data);
}

ExceptionOr<bool> CryptoAlgorithmHMAC::platformVerify(const CryptoKeyHMAC& key, const Vector<uint8_t>& signature, const Vector<uint8_t>& data)
{
    return platformVerifyCryptoKit(key, signature, data);
}
} // namespace WebCore

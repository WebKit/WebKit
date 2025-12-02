/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#include "CryptoAlgorithmEd25519.h"

#include "CryptoKeyOKP.h"
#include "ExceptionOr.h"
#include <pal/PALSwift.h>
#include <pal/spi/cocoa/CoreCryptoSPI.h>

namespace WebCore {

static ExceptionOr<Vector<uint8_t>> signEd25519CryptoKit(const Vector<uint8_t>&sk, const Vector<uint8_t>& data)
{
#if !defined(CLANG_WEBKIT_BRANCH)
    if (sk.size() != ed25519KeySize)
        return Exception { ExceptionCode::OperationError };
    auto rv = PAL::EdKey::sign(PAL::EdSigningAlgorithm::ed25519(), sk.span(), data.span());
    if (rv.errorCode != Cpp::ErrorCodes::Success)
        return Exception { ExceptionCode::OperationError };
    return WTFMove(rv.result);
#else
    UNUSED_PARAM(sk);
    UNUSED_PARAM(data);
    RELEASE_ASSERT_NOT_REACHED_WITH_MESSAGE("CLANG_WEBKIT_BRANCH");
#endif
}

static ExceptionOr<bool>  verifyEd25519CryptoKit(const Vector<uint8_t>& pubKey, const Vector<uint8_t>& signature, const Vector<uint8_t>& data)
{
#if !defined(CLANG_WEBKIT_BRANCH)
    if (pubKey.size() != ed25519KeySize || signature.size() != ed25519SignatureSize)
        return false;
    auto rv = PAL::EdKey::verify(PAL::EdSigningAlgorithm::ed25519(), pubKey.span(), signature.span(), data.span());
    return rv.errorCode == Cpp::ErrorCodes::Success;
#else
    UNUSED_PARAM(pubKey);
    UNUSED_PARAM(signature);
    UNUSED_PARAM(data);
    RELEASE_ASSERT_NOT_REACHED_WITH_MESSAGE("CLANG_WEBKIT_BRANCH");
#endif
}

ExceptionOr<Vector<uint8_t>> CryptoAlgorithmEd25519::platformSign(const CryptoKeyOKP& key, const Vector<uint8_t>& data)
{
    return signEd25519CryptoKit(key.platformKey(), data);
}

ExceptionOr<bool> CryptoAlgorithmEd25519::platformVerify(const CryptoKeyOKP& key, const Vector<uint8_t>& signature, const Vector<uint8_t>& data)
{
    return verifyEd25519CryptoKit(key.platformKey(), signature, data);
}

} // namespace WebCore

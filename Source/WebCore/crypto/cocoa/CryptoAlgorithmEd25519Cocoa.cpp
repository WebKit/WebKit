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
#if HAVE(SWIFT_CPP_INTEROP)
#include <pal/PALSwift.h>
#endif
#include <pal/spi/cocoa/CoreCryptoSPI.h>

namespace WebCore {

#if !HAVE(SWIFT_CPP_INTEROP)
static ExceptionOr<Vector<uint8_t>> signEd25519(const Vector<uint8_t>& sk, const Vector<uint8_t>& data)
{
    if (sk.size() != ed25519KeySize)
        return Exception { ExceptionCode::OperationError };
    ccec25519pubkey pk;
    const struct ccdigest_info* di = ccsha512_di();
    if (cced25519_make_pub(di, pk, sk.data()))
        return Exception { ExceptionCode::OperationError };
    ccec25519signature newSignature;

#if HAVE(CORE_CRYPTO_SIGNATURES_INT_RETURN_VALUE)
    if (cced25519_sign(di, newSignature, data.size(), data.data(), pk, sk.data()))
        return Exception { ExceptionCode::OperationError };
#else
    cced25519_sign(di, newSignature, data.size(), data.data(), pk, sk.data());
#endif
    return Vector<uint8_t> { std::span { newSignature } };
}

static ExceptionOr<bool> verifyEd25519(const Vector<uint8_t>& key, const Vector<uint8_t>& signature, const Vector<uint8_t> data)
{
    if (key.size() != ed25519KeySize || signature.size() != ed25519SignatureSize)
        return false;
    const struct ccdigest_info* di = ccsha512_di();
    return !cced25519_verify(di, data.size(), data.data(), signature.data(), key.data());
}
#else
static ExceptionOr<Vector<uint8_t>> signEd25519CryptoKit(const Vector<uint8_t>&sk, const Vector<uint8_t>& data)
{
    if (sk.size() != ed25519KeySize)
        return Exception { ExceptionCode::OperationError };
    auto rv = PAL::EdKey::sign(PAL::EdSigningAlgorithm::ed25519(), sk.span(), data.span());
    if (rv.errorCode != Cpp::ErrorCodes::Success)
        return Exception { ExceptionCode::OperationError };
    return WTFMove(rv.result);
}

static ExceptionOr<bool>  verifyEd25519CryptoKit(const Vector<uint8_t>& pubKey, const Vector<uint8_t>& signature, const Vector<uint8_t>& data)
{
    if (pubKey.size() != ed25519KeySize || signature.size() != ed25519SignatureSize)
        return false;
    auto rv = PAL::EdKey::verify(PAL::EdSigningAlgorithm::ed25519(), pubKey.span(), signature.span(), data.span());
    return rv.errorCode == Cpp::ErrorCodes::Success;
}
#endif

ExceptionOr<Vector<uint8_t>> CryptoAlgorithmEd25519::platformSign(const CryptoKeyOKP& key, const Vector<uint8_t>& data)
{
#if HAVE(SWIFT_CPP_INTEROP)
    return signEd25519CryptoKit(key.platformKey(), data);
#else
    return signEd25519(key.platformKey(), data);
#endif
}

ExceptionOr<bool> CryptoAlgorithmEd25519::platformVerify(const CryptoKeyOKP& key, const Vector<uint8_t>& signature, const Vector<uint8_t>& data)
{
#if HAVE(SWIFT_CPP_INTEROP)
    return verifyEd25519CryptoKit(key.platformKey(), signature, data);
#else
    return verifyEd25519(key.platformKey(), signature, data);
#endif
}

} // namespace WebCore

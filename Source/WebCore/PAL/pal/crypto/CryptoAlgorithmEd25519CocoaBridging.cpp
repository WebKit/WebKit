/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "CryptoAlgorithmEd25519CocoaBridging.h"

#include "PALSwift-Generated.h"

namespace PAL::Crypto {

Expected<VectorUInt8, Error> signEd25519CryptoKit(const VectorUInt8 &sk, const VectorUInt8& data)
{
    if (sk.size() != ed25519KeySize)
        return makeUnexpected(Error::FailedToSign);
    auto rv = pal::EdKey::sign(PAL::Crypto::EdSigningAlgorithm::ED25519, sk.span(), data.span());
    if (rv.errorCode != PAL::Crypto::Error::Success)
        return makeUnexpected(rv.errorCode);
    return WTF::move(rv.result);
}

Expected<bool, Error> verifyEd25519CryptoKit(const VectorUInt8& pubKey, const VectorUInt8& signature, const VectorUInt8& data)
{
    if (pubKey.size() != ed25519KeySize || signature.size() != ed25519SignatureSize)
        return false;
    auto rv = pal::EdKey::verify(PAL::Crypto::EdSigningAlgorithm::ED25519, pubKey.span(), signature.span(), data.span());
    return rv.errorCode == PAL::Crypto::Error::Success;
}

}

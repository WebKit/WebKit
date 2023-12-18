/*
 * Copyright (C) 2019-2021 Apple Inc. All rights reserved.
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
#include "AuthenticatorAttestationResponse.h"

#if ENABLE(WEB_AUTHN)

#include "AuthenticatorResponseData.h"
#include "CBORReader.h"
#include "CryptoAlgorithmECDH.h"
#include "CryptoKeyEC.h"
#include "WebAuthenticationUtils.h"

namespace WebCore {

static std::optional<cbor::CBORValue> coseKeyForAttestationObject(Ref<ArrayBuffer> attObj)
{
    auto decodedResponse = cbor::CBORReader::read(attObj->toVector());
    if (!decodedResponse || !decodedResponse->isMap()) {
        ASSERT_NOT_REACHED();
        return std::nullopt;
    }
    const auto& attObjMap = decodedResponse->getMap();
    auto it = attObjMap.find(cbor::CBORValue("authData"));
    if (it == attObjMap.end() || !it->second.isByteString()) {
        ASSERT_NOT_REACHED();
        return std::nullopt;
    }
    auto authData = it->second.getByteString();
    const size_t credentialIdLengthOffset = rpIdHashLength + flagsLength + signCounterLength + aaguidLength;
    if (authData.size() < credentialIdLengthOffset + credentialIdLengthLength)
        return std::nullopt;

    const size_t credentialIdLength = (static_cast<size_t>(authData[credentialIdLengthOffset]) << 8) | static_cast<size_t>(authData[credentialIdLengthOffset + 1]);
    const size_t cosePublicKeyOffset = credentialIdLengthOffset + credentialIdLengthLength + credentialIdLength;
    if (authData.size() <= cosePublicKeyOffset)
        return std::nullopt;

    const size_t cosePublicKeyLength = authData.size() - cosePublicKeyOffset;
    Vector<uint8_t> cosePublicKey;
    auto beginIt = authData.begin() + cosePublicKeyOffset;
    cosePublicKey.appendRange(beginIt, beginIt + cosePublicKeyLength);

    return cbor::CBORReader::read(cosePublicKey);
}

Ref<AuthenticatorAttestationResponse> AuthenticatorAttestationResponse::create(Ref<ArrayBuffer>&& rawId, Ref<ArrayBuffer>&& attestationObject, AuthenticatorAttachment attachment, Vector<AuthenticatorTransport>&& transports)
{
    return adoptRef(*new AuthenticatorAttestationResponse(WTFMove(rawId), WTFMove(attestationObject), attachment, WTFMove(transports)));
}

Ref<AuthenticatorAttestationResponse> AuthenticatorAttestationResponse::create(const Vector<uint8_t>& rawId, const Vector<uint8_t>& attestationObject, AuthenticatorAttachment attachment, Vector<AuthenticatorTransport>&& transports)
{
    return create(ArrayBuffer::create(rawId.data(), rawId.size()), ArrayBuffer::create(attestationObject.data(), attestationObject.size()), attachment, WTFMove(transports));
}

AuthenticatorAttestationResponse::AuthenticatorAttestationResponse(Ref<ArrayBuffer>&& rawId, Ref<ArrayBuffer>&& attestationObject, AuthenticatorAttachment attachment, Vector<AuthenticatorTransport>&& transports)
    : AuthenticatorResponse(WTFMove(rawId), attachment)
    , m_attestationObject(WTFMove(attestationObject))
    , m_transports(WTFMove(transports))
{
}

AuthenticatorResponseData AuthenticatorAttestationResponse::data() const
{
    auto data = AuthenticatorResponse::data();
    data.isAuthenticatorAttestationResponse = true;
    data.attestationObject = m_attestationObject.copyRef();
    data.transports = m_transports;
    return data;
}

RefPtr<ArrayBuffer> AuthenticatorAttestationResponse::getAuthenticatorData() const
{
    auto decodedResponse = cbor::CBORReader::read(m_attestationObject->toVector());
    if (!decodedResponse || !decodedResponse->isMap()) {
        ASSERT_NOT_REACHED();
        return nullptr;
    }
    const auto& attObjMap = decodedResponse->getMap();
    auto it = attObjMap.find(cbor::CBORValue("authData"));
    if (it == attObjMap.end() || !it->second.isByteString()) {
        ASSERT_NOT_REACHED();
        return nullptr;
    }
    auto authData = it->second.getByteString();
    return ArrayBuffer::tryCreate(authData.data(), authData.size());
}

int64_t AuthenticatorAttestationResponse::getPublicKeyAlgorithm() const
{
    auto key = coseKeyForAttestationObject(m_attestationObject);
    if (!key || !key->isMap())
        return 0;
    auto& keyMap = key->getMap();

    auto it = keyMap.find(cbor::CBORValue(COSE::alg));
    if (it == keyMap.end() || !it->second.isInteger()) {
        ASSERT_NOT_REACHED();
        return 0;
    }
    return it->second.getInteger();
}

RefPtr<ArrayBuffer> AuthenticatorAttestationResponse::getPublicKey() const
{
    auto key = coseKeyForAttestationObject(m_attestationObject);
    if (!key || !key->isMap())
        return nullptr;
    auto& keyMap = key->getMap();

    auto it = keyMap.find(cbor::CBORValue(COSE::alg));
    if (it == keyMap.end() || !it->second.isInteger()) {
        ASSERT_NOT_REACHED();
        return nullptr;
    }
    auto alg = it->second.getInteger();

    it = keyMap.find(cbor::CBORValue(COSE::kty));
    if (it == keyMap.end() || !it->second.isInteger()) {
        ASSERT_NOT_REACHED();
        return nullptr;
    }
    auto kty = it->second.getInteger();

    std::optional<int64_t> crv;
    it = keyMap.find(cbor::CBORValue(COSE::crv));
    if (it != keyMap.end() && it->second.isInteger())
        crv = it->second.getInteger();

    switch (alg) {
    case COSE::ES256: {
        if (kty != COSE::EC2 || crv != COSE::P_256)
            return nullptr;

        auto it = keyMap.find(cbor::CBORValue(COSE::x));
        if (it == keyMap.end() || !it->second.isByteString()) {
            ASSERT_NOT_REACHED();
            return nullptr;
        }
        auto x = it->second.getByteString();

        it = keyMap.find(cbor::CBORValue(COSE::y));
        if (it == keyMap.end() || !it->second.isByteString()) {
            ASSERT_NOT_REACHED();
            return nullptr;
        }
        auto y = it->second.getByteString();

        auto peerKey = CryptoKeyEC::importRaw(CryptoAlgorithmIdentifier::ECDH, "P-256"_s, encodeRawPublicKey(x, y), true, CryptoKeyUsageDeriveBits);
        if (!peerKey)
            return nullptr;
        auto keySpki = peerKey->exportSpki().releaseReturnValue();
        return ArrayBuffer::tryCreate(keySpki.data(), keySpki.size());
    }
    default:
        break;
    }

    return nullptr;
}

} // namespace WebCore

#endif // ENABLE(WEB_AUTHN)

/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(WEB_AUTHN)

#include <JavaScriptCore/ArrayBuffer.h>
#include <wtf/Forward.h>

namespace WebCore {

class AuthenticatorResponse;

struct PublicKeyCredentialData {
    mutable RefPtr<ArrayBuffer> rawId;

    // AuthenticatorResponse
    bool isAuthenticatorAttestationResponse;
    mutable RefPtr<ArrayBuffer> clientDataJSON;

    // AuthenticatorAttestationResponse
    mutable RefPtr<ArrayBuffer> attestationObject;

    // AuthenticatorAssertionResponse
    mutable RefPtr<ArrayBuffer> authenticatorData;
    mutable RefPtr<ArrayBuffer> signature;
    mutable RefPtr<ArrayBuffer> userHandle;

    // Extensions
    Optional<bool> appid;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<PublicKeyCredentialData> decode(Decoder&);
};

// Noted: clientDataJSON is never encoded or decoded as it is never sent across different processes.
template<class Encoder>
void PublicKeyCredentialData::encode(Encoder& encoder) const
{
    if (!rawId) {
        encoder << true;
        return;
    }
    encoder << false;

    encoder << static_cast<uint64_t>(rawId->byteLength());
    encoder.encodeFixedLengthData(reinterpret_cast<const uint8_t*>(rawId->data()), rawId->byteLength(), 1);

    encoder << isAuthenticatorAttestationResponse;

    if (isAuthenticatorAttestationResponse && attestationObject) {
        encoder << static_cast<uint64_t>(attestationObject->byteLength());
        encoder.encodeFixedLengthData(reinterpret_cast<const uint8_t*>(attestationObject->data()), attestationObject->byteLength(), 1);
        return;
    }

    if (!authenticatorData || !signature)
        return;
    encoder << static_cast<uint64_t>(authenticatorData->byteLength());
    encoder.encodeFixedLengthData(reinterpret_cast<const uint8_t*>(authenticatorData->data()), authenticatorData->byteLength(), 1);
    encoder << static_cast<uint64_t>(signature->byteLength());
    encoder.encodeFixedLengthData(reinterpret_cast<const uint8_t*>(signature->data()), signature->byteLength(), 1);

    // Encode AppID before user handle to avoid the userHandle flag.
    encoder << appid;

    if (!userHandle) {
        encoder << false;
        return;
    }
    encoder << true;
    encoder << static_cast<uint64_t>(userHandle->byteLength());
    encoder.encodeFixedLengthData(reinterpret_cast<const uint8_t*>(userHandle->data()), userHandle->byteLength(), 1);
}

template<class Decoder>
Optional<PublicKeyCredentialData> PublicKeyCredentialData::decode(Decoder& decoder)
{
    PublicKeyCredentialData result;

    Optional<bool> isEmpty;
    decoder >> isEmpty;
    if (!isEmpty)
        return WTF::nullopt;
    if (isEmpty.value())
        return result;

    Optional<uint64_t> rawIdLength;
    decoder >> rawIdLength;
    if (!rawIdLength)
        return WTF::nullopt;

    result.rawId = ArrayBuffer::create(rawIdLength.value(), sizeof(uint8_t));
    if (!decoder.decodeFixedLengthData(reinterpret_cast<uint8_t*>(result.rawId->data()), rawIdLength.value(), 1))
        return WTF::nullopt;

    Optional<bool> isAuthenticatorAttestationResponse;
    decoder >> isAuthenticatorAttestationResponse;
    if (!isAuthenticatorAttestationResponse)
        return WTF::nullopt;
    result.isAuthenticatorAttestationResponse = isAuthenticatorAttestationResponse.value();

    if (result.isAuthenticatorAttestationResponse) {
        Optional<uint64_t> attestationObjectLength;
        decoder >> attestationObjectLength;
        if (!attestationObjectLength)
            return WTF::nullopt;

        result.attestationObject = ArrayBuffer::create(attestationObjectLength.value(), sizeof(uint8_t));
        if (!decoder.decodeFixedLengthData(reinterpret_cast<uint8_t*>(result.attestationObject->data()), attestationObjectLength.value(), 1))
            return WTF::nullopt;

        return result;
    }

    Optional<uint64_t> authenticatorDataLength;
    decoder >> authenticatorDataLength;
    if (!authenticatorDataLength)
        return WTF::nullopt;

    result.authenticatorData = ArrayBuffer::create(authenticatorDataLength.value(), sizeof(uint8_t));
    if (!decoder.decodeFixedLengthData(reinterpret_cast<uint8_t*>(result.authenticatorData->data()), authenticatorDataLength.value(), 1))
        return WTF::nullopt;

    Optional<uint64_t> signatureLength;
    decoder >> signatureLength;
    if (!signatureLength)
        return WTF::nullopt;

    result.signature = ArrayBuffer::create(signatureLength.value(), sizeof(uint8_t));
    if (!decoder.decodeFixedLengthData(reinterpret_cast<uint8_t*>(result.signature->data()), signatureLength.value(), 1))
        return WTF::nullopt;

    Optional<Optional<bool>> appid;
    decoder >> appid;
    if (!appid)
        return WTF::nullopt;
    result.appid = WTFMove(*appid);

    Optional<bool> hasUserHandle;
    decoder >> hasUserHandle;
    if (!hasUserHandle)
        return WTF::nullopt;
    if (!*hasUserHandle)
        return result;

    Optional<uint64_t> userHandleLength;
    decoder >> userHandleLength;
    if (!userHandleLength)
        return WTF::nullopt;

    result.userHandle = ArrayBuffer::create(userHandleLength.value(), sizeof(uint8_t));
    if (!decoder.decodeFixedLengthData(reinterpret_cast<uint8_t*>(result.userHandle->data()), userHandleLength.value(), 1))
        return WTF::nullopt;

    return result;
}
    
} // namespace WebCore

#endif // ENABLE(WEB_AUTHN)

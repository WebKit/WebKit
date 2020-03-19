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

struct AuthenticatorResponseData {
    bool isAuthenticatorAttestationResponse;

    // AuthenticatorResponse
    RefPtr<ArrayBuffer> rawId;

    // Extensions
    Optional<bool> appid;

    // AuthenticatorAttestationResponse
    RefPtr<ArrayBuffer> attestationObject;

    // AuthenticatorAssertionResponse
    RefPtr<ArrayBuffer> authenticatorData;
    RefPtr<ArrayBuffer> signature;
    RefPtr<ArrayBuffer> userHandle;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<AuthenticatorResponseData> decode(Decoder&);
};

template<class Encoder>
static void encodeArrayBuffer(Encoder& encoder, const ArrayBuffer& buffer)
{
    encoder << static_cast<uint64_t>(buffer.byteLength());
    encoder.encodeFixedLengthData(reinterpret_cast<const uint8_t*>(buffer.data()), buffer.byteLength(), 1);
}

template<class Decoder>
RefPtr<ArrayBuffer> decodeArrayBuffer(Decoder& decoder)
{
    Optional<uint64_t> length;
    decoder >> length;
    if (!length)
        return nullptr;

    if (!decoder.template bufferIsLargeEnoughToContain<uint8_t>(length.value()))
        return nullptr;
    auto buffer = ArrayBuffer::tryCreate(length.value(), sizeof(uint8_t));
    if (!buffer)
        return nullptr;
    if (!decoder.decodeFixedLengthData(reinterpret_cast<uint8_t*>(buffer->data()), length.value(), 1))
        return nullptr;
    return buffer;
}

template<class Encoder>
void AuthenticatorResponseData::encode(Encoder& encoder) const
{
    if (!rawId) {
        encoder << true;
        return;
    }
    encoder << false;
    encodeArrayBuffer(encoder, *rawId);

    encoder << isAuthenticatorAttestationResponse;

    if (isAuthenticatorAttestationResponse && attestationObject) {
        encodeArrayBuffer(encoder, *attestationObject);
        return;
    }

    if (!authenticatorData || !signature)
        return;
    encodeArrayBuffer(encoder, *authenticatorData);
    encodeArrayBuffer(encoder, *signature);

    // Encode AppID before user handle to avoid the userHandle flag.
    encoder << appid;

    if (!userHandle) {
        encoder << false;
        return;
    }
    encoder << true;
    encodeArrayBuffer(encoder, *userHandle);
}

template<class Decoder>
Optional<AuthenticatorResponseData> AuthenticatorResponseData::decode(Decoder& decoder)
{
    AuthenticatorResponseData result;

    Optional<bool> isEmpty;
    decoder >> isEmpty;
    if (!isEmpty)
        return WTF::nullopt;
    if (isEmpty.value())
        return result;

    result.rawId = decodeArrayBuffer(decoder);
    if (!result.rawId)
        return WTF::nullopt;

    Optional<bool> isAuthenticatorAttestationResponse;
    decoder >> isAuthenticatorAttestationResponse;
    if (!isAuthenticatorAttestationResponse)
        return WTF::nullopt;
    result.isAuthenticatorAttestationResponse = isAuthenticatorAttestationResponse.value();

    if (result.isAuthenticatorAttestationResponse) {
        result.attestationObject = decodeArrayBuffer(decoder);
        if (!result.attestationObject)
            return WTF::nullopt;
        return result;
    }

    result.authenticatorData = decodeArrayBuffer(decoder);
    if (!result.authenticatorData)
        return WTF::nullopt;

    result.signature = decodeArrayBuffer(decoder);
    if (!result.signature)
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

    result.userHandle = decodeArrayBuffer(decoder);
    if (!result.userHandle)
        return WTF::nullopt;

    return result;
}
    
} // namespace WebCore

#endif // ENABLE(WEB_AUTHN)

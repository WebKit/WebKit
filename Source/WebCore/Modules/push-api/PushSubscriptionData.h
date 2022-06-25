/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#if ENABLE(SERVICE_WORKER)

#include "EpochTimeStamp.h"
#include "PushSubscriptionIdentifier.h"

#include <wtf/text/WTFString.h>

namespace WebCore {

struct PushSubscriptionData {
    PushSubscriptionIdentifier identifier;
    String endpoint;
    std::optional<WebCore::EpochTimeStamp> expirationTime;
    Vector<uint8_t> serverVAPIDPublicKey;
    Vector<uint8_t> clientECDHPublicKey;
    Vector<uint8_t> sharedAuthenticationSecret;

    WEBCORE_EXPORT PushSubscriptionData isolatedCopy() const &;
    WEBCORE_EXPORT PushSubscriptionData isolatedCopy() &&;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<PushSubscriptionData> decode(Decoder&);
};

template<class Encoder>
void PushSubscriptionData::encode(Encoder& encoder) const
{
    encoder << identifier;
    encoder << endpoint;
    encoder << expirationTime;
    encoder << serverVAPIDPublicKey;
    encoder << clientECDHPublicKey;
    encoder << sharedAuthenticationSecret;
}

template<class Decoder>
std::optional<PushSubscriptionData> PushSubscriptionData::decode(Decoder& decoder)
{
    std::optional<PushSubscriptionIdentifier> identifier;
    decoder >> identifier;
    if (!identifier)
        return std::nullopt;

    std::optional<String> endpoint;
    decoder >> endpoint;
    if (!endpoint)
        return std::nullopt;

    std::optional<std::optional<WebCore::EpochTimeStamp>> expirationTime;
    decoder >> expirationTime;
    if (!expirationTime)
        return std::nullopt;

    std::optional<Vector<uint8_t>> serverVAPIDPublicKey;
    decoder >> serverVAPIDPublicKey;
    if (!serverVAPIDPublicKey)
        return std::nullopt;

    std::optional<Vector<uint8_t>> clientECDHPublicKey;
    decoder >> clientECDHPublicKey;
    if (!clientECDHPublicKey)
        return std::nullopt;

    std::optional<Vector<uint8_t>> sharedAuthenticationSecret;
    decoder >> sharedAuthenticationSecret;
    if (!sharedAuthenticationSecret)
        return std::nullopt;

    return PushSubscriptionData { WTFMove(*identifier), WTFMove(*endpoint), WTFMove(*expirationTime), WTFMove(*serverVAPIDPublicKey), WTFMove(*clientECDHPublicKey), WTFMove(*sharedAuthenticationSecret) };
}

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)

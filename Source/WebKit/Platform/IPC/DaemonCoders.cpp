/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "DaemonCoders.h"

#include "DaemonDecoder.h"
#include "DaemonEncoder.h"
#include "PushMessageForTesting.h"
#include "WebPushDaemonConnectionConfiguration.h"
#include "WebPushMessage.h"
#include <WebCore/CertificateInfo.h>
#include <WebCore/ExceptionData.h>
#include <WebCore/PrivateClickMeasurement.h>
#include <WebCore/PushSubscriptionData.h>
#include <WebCore/RegistrableDomain.h>
#include <WebCore/SecurityOriginData.h>

#if PLATFORM(COCOA)
#include <CoreFoundation/CoreFoundation.h>
#include <wtf/spi/cocoa/SecuritySPI.h>
#endif

namespace WebKit::Daemon {

#define IMPLEMENT_CODER(class) \
void Coder<class>::encode(Encoder& encoder, const class& instance) { instance.encode(encoder); } \
std::optional<class> Coder<class>::decode(Decoder& decoder) { return class::decode(decoder); }

IMPLEMENT_CODER(WebCore::ExceptionData);
IMPLEMENT_CODER(WebCore::PrivateClickMeasurement);
IMPLEMENT_CODER(WebCore::PrivateClickMeasurement::AttributionTimeToSendData);
IMPLEMENT_CODER(WebCore::PrivateClickMeasurement::AttributionTriggerData);
IMPLEMENT_CODER(WebCore::PrivateClickMeasurement::EphemeralNonce);
IMPLEMENT_CODER(WebCore::PushSubscriptionData);
IMPLEMENT_CODER(WebCore::PushSubscriptionIdentifier);
IMPLEMENT_CODER(WebCore::RegistrableDomain);
IMPLEMENT_CODER(WebCore::SecurityOriginData);
IMPLEMENT_CODER(WebKit::WebPushMessage);
IMPLEMENT_CODER(WebPushD::WebPushDaemonConnectionConfiguration);

#undef IMPLEMENT_CODER

void Coder<WTF::WallTime>::encode(Encoder& encoder, const WTF::WallTime& instance)
{
    encoder << instance.secondsSinceEpoch().value();
}

std::optional<WTF::WallTime> Coder<WTF::WallTime>::decode(Decoder& decoder)
{
    std::optional<double> value;
    decoder >> value;
    if (!value)
        return std::nullopt;
    return WallTime::fromRawSeconds(*value);
}

void Coder<WebCore::CertificateInfo>::encode(Encoder& encoder, const WebCore::CertificateInfo& instance)
{
#if PLATFORM(COCOA)
    auto data = adoptCF(SecTrustSerialize(instance.trust(), nullptr));
    if (!data) {
        encoder << false;
        return;
    }
    encoder << true;
    uint64_t length = CFDataGetLength(data.get());
    encoder << length;
    encoder.encodeFixedLengthData({ CFDataGetBytePtr(data.get()), static_cast<size_t>(length) });
#endif
}

std::optional<WebCore::CertificateInfo> Coder<WebCore::CertificateInfo>::decode(Decoder& decoder)
{
#if PLATFORM(COCOA)
    std::optional<bool> hasTrust;
    decoder >> hasTrust;
    if (!hasTrust)
        return std::nullopt;
    if (!*hasTrust)
        return WebCore::CertificateInfo();
    std::optional<uint64_t> length;
    decoder >> length;
    if (!length)
        return std::nullopt;
    auto* bytes = decoder.decodeFixedLengthReference(*length);
    if (!bytes)
        return std::nullopt;
    auto trust = adoptCF(SecTrustDeserialize(adoptCF(CFDataCreate(nullptr, bytes, *length)).get(), nullptr));
    if (!trust)
        return std::nullopt;
    return WebCore::CertificateInfo(WTFMove(trust));
#else
    return WebCore::CertificateInfo();
#endif
}

}

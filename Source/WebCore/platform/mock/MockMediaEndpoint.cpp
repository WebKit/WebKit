/*
 * Copyright (C) 2015 Ericsson AB. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Ericsson nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(WEB_RTC)
#include "MockMediaEndpoint.h"

#include "MediaPayload.h"
#include "MockRealtimeAudioSource.h"
#include "MockRealtimeVideoSource.h"
#include <wtf/MainThread.h>

namespace WebCore {

static const char* fingerprint = "8B:87:09:8A:5D:C2:F3:33:EF:C5:B1:F6:84:3A:3D:D6:A3:E2:9C:17:4C:E7:46:3B:1B:CE:84:98:DD:8E:AF:7B";
static const char* fingerprintFunction = "sha-256";

std::unique_ptr<MediaEndpoint> MockMediaEndpoint::create(MediaEndpointClient& client)
{
    return std::unique_ptr<MediaEndpoint>(new MockMediaEndpoint(client));
}

MockMediaEndpoint::MockMediaEndpoint(MediaEndpointClient& client)
    : m_client(client)
{
}

MockMediaEndpoint::~MockMediaEndpoint()
{
    stop();
}

void MockMediaEndpoint::setConfiguration(RefPtr<MediaEndpointConfiguration>&& configuration)
{
    UNUSED_PARAM(configuration);
}

void MockMediaEndpoint::generateDtlsInfo()
{
    callOnMainThread([this]() {
        m_client.gotDtlsFingerprint(String(fingerprint), String(fingerprintFunction));
    });
}

Vector<RefPtr<MediaPayload>> MockMediaEndpoint::getDefaultAudioPayloads()
{
    Vector<RefPtr<MediaPayload>> payloads;

    RefPtr<MediaPayload> payload = MediaPayload::create();
    payload->setType(111);
    payload->setEncodingName("OPUS");
    payload->setClockRate(48000);
    payload->setChannels(2);
    payloads.append(payload);

    payload = MediaPayload::create();
    payload->setType(8);
    payload->setEncodingName("PCMA");
    payload->setClockRate(8000);
    payload->setChannels(1);
    payloads.append(payload);

    payload = MediaPayload::create();
    payload->setType(0);
    payload->setEncodingName("PCMU");
    payload->setClockRate(8000);
    payload->setChannels(1);
    payloads.append(payload);

    return payloads;
}

Vector<RefPtr<MediaPayload>> MockMediaEndpoint::getDefaultVideoPayloads()
{
    Vector<RefPtr<MediaPayload>> payloads;

    RefPtr<MediaPayload> payload = MediaPayload::create();
    payload->setType(103);
    payload->setEncodingName("H264");
    payload->setClockRate(90000);
    payload->setCcmfir(true);
    payload->setNackpli(true);
    payload->addParameter("packetizationMode", 1);
    payloads.append(payload);

    payload = MediaPayload::create();
    payload->setType(100);
    payload->setEncodingName("VP8");
    payload->setClockRate(90000);
    payload->setCcmfir(true);
    payload->setNackpli(true);
    payload->setNack(true);
    payloads.append(payload);

    payload = MediaPayload::create();
    payload->setType(120);
    payload->setEncodingName("RTX");
    payload->setClockRate(90000);
    payload->addParameter("apt", 100);
    payload->addParameter("rtxTime", 200);
    payloads.append(payload);

    return payloads;
}

MediaPayloadVector MockMediaEndpoint::filterPayloads(const MediaPayloadVector& remotePayloads, const MediaPayloadVector& defaultPayloads)
{
    MediaPayloadVector filteredPayloads;

    for (auto& remotePayload : remotePayloads) {
        MediaPayload* defaultPayload = nullptr;
        for (auto& payload : defaultPayloads) {
            if (payload->encodingName() == remotePayload->encodingName().convertToASCIIUppercase()) {
                defaultPayload = payload.get();
                break;
            }
        }
        if (!defaultPayload)
            continue;

        if (defaultPayload->parameters().contains("packetizationMode") && remotePayload->parameters().contains("packetizationMode")
            && (defaultPayload->parameters().get("packetizationMode") != defaultPayload->parameters().get("packetizationMode")))
            continue;

        filteredPayloads.append(remotePayload);
    }

    return filteredPayloads;
}

MediaEndpoint::UpdateResult MockMediaEndpoint::updateReceiveConfiguration(MediaEndpointSessionConfiguration* configuration, bool isInitiator)
{
    UNUSED_PARAM(configuration);
    UNUSED_PARAM(isInitiator);

    return UpdateResult::Success;
}

MediaEndpoint::UpdateResult MockMediaEndpoint::updateSendConfiguration(MediaEndpointSessionConfiguration* configuration, const RealtimeMediaSourceMap& sendSourceMap, bool isInitiator)
{
    UNUSED_PARAM(configuration);
    UNUSED_PARAM(sendSourceMap);
    UNUSED_PARAM(isInitiator);

    return UpdateResult::Success;
}

void MockMediaEndpoint::addRemoteCandidate(IceCandidate& candidate, const String& mid, const String& ufrag, const String& password)
{
    UNUSED_PARAM(candidate);
    UNUSED_PARAM(mid);
    UNUSED_PARAM(ufrag);
    UNUSED_PARAM(password);
}

Ref<RealtimeMediaSource> MockMediaEndpoint::createMutedRemoteSource(const String&, RealtimeMediaSource::Type type)
{
    if (type == RealtimeMediaSource::Audio)
        return MockRealtimeAudioSource::createMuted("remote audio");

    ASSERT(type == RealtimeMediaSource::Video);
    return MockRealtimeVideoSource::createMuted("remote video");
}

void MockMediaEndpoint::replaceSendSource(RealtimeMediaSource& newSource, const String& mid)
{
    UNUSED_PARAM(newSource);
    UNUSED_PARAM(mid);
}

void MockMediaEndpoint::stop()
{
}

} // namespace WebCore

#endif // ENABLE(WEB_RTC)

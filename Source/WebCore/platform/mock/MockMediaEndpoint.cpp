/*
 * Copyright (C) 2015 Ericsson AB. All rights reserved.
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include "MediaEndpointSessionConfiguration.h"
#include "MediaPayload.h"
#include "MockRealtimeAudioSource.h"
#include "MockRealtimeVideoSource.h"
#include "RTCDataChannelHandlerMock.h"
#include "RealtimeMediaSource.h"
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
    , m_iceCandidateTimer(*this, &MockMediaEndpoint::iceCandidateTimerFired)
    , m_iceTransportTimer(*this, &MockMediaEndpoint::iceTransportTimerFired)
    , m_unmuteTimer(*this, &MockMediaEndpoint::unmuteTimerFired)
    , m_weakPtrFactory(this)
{
}

MockMediaEndpoint::~MockMediaEndpoint()
{
    stop();
}

std::unique_ptr<RTCDataChannelHandler> MockMediaEndpoint::createDataChannelHandler(const String& label, const RTCDataChannelInit& options)
{
    return std::make_unique<RTCDataChannelHandlerMock>(label, options);
}

void MockMediaEndpoint::generateDtlsInfo()
{
    auto weakThis = m_weakPtrFactory.createWeakPtr();

    callOnMainThread([weakThis] {
        if (weakThis)
            weakThis->m_client.gotDtlsFingerprint(fingerprint, fingerprintFunction);
    });
}

MediaPayloadVector MockMediaEndpoint::getDefaultAudioPayloads()
{
    MediaPayloadVector payloads;

    MediaPayload payload1;
    payload1.type = 111;
    payload1.encodingName = "OPUS";
    payload1.clockRate = 48000;
    payload1.channels = 2;
    payloads.append(WTFMove(payload1));

    MediaPayload payload2;
    payload2.type = 8;
    payload2.encodingName = "PCMA";
    payload2.clockRate = 8000;
    payload2.channels = 1;
    payloads.append(WTFMove(payload2));

    MediaPayload payload3;
    payload3.type = 0;
    payload3.encodingName = "PCMU";
    payload3.clockRate = 8000;
    payload3.channels = 1;
    payloads.append(WTFMove(payload3));

    return payloads;
}

MediaPayloadVector MockMediaEndpoint::getDefaultVideoPayloads()
{
    MediaPayloadVector payloads;

    MediaPayload payload1;
    payload1.type = 103;
    payload1.encodingName = "H264";
    payload1.clockRate = 90000;
    payload1.ccmfir = true;
    payload1.nackpli = true;
    payload1.addParameter("packetizationMode", 1);
    payloads.append(WTFMove(payload1));

    MediaPayload payload2;
    payload2.type = 100;
    payload2.encodingName = "VP8";
    payload2.clockRate = 90000;
    payload2.ccmfir = true;
    payload2.nackpli = true;
    payload2.nack = true;
    payloads.append(WTFMove(payload2));

    MediaPayload payload3;
    payload3.type = 120;
    payload3.encodingName = "RTX";
    payload3.clockRate = 90000;
    payload3.addParameter("apt", 100);
    payload3.addParameter("rtxTime", 200);
    payloads.append(WTFMove(payload3));

    return payloads;
}

MediaPayloadVector MockMediaEndpoint::filterPayloads(const MediaPayloadVector& remotePayloads, const MediaPayloadVector& defaultPayloads)
{
    MediaPayloadVector filteredPayloads;

    for (auto& remotePayload : remotePayloads) {
        const MediaPayload* defaultPayload = nullptr;
        for (auto& payload : defaultPayloads) {
            if (payload.encodingName == remotePayload.encodingName.convertToASCIIUppercase()) {
                defaultPayload = &payload;
                break;
            }
        }
        if (!defaultPayload)
            continue;

        if (defaultPayload->parameters.contains("packetizationMode") && remotePayload.parameters.contains("packetizationMode")
            && (defaultPayload->parameters.get("packetizationMode") != defaultPayload->parameters.get("packetizationMode")))
            continue;

        filteredPayloads.append(remotePayload);
    }

    return filteredPayloads;
}

MediaEndpoint::UpdateResult MockMediaEndpoint::updateReceiveConfiguration(MediaEndpointSessionConfiguration* configuration, bool isInitiator)
{
    UNUSED_PARAM(isInitiator);

    updateConfigurationMids(*configuration);
    return UpdateResult::Success;
}

MediaEndpoint::UpdateResult MockMediaEndpoint::updateSendConfiguration(MediaEndpointSessionConfiguration* configuration, const RealtimeMediaSourceMap& sendSourceMap, bool isInitiator)
{
    UNUSED_PARAM(sendSourceMap);
    UNUSED_PARAM(isInitiator);

    updateConfigurationMids(*configuration);
    return UpdateResult::Success;
}

void MockMediaEndpoint::addRemoteCandidate(const IceCandidate& candidate, const String& mid, const String& ufrag, const String& password)
{
    UNUSED_PARAM(candidate);
    UNUSED_PARAM(mid);
    UNUSED_PARAM(ufrag);
    UNUSED_PARAM(password);
}

Ref<RealtimeMediaSource> MockMediaEndpoint::createMutedRemoteSource(const String& mid, RealtimeMediaSource::Type type)
{
    RefPtr<RealtimeMediaSource> source;

    switch (type) {
    case RealtimeMediaSource::Type::Audio:
        source = MockRealtimeAudioSource::createMuted("remote audio");
        break;
    case RealtimeMediaSource::Type::Video:
        source = MockRealtimeVideoSource::createMuted("remote video");
        break;
    case RealtimeMediaSource::Type::None:
        ASSERT_NOT_REACHED();
    }

    m_mutedRemoteSources.set(mid, source);
    return source.releaseNonNull();
}

void MockMediaEndpoint::replaceSendSource(RealtimeMediaSource& newSource, const String& mid)
{
    UNUSED_PARAM(newSource);
    UNUSED_PARAM(mid);
}

void MockMediaEndpoint::replaceMutedRemoteSourceMid(const String& oldMid, const String& newMid)
{
    RefPtr<RealtimeMediaSource> remoteSource = m_mutedRemoteSources.take(oldMid);
    m_mutedRemoteSources.set(newMid, WTFMove(remoteSource));
}

void MockMediaEndpoint::stop()
{
}

void MockMediaEndpoint::emulatePlatformEvent(const String& action)
{
    if (action == "dispatch-fake-ice-candidates")
        dispatchFakeIceCandidates();
    else if (action == "step-ice-transport-states")
        stepIceTransportStates();
    else if (action == "unmute-remote-sources-by-mid")
        unmuteRemoteSourcesByMid();
}

void MockMediaEndpoint::updateConfigurationMids(const MediaEndpointSessionConfiguration& configuration)
{
    Vector<String> mids;
    for (auto& mediaDescription : configuration.mediaDescriptions())
        mids.append(mediaDescription.mid);
    m_mids.swap(mids);
}

void MockMediaEndpoint::dispatchFakeIceCandidates()
{
    m_fakeIceCandidates.append({ "host", "1", 1, "UDP", 2013266431, "192.168.0.100", 38838, { }, { }, 0 });
    m_fakeIceCandidates.append({ "host", "2", 1, "TCP", 1019216383, "192.168.0.100", 9, "active", { }, 0 });
    m_fakeIceCandidates.append({ "srflx", "3", 1, "UDP", 1677722111, "172.18.0.1", 47989, { }, "192.168.0.100", 47989 });

    // Reverse order to use takeLast() while keeping the above order
    m_fakeIceCandidates.reverse();

    m_iceCandidateTimer.startOneShot(0_s);
}

void MockMediaEndpoint::iceCandidateTimerFired()
{
    if (m_mids.isEmpty())
        return;

    if (!m_fakeIceCandidates.isEmpty()) {
        m_client.gotIceCandidate(m_mids[0], m_fakeIceCandidates.takeLast());
        m_iceCandidateTimer.startOneShot(0_s);
    } else
        m_client.doneGatheringCandidates(m_mids[0]);
}

void MockMediaEndpoint::stepIceTransportStates()
{
    if (m_mids.size() != 3) {
        LOG_ERROR("The 'step-ice-transport-states' action requires 3 transceivers");
        return;
    }

    // Should go to:
    // 'checking'
    m_iceTransportStateChanges.append(std::make_pair(m_mids[0], RTCIceTransportState::Checking));
    m_iceTransportStateChanges.append(std::make_pair(m_mids[1], RTCIceTransportState::Checking));
    m_iceTransportStateChanges.append(std::make_pair(m_mids[2], RTCIceTransportState::Checking));

    // 'connected'
    m_iceTransportStateChanges.append(std::make_pair(m_mids[0], RTCIceTransportState::Connected));
    m_iceTransportStateChanges.append(std::make_pair(m_mids[1], RTCIceTransportState::Completed));
    m_iceTransportStateChanges.append(std::make_pair(m_mids[2], RTCIceTransportState::Closed));

    // 'completed'
    m_iceTransportStateChanges.append(std::make_pair(m_mids[0], RTCIceTransportState::Completed));

    // 'failed'
    m_iceTransportStateChanges.append(std::make_pair(m_mids[0], RTCIceTransportState::Failed));

    // 'disconnected'
    m_iceTransportStateChanges.append(std::make_pair(m_mids[1], RTCIceTransportState::Disconnected));
    m_iceTransportStateChanges.append(std::make_pair(m_mids[0], RTCIceTransportState::Closed));

    // 'new'
    m_iceTransportStateChanges.append(std::make_pair(m_mids[1], RTCIceTransportState::Closed));

    // Reverse order to use takeLast() while keeping the above order
    m_iceTransportStateChanges.reverse();

    m_iceTransportTimer.startOneShot(0_s);
}

void MockMediaEndpoint::iceTransportTimerFired()
{
    if (m_iceTransportStateChanges.isEmpty() || m_mids.size() != 3)
        return;

    auto stateChange = m_iceTransportStateChanges.takeLast();
    m_client.iceTransportStateChanged(stateChange.first, stateChange.second);

    m_iceTransportTimer.startOneShot(0_s);
}

void MockMediaEndpoint::unmuteRemoteSourcesByMid()
{
    if (m_mids.isEmpty())
        return;

    // Looking up each source by its mid, instead of simply iterating over the list of muted sources,
    // emulates remote media arriving on a media description with a specific mid (RTCRtpTransceiver).

    // Copy values in reverse order to maintain the original order while using takeLast()
    for (int i = m_mids.size() - 1; i >= 0; --i)
        m_midsOfSourcesToUnmute.append(m_mids[i]);

    m_unmuteTimer.startOneShot(0_s);
}

void MockMediaEndpoint::unmuteTimerFired()
{
    auto* source = m_mutedRemoteSources.get(m_midsOfSourcesToUnmute.takeLast());
    if (source)
        source->setMuted(false);

    if (!m_midsOfSourcesToUnmute.isEmpty())
        m_unmuteTimer.startOneShot(0_s);
}

} // namespace WebCore

#endif // ENABLE(WEB_RTC)

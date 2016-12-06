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

#pragma once

#if ENABLE(WEB_RTC)

#include "MediaEndpoint.h"
#include "Timer.h"

namespace WebCore {

class MockMediaEndpoint final : public MediaEndpoint {
public:
    WEBCORE_EXPORT static std::unique_ptr<MediaEndpoint> create(MediaEndpointClient&);

    MockMediaEndpoint(MediaEndpointClient&);
    ~MockMediaEndpoint();

    void setConfiguration(MediaEndpointConfiguration&&) final { };

    void generateDtlsInfo() final;
    MediaPayloadVector getDefaultAudioPayloads() final;
    MediaPayloadVector getDefaultVideoPayloads() final;
    MediaPayloadVector filterPayloads(const MediaPayloadVector& remotePayloads, const MediaPayloadVector& defaultPayloads) final;

    UpdateResult updateReceiveConfiguration(MediaEndpointSessionConfiguration*, bool isInitiator) final;
    UpdateResult updateSendConfiguration(MediaEndpointSessionConfiguration*, const RealtimeMediaSourceMap&, bool isInitiator) final;

    void addRemoteCandidate(const IceCandidate&, const String& mid, const String& ufrag, const String& password) final;

    Ref<RealtimeMediaSource> createMutedRemoteSource(const String& mid, RealtimeMediaSource::Type) final;
    void replaceSendSource(RealtimeMediaSource&, const String& mid) final;
    void replaceMutedRemoteSourceMid(const String& oldMid, const String& newMid) final;

    void stop() final;

    void emulatePlatformEvent(const String& action) final;

private:
    void updateConfigurationMids(const MediaEndpointSessionConfiguration&);

    std::unique_ptr<RTCDataChannelHandler> createDataChannelHandler(const String&, const RTCDataChannelInit&) final;

    void dispatchFakeIceCandidates();
    void iceCandidateTimerFired();

    void stepIceTransportStates();
    void iceTransportTimerFired();

    void unmuteRemoteSourcesByMid();
    void unmuteTimerFired();

    MediaEndpointClient& m_client;
    Vector<String> m_mids;
    HashMap<String, RefPtr<RealtimeMediaSource>> m_mutedRemoteSources;

    Vector<IceCandidate> m_fakeIceCandidates;
    Timer m_iceCandidateTimer;

    Vector<std::pair<String, MediaEndpoint::IceTransportState>> m_iceTransportStateChanges;
    Timer m_iceTransportTimer;

    Vector<String> m_midsOfSourcesToUnmute;
    Timer m_unmuteTimer;
    
    WeakPtrFactory<MockMediaEndpoint> m_weakPtrFactory;
};

} // namespace WebCore

#endif // ENABLE(WEB_RTC)

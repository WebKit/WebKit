/*
 * Copyright (C) 2015, 2016 Ericsson AB. All rights reserved.
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
#include <owr/owr_session.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

typedef struct _OwrMediaSession OwrMediaSession;
typedef struct _OwrMediaSource OwrMediaSource;
typedef struct _OwrTransportAgent OwrTransportAgent;

namespace WebCore {

class RealtimeMediaSourceOwr;
class RTCConfigurationPrivate;

struct PeerMediaDescription;

class OwrTransceiver : public RefCounted<OwrTransceiver> {
public:
    static Ref<OwrTransceiver> create(const String& mid, OwrSession* session)
    {
        return adoptRef(*new OwrTransceiver(mid, session));
    }
    virtual ~OwrTransceiver() { }

    const String& mid() const { return m_mid; }
    OwrSession* session() const { return m_session; }

    OwrIceState owrIceState() const { return m_owrIceState; }
    void setOwrIceState(OwrIceState state) { m_owrIceState = state; }

    bool gotEndOfRemoteCandidates() const { return m_gotEndOfRemoteCandidates; }
    void markGotEndOfRemoteCandidates() { m_gotEndOfRemoteCandidates = true; }

private:
    OwrTransceiver(const String& mid, OwrSession* session)
        : m_mid(mid)
        , m_session(session)
    { }

    String m_mid;
    OwrSession* m_session;

    OwrIceState m_owrIceState { OWR_ICE_STATE_DISCONNECTED };
    bool m_gotEndOfRemoteCandidates { false };
};

class MediaEndpointOwr : public MediaEndpoint {
public:
    MediaEndpointOwr(MediaEndpointClient&);
    ~MediaEndpointOwr();

    void setConfiguration(MediaEndpointConfiguration&&) override;

    void generateDtlsInfo() override;
    MediaPayloadVector getDefaultAudioPayloads() override;
    MediaPayloadVector getDefaultVideoPayloads() override;
    MediaPayloadVector filterPayloads(const MediaPayloadVector& remotePayloads, const MediaPayloadVector& defaultPayloads) override;

    UpdateResult updateReceiveConfiguration(MediaEndpointSessionConfiguration*, bool isInitiator) override;
    UpdateResult updateSendConfiguration(MediaEndpointSessionConfiguration*, const RealtimeMediaSourceMap&, bool isInitiator) override;

    void addRemoteCandidate(const IceCandidate&, const String& mid, const String& ufrag, const String& password) override;

    Ref<RealtimeMediaSource> createMutedRemoteSource(const String& mid, RealtimeMediaSource::Type) override;
    void replaceMutedRemoteSourceMid(const String&, const String&) final;
    void replaceSendSource(RealtimeMediaSource&, const String& mid) override;

    void stop() override;

    size_t transceiverIndexForSession(OwrSession*) const;
    const String& sessionMid(OwrSession*) const;
    OwrTransceiver* matchTransceiverByMid(const String& mid) const;

    void dispatchNewIceCandidate(const String& mid, IceCandidate&&);
    void dispatchGatheringDone(const String& mid);
    void processIceTransportStateChange(OwrSession*);
    void dispatchDtlsFingerprint(gchar* privateKey, gchar* certificate, const String& fingerprint, const String& fingerprintFunction);
    void unmuteRemoteSource(const String& mid, OwrMediaSource*);

private:
    enum SessionType { SessionTypeMedia };

    struct TransceiverConfig {
        SessionType type;
        bool isDtlsClient;
        String mid;
    };

    std::unique_ptr<RTCDataChannelHandler> createDataChannelHandler(const String&, const RTCDataChannelInit&) final;

    void prepareSession(OwrSession*, PeerMediaDescription*);
    void prepareMediaSession(OwrMediaSession*, PeerMediaDescription*, bool isInitiator);

    void ensureTransportAgentAndTransceivers(bool isInitiator, const Vector<TransceiverConfig>&);
    void internalAddRemoteCandidate(OwrSession*, const IceCandidate&, const String& ufrag, const String& password);

    std::optional<MediaEndpointConfiguration> m_configuration;
    GRegex* m_helperServerRegEx;

    OwrTransportAgent* m_transportAgent;
    Vector<RefPtr<OwrTransceiver>> m_transceivers;
    HashMap<String, RefPtr<RealtimeMediaSourceOwr>> m_mutedRemoteSources;

    MediaEndpointClient& m_client;

    unsigned m_numberOfReceivePreparedSessions;
    unsigned m_numberOfSendPreparedSessions;

    String m_dtlsPrivateKey;
    String m_dtlsCertificate;
};

} // namespace WebCore

#endif // ENABLE(WEB_RTC)

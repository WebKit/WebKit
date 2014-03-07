/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2013 Nokia Corporation and/or its subsidiary(-ies).
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
 * 3. Neither the name of Google Inc. nor the names of its contributors
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

#ifndef RTCPeerConnection_h
#define RTCPeerConnection_h

#if ENABLE(MEDIA_STREAM)

#include "ActiveDOMObject.h"
#include "Dictionary.h"
#include "EventTarget.h"
#include "ExceptionBase.h"
#include "MediaStream.h"
#include "RTCIceCandidate.h"
#include "RTCPeerConnectionHandler.h"
#include "RTCPeerConnectionHandlerClient.h"
#include "ScriptWrappable.h"
#include "Timer.h"
#include <wtf/RefCounted.h>

namespace WebCore {

class MediaStreamTrack;
class RTCConfiguration;
class RTCDTMFSender;
class RTCDataChannel;
class RTCPeerConnectionErrorCallback;
class RTCSessionDescription;
class RTCSessionDescriptionCallback;
class RTCStatsCallback;
class VoidCallback;

class RTCPeerConnection final : public RefCounted<RTCPeerConnection>, public ScriptWrappable, public RTCPeerConnectionHandlerClient, public EventTargetWithInlineData, public ActiveDOMObject, public MediaStream::Observer {
public:
    static PassRefPtr<RTCPeerConnection> create(ScriptExecutionContext&, const Dictionary& rtcConfiguration, ExceptionCode&);
    ~RTCPeerConnection();

    void createOffer(PassRefPtr<RTCSessionDescriptionCallback>, PassRefPtr<RTCPeerConnectionErrorCallback>, const Dictionary& offerOptions, ExceptionCode&);

    void createAnswer(PassRefPtr<RTCSessionDescriptionCallback>, PassRefPtr<RTCPeerConnectionErrorCallback>, const Dictionary& answerOptions, ExceptionCode&);

    void setLocalDescription(PassRefPtr<RTCSessionDescription>, PassRefPtr<VoidCallback>, PassRefPtr<RTCPeerConnectionErrorCallback>, ExceptionCode&);
    PassRefPtr<RTCSessionDescription> localDescription(ExceptionCode&);

    void setRemoteDescription(PassRefPtr<RTCSessionDescription>, PassRefPtr<VoidCallback>, PassRefPtr<RTCPeerConnectionErrorCallback>, ExceptionCode&);
    PassRefPtr<RTCSessionDescription> remoteDescription(ExceptionCode&);

    String signalingState() const;

    void updateIce(const Dictionary& rtcConfiguration, ExceptionCode&);

    void addIceCandidate(RTCIceCandidate*, PassRefPtr<VoidCallback>, PassRefPtr<RTCPeerConnectionErrorCallback>, ExceptionCode&);

    String iceGatheringState() const;

    String iceConnectionState() const;

    RTCConfiguration* getConfiguration() const;

    MediaStreamVector getLocalStreams() const;

    MediaStreamVector getRemoteStreams() const;

    MediaStream* getStreamById(const String& streamId);

    void addStream(PassRefPtr<MediaStream>, ExceptionCode&);

    void removeStream(PassRefPtr<MediaStream>, ExceptionCode&);

    void getStats(PassRefPtr<RTCStatsCallback> successCallback, PassRefPtr<RTCPeerConnectionErrorCallback>, PassRefPtr<MediaStreamTrack> selector);

    PassRefPtr<RTCDataChannel> createDataChannel(String label, const Dictionary& dataChannelDict, ExceptionCode&);

    PassRefPtr<RTCDTMFSender> createDTMFSender(PassRefPtr<MediaStreamTrack>, ExceptionCode&);

    void close(ExceptionCode&);

    DEFINE_ATTRIBUTE_EVENT_LISTENER(negotiationneeded);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(icecandidate);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(signalingstatechange);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(addstream);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(removestream);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(iceconnectionstatechange);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(datachannel);

    // RTCPeerConnectionHandlerClient
    virtual void negotiationNeeded() override;
    virtual void didGenerateIceCandidate(PassRefPtr<RTCIceCandidateDescriptor>) override;
    virtual void didChangeSignalingState(SignalingState) override;
    virtual void didChangeIceGatheringState(IceGatheringState) override;
    virtual void didChangeIceConnectionState(IceConnectionState) override;
    virtual void didAddRemoteStream(PassRefPtr<MediaStreamPrivate>) override;
    virtual void didRemoveRemoteStream(MediaStreamPrivate*) override;
    virtual void didAddRemoteDataChannel(std::unique_ptr<RTCDataChannelHandler>) override;

    // EventTarget
    virtual EventTargetInterface eventTargetInterface() const override { return RTCPeerConnectionEventTargetInterfaceType; }
    virtual ScriptExecutionContext* scriptExecutionContext() const override { return ActiveDOMObject::scriptExecutionContext(); }

    // ActiveDOMObject
    virtual void stop() override;

    // MediaStream::Observer
    virtual void didAddOrRemoveTrack() override;

    using RefCounted<RTCPeerConnection>::ref;
    using RefCounted<RTCPeerConnection>::deref;

private:
    RTCPeerConnection(ScriptExecutionContext&, PassRefPtr<RTCConfiguration>, ExceptionCode&);

    static PassRefPtr<RTCConfiguration> parseConfiguration(const Dictionary& configuration, ExceptionCode&);
    void scheduleDispatchEvent(PassRefPtr<Event>);
    void scheduledEventTimerFired(Timer<RTCPeerConnection>*);
    bool hasLocalStreamWithTrackId(const String& trackId);

    // EventTarget implementation.
    virtual void refEventTarget() override { ref(); }
    virtual void derefEventTarget() override { deref(); }

    void changeSignalingState(SignalingState);
    void changeIceGatheringState(IceGatheringState);
    void changeIceConnectionState(IceConnectionState);

    bool checkStateForLocalDescription(RTCSessionDescription*);
    bool checkStateForRemoteDescription(RTCSessionDescription*);

    SignalingState m_signalingState;
    IceGatheringState m_iceGatheringState;
    IceConnectionState m_iceConnectionState;

    MediaStreamVector m_localStreams;
    MediaStreamVector m_remoteStreams;

    Vector<RefPtr<RTCDataChannel>> m_dataChannels;

    std::unique_ptr<RTCPeerConnectionHandler> m_peerHandler;

    Timer<RTCPeerConnection> m_scheduledEventTimer;
    Vector<RefPtr<Event>> m_scheduledEvents;

    RefPtr<RTCConfiguration> m_configuration;

    bool m_stopped;
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

#endif // RTCPeerConnection_h

/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#ifndef PeerConnection00_h
#define PeerConnection00_h

#if ENABLE(MEDIA_STREAM)

#include "ActiveDOMObject.h"
#include "Dictionary.h"
#include "EventTarget.h"
#include "ExceptionBase.h"
#include "IceCallback.h"
#include "MediaStream.h"
#include "MediaStreamList.h"
#include "PeerConnection00Handler.h"
#include "PeerConnection00HandlerClient.h"
#include "SessionDescription.h"
#include "Timer.h"
#include <wtf/OwnPtr.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class MediaHints;
class IceOptions;

// Note:
// SDP stands for Session Description Protocol, which is intended for describing
// multimedia sessions for the purposes of session announcement, session
// invitation, and other forms of multimedia session initiation.
//
// More information can be found here:
// http://tools.ietf.org/html/rfc4566
// http://en.wikipedia.org/wiki/Session_Description_Protocol

class PeerConnection00 : public RefCounted<PeerConnection00>, public PeerConnection00HandlerClient, public EventTarget, public ActiveDOMObject {
public:
    enum ReadyState {
        NEW = 0,
        OPENING = 1,
        ACTIVE = 2,
        CLOSED = 3
    };

    enum Action {
        SDP_OFFER = 0x100,
        SDP_PRANSWER = 0x200,
        SDP_ANSWER = 0x300
    };

    enum IceState {
        ICE_GATHERING = 0x100,
        ICE_WAITING = 0x200,
        ICE_CHECKING = 0x300,
        ICE_CONNECTED = 0x400,
        ICE_COMPLETED = 0x500,
        ICE_FAILED = 0x600,
        ICE_CLOSED = 0x700
    };

    static PassRefPtr<PeerConnection00> create(ScriptExecutionContext*, const String& serverConfiguration, PassRefPtr<IceCallback>);
    ~PeerConnection00();

    PassRefPtr<SessionDescription> createOffer(ExceptionCode&);
    PassRefPtr<SessionDescription> createOffer(const Dictionary& mediaHints, ExceptionCode&);
    PassRefPtr<SessionDescription> createAnswer(const String& offer, ExceptionCode&);
    PassRefPtr<SessionDescription> createAnswer(const String& offer, const Dictionary& mediaHints, ExceptionCode&);

    void setLocalDescription(int action, PassRefPtr<SessionDescription>, ExceptionCode&);
    void setRemoteDescription(int action, PassRefPtr<SessionDescription>, ExceptionCode&);
    PassRefPtr<SessionDescription> localDescription();
    PassRefPtr<SessionDescription> remoteDescription();

    void startIce(ExceptionCode&);
    void startIce(const Dictionary& iceOptions, ExceptionCode&);
    void processIceMessage(PassRefPtr<IceCandidate>, ExceptionCode&);

    IceState iceState() const;
    ReadyState readyState() const;

    void addStream(const PassRefPtr<MediaStream>, ExceptionCode&);
    void addStream(const PassRefPtr<MediaStream>, const Dictionary& mediaStreamHints, ExceptionCode&);
    void removeStream(MediaStream*, ExceptionCode&);
    MediaStreamList* localStreams() const;
    MediaStreamList* remoteStreams() const;

    void close(ExceptionCode&);

    DEFINE_ATTRIBUTE_EVENT_LISTENER(connecting);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(open);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(statechange);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(addstream);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(removestream);

    // PeerConnection00HandlerClient
    virtual void didGenerateIceCandidate(PassRefPtr<IceCandidateDescriptor>, bool moreToFollow) OVERRIDE;
    virtual void didChangeReadyState(uint32_t state) OVERRIDE;
    virtual void didChangeIceState(uint32_t state) OVERRIDE;
    virtual void didAddRemoteStream(PassRefPtr<MediaStreamDescriptor>) OVERRIDE;
    virtual void didRemoveRemoteStream(MediaStreamDescriptor*) OVERRIDE;

    // EventTarget
    virtual const AtomicString& interfaceName() const OVERRIDE;
    virtual ScriptExecutionContext* scriptExecutionContext() const OVERRIDE;

    // ActiveDOMObject
    virtual void stop() OVERRIDE;

    using RefCounted<PeerConnection00>::ref;
    using RefCounted<PeerConnection00>::deref;

private:
    PeerConnection00(ScriptExecutionContext*, const String& serverConfiguration, PassRefPtr<IceCallback>);

    // EventTarget implementation.
    virtual EventTargetData* eventTargetData();
    virtual EventTargetData* ensureEventTargetData();
    virtual void refEventTarget() { ref(); }
    virtual void derefEventTarget() { deref(); }
    EventTargetData m_eventTargetData;

    void changeReadyState(ReadyState);
    void changeIceState(IceState);

    bool hasLocalAudioTrack();
    bool hasLocalVideoTrack();
    PassRefPtr<MediaHints> createMediaHints(const Dictionary&);
    PassRefPtr<MediaHints> createMediaHints();
    PassRefPtr<IceOptions> createIceOptions(const Dictionary&, ExceptionCode&);
    PassRefPtr<IceOptions> createDefaultIceOptions();
    PassRefPtr<SessionDescription> createOffer(PassRefPtr<MediaHints>, ExceptionCode&);
    PassRefPtr<SessionDescription> createAnswer(const String& offer, PassRefPtr<MediaHints>, ExceptionCode&);
    void startIce(PassRefPtr<IceOptions>, ExceptionCode&);

    RefPtr<IceCallback> m_iceCallback;

    ReadyState m_readyState;
    IceState m_iceState;

    RefPtr<MediaStreamList> m_localStreams;
    RefPtr<MediaStreamList> m_remoteStreams;

    OwnPtr<PeerConnection00Handler> m_peerHandler;
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

#endif // PeerConnection00_h

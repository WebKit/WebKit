/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef PeerConnection_h
#define PeerConnection_h

#if ENABLE(MEDIA_STREAM)

#include "EventTarget.h"
#include "MediaStreamFrameController.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class MediaStream;
class MediaStreamList;
class ScriptExecutionContext;
class SerializedScriptValue;
class SignalingCallback;

class PeerConnection : public RefCounted<PeerConnection>, public EventTarget, public MediaStreamFrameController::PeerConnectionClient {
public:
    // Name and values of the enum must match the corressponding constants in the .idl file.
    enum {
        NEW = 0,
        NEGOTIATING = 1,
        ACTIVE = 2,
        CLOSED = 3
    };

    static PassRefPtr<PeerConnection> create(MediaStreamFrameController*, int id, const String& configuration, PassRefPtr<SignalingCallback>);
    virtual ~PeerConnection();

    MediaStreamList* localStreams();
    MediaStreamList* remoteStreams();

    unsigned short readyState() const { return m_readyState; }

    void processSignalingMessage(const String& message, ExceptionCode&);
    void send(const String& text, ExceptionCode&);
    void addStream(PassRefPtr<MediaStream>, ExceptionCode&);
    void removeStream(PassRefPtr<MediaStream>, ExceptionCode&);
    void close(ExceptionCode&);

    DEFINE_ATTRIBUTE_EVENT_LISTENER(connecting);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(open);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(message);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(addstream);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(removestream);

    // EventTarget
    virtual PeerConnection* toPeerConnection();

    // EventTarget implementation.
    virtual ScriptExecutionContext* scriptExecutionContext() const;

    // Callbacks
    void onMessage(const String& message);
    void onSignalingMessage(const String& message);
    void onNegotiationStarted();
    void onNegotiationDone();
    void streamAdded(PassRefPtr<MediaStream>);
    void streamRemoved(const String& streamLabel);

    using RefCounted<PeerConnection>::ref;
    using RefCounted<PeerConnection>::deref;

protected:
    // EventTarget implementation.
    virtual EventTargetData* eventTargetData();
    virtual EventTargetData* ensureEventTargetData();

private:
    static const size_t kMaxMessageUTF8Length = 504;

    PeerConnection(MediaStreamFrameController*, int id, const String& configuration, PassRefPtr<SignalingCallback>);
    void init();

    void postSimpleEvent(const String& eventName);
    void postMessageEvent(const String& message);
    void postSignalingEvent(const String& message);
    void postStreamAddedEvent(PassRefPtr<MediaStream>);
    void postStreamRemovedEvent(PassRefPtr<MediaStream>);
    void postStartNegotiationTask();

    void dispatchSimpleEvent(const String& eventName);
    void dispatchMessageEvent(const int&, PassRefPtr<SerializedScriptValue> data);
    void dispatchSignalingEvent(const String& eventName);
    void dispatchStreamEvent(const String& name, PassRefPtr<MediaStream>);
    void dispatchStartNegotiationTask(const int&);

    // EventTarget implementation.
    virtual void refEventTarget() { ref(); }
    virtual void derefEventTarget() { deref(); }

    EventTargetData m_eventTargetData;

    String m_configuration;
    unsigned short m_readyState;
    RefPtr<SignalingCallback> m_signalingCallback;
    RefPtr<MediaStreamList> m_localStreamList;
    RefPtr<MediaStreamList> m_remoteStreamList;
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

#endif // PeerConnection_h

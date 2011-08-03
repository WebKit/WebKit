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

#include "config.h"
#include "PeerConnection.h"

#if ENABLE(MEDIA_STREAM)

#include "DispatchTask.h"
#include "ErrorEvent.h"
#include "Event.h"
#include "EventNames.h"
#include "MediaStream.h"
#include "MediaStreamList.h"
#include "MessageEvent.h"
#include "ScriptExecutionContext.h"
#include "SignalingCallback.h"
#include "StreamEvent.h"

namespace WebCore {

PassRefPtr<PeerConnection> PeerConnection::create(MediaStreamFrameController* frameController, int id, const String& configuration, PassRefPtr<SignalingCallback> signalingCallback)
{
    // FIXME: Verify that configuration follows the specified format, if not return null.

    RefPtr<PeerConnection> peerConnection = adoptRef(new PeerConnection(frameController, id, configuration, signalingCallback));
    peerConnection->init();
    return peerConnection.release();
}

PeerConnection::PeerConnection(MediaStreamFrameController* frameController, int id, const String& configuration, PassRefPtr<SignalingCallback> signalingCallback)
    : PeerConnectionClient(frameController, id)
    , m_configuration(configuration)
    , m_readyState(NEW)
    , m_signalingCallback(signalingCallback)
{
    ASSERT(frameController);
    m_localStreamList = MediaStreamList::create();
    m_remoteStreamList = MediaStreamList::create();
}

void PeerConnection::init()
{
    if (MediaStreamFrameController* frameController = mediaStreamFrameController())
        frameController->newPeerConnection(m_clientId, m_configuration);

    postStartNegotiationTask();
}

PeerConnection::~PeerConnection()
{
}

MediaStreamList* PeerConnection::localStreams()
{
    return m_localStreamList.get();
}

MediaStreamList* PeerConnection::remoteStreams()
{
    return m_remoteStreamList.get();
}

void PeerConnection::processSignalingMessage(const String& message, ExceptionCode& ec)
{
    if (m_readyState == CLOSED) {
        ec = INVALID_STATE_ERR;
        return;
    }

    if (mediaStreamFrameController())
        mediaStreamFrameController()->processSignalingMessage(m_clientId, message);
}

void PeerConnection::send(const String& text, ExceptionCode& ec)
{
    if (m_readyState == CLOSED) {
        ec = INVALID_STATE_ERR;
        return;
    }

    CString utf8 = text.utf8();
    if (utf8.length() > kMaxMessageUTF8Length) {
        ec = INVALID_ACCESS_ERR;
        return;
    }

    if (mediaStreamFrameController())
        mediaStreamFrameController()->message(m_clientId, text);
}

void PeerConnection::addStream(PassRefPtr<MediaStream> prpStream, ExceptionCode& ec)
{
    if (m_readyState == CLOSED) {
        ec = INVALID_STATE_ERR;
        return;
    }

    // The Stream object is guaranteed to exist since StrictTypeChecking is set in the idl.

    RefPtr<MediaStream> stream = prpStream;

    if (m_localStreamList->contains(stream))
        return;

    m_localStreamList->add(stream);

    if (mediaStreamFrameController())
        mediaStreamFrameController()->addStream(m_clientId, stream);
}

void PeerConnection::removeStream(PassRefPtr<MediaStream> prpStream, ExceptionCode& ec)
{
    if (m_readyState == CLOSED) {
        ec = INVALID_STATE_ERR;
        return;
    }

    // The Stream object is guaranteed to exist since StrictTypeChecking is set in the idl.

    RefPtr<MediaStream> stream = prpStream;

    if (!m_localStreamList->contains(stream))
        return;

    m_localStreamList->remove(stream);

    if (mediaStreamFrameController())
        mediaStreamFrameController()->removeStream(m_clientId, stream);
}

void PeerConnection::close(ExceptionCode& ec)
{
    if (m_readyState == CLOSED) {
        ec = INVALID_STATE_ERR;
        return;
    }

    if (mediaStreamFrameController())
        mediaStreamFrameController()->closePeerConnection(m_clientId);

    m_readyState = CLOSED;
}

void PeerConnection::onNegotiationStarted()
{
    m_readyState = NEGOTIATING;

    postSimpleEvent(eventNames().connectingEvent);
}

void PeerConnection::onNegotiationDone()
{
    m_readyState = ACTIVE;

    postSimpleEvent(eventNames().openEvent);
}

void PeerConnection::streamAdded(PassRefPtr<MediaStream> prpStream)
{
    RefPtr<MediaStream> stream = prpStream;
    m_remoteStreamList->add(stream);
    postStreamAddedEvent(stream);
}

void PeerConnection::streamRemoved(const String& streamLabel)
{
    ASSERT(m_remoteStreamList->contains(streamLabel));

    RefPtr<MediaStream> stream = m_remoteStreamList->get(streamLabel);
    m_remoteStreamList->remove(stream);
    postStreamRemovedEvent(stream);
}

void PeerConnection::onMessage(const String& message)
{
    postMessageEvent(message);
}

void PeerConnection::onSignalingMessage(const String& message)
{
    postSignalingEvent(message);
}

PeerConnection* PeerConnection::toPeerConnection()
{
    return this;
}

ScriptExecutionContext* PeerConnection::scriptExecutionContext() const
{
    return mediaStreamFrameController() ? mediaStreamFrameController()->scriptExecutionContext() : 0;
}

EventTargetData* PeerConnection::eventTargetData()
{
    return &m_eventTargetData;
}

EventTargetData* PeerConnection::ensureEventTargetData()
{
    return &m_eventTargetData;
}

void PeerConnection::postMessageEvent(const String& message)
{
    RefPtr<SerializedScriptValue> data = SerializedScriptValue::create(message);

    ScriptExecutionContext* context = scriptExecutionContext();
    if (!context)
        return;
    ASSERT(context->isContextThread());
    context->postTask(DispatchTask<PeerConnection, int, SerializedScriptValue>::create(this, &PeerConnection::dispatchMessageEvent, 0, data.release()));
}

void PeerConnection::postSignalingEvent(const String& message)
{
    ScriptExecutionContext* context = scriptExecutionContext();
    if (!context)
        return;
    ASSERT(context->isContextThread());
    context->postTask(SimpleDispatchTask<PeerConnection, String>::create(this, &PeerConnection::dispatchSignalingEvent, message));
}

void PeerConnection::postStreamAddedEvent(PassRefPtr<MediaStream> prpStream)
{
    ScriptExecutionContext* context = scriptExecutionContext();
    if (!context)
        return;
    ASSERT(context->isContextThread());
    scriptExecutionContext()->postTask(DispatchTask<PeerConnection, String, MediaStream>::create(this, &PeerConnection::dispatchStreamEvent, eventNames().addstreamEvent, prpStream));
}

void PeerConnection::postStreamRemovedEvent(PassRefPtr<MediaStream> prpStream)
{
    ScriptExecutionContext* context = scriptExecutionContext();
    if (!context)
        return;
    ASSERT(context->isContextThread());
    context->postTask(DispatchTask<PeerConnection, String, MediaStream>::create(this, &PeerConnection::dispatchStreamEvent, eventNames().removestreamEvent, prpStream));
}

void PeerConnection::postSimpleEvent(const String& eventName)
{
    ScriptExecutionContext* context = scriptExecutionContext();
    if (!context)
        return;
    ASSERT(context->isContextThread());
    context->postTask(SimpleDispatchTask<PeerConnection, String>::create(this, &PeerConnection::dispatchSimpleEvent, eventName));
}

void PeerConnection::postStartNegotiationTask()
{
    ScriptExecutionContext* context = scriptExecutionContext();
    if (!context)
        return;
    ASSERT(context->isContextThread());
    context->postTask(SimpleDispatchTask<PeerConnection, int>::create(this, &PeerConnection::dispatchStartNegotiationTask, 0));
}

void PeerConnection::dispatchMessageEvent(const int&, PassRefPtr<SerializedScriptValue> data)
{
    RefPtr<SerializedScriptValue> d = data;
    dispatchEvent(MessageEvent::create(PassOwnPtr<MessagePortArray>(), d));
}

void PeerConnection::dispatchSignalingEvent(const String& message)
{
    m_signalingCallback->handleEvent(message, this);
}

void PeerConnection::dispatchStreamEvent(const String& name, PassRefPtr<MediaStream> stream)
{
    RefPtr<MediaStream> s = stream;
    dispatchEvent(StreamEvent::create(name, false, false, s));
}

void PeerConnection::dispatchSimpleEvent(const String& eventName)
{
    dispatchEvent(Event::create(eventName, false, false));
}

void PeerConnection::dispatchStartNegotiationTask(const int&)
{
    if (mediaStreamFrameController())
        mediaStreamFrameController()->startNegotiation(m_clientId);
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

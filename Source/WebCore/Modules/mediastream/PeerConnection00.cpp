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

#include "config.h"

#if ENABLE(MEDIA_STREAM)

#include "PeerConnection00.h"

#include "ExceptionCode.h"
#include "IceCallback.h"
#include "IceCandidate.h"
#include "IceCandidateDescriptor.h"
#include "IceOptions.h"
#include "MediaHints.h"
#include "MediaStreamEvent.h"
#include "MessageEvent.h"
#include "ScriptExecutionContext.h"
#include "SecurityOrigin.h"
#include "SessionDescription.h"
#include "SessionDescriptionDescriptor.h"

namespace WebCore {

PassRefPtr<PeerConnection00> PeerConnection00::create(ScriptExecutionContext* context, const String& serverConfiguration, PassRefPtr<IceCallback> iceCallback)
{
    RefPtr<PeerConnection00> peerConnection = adoptRef(new PeerConnection00(context, serverConfiguration, iceCallback));
    peerConnection->suspendIfNeeded();
    return peerConnection.release();
}

PeerConnection00::PeerConnection00(ScriptExecutionContext* context, const String& serverConfiguration, PassRefPtr<IceCallback> iceCallback)
    : ActiveDOMObject(context, this)
    , m_iceCallback(iceCallback)
    , m_readyState(NEW)
    , m_iceState(ICE_CLOSED)
    , m_localStreams(MediaStreamList::create())
    , m_remoteStreams(MediaStreamList::create())
    , m_peerHandler(PeerConnection00Handler::create(this, serverConfiguration, context->securityOrigin()->toString()))
{
}

PeerConnection00::~PeerConnection00()
{
}

bool PeerConnection00::hasLocalAudioTrack()
{
    for (size_t i = 0; i < m_localStreams->length(); ++i) {
        MediaStream* curr = m_localStreams->item(i);
        if (curr->audioTracks()->length() > 0)
            return true;
    }
    return false;
}

bool PeerConnection00::hasLocalVideoTrack()
{
    for (size_t i = 0; i < m_localStreams->length(); ++i) {
        MediaStream* curr = m_localStreams->item(i);
        if (curr->videoTracks()->length() > 0)
            return true;
    }
    return false;
}

PassRefPtr<MediaHints> PeerConnection00::createMediaHints(const Dictionary& dictionary)
{
    bool audio = hasLocalAudioTrack();
    bool video = hasLocalVideoTrack();
    dictionary.get("has_audio", audio);
    dictionary.get("has_video", audio);
    return MediaHints::create(audio, video);
}

PassRefPtr<MediaHints> PeerConnection00::createMediaHints()
{
    bool audio = hasLocalAudioTrack();
    bool video = hasLocalVideoTrack();
    return MediaHints::create(audio, video);
}

PassRefPtr<SessionDescription> PeerConnection00::createOffer(ExceptionCode& ec)
{
    return createOffer(createMediaHints(), ec);
}

PassRefPtr<SessionDescription> PeerConnection00::createOffer(const Dictionary& dictionary, ExceptionCode& ec)
{
    return createOffer(createMediaHints(dictionary), ec);
}

PassRefPtr<SessionDescription> PeerConnection00::createOffer(PassRefPtr<MediaHints> mediaHints, ExceptionCode& ec)
{
    RefPtr<SessionDescriptionDescriptor> descriptor = m_peerHandler->createOffer(mediaHints);
    if (!descriptor) {
        ec = SYNTAX_ERR;
        return 0;
    }

    return SessionDescription::create(descriptor.release());
}

PassRefPtr<SessionDescription> PeerConnection00::createAnswer(const String& offer, ExceptionCode& ec)
{
    return createAnswer(offer, createMediaHints(), ec);
}

PassRefPtr<SessionDescription> PeerConnection00::createAnswer(const String& offer, const Dictionary& dictionary, ExceptionCode& ec)
{
    return createAnswer(offer, createMediaHints(dictionary), ec);
}

PassRefPtr<SessionDescription> PeerConnection00::createAnswer(const String& offer, PassRefPtr<MediaHints> hints, ExceptionCode& ec)
{
    RefPtr<SessionDescriptionDescriptor> descriptor = m_peerHandler->createAnswer(offer, hints);
    if (!descriptor) {
        ec = SYNTAX_ERR;
        return 0;
    }

    return SessionDescription::create(descriptor.release());
}

void PeerConnection00::setLocalDescription(int action, PassRefPtr<SessionDescription> sessionDescription, ExceptionCode& ec)
{
    if (m_readyState == CLOSED) {
        ec = INVALID_STATE_ERR;
        return;
    }

    switch (action) {
    case SDP_OFFER:
    case SDP_PRANSWER:
    case SDP_ANSWER:
        break;
    default:
        ec = SYNTAX_ERR;
        return;
    }

    if (!sessionDescription) {
        ec = TYPE_MISMATCH_ERR;
        return;
    }

    bool valid = m_peerHandler->setLocalDescription(action, sessionDescription->descriptor());
    if (!valid)
        ec = SYNTAX_ERR;
}

void PeerConnection00::setRemoteDescription(int action, PassRefPtr<SessionDescription> sessionDescription, ExceptionCode& ec)
{
    if (m_readyState == CLOSED) {
        ec = INVALID_STATE_ERR;
        return;
    }

    switch (action) {
    case SDP_OFFER:
    case SDP_PRANSWER:
    case SDP_ANSWER:
        break;
    default:
        ec = SYNTAX_ERR;
        return;
    }

    if (!sessionDescription) {
        ec = TYPE_MISMATCH_ERR;
        return;
    }

    bool valid = m_peerHandler->setRemoteDescription(action, sessionDescription->descriptor());
    if (!valid)
        ec = SYNTAX_ERR;
}

PassRefPtr<SessionDescription> PeerConnection00::localDescription()
{
    RefPtr<SessionDescriptionDescriptor> descriptor = m_peerHandler->localDescription();
    if (!descriptor)
        return 0;

    RefPtr<SessionDescription> desc = SessionDescription::create(descriptor.release());
    return desc.release();
}

PassRefPtr<SessionDescription> PeerConnection00::remoteDescription()
{
    RefPtr<SessionDescriptionDescriptor> descriptor = m_peerHandler->remoteDescription();
    if (!descriptor)
        return 0;

    RefPtr<SessionDescription> desc = SessionDescription::create(descriptor.release());
    return desc.release();
}

PassRefPtr<IceOptions> PeerConnection00::createIceOptions(const Dictionary& dictionary, ExceptionCode& ec)
{
    String useCandidates = "";
    dictionary.get("use_candidates", useCandidates);

    IceOptions::UseCandidatesOption option;
    if (useCandidates == "" || useCandidates == "all")
        option = IceOptions::ALL;
    else if (useCandidates == "no_relay")
        option = IceOptions::NO_RELAY;
    else if (useCandidates == "only_relay")
        option = IceOptions::ONLY_RELAY;
    else {
        ec = TYPE_MISMATCH_ERR;
        return 0;
    }

    return IceOptions::create(option);
}

PassRefPtr<IceOptions> PeerConnection00::createDefaultIceOptions()
{
    return IceOptions::create(IceOptions::ALL);
}

void PeerConnection00::startIce(ExceptionCode& ec)
{
    startIce(createDefaultIceOptions(), ec);
}

void PeerConnection00::startIce(const Dictionary& dictionary, ExceptionCode& ec)
{
    RefPtr<IceOptions> iceOptions = createIceOptions(dictionary, ec);
    if (ec)
        return;

    startIce(iceOptions.release(), ec);
}

void PeerConnection00::startIce(PassRefPtr<IceOptions> iceOptions, ExceptionCode& ec)
{
    if (m_readyState == CLOSED) {
        ec = INVALID_STATE_ERR;
        return;
    }

    bool valid = m_peerHandler->startIce(iceOptions);
    if (!valid)
        ec = SYNTAX_ERR;
}

void PeerConnection00::processIceMessage(PassRefPtr<IceCandidate> prpIceCandidate, ExceptionCode& ec)
{
    if (m_readyState == CLOSED) {
        ec = INVALID_STATE_ERR;
        return;
    }

    RefPtr<IceCandidate> iceCandidate = prpIceCandidate;
    if (!iceCandidate) {
        ec = TYPE_MISMATCH_ERR;
        return;
    }

    bool valid = m_peerHandler->processIceMessage(iceCandidate->descriptor());
    if (!valid)
        ec = SYNTAX_ERR;
}

PeerConnection00::ReadyState PeerConnection00::readyState() const
{
    return m_readyState;
}

PeerConnection00::IceState PeerConnection00::iceState() const
{
    return m_iceState;
}

void PeerConnection00::addStream(PassRefPtr<MediaStream> prpStream, ExceptionCode& ec)
{
    RefPtr<MediaStream> stream = prpStream;
    if (!stream) {
        ec =  TYPE_MISMATCH_ERR;
        return;
    }

    if (m_readyState == CLOSED) {
        ec = INVALID_STATE_ERR;
        return;
    }

    if (m_localStreams->contains(stream.get()))
        return;

    m_localStreams->append(stream);

    m_peerHandler->addStream(stream->descriptor());
}

void PeerConnection00::addStream(PassRefPtr<MediaStream> stream, const Dictionary& mediaStreamHints, ExceptionCode& ec)
{
    // FIXME: When the spec says what the mediaStreamHints should look like use it.
    addStream(stream, ec);
}

void PeerConnection00::removeStream(MediaStream* stream, ExceptionCode& ec)
{
    if (m_readyState == CLOSED) {
        ec = INVALID_STATE_ERR;
        return;
    }

    if (!stream) {
        ec = TYPE_MISMATCH_ERR;
        return;
    }

    if (!m_localStreams->contains(stream))
        return;

    m_localStreams->remove(stream);

    m_peerHandler->removeStream(stream->descriptor());
}

MediaStreamList* PeerConnection00::localStreams() const
{
    return m_localStreams.get();
}

MediaStreamList* PeerConnection00::remoteStreams() const
{
    return m_remoteStreams.get();
}

void PeerConnection00::close(ExceptionCode& ec)
{
    if (m_readyState == CLOSED) {
        ec = INVALID_STATE_ERR;
        return;
    }

    stop();
}

void PeerConnection00::didGenerateIceCandidate(PassRefPtr<IceCandidateDescriptor> iceCandidateDescriptor, bool moreToFollow)
{
    ASSERT(scriptExecutionContext()->isContextThread());
    if (!iceCandidateDescriptor)
        m_iceCallback->handleEvent(0, moreToFollow, this);
    else {
        RefPtr<IceCandidate> iceCandidate = IceCandidate::create(iceCandidateDescriptor);
        m_iceCallback->handleEvent(iceCandidate.get(), moreToFollow, this);
    }
}

void PeerConnection00::didChangeReadyState(uint32_t newState)
{
    ASSERT(scriptExecutionContext()->isContextThread());
    changeReadyState(static_cast<ReadyState>(newState));
}

void PeerConnection00::didChangeIceState(uint32_t newState)
{
    ASSERT(scriptExecutionContext()->isContextThread());
    changeIceState(static_cast<IceState>(newState));
}

void PeerConnection00::didAddRemoteStream(PassRefPtr<MediaStreamDescriptor> streamDescriptor)
{
    ASSERT(scriptExecutionContext()->isContextThread());

    if (m_readyState == CLOSED)
        return;

    RefPtr<MediaStream> stream = MediaStream::create(scriptExecutionContext(), streamDescriptor);
    m_remoteStreams->append(stream);

    dispatchEvent(MediaStreamEvent::create(eventNames().addstreamEvent, false, false, stream.release()));
}

void PeerConnection00::didRemoveRemoteStream(MediaStreamDescriptor* streamDescriptor)
{
    ASSERT(scriptExecutionContext()->isContextThread());
    ASSERT(streamDescriptor->owner());

    RefPtr<MediaStream> stream = static_cast<MediaStream*>(streamDescriptor->owner());
    stream->streamEnded();

    if (m_readyState == CLOSED)
        return;

    ASSERT(m_remoteStreams->contains(stream.get()));
    m_remoteStreams->remove(stream.get());

    dispatchEvent(MediaStreamEvent::create(eventNames().removestreamEvent, false, false, stream.release()));
}

const AtomicString& PeerConnection00::interfaceName() const
{
    return eventNames().interfaceForPeerConnection00;
}

ScriptExecutionContext* PeerConnection00::scriptExecutionContext() const
{
    return ActiveDOMObject::scriptExecutionContext();
}

void PeerConnection00::stop()
{
    if (m_readyState == CLOSED)
        return;

    if (m_peerHandler)
        m_peerHandler->stop();
    m_peerHandler.clear();

    changeReadyState(CLOSED);
    changeIceState(ICE_CLOSED);
}

EventTargetData* PeerConnection00::eventTargetData()
{
    return &m_eventTargetData;
}

EventTargetData* PeerConnection00::ensureEventTargetData()
{
    return &m_eventTargetData;
}

void PeerConnection00::changeReadyState(ReadyState readyState)
{
    if (readyState == m_readyState)
        return;

    m_readyState = readyState;

    switch (m_readyState) {
    case OPENING:
        dispatchEvent(Event::create(eventNames().connectingEvent, false, false));
        break;
    case ACTIVE:
        dispatchEvent(Event::create(eventNames().openEvent, false, false));
        break;
    case CLOSED:
        break;
    case NEW:
        ASSERT_NOT_REACHED();
        break;
    }

    dispatchEvent(Event::create(eventNames().statechangeEvent, false, false));
}

void PeerConnection00::changeIceState(IceState iceState)
{
    if (iceState == m_iceState)
        return;

    m_iceState = iceState;
    dispatchEvent(Event::create(eventNames().statechangeEvent, false, false));
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

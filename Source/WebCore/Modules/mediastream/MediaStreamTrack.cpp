/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2011 Ericsson AB. All rights reserved.
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#include "MediaStreamTrack.h"

#if ENABLE(MEDIA_STREAM)

#include "Dictionary.h"
#include "Event.h"
#include "ExceptionCode.h"
#include "MediaStream.h"
#include "MediaStreamCenter.h"
#include "MediaStreamDescriptor.h"
#include "MediaStreamTrackSourcesCallback.h"
#include "MediaStreamTrackSourcesRequest.h"
#include "UUID.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

MediaStreamTrack::MediaStreamTrack(ScriptExecutionContext* context, MediaStreamSource* source, const Dictionary*)
    : ActiveDOMObject(context)
    , m_source(source)
    , m_readyState(MediaStreamSource::New)
    , m_stopped(false)
    , m_enabled(true)
    , m_muted(false)
{
    suspendIfNeeded();
    if (m_source) {
        m_source->addObserver(this);
        m_muted = m_source->muted();
    }
}

MediaStreamTrack::~MediaStreamTrack()
{
    if (m_source)
        m_source->removeObserver(this);
}

const AtomicString& MediaStreamTrack::kind() const
{
    if (!m_source)
        return emptyAtom;

    static NeverDestroyed<AtomicString> audioKind("audio", AtomicString::ConstructFromLiteral);
    static NeverDestroyed<AtomicString> videoKind("video", AtomicString::ConstructFromLiteral);

    switch (m_source->type()) {
    case MediaStreamSource::Audio:
        return audioKind;
    case MediaStreamSource::Video:
        return videoKind;
    }

    ASSERT_NOT_REACHED();
    return emptyAtom;
}

void MediaStreamTrack::setSource(MediaStreamSource* source)
{
    ASSERT(source == m_source || !m_source);
    m_source = source;
}

const String& MediaStreamTrack::id() const
{
    if (!m_id.isEmpty())
        return m_id;

    if (!m_source)
        return emptyString();

    const String& id = m_source->id();
    if (!id.isEmpty())
        return id;

    // The spec says:
    //   Unless a MediaStreamTrack object is created as a part a of special purpose algorithm that
    //   specifies how the track id must be initialized, the user agent must generate a globally
    //   unique identifier string and initialize the object's id attribute to that string.
    // so generate a UUID if the source doesn't have an ID.
    m_id = createCanonicalUUIDString();
    return m_id;
}

const String& MediaStreamTrack::label() const
{
    if (m_source)
        return m_source->name();
    return emptyString();
}

bool MediaStreamTrack::enabled() const
{
    return m_enabled;
}

void MediaStreamTrack::setEnabled(bool enabled)
{
    if (m_stopped)
        return;
    
    m_enabled = enabled;

    if (!m_source || enabled == m_source->enabled())
        return;

    m_source->setEnabled(enabled);

    if (m_source->stream()->ended())
        return;

    MediaStreamCenter::shared().didSetMediaStreamTrackEnabled(m_source.get());
}

const AtomicString& MediaStreamTrack::readyState() const
{
    static NeverDestroyed<AtomicString> ended("ended", AtomicString::ConstructFromLiteral);
    static NeverDestroyed<AtomicString> live("live", AtomicString::ConstructFromLiteral);
    static NeverDestroyed<AtomicString> newState("new", AtomicString::ConstructFromLiteral);

    if (!m_source)
        return newState;

    if (m_stopped)
        return ended;

    switch (m_source->readyState()) {
    case MediaStreamSource::Live:
        return live;
    case MediaStreamSource::New:
        return newState;
    case MediaStreamSource::Ended:
        return ended;
    }

    ASSERT_NOT_REACHED();
    return emptyAtom;
}

void MediaStreamTrack::getSources(ScriptExecutionContext* context, PassRefPtr<MediaStreamTrackSourcesCallback> callback, ExceptionCode& ec)
{
    RefPtr<MediaStreamTrackSourcesRequest> request = MediaStreamTrackSourcesRequest::create(context, callback);
    if (!MediaStreamCenter::shared().getMediaStreamTrackSources(request.release()))
        ec = NOT_SUPPORTED_ERR;
}

bool MediaStreamTrack::ended() const
{
    return m_stopped || (m_source && m_source->readyState() == MediaStreamSource::Ended);
}

void MediaStreamTrack::sourceChangedState()
{
    if (m_stopped)
        return;

    MediaStreamSource::ReadyState oldReadyState = m_readyState;
    m_readyState = m_source->readyState();

    if (m_readyState >= MediaStreamSource::Live && oldReadyState == MediaStreamSource::New)
        dispatchEvent(Event::create(eventNames().startedEvent, false, false));
    if (m_readyState == MediaStreamSource::Ended && oldReadyState != MediaStreamSource::Ended)
        dispatchEvent(Event::create(eventNames().endedEvent, false, false));

    if (m_muted == m_source->muted())
        return;

    m_muted = m_source->muted();
    if (m_muted)
        dispatchEvent(Event::create(eventNames().muteEvent, false, false));
    else
        dispatchEvent(Event::create(eventNames().unmuteEvent, false, false));
}

void MediaStreamTrack::trackDidEnd()
{
    // FIXME: this is wrong, the track shouldn't have to call the descriptor's client!
    MediaStreamDescriptorClient* client = m_source ? m_source->stream()->client() : 0;
    if (!client)
        return;
    
    client->trackDidEnd();
}

void MediaStreamTrack::stop()
{
    m_stopped = true;
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

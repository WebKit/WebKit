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

#include "AllAudioCapabilities.h"
#include "AllVideoCapabilities.h"
#include "AudioStreamTrack.h"
#include "Dictionary.h"
#include "Event.h"
#include "ExceptionCode.h"
#include "MediaConstraintsImpl.h"
#include "MediaSourceStates.h"
#include "MediaStream.h"
#include "MediaStreamCenter.h"
#include "MediaStreamDescriptor.h"
#include "MediaStreamTrackSourcesCallback.h"
#include "MediaStreamTrackSourcesRequest.h"
#include "MediaTrackConstraints.h"
#include "NotImplemented.h"
#include "UUID.h"
#include "VideoStreamTrack.h"
#include <wtf/Functional.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {

MediaStreamTrack::MediaStreamTrack(ScriptExecutionContext* context, MediaStreamSource* source, const Dictionary*)
    : ActiveDOMObject(context)
    , m_source(0)
    , m_readyState(MediaStreamSource::New)
    , m_stopped(false)
    , m_enabled(true)
    , m_muted(false)
    , m_eventDispatchScheduled(false)
{
    suspendIfNeeded();
    if (source)
        setSource(source);
}

MediaStreamTrack::MediaStreamTrack(MediaStreamTrack* other)
    : ActiveDOMObject(other->scriptExecutionContext())
{
    suspendIfNeeded();

    // When the clone() method is invoked, the user agent must run the following steps:
    // 1. Let trackClone be a newly constructed MediaStreamTrack object.
    // 2. Initialize trackClone's id attribute to a newly generated value.
    m_id = createCanonicalUUIDString();

    // 3. Let trackClone inherit this track's underlying source, kind, label and enabled attributes.
    setSource(other->source());
    m_readyState = m_source ? m_source->readyState() : MediaStreamSource::New;
    m_enabled = other->enabled();

    // Note: the "clone" steps don't say anything about 'muted', but 4.3.1 says:
    // For a newly created MediaStreamTrack object, the following applies. The track is always enabled
    // unless stated otherwise (for examlpe when cloned) and the muted state reflects the state of the
    // source at the time the track is created.
    m_muted = other->muted();

    m_eventDispatchScheduled =false;
    m_stopped = other->stopped();
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
    ASSERT(!source || !m_source);

    if (m_source)
        m_source->removeObserver(this);

    if (source) {
        source->addObserver(this);
        m_muted = source->muted();
    }

    m_source = source;
}

const String& MediaStreamTrack::id() const
{
    if (!m_id.isEmpty())
        return m_id;

    // The spec says:
    //   Unless a MediaStreamTrack object is created as a part a of special purpose algorithm that
    //   specifies how the track id must be initialized, the user agent must generate a globally
    //   unique identifier string and initialize the object's id attribute to that string.
    if (m_source && m_source->useIDForTrackID())
        return m_source->id();

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

    // 4.3.3.1
    // ... after a MediaStreamTrack is disassociated from its track, its enabled attribute still
    // changes value when set; it just doesn't do anything with that new value.
    m_enabled = enabled;

    if (!m_source)
        return;

    m_source->setEnabled(enabled);
}

bool MediaStreamTrack::muted() const
{
    if (m_stopped || !m_source)
        return false;

    return m_source->muted();
}

bool MediaStreamTrack::readonly() const
{
    if (m_stopped || !m_source)
        return true;

    return m_source->readonly();
}

bool MediaStreamTrack::remote() const
{
    if (!m_source)
        return false;

    return m_source->remote();
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

RefPtr<MediaTrackConstraints> MediaStreamTrack::constraints() const
{
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=122428
    notImplemented();
    return 0;
}

RefPtr<MediaSourceStates> MediaStreamTrack::states() const
{
    if (!m_source)
        return 0;
    
    return MediaSourceStates::create(m_source->states());
}

RefPtr<MediaStreamCapabilities> MediaStreamTrack::capabilities() const
{
    if (!m_source)
        return 0;

    return MediaStreamCapabilities::create(m_source->capabilities());
}

void MediaStreamTrack::applyConstraints(const Dictionary& constraints)
{
    // FIXME: apply the new constraints to the track
    // https://bugs.webkit.org/show_bug.cgi?id=122428
    m_constraints->initialize(constraints);
}

RefPtr<MediaStreamTrack> MediaStreamTrack::clone()
{
    if (m_source && m_source->type() == MediaStreamSource::Audio)
        return AudioStreamTrack::create(this);

    return VideoStreamTrack::create(this);
}

void MediaStreamTrack::stopProducingData()
{
    if (m_stopped || !m_source)
        return;

    // Set m_stopped before stopping the source because that may result in a call to sourceChangedState
    // and we only want to dispatch the 'ended' event if necessary.
    m_stopped = true;
    m_source->stop();

    if (m_readyState != MediaStreamSource::Ended)
        scheduleEventDispatch(Event::create(eventNames().endedEvent, false, false));
}

bool MediaStreamTrack::ended() const
{
    return m_stopped || (m_source && m_source->readyState() == MediaStreamSource::Ended);
}

void MediaStreamTrack::sourceStateChanged()
{
    if (m_stopped)
        return;

    MediaStreamSource::ReadyState oldReadyState = m_readyState;
    m_readyState = m_source->readyState();

    if (m_readyState >= MediaStreamSource::Live && oldReadyState == MediaStreamSource::New)
        scheduleEventDispatch(Event::create(eventNames().startedEvent, false, false));
    if (m_readyState == MediaStreamSource::Ended && oldReadyState != MediaStreamSource::Ended)
        scheduleEventDispatch(Event::create(eventNames().endedEvent, false, false));
}
    
void MediaStreamTrack::sourceMutedChanged()
{
    if (m_stopped)
        return;

    bool muted = m_source->muted();
    if (m_muted == muted)
        return;

    m_muted = muted;
    if (m_muted)
        scheduleEventDispatch(Event::create(eventNames().muteEvent, false, false));
    else
        scheduleEventDispatch(Event::create(eventNames().unmuteEvent, false, false));

    configureTrackRendering();
}

void MediaStreamTrack::sourceEnabledChanged()
{
    if (m_stopped)
        return;
    
    // media from the source only flows when a MediaStreamTrack object is both unmuted and enabled
    configureTrackRendering();
}

void MediaStreamTrack::configureTrackRendering()
{
    if (m_stopped)
        return;

    // 4.3.1
    // ... media from the source only flows when a MediaStreamTrack object is both unmuted and enabled
}

bool MediaStreamTrack::stopped()
{
    return m_stopped;
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
    stopProducingData();
}

void MediaStreamTrack::scheduleEventDispatch(PassRefPtr<Event> event)
{
    {
        MutexLocker locker(m_mutex);
        m_scheduledEvents.append(event);
        if (m_eventDispatchScheduled)
            return;
        m_eventDispatchScheduled = true;
    }

    callOnMainThread(bind(&MediaStreamTrack::dispatchQueuedEvents, this));
}

void MediaStreamTrack::dispatchQueuedEvents()
{
    Vector<RefPtr<Event>> events;
    {
        MutexLocker locker(m_mutex);
        m_eventDispatchScheduled = false;
        events.swap(m_scheduledEvents);
    }
    if (!scriptExecutionContext())
        return;

    for (auto it = events.begin(); it != events.end(); ++it)
        dispatchEvent((*it).release());

    events.clear();
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

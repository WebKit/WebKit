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

MediaStreamTrack::MediaStreamTrack(ScriptExecutionContext* context, PassRefPtr<MediaStreamTrackPrivate> privateTrack, const Dictionary*)
    : ActiveDOMObject(context)
    , m_privateTrack(privateTrack)
    , m_eventDispatchScheduled(false)
{
    suspendIfNeeded();

    if (m_privateTrack->source())
        m_privateTrack->source()->addObserver(this);
}

MediaStreamTrack::MediaStreamTrack(MediaStreamTrack* other)
    : ActiveDOMObject(other->scriptExecutionContext())
{
    suspendIfNeeded();

    m_privateTrack = other->privateTrack()->clone();

    if (m_privateTrack->source())
        m_privateTrack->source()->addObserver(this);

    m_eventDispatchScheduled = false;
}

MediaStreamTrack::~MediaStreamTrack()
{
}

const AtomicString& MediaStreamTrack::kind() const
{
    return m_privateTrack->kind();
}

void MediaStreamTrack::setSource(MediaStreamSource* newSource)
{
    MediaStreamSource* trackSource = source();

    if (trackSource)
        trackSource->removeObserver(this);

    if (newSource)
        newSource->addObserver(this);

    m_privateTrack->setSource(newSource);
}

const String& MediaStreamTrack::id() const
{
    return m_privateTrack->id();
}

const String& MediaStreamTrack::label() const
{
    return m_privateTrack->label();
}

bool MediaStreamTrack::enabled() const
{
    return m_privateTrack->enabled();
}

void MediaStreamTrack::setEnabled(bool enabled)
{
    m_privateTrack->setEnabled(enabled);
}

bool MediaStreamTrack::muted() const
{
    return m_privateTrack->muted();
}

bool MediaStreamTrack::readonly() const
{
    return m_privateTrack->readonly();
}

bool MediaStreamTrack::remote() const
{
    return m_privateTrack->remote();
}

const AtomicString& MediaStreamTrack::readyState() const
{
    static NeverDestroyed<AtomicString> ended("ended", AtomicString::ConstructFromLiteral);
    static NeverDestroyed<AtomicString> live("live", AtomicString::ConstructFromLiteral);
    static NeverDestroyed<AtomicString> newState("new", AtomicString::ConstructFromLiteral);

    switch (m_privateTrack->readyState()) {
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
    if (!source())
        return 0;

    return MediaSourceStates::create(source()->states());
}

RefPtr<MediaStreamCapabilities> MediaStreamTrack::capabilities() const
{
    if (!source())
        return 0;

    return MediaStreamCapabilities::create(source()->capabilities());
}

void MediaStreamTrack::applyConstraints(const Dictionary& constraints)
{
    // FIXME: apply the new constraints to the track
    // https://bugs.webkit.org/show_bug.cgi?id=122428
    m_constraints->initialize(constraints);
}

RefPtr<MediaStreamTrack> MediaStreamTrack::clone()
{
    if (source() && source()->type() == MediaStreamSource::Audio)
        return AudioStreamTrack::create(this);

    return VideoStreamTrack::create(this);
}

void MediaStreamTrack::stopProducingData()
{
    if (stopped() || !source())
        return;

    m_privateTrack->setReadyState(MediaStreamSource::Ended);
    m_privateTrack->stop();
}

bool MediaStreamTrack::ended() const
{
    return m_privateTrack->ended();
}

void MediaStreamTrack::addObserver(MediaStreamTrack::Observer* observer)
{
    m_observers.append(observer);
}

void MediaStreamTrack::removeObserver(MediaStreamTrack::Observer* observer)
{
    size_t pos = m_observers.find(observer);
    if (pos != notFound)
        m_observers.remove(pos);
}

void MediaStreamTrack::sourceStateChanged()
{
    if (stopped())
        return;

    m_privateTrack->setReadyState(source()->readyState());
}
    
void MediaStreamTrack::sourceMutedChanged()
{
    if (stopped())
        return;

    m_privateTrack->setMuted(source()->muted());

    configureTrackRendering();
}

void MediaStreamTrack::sourceEnabledChanged()
{
    if (stopped())
        return;

    setEnabled(source()->enabled());
    // media from the source only flows when a MediaStreamTrack object is both unmuted and enabled
    configureTrackRendering();
}

void MediaStreamTrack::configureTrackRendering()
{
    if (stopped())
        return;

    // 4.3.1
    // ... media from the source only flows when a MediaStreamTrack object is both unmuted and enabled
}

bool MediaStreamTrack::stopped()
{
    return m_privateTrack->stopped();
}

void MediaStreamTrack::trackDidEnd()
{
    m_privateTrack->setReadyState(MediaStreamSource::Ended);

    for (Vector<Observer*>::iterator i = m_observers.begin(); i != m_observers.end(); ++i)
        (*i)->trackDidEnd();
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

void MediaStreamTrack::trackReadyStateChanged()
{
    if (m_privateTrack->readyState() == MediaStreamSource::Live)
        scheduleEventDispatch(Event::create(eventNames().startedEvent, false, false));
    else if (m_privateTrack->readyState() == MediaStreamSource::Ended)
        scheduleEventDispatch(Event::create(eventNames().endedEvent, false, false));
}

void MediaStreamTrack::trackMutedChanged()
{
    if (muted())
        scheduleEventDispatch(Event::create(eventNames().muteEvent, false, false));
    else
        scheduleEventDispatch(Event::create(eventNames().unmuteEvent, false, false));
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

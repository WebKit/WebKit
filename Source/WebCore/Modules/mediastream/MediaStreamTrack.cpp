/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2011 Ericsson AB. All rights reserved.
 * Copyright (C) 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Nokia Corporation and/or its subsidiary(-ies).
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
#include "ExceptionCodePlaceholder.h"
#include "MediaConstraintsImpl.h"
#include "MediaSourceStates.h"
#include "MediaStream.h"
#include "MediaStreamPrivate.h"
#include "MediaStreamTrackSourcesCallback.h"
#include "MediaStreamTrackSourcesRequest.h"
#include "MediaTrackConstraints.h"
#include "NotImplemented.h"
#include "RealtimeMediaSourceCenter.h"
#include "VideoStreamTrack.h"
#include <wtf/Functional.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {

MediaStreamTrack::MediaStreamTrack(ScriptExecutionContext& context, MediaStreamTrackPrivate& privateTrack, const Dictionary* constraints)
    : RefCounted()
    , ActiveDOMObject(&context)
    , m_privateTrack(privateTrack)
    , m_eventDispatchScheduled(false)
    , m_stoppingTrack(false)
{
    suspendIfNeeded();

    m_privateTrack->setClient(this);

    if (constraints)
        applyConstraints(*constraints);
}

MediaStreamTrack::MediaStreamTrack(MediaStreamTrack& other)
    : RefCounted()
    , ActiveDOMObject(other.scriptExecutionContext())
    , m_privateTrack(*other.privateTrack().clone())
    , m_eventDispatchScheduled(false)
    , m_stoppingTrack(false)
{
    suspendIfNeeded();

    m_privateTrack->setClient(this);
}

MediaStreamTrack::~MediaStreamTrack()
{
    m_privateTrack->setClient(nullptr);
}

void MediaStreamTrack::setSource(PassRefPtr<RealtimeMediaSource> newSource)
{
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

bool MediaStreamTrack::stopped() const
{
    return m_privateTrack->stopped();
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
    case RealtimeMediaSource::Live:
        return live;
    case RealtimeMediaSource::New:
        return newState;
    case RealtimeMediaSource::Ended:
        return ended;
    }

    ASSERT_NOT_REACHED();
    return emptyAtom;
}

void MediaStreamTrack::getSources(ScriptExecutionContext* context, PassRefPtr<MediaStreamTrackSourcesCallback> callback, ExceptionCode& ec)
{
    RefPtr<MediaStreamTrackSourcesRequest> request = MediaStreamTrackSourcesRequest::create(context, callback);
    if (!RealtimeMediaSourceCenter::singleton().getMediaStreamTrackSources(request.release()))
        ec = NOT_SUPPORTED_ERR;
}

RefPtr<MediaTrackConstraints> MediaStreamTrack::getConstraints() const
{
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=122428
    notImplemented();
    return 0;
}

RefPtr<MediaSourceStates> MediaStreamTrack::states() const
{
    return MediaSourceStates::create(m_privateTrack->states());
}

RefPtr<MediaStreamCapabilities> MediaStreamTrack::getCapabilities() const
{
    // The source may be shared by multiple tracks, so its states is not necessarily
    // in sync with the track state. A track that is new or has ended always has a source
    // type of "none".
    RefPtr<RealtimeMediaSourceCapabilities> sourceCapabilities = m_privateTrack->capabilities();
    RealtimeMediaSource::ReadyState readyState = m_privateTrack->readyState();
    if (readyState == RealtimeMediaSource::New || readyState == RealtimeMediaSource::Ended)
        sourceCapabilities->setSourceType(RealtimeMediaSourceStates::None);
    
    return MediaStreamCapabilities::create(sourceCapabilities.release());
}

void MediaStreamTrack::applyConstraints(const Dictionary& constraints)
{
    m_constraints->initialize(constraints);
    m_privateTrack->applyConstraints(m_constraints);
}

void MediaStreamTrack::applyConstraints(PassRefPtr<MediaConstraints>)
{
    // FIXME: apply the new constraints to the track
    // https://bugs.webkit.org/show_bug.cgi?id=122428
}

RefPtr<MediaStreamTrack> MediaStreamTrack::clone()
{
    if (m_privateTrack->type() == RealtimeMediaSource::Audio)
        return AudioStreamTrack::create(*this);

    return VideoStreamTrack::create(*this);
}

void MediaStreamTrack::stopProducingData()
{
    // NOTE: this method is called when the "stop" method is called from JS, using
    // the "ImplementedAs" IDL attribute. This is done because ActiveDOMObject requires
    // a "stop" method.
    
    // The stop method should "Permanently stop the generation of data for track's source", but it
    // should not post an 'ended' event. 
    m_stoppingTrack = true;
    m_privateTrack->stop(MediaStreamTrackPrivate::StopTrackAndStopSource);
    m_stoppingTrack = false;
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

void MediaStreamTrack::trackReadyStateChanged()
{
    if (stopped())
        return;

    RealtimeMediaSource::ReadyState readyState = m_privateTrack->readyState();
    if (readyState == RealtimeMediaSource::Live)
        scheduleEventDispatch(Event::create(eventNames().startedEvent, false, false));
    else if (readyState == RealtimeMediaSource::Ended && !m_stoppingTrack)
        scheduleEventDispatch(Event::create(eventNames().endedEvent, false, false));

    configureTrackRendering();
}
    
void MediaStreamTrack::trackMutedChanged()
{
    if (stopped())
        return;

    if (muted())
        scheduleEventDispatch(Event::create(eventNames().muteEvent, false, false));
    else
        scheduleEventDispatch(Event::create(eventNames().unmuteEvent, false, false));

    configureTrackRendering();
}

void MediaStreamTrack::trackEnabledChanged()
{
    if (stopped())
        return;

    setEnabled(m_privateTrack->enabled());
    configureTrackRendering();
}

void MediaStreamTrack::configureTrackRendering()
{
    if (stopped())
        return;

    // 4.3.1
    // ... media from the source only flows when a MediaStreamTrack object is both unmuted and enabled
}

void MediaStreamTrack::trackDidEnd()
{
    m_privateTrack->setReadyState(RealtimeMediaSource::Ended);

    for (Vector<Observer*>::iterator i = m_observers.begin(); i != m_observers.end(); ++i)
        (*i)->trackDidEnd();
}

void MediaStreamTrack::stop()
{
    m_privateTrack->stop(MediaStreamTrackPrivate::StopTrackOnly);
}

const char* MediaStreamTrack::activeDOMObjectName() const
{
    return "MediaStreamTrack";
}

bool MediaStreamTrack::canSuspend() const
{
    // FIXME: We should try and do better here.
    return false;
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

    RefPtr<MediaStreamTrack> protectedThis(this);
    callOnMainThread([protectedThis] {
        Vector<RefPtr<Event>> events;
        {
            MutexLocker locker(protectedThis->m_mutex);
            protectedThis->m_eventDispatchScheduled = false;
            events = WTF::move(protectedThis->m_scheduledEvents);
        }

        if (!protectedThis->scriptExecutionContext())
            return;

        for (auto& event : events)
            protectedThis->dispatchEvent(event.release());
    });
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

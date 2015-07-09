/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2011, 2012, 2015 Ericsson AB. All rights reserved.
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
#include "MediaStream.h"

#if ENABLE(MEDIA_STREAM)

#include "Event.h"
#include "ExceptionCode.h"
#include "MediaStreamRegistry.h"
#include "MediaStreamTrackEvent.h"
#include "RealtimeMediaSource.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

Ref<MediaStream> MediaStream::create(ScriptExecutionContext& context)
{
    return MediaStream::create(context, MediaStreamPrivate::create());
}

Ref<MediaStream> MediaStream::create(ScriptExecutionContext& context, MediaStream* stream)
{
    ASSERT(stream);

    return adoptRef(*new MediaStream(context, stream->getTracks()));
}

Ref<MediaStream> MediaStream::create(ScriptExecutionContext& context, const Vector<RefPtr<MediaStreamTrack>>& tracks)
{
    return adoptRef(*new MediaStream(context, tracks));
}

Ref<MediaStream> MediaStream::create(ScriptExecutionContext& context, RefPtr<MediaStreamPrivate>&& streamPrivate)
{
    return adoptRef(*new MediaStream(context, WTF::move(streamPrivate)));
}

MediaStream::MediaStream(ScriptExecutionContext& context, const Vector<RefPtr<MediaStreamTrack>>& tracks)
    : ContextDestructionObserver(&context)
    , m_activityEventTimer(*this, &MediaStream::activityEventTimerFired)
{
    // This constructor preserves MediaStreamTrack instances and must be used by calls originating
    // from the JavaScript MediaStream constructor.
    Vector<RefPtr<MediaStreamTrackPrivate>> trackPrivates;
    trackPrivates.reserveCapacity(tracks.size());

    for (auto& track : tracks) {
        track->addObserver(this);
        m_trackSet.add(track->id(), track);
        trackPrivates.append(&track->privateTrack());
    }

    m_private = MediaStreamPrivate::create(trackPrivates);
    m_isActive = m_private->active();
    m_private->setClient(this);
}

MediaStream::MediaStream(ScriptExecutionContext& context, RefPtr<MediaStreamPrivate>&& streamPrivate)
    : ContextDestructionObserver(&context)
    , m_private(streamPrivate)
    , m_isActive(m_private->active())
    , m_activityEventTimer(*this, &MediaStream::activityEventTimerFired)
{
    ASSERT(m_private);
    m_private->setClient(this);

    for (auto& trackPrivate : m_private->tracks()) {
        RefPtr<MediaStreamTrack> track = MediaStreamTrack::create(context, *trackPrivate);
        track->addObserver(this);
        m_trackSet.add(track->id(), WTF::move(track));
    }
}

MediaStream::~MediaStream()
{
    m_private->setClient(nullptr);
}

RefPtr<MediaStream> MediaStream::clone()
{
    Vector<RefPtr<MediaStreamTrack>> clonedTracks;
    clonedTracks.reserveCapacity(m_trackSet.size());

    for (auto& track : m_trackSet.values())
        clonedTracks.append(track->clone());

    return MediaStream::create(*scriptExecutionContext(), clonedTracks);
}

void MediaStream::addTrack(RefPtr<MediaStreamTrack>&& track)
{
    if (!internalAddTrack(WTF::move(track), StreamModifier::DomAPI))
        return;

    for (auto& observer : m_observers)
        observer->didAddOrRemoveTrack();
}

void MediaStream::removeTrack(MediaStreamTrack* track)
{
    if (!internalRemoveTrack(track, StreamModifier::DomAPI))
        return;

    for (auto& observer : m_observers)
        observer->didAddOrRemoveTrack();
}

MediaStreamTrack* MediaStream::getTrackById(String id)
{
    auto it = m_trackSet.find(id);
    if (it != m_trackSet.end())
        return it->value.get();

    return nullptr;
}

Vector<RefPtr<MediaStreamTrack>> MediaStream::getAudioTracks()
{
    return trackVectorForType(RealtimeMediaSource::Audio);
}

Vector<RefPtr<MediaStreamTrack>> MediaStream::getVideoTracks()
{
    return trackVectorForType(RealtimeMediaSource::Video);
}

Vector<RefPtr<MediaStreamTrack>> MediaStream::getTracks() const
{
    Vector<RefPtr<MediaStreamTrack>> tracks;
    tracks.reserveCapacity(m_trackSet.size());
    copyValuesToVector(m_trackSet, tracks);

    return tracks;
}

void MediaStream::contextDestroyed()
{
    ContextDestructionObserver::contextDestroyed();
}

void MediaStream::trackDidEnd()
{
    m_private->updateActiveState(MediaStreamPrivate::NotifyClientOption::Notify);
}

void MediaStream::activeStatusChanged()
{
    // Schedule the active state change and event dispatch since this callback may be called
    // synchronously from the DOM API (e.g. as a result of addTrack()).
    scheduleActiveStateChange();
}

void MediaStream::didAddTrackToPrivate(MediaStreamTrackPrivate& trackPrivate)
{
    ScriptExecutionContext* context = scriptExecutionContext();
    if (!context)
        return;

    internalAddTrack(MediaStreamTrack::create(*context, trackPrivate), StreamModifier::Platform);
}

void MediaStream::didRemoveTrackFromPrivate(MediaStreamTrackPrivate& trackPrivate)
{
    RefPtr<MediaStreamTrack> track = getTrackById(trackPrivate.id());
    internalRemoveTrack(WTF::move(track), StreamModifier::Platform);
}

bool MediaStream::internalAddTrack(RefPtr<MediaStreamTrack>&& track, StreamModifier streamModifier)
{
    if (getTrackById(track->id()))
        return false;

    m_trackSet.add(track->id(), track);
    track->addObserver(this);

    if (streamModifier == StreamModifier::DomAPI)
        m_private->addTrack(&track->privateTrack(), MediaStreamPrivate::NotifyClientOption::DontNotify);
    else
        dispatchEvent(MediaStreamTrackEvent::create(eventNames().addtrackEvent, false, false, WTF::move(track)));

    return true;
}

bool MediaStream::internalRemoveTrack(RefPtr<MediaStreamTrack>&& track, StreamModifier streamModifier)
{
    if (!m_trackSet.remove(track->id()))
        return false;

    track->removeObserver(this);

    if (streamModifier == StreamModifier::DomAPI)
        m_private->removeTrack(track->privateTrack(), MediaStreamPrivate::NotifyClientOption::DontNotify);
    else
        dispatchEvent(MediaStreamTrackEvent::create(eventNames().removetrackEvent, false, false, WTF::move(track)));

    return true;
}

void MediaStream::scheduleActiveStateChange()
{
    const AtomicString& eventName = m_isActive ? eventNames().inactiveEvent : eventNames().activeEvent;
    m_scheduledActivityEvents.append(Event::create(eventName, false, false));

    if (!m_activityEventTimer.isActive())
        m_activityEventTimer.startOneShot(0);
}

void MediaStream::activityEventTimerFired()
{
    Vector<RefPtr<Event>> events;
    events.swap(m_scheduledActivityEvents);

    for (auto& event : events) {
        m_isActive = event->type() == eventNames().activeEvent;
        dispatchEvent(event.release());
    }

    events.clear();
}

URLRegistry& MediaStream::registry() const
{
    return MediaStreamRegistry::registry();
}

Vector<RefPtr<MediaStreamTrack>> MediaStream::trackVectorForType(RealtimeMediaSource::Type filterType) const
{
    Vector<RefPtr<MediaStreamTrack>> tracks;
    for (auto& track : m_trackSet.values()) {
        if (track->source()->type() == filterType)
            tracks.append(track);
    }

    return tracks;
}

void MediaStream::addObserver(MediaStream::Observer* observer)
{
    if (m_observers.find(observer) == notFound)
        m_observers.append(observer);
}

void MediaStream::removeObserver(MediaStream::Observer* observer)
{
    size_t pos = m_observers.find(observer);
    if (pos != notFound)
        m_observers.remove(pos);
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2011, 2012, 2015 Ericsson AB. All rights reserved.
 * Copyright (C) 2013-2015 Apple Inc. All rights reserved.
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
#include "URL.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

static Vector<MediaStream*>& mediaStreams()
{
    static NeverDestroyed<Vector<MediaStream*>> streams;
    return streams;
}

static void registerMediaStream(MediaStream* stream)
{
    mediaStreams().append(stream);
}

static void unRegisterMediaStream(MediaStream* stream)
{
    Vector<MediaStream*>& allStreams = mediaStreams();
    size_t pos = allStreams.find(stream);
    if (pos != notFound)
        allStreams.remove(pos);
}

MediaStream* MediaStream::lookUp(const MediaStreamPrivate& privateStream)
{
    Vector<MediaStream*>& allStreams = mediaStreams();
    for (auto& stream : allStreams) {
        if (stream->m_private == &privateStream)
            return stream;
    }

    return nullptr;
}

static URLRegistry* s_registry;

void MediaStream::setRegistry(URLRegistry& registry)
{
    ASSERT(!s_registry);
    s_registry = &registry;
}

MediaStream* MediaStream::lookUp(const URL& url)
{
    if (!s_registry)
        return nullptr;

    return static_cast<MediaStream*>(s_registry->lookup(url.string()));
}

Ref<MediaStream> MediaStream::create(ScriptExecutionContext& context)
{
    return MediaStream::create(context, MediaStreamPrivate::create(Vector<RefPtr<MediaStreamTrackPrivate>>()));
}

Ref<MediaStream> MediaStream::create(ScriptExecutionContext& context, MediaStream* stream)
{
    ASSERT(stream);

    return adoptRef(*new MediaStream(context, stream->getTracks()));
}

Ref<MediaStream> MediaStream::create(ScriptExecutionContext& context, const MediaStreamTrackVector& tracks)
{
    return adoptRef(*new MediaStream(context, tracks));
}

Ref<MediaStream> MediaStream::create(ScriptExecutionContext& context, RefPtr<MediaStreamPrivate>&& streamPrivate)
{
    return adoptRef(*new MediaStream(context, WTF::move(streamPrivate)));
}

MediaStream::MediaStream(ScriptExecutionContext& context, const MediaStreamTrackVector& tracks)
    : ContextDestructionObserver(&context)
    , m_activityEventTimer(*this, &MediaStream::activityEventTimerFired)
{
    // This constructor preserves MediaStreamTrack instances and must be used by calls originating
    // from the JavaScript MediaStream constructor.
    MediaStreamTrackPrivateVector trackPrivates;
    trackPrivates.reserveCapacity(tracks.size());

    for (auto& track : tracks) {
        track->addObserver(this);
        m_trackSet.add(track->id(), track);
        trackPrivates.append(&track->privateTrack());
    }

    m_private = MediaStreamPrivate::create(trackPrivates);
    m_isActive = m_private->active();
    m_private->addObserver(*this);
    registerMediaStream(this);
}

MediaStream::MediaStream(ScriptExecutionContext& context, RefPtr<MediaStreamPrivate>&& streamPrivate)
    : ContextDestructionObserver(&context)
    , m_private(streamPrivate)
    , m_isActive(m_private->active())
    , m_activityEventTimer(*this, &MediaStream::activityEventTimerFired)
{
    ASSERT(m_private);
    m_private->addObserver(*this);
    registerMediaStream(this);

    for (auto& trackPrivate : m_private->tracks()) {
        RefPtr<MediaStreamTrack> track = MediaStreamTrack::create(context, *trackPrivate);
        track->addObserver(this);
        m_trackSet.add(track->id(), WTF::move(track));
    }
}

MediaStream::~MediaStream()
{
    unRegisterMediaStream(this);
    m_private->removeObserver(*this);
    for (auto& track : m_trackSet.values())
        track->removeObserver(this);
}

RefPtr<MediaStream> MediaStream::clone()
{
    MediaStreamTrackVector clonedTracks;
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

MediaStreamTrackVector MediaStream::getAudioTracks() const
{
    return trackVectorForType(RealtimeMediaSource::Audio);
}

MediaStreamTrackVector MediaStream::getVideoTracks() const
{
    return trackVectorForType(RealtimeMediaSource::Video);
}

MediaStreamTrackVector MediaStream::getTracks() const
{
    MediaStreamTrackVector tracks;
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

void MediaStream::didAddTrack(MediaStreamTrackPrivate& trackPrivate)
{
    ScriptExecutionContext* context = scriptExecutionContext();
    if (!context)
        return;

    if (!getTrackById(trackPrivate.id()))
        internalAddTrack(MediaStreamTrack::create(*context, trackPrivate), StreamModifier::Platform);
}

void MediaStream::didRemoveTrack(MediaStreamTrackPrivate& trackPrivate)
{
    RefPtr<MediaStreamTrack> track = getTrackById(trackPrivate.id());
    ASSERT(track);
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
    bool active = false;
    for (auto& track : m_trackSet.values()) {
        if (!track->ended()) {
            active = true;
            break;
        }
    }
    if (m_isActive == active)
        return;

    m_isActive = active;

    const AtomicString& eventName = m_isActive ? eventNames().inactiveEvent : eventNames().activeEvent;
    m_scheduledActivityEvents.append(Event::create(eventName, false, false));

    if (!m_activityEventTimer.isActive())
        m_activityEventTimer.startOneShot(0);
}

void MediaStream::activityEventTimerFired()
{
    Vector<Ref<Event>> events;
    events.swap(m_scheduledActivityEvents);

    for (auto& event : events)
        dispatchEvent(event);
}

URLRegistry& MediaStream::registry() const
{
    return MediaStreamRegistry::registry();
}

MediaStreamTrackVector MediaStream::trackVectorForType(RealtimeMediaSource::Type filterType) const
{
    MediaStreamTrackVector tracks;
    for (auto& track : m_trackSet.values()) {
        if (track->source().type() == filterType)
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

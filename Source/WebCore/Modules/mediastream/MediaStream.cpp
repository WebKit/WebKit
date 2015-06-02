/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2011, 2012 Ericsson AB. All rights reserved.
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
    return MediaStream::create(context, MediaStreamPrivate::create(Vector<RefPtr<RealtimeMediaSource>>(), Vector<RefPtr<RealtimeMediaSource>>()));
}

Ref<MediaStream> MediaStream::create(ScriptExecutionContext& context, PassRefPtr<MediaStream> stream)
{
    ASSERT(stream);

    Vector<RefPtr<MediaStreamTrackPrivate>> audioTracks;
    Vector<RefPtr<MediaStreamTrackPrivate>> videoTracks;

    for (auto& track : stream->m_audioTracks)
        audioTracks.append(&track->privateTrack());

    for (auto& track : stream->m_videoTracks)
        videoTracks.append(&track->privateTrack());

    return MediaStream::create(context, MediaStreamPrivate::create(audioTracks, videoTracks));
}

Ref<MediaStream> MediaStream::create(ScriptExecutionContext& context, const Vector<RefPtr<MediaStreamTrack>>& tracks)
{
    Vector<RefPtr<MediaStreamTrackPrivate>> audioTracks;
    Vector<RefPtr<MediaStreamTrackPrivate>> videoTracks;

    for (auto& track : tracks) {
        if (track->kind() == "audio")
            audioTracks.append(&track->privateTrack());
        else
            videoTracks.append(&track->privateTrack());
    }

    return MediaStream::create(context, MediaStreamPrivate::create(audioTracks, videoTracks));
}

Ref<MediaStream> MediaStream::create(ScriptExecutionContext& context, PassRefPtr<MediaStreamPrivate> privateStream)
{
    return adoptRef(*new MediaStream(context, privateStream));
}

MediaStream::MediaStream(ScriptExecutionContext& context, PassRefPtr<MediaStreamPrivate> privateStream)
    : ContextDestructionObserver(&context)
    , m_private(privateStream)
    , m_scheduledEventTimer(*this, &MediaStream::scheduledEventTimerFired)
{
    ASSERT(m_private);
    m_private->setClient(this);

    RefPtr<MediaStreamTrack> track;
    size_t numberOfAudioTracks = m_private->numberOfAudioTracks();
    m_audioTracks.reserveCapacity(numberOfAudioTracks);
    for (size_t i = 0; i < numberOfAudioTracks; i++) {
        track = MediaStreamTrack::create(context, *m_private->audioTracks(i));
        track->addObserver(this);
        m_audioTracks.append(track.release());
    }

    size_t numberOfVideoTracks = m_private->numberOfVideoTracks();
    m_videoTracks.reserveCapacity(numberOfVideoTracks);
    for (size_t i = 0; i < numberOfVideoTracks; i++) {
        track = MediaStreamTrack::create(context, *m_private->videoTracks(i));
        track->addObserver(this);
        m_videoTracks.append(track.release());
    }
}

MediaStream::~MediaStream()
{
    m_private->setClient(0);
}

bool MediaStream::active() const
{
    return m_private->active();
}

void MediaStream::setActive(bool isActive)
{
    if (active() == isActive)
        return;
    m_private->setActive(isActive);
}

PassRefPtr<MediaStream> MediaStream::clone()
{
    Vector<RefPtr<MediaStreamTrack>> trackSet;

    cloneMediaStreamTrackVector(trackSet, getAudioTracks());
    cloneMediaStreamTrackVector(trackSet, getVideoTracks());
    return MediaStream::create(*scriptExecutionContext(), trackSet);
}

void MediaStream::cloneMediaStreamTrackVector(Vector<RefPtr<MediaStreamTrack>>& destination, const Vector<RefPtr<MediaStreamTrack>>& source)
{
    for (auto& track : source)
        destination.append(track->clone());
}

void MediaStream::addTrack(PassRefPtr<MediaStreamTrack> prpTrack, ExceptionCode& ec)
{
    if (!prpTrack) {
        ec = TYPE_MISMATCH_ERR;
        return;
    }

    if (addTrack(prpTrack)) {
        for (auto& observer : m_observers)
            observer->didAddOrRemoveTrack();
    }
}

bool MediaStream::addTrack(PassRefPtr<MediaStreamTrack> prpTrack)
{
    // This is a common part used by addTrack called by JavaScript
    // and addRemoteTrack and only addRemoteTrack must fire addtrack event
    RefPtr<MediaStreamTrack> track = prpTrack;
    if (getTrackById(track->id()))
        return false;

    Vector<RefPtr<MediaStreamTrack>>* tracks = trackVectorForType(track->source()->type());

    tracks->append(track);
    track->addObserver(this);
    m_private->addTrack(&track->privateTrack());
    setActive(true);
    return true;
}

void MediaStream::removeTrack(PassRefPtr<MediaStreamTrack> prpTrack, ExceptionCode& ec)
{
    if (!active()) {
        ec = INVALID_STATE_ERR;
        return;
    }

    if (!prpTrack) {
        ec = TYPE_MISMATCH_ERR;
        return;
    }

    if (removeTrack(prpTrack)) {
        for (auto& observer : m_observers)
            observer->didAddOrRemoveTrack();
    }
}

bool MediaStream::removeTrack(PassRefPtr<MediaStreamTrack> prpTrack)
{
    // This is a common part used by removeTrack called by JavaScript
    // and removeRemoteTrack and only removeRemoteTrack must fire removetrack event
    RefPtr<MediaStreamTrack> track = prpTrack;
    Vector<RefPtr<MediaStreamTrack>>* tracks = trackVectorForType(track->source()->type());

    size_t pos = tracks->find(track);
    if (pos == notFound)
        return false;

    tracks->remove(pos);
    m_private->removeTrack(&track->privateTrack());
    // There can be other tracks using the same source in the same MediaStream,
    // like when MediaStreamTrack::clone() is called, for instance.
    // Spec says that a source can be shared, so we must assure that there is no
    // other track using it.
    if (!haveTrackWithSource(track->source()))
        m_private->removeSource(track->source());

    track->removeObserver(this);
    if (!m_audioTracks.size() && !m_videoTracks.size())
        setActive(false);

    return true;
}

bool MediaStream::haveTrackWithSource(PassRefPtr<RealtimeMediaSource> source)
{
    if (source->type() == RealtimeMediaSource::Audio) {
        for (auto& track : m_audioTracks) {
            if (track->source() == source.get())
                return true;
        }
        return false;
    }

    for (auto& track : m_videoTracks) {
        if (track->source() == source.get())
            return true;
    }

    return false;
}

MediaStreamTrack* MediaStream::getTrackById(String id)
{
    for (auto& track : m_audioTracks) {
        if (track->id() == id)
            return track.get();
    }

    for (auto& track :  m_videoTracks) {
        if (track->id() == id)
            return track.get();
    }

    return nullptr;
}

Vector<RefPtr<MediaStreamTrack>> MediaStream::getTracks()
{
    Vector<RefPtr<MediaStreamTrack>> tracks;
    for (auto& track : m_audioTracks)
        tracks.append(track.get());
    for (auto& track : m_videoTracks)
        tracks.append(track.get());

    return tracks;
}

void MediaStream::trackDidEnd()
{
    for (auto& track : m_audioTracks) {
        if (!track->ended())
            return;
    }
    for (auto& track : m_videoTracks) {
        if (!track->ended())
            return;
    }

    if (!m_audioTracks.size() && !m_videoTracks.size())
        setActive(false);
}

void MediaStream::setStreamIsActive(bool streamActive)
{
    if (streamActive)
        scheduleDispatchEvent(Event::create(eventNames().activeEvent, false, false));
    else
        scheduleDispatchEvent(Event::create(eventNames().inactiveEvent, false, false));
}

void MediaStream::contextDestroyed()
{
    ContextDestructionObserver::contextDestroyed();
}

void MediaStream::addRemoteSource(RealtimeMediaSource* source)
{
    ASSERT(source);
    addRemoteTrack(MediaStreamTrackPrivate::create(source).get());
}

void MediaStream::removeRemoteSource(RealtimeMediaSource* source)
{
    ASSERT(source);
    if (!active())
        return;

    Vector<RefPtr<MediaStreamTrack>>* tracks = trackVectorForType(source->type());

    for (int i = tracks->size() - 1; i >= 0; --i) {
        if ((*tracks)[i]->source() != source)
            continue;

        RefPtr<MediaStreamTrack> track = (*tracks)[i];
        track->removeObserver(this);
        tracks->remove(i);
        m_private->removeTrack(&track->privateTrack());
        scheduleDispatchEvent(MediaStreamTrackEvent::create(eventNames().removetrackEvent, false, false, track.release()));
    }

    m_private->removeSource(source);
}

void MediaStream::addRemoteTrack(MediaStreamTrackPrivate* privateTrack)
{
    ASSERT(privateTrack);
    if (!active())
        return;

    RefPtr<MediaStreamTrack> track;
    switch (privateTrack->type()) {
    case RealtimeMediaSource::Audio:
    case RealtimeMediaSource::Video:
        track = MediaStreamTrack::create(*scriptExecutionContext(), *privateTrack);
        break;
    case RealtimeMediaSource::None:
        ASSERT_NOT_REACHED();
        break;
    }

    if (!track)
        return;

    if (addTrack(track))
        scheduleDispatchEvent(MediaStreamTrackEvent::create(eventNames().addtrackEvent, false, false, track));
}

void MediaStream::removeRemoteTrack(MediaStreamTrackPrivate* privateTrack)
{
    ASSERT(privateTrack);
    if (!active())
        return;

    RefPtr<MediaStreamTrack> track = getTrackById(privateTrack->id());
    if (removeTrack(track))
        scheduleDispatchEvent(MediaStreamTrackEvent::create(eventNames().removetrackEvent, false, false, track.release()));
}

void MediaStream::scheduleDispatchEvent(PassRefPtr<Event> event)
{
    m_scheduledEvents.append(event);

    if (!m_scheduledEventTimer.isActive())
        m_scheduledEventTimer.startOneShot(0);
}

void MediaStream::scheduledEventTimerFired()
{
    Vector<RefPtr<Event>> events;
    events.swap(m_scheduledEvents);

    for (auto& event : events)
        dispatchEvent(event.release());

    events.clear();
}

URLRegistry& MediaStream::registry() const
{
    return MediaStreamRegistry::registry();
}

Vector<RefPtr<MediaStreamTrack>>* MediaStream::trackVectorForType(RealtimeMediaSource::Type type)
{
    switch (type) {
    case RealtimeMediaSource::Audio:
        return &m_audioTracks;
    case RealtimeMediaSource::Video:
        return &m_videoTracks;
    case RealtimeMediaSource::None:
        ASSERT_NOT_REACHED();
    }
    return nullptr;
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

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

#include "AudioStreamTrack.h"
#include "Event.h"
#include "ExceptionCode.h"
#include "MediaStreamCenter.h"
#include "MediaStreamRegistry.h"
#include "MediaStreamSource.h"
#include "MediaStreamTrackEvent.h"
#include "VideoStreamTrack.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

PassRefPtr<MediaStream> MediaStream::create(ScriptExecutionContext& context)
{
    return MediaStream::create(context, MediaStreamPrivate::create(Vector<RefPtr<MediaStreamSource>>(), Vector<RefPtr<MediaStreamSource>>()));
}

PassRefPtr<MediaStream> MediaStream::create(ScriptExecutionContext& context, PassRefPtr<MediaStream> stream)
{
    ASSERT(stream);

    Vector<RefPtr<MediaStreamTrackPrivate>> audioTracks;
    Vector<RefPtr<MediaStreamTrackPrivate>> videoTracks;

    for (size_t i = 0; i < stream->m_audioTracks.size(); ++i)
        audioTracks.append(&stream->m_audioTracks[i]->privateTrack());

    for (size_t i = 0; i < stream->m_videoTracks.size(); ++i)
        videoTracks.append(&stream->m_videoTracks[i]->privateTrack());

    return MediaStream::create(context, MediaStreamPrivate::create(audioTracks, videoTracks));
}

PassRefPtr<MediaStream> MediaStream::create(ScriptExecutionContext& context, const Vector<RefPtr<MediaStreamTrack>>& tracks)
{
    Vector<RefPtr<MediaStreamTrackPrivate>> audioTracks;
    Vector<RefPtr<MediaStreamTrackPrivate>> videoTracks;

    for (size_t i = 0; i < tracks.size(); ++i) {
        if (tracks[i]->kind() == "audio")
            audioTracks.append(&tracks[i]->privateTrack());
        else
            videoTracks.append(&tracks[i]->privateTrack());
    }

    return MediaStream::create(context, MediaStreamPrivate::create(audioTracks, videoTracks));
}

PassRefPtr<MediaStream> MediaStream::create(ScriptExecutionContext& context, PassRefPtr<MediaStreamPrivate> privateStream)
{
    return adoptRef(new MediaStream(context, privateStream));
}

MediaStream::MediaStream(ScriptExecutionContext& context, PassRefPtr<MediaStreamPrivate> privateStream)
    : ContextDestructionObserver(&context)
    , m_private(privateStream)
    , m_scheduledEventTimer(this, &MediaStream::scheduledEventTimerFired)
{
    ASSERT(m_private);
    m_private->setClient(this);

    RefPtr<MediaStreamTrack> track;
    size_t numberOfAudioTracks = m_private->numberOfAudioTracks();
    m_audioTracks.reserveCapacity(numberOfAudioTracks);
    for (size_t i = 0; i < numberOfAudioTracks; i++) {
        track = AudioStreamTrack::create(context, *m_private->audioTracks(i));
        track->addObserver(this);
        m_audioTracks.append(track.release());
    }

    size_t numberOfVideoTracks = m_private->numberOfVideoTracks();
    m_videoTracks.reserveCapacity(numberOfVideoTracks);
    for (size_t i = 0; i < numberOfVideoTracks; i++) {
        track = VideoStreamTrack::create(context, *m_private->videoTracks(i));
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
    for (auto it = source.begin(), end = source.end(); it != end; ++it)
        destination.append((*it)->clone());
}

void MediaStream::addTrack(PassRefPtr<MediaStreamTrack> prpTrack, ExceptionCode& ec)
{
    if (!prpTrack) {
        ec = TYPE_MISMATCH_ERR;
        return;
    }

    if (addTrack(prpTrack)) {
        for (auto observer = m_observers.begin(), end = m_observers.end(); observer != end; ++observer)
            (*observer)->didAddOrRemoveTrack();
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
        for (auto observer = m_observers.begin(), end = m_observers.end(); observer != end; ++observer)
            (*observer)->didAddOrRemoveTrack();
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

bool MediaStream::haveTrackWithSource(PassRefPtr<MediaStreamSource> source)
{
    if (source->type() == MediaStreamSource::Audio) {
        for (auto it = m_audioTracks.begin(), end = m_audioTracks.end(); it != end; ++it) {
            if ((*it)->source() == source.get())
                return true;
        }
        return false;
    }

    for (auto it = m_videoTracks.begin(), end = m_videoTracks.end(); it != end; ++it) {
        if ((*it)->source() == source.get())
            return true;
    }

    return false;
}

MediaStreamTrack* MediaStream::getTrackById(String id)
{
    for (auto it = m_audioTracks.begin(), end = m_audioTracks.end(); it != end; ++it) {
        if ((*it)->id() == id)
            return (*it).get();
    }

    for (auto it = m_videoTracks.begin(), end = m_videoTracks.end(); it != end; ++it) {
        if ((*it)->id() == id)
            return (*it).get();
    }

    return nullptr;
}

Vector<RefPtr<MediaStreamTrack>> MediaStream::getTracks()
{
    Vector<RefPtr<MediaStreamTrack>> tracks;
    for (auto it = m_audioTracks.begin(), end = m_audioTracks.end(); it != end; ++it)
        tracks.append((*it).get());
    for (auto it = m_videoTracks.begin(), end = m_videoTracks.end(); it != end; ++it)
        tracks.append((*it).get());

    return tracks;
}

void MediaStream::trackDidEnd()
{
    for (auto it = m_audioTracks.begin(), end = m_audioTracks.end(); it != end; ++it) {
        if (!(*it)->ended())
            return;
    }
    for (auto it = m_videoTracks.begin(), end = m_videoTracks.end(); it != end; ++it) {
        if (!(*it)->ended())
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

void MediaStream::addRemoteSource(MediaStreamSource* source)
{
    ASSERT(source);
    addRemoteTrack(MediaStreamTrackPrivate::create(source).get());
}

void MediaStream::removeRemoteSource(MediaStreamSource* source)
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
    case MediaStreamSource::Audio:
        track = AudioStreamTrack::create(*scriptExecutionContext(), *privateTrack);
        break;
    case MediaStreamSource::Video:
        track = VideoStreamTrack::create(*scriptExecutionContext(), *privateTrack);
        break;
    case MediaStreamSource::None:
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

void MediaStream::scheduledEventTimerFired(Timer*)
{
    Vector<RefPtr<Event>> events;
    events.swap(m_scheduledEvents);

    for (auto it = events.begin(), end = events.end(); it != end; ++it)
        dispatchEvent((*it).release());

    events.clear();
}

URLRegistry& MediaStream::registry() const
{
    return MediaStreamRegistry::registry();
}

Vector<RefPtr<MediaStreamTrack>>* MediaStream::trackVectorForType(MediaStreamSource::Type type)
{
    switch (type) {
    case MediaStreamSource::Audio:
        return &m_audioTracks;
    case MediaStreamSource::Video:
        return &m_videoTracks;
    case MediaStreamSource::None:
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

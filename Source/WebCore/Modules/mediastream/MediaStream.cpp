/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2011, 2012 Ericsson AB. All rights reserved.
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

static bool containsSource(MediaStreamSourceVector& sourceVector, MediaStreamSource* source)
{
    for (size_t i = 0; i < sourceVector.size(); ++i) {
        if (source->id() == sourceVector[i]->id())
            return true;
    }
    return false;
}

static void processTrack(MediaStreamTrack* track, MediaStreamSourceVector& sourceVector)
{
    if (track->ended())
        return;

    MediaStreamSource* source = track->source();
    if (!containsSource(sourceVector, source))
        sourceVector.append(source);
}

static PassRefPtr<MediaStream> createFromSourceVectors(ScriptExecutionContext* context, const MediaStreamSourceVector& audioSources, const MediaStreamSourceVector& videoSources, bool ended)
{
    MediaStreamDescriptor::EndedAtCreationFlag flag = ended ? MediaStreamDescriptor::IsEnded : MediaStreamDescriptor::IsNotEnded;
    RefPtr<MediaStreamDescriptor> descriptor = MediaStreamDescriptor::create(audioSources, videoSources, flag);
    return MediaStream::create(context, descriptor.release());
}

PassRefPtr<MediaStream> MediaStream::create(ScriptExecutionContext* context)
{
    MediaStreamSourceVector audioSources;
    MediaStreamSourceVector videoSources;

    return createFromSourceVectors(context, audioSources, videoSources, false);
}

PassRefPtr<MediaStream> MediaStream::create(ScriptExecutionContext* context, PassRefPtr<MediaStream> stream)
{
    ASSERT(stream);

    MediaStreamSourceVector audioSources;
    MediaStreamSourceVector videoSources;

    for (size_t i = 0; i < stream->m_audioTracks.size(); ++i)
        processTrack(stream->m_audioTracks[i].get(), audioSources);

    for (size_t i = 0; i < stream->m_videoTracks.size(); ++i)
        processTrack(stream->m_videoTracks[i].get(), videoSources);

    return createFromSourceVectors(context, audioSources, videoSources, stream->ended());
}

PassRefPtr<MediaStream> MediaStream::create(ScriptExecutionContext* context, const MediaStreamTrackVector& tracks)
{
    MediaStreamSourceVector audioSources;
    MediaStreamSourceVector videoSources;

    bool allEnded = true;
    for (size_t i = 0; i < tracks.size(); ++i) {
        allEnded &= tracks[i]->ended();
        processTrack(tracks[i].get(), tracks[i]->kind() == "audio" ? audioSources : videoSources);
    }

    return createFromSourceVectors(context, audioSources, videoSources, allEnded);
}

PassRefPtr<MediaStream> MediaStream::create(ScriptExecutionContext* context, PassRefPtr<MediaStreamDescriptor> streamDescriptor)
{
    return adoptRef(new MediaStream(context, streamDescriptor));
}

MediaStream::MediaStream(ScriptExecutionContext* context, PassRefPtr<MediaStreamDescriptor> streamDescriptor)
    : ContextDestructionObserver(context)
    , m_descriptor(streamDescriptor)
    , m_scheduledEventTimer(this, &MediaStream::scheduledEventTimerFired)
{
    ASSERT(m_descriptor);
    m_descriptor->setClient(this);

    size_t numberOfAudioTracks = m_descriptor->numberOfAudioStreams();
    m_audioTracks.reserveCapacity(numberOfAudioTracks);
    for (size_t i = 0; i < numberOfAudioTracks; i++)
        m_audioTracks.append(AudioStreamTrack::create(context, m_descriptor->audioStreams(i)));

    size_t numberOfVideoTracks = m_descriptor->numberOfVideoStreams();
    m_videoTracks.reserveCapacity(numberOfVideoTracks);
    for (size_t i = 0; i < numberOfVideoTracks; i++)
        m_videoTracks.append(VideoStreamTrack::create(context, m_descriptor->videoStreams(i)));
}

MediaStream::~MediaStream()
{
    m_descriptor->setClient(0);
}

bool MediaStream::ended() const
{
    return m_descriptor->ended();
}

void MediaStream::setEnded()
{
    if (ended())
        return;
    m_descriptor->setEnded();
}

PassRefPtr<MediaStream> MediaStream::clone()
{
    MediaStreamTrackVector trackSet;

    cloneMediaStreamTrackVector(trackSet, getAudioTracks());
    cloneMediaStreamTrackVector(trackSet, getVideoTracks());
    return MediaStream::create(scriptExecutionContext(), trackSet);
}

void MediaStream::cloneMediaStreamTrackVector(MediaStreamTrackVector& destination, const MediaStreamTrackVector& origin)
{
    for (unsigned i = 0; i < origin.size(); i++)
        destination.append(origin[i]->clone());
}

void MediaStream::addTrack(PassRefPtr<MediaStreamTrack> prpTrack, ExceptionCode& ec)
{
    if (ended()) {
        ec = INVALID_STATE_ERR;
        return;
    }

    if (!prpTrack) {
        ec = TYPE_MISMATCH_ERR;
        return;
    }

    RefPtr<MediaStreamTrack> track = prpTrack;
    if (getTrackById(track->id()))
        return;

    switch (track->source()->type()) {
    case MediaStreamSource::Audio:
        m_audioTracks.append(track);
        break;
    case MediaStreamSource::Video:
        m_videoTracks.append(track);
        break;
    }
}

void MediaStream::removeTrack(PassRefPtr<MediaStreamTrack> prpTrack, ExceptionCode& ec)
{
    if (ended()) {
        ec = INVALID_STATE_ERR;
        return;
    }

    if (!prpTrack) {
        ec = TYPE_MISMATCH_ERR;
        return;
    }

    RefPtr<MediaStreamTrack> track = prpTrack;

    size_t pos = notFound;
    switch (track->source()->type()) {
    case MediaStreamSource::Audio:
        pos = m_audioTracks.find(track);
        if (pos != notFound)
            m_audioTracks.remove(pos);
        break;
    case MediaStreamSource::Video:
        pos = m_videoTracks.find(track);
        if (pos != notFound)
            m_videoTracks.remove(pos);
        break;
    }

    if (pos == notFound)
        return;

    m_descriptor->removeSource(track->source());

    if (!m_audioTracks.size() && !m_videoTracks.size())
        setEnded();
}

MediaStreamTrack* MediaStream::getTrackById(String id)
{
    for (MediaStreamTrackVector::iterator iter = m_audioTracks.begin(); iter != m_audioTracks.end(); ++iter) {
        if ((*iter)->id() == id)
            return (*iter).get();
    }

    for (MediaStreamTrackVector::iterator iter = m_videoTracks.begin(); iter != m_videoTracks.end(); ++iter) {
        if ((*iter)->id() == id)
            return (*iter).get();
    }

    return 0;
}

void MediaStream::trackDidEnd()
{
    for (size_t i = 0; i < m_audioTracks.size(); ++i)
        if (!m_audioTracks[i]->ended())
            return;
    
    for (size_t i = 0; i < m_videoTracks.size(); ++i)
        if (!m_videoTracks[i]->ended())
            return;
    
    setEnded();
}

void MediaStream::streamDidEnd()
{
    if (ended())
        return;

    scheduleDispatchEvent(Event::create(eventNames().endedEvent, false, false));
}

void MediaStream::contextDestroyed()
{
    ContextDestructionObserver::contextDestroyed();
}

void MediaStream::addRemoteSource(MediaStreamSource* source)
{
    ASSERT(source);
    if (ended())
        return;

    source->setStream(descriptor());

    RefPtr<MediaStreamTrack> track;
    switch (source->type()) {
    case MediaStreamSource::Audio:
        track = AudioStreamTrack::create(scriptExecutionContext(), source);
        m_audioTracks.append(track);
        break;
    case MediaStreamSource::Video:
        track = VideoStreamTrack::create(scriptExecutionContext(), source);
        m_videoTracks.append(track);
        break;
    }
    m_descriptor->addSource(source);

    scheduleDispatchEvent(MediaStreamTrackEvent::create(eventNames().addtrackEvent, false, false, track));
}

void MediaStream::removeRemoteSource(MediaStreamSource* source)
{
    if (ended())
        return;

    MediaStreamTrackVector* tracks = 0;
    switch (source->type()) {
    case MediaStreamSource::Audio:
        tracks = &m_audioTracks;
        break;
    case MediaStreamSource::Video:
        tracks = &m_videoTracks;
        break;
    }

    size_t index = notFound;
    for (size_t i = 0; i < tracks->size(); ++i) {
        if ((*tracks)[i]->source() == source) {
            index = i;
            break;
        }
    }
    if (index == notFound)
        return;

    m_descriptor->removeSource(source);

    RefPtr<MediaStreamTrack> track = (*tracks)[index];
    tracks->remove(index);
    scheduleDispatchEvent(MediaStreamTrackEvent::create(eventNames().removetrackEvent, false, false, track));
}

void MediaStream::scheduleDispatchEvent(PassRefPtr<Event> event)
{
    m_scheduledEvents.append(event);

    if (!m_scheduledEventTimer.isActive())
        m_scheduledEventTimer.startOneShot(0);
}

void MediaStream::scheduledEventTimerFired(Timer<MediaStream>*)
{
    Vector<RefPtr<Event> > events;
    events.swap(m_scheduledEvents);

    Vector<RefPtr<Event> >::iterator it = events.begin();
    for (; it != events.end(); ++it)
        dispatchEvent((*it).release());

    events.clear();
}

URLRegistry& MediaStream::registry() const
{
    return MediaStreamRegistry::registry();
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

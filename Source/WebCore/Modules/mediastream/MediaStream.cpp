/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2011, 2012 Ericsson AB. All rights reserved.
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
#include "MediaStreamCenter.h"
#include "MediaStreamSource.h"
#include "UUID.h"

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
    if (track->readyState() == MediaStreamTrack::ENDED)
        return;

    MediaStreamSource* source = track->component()->source();
    if (!containsSource(sourceVector, source))
        sourceVector.append(source);
}

static PassRefPtr<MediaStream> createFromSourceVectors(ScriptExecutionContext* context, const MediaStreamSourceVector& audioSources, const MediaStreamSourceVector& videoSources)
{
    RefPtr<MediaStreamDescriptor> descriptor = MediaStreamDescriptor::create(createCanonicalUUIDString(), audioSources, videoSources);
    MediaStreamCenter::instance().didCreateMediaStream(descriptor.get());

    return MediaStream::create(context, descriptor.release());
}

PassRefPtr<MediaStream> MediaStream::create(ScriptExecutionContext* context)
{
    MediaStreamSourceVector audioSources;
    MediaStreamSourceVector videoSources;

    return createFromSourceVectors(context, audioSources, videoSources);
}

PassRefPtr<MediaStream> MediaStream::create(ScriptExecutionContext* context, PassRefPtr<MediaStream> stream)
{
    ASSERT(stream);

    MediaStreamSourceVector audioSources;
    MediaStreamSourceVector videoSources;

    for (size_t i = 0; i < stream->audioTracks()->length(); ++i)
        processTrack(stream->audioTracks()->item(i), audioSources);

    for (size_t i = 0; i < stream->videoTracks()->length(); ++i)
        processTrack(stream->videoTracks()->item(i), videoSources);

    return createFromSourceVectors(context, audioSources, videoSources);
}

PassRefPtr<MediaStream> MediaStream::create(ScriptExecutionContext* context, const MediaStreamTrackVector& tracks)
{
    MediaStreamSourceVector audioSources;
    MediaStreamSourceVector videoSources;

    for (size_t i = 0; i < tracks.size(); ++i)
        processTrack(tracks[i].get(), tracks[i]->kind() == "audio" ? audioSources : videoSources);

    return createFromSourceVectors(context, audioSources, videoSources);
}

PassRefPtr<MediaStream> MediaStream::create(ScriptExecutionContext* context, PassRefPtr<MediaStreamDescriptor> streamDescriptor)
{
    return adoptRef(new MediaStream(context, streamDescriptor));
}

MediaStream::MediaStream(ScriptExecutionContext* context, PassRefPtr<MediaStreamDescriptor> streamDescriptor)
    : ContextDestructionObserver(context)
    , m_descriptor(streamDescriptor)
{
    m_descriptor->setClient(this);

    MediaStreamTrackVector audioTrackVector;
    size_t numberOfAudioTracks = m_descriptor->numberOfAudioComponents();
    audioTrackVector.reserveCapacity(numberOfAudioTracks);
    for (size_t i = 0; i < numberOfAudioTracks; i++)
        audioTrackVector.append(MediaStreamTrack::create(context, m_descriptor, m_descriptor->audioComponent(i)));
    m_audioTracks = MediaStreamTrackList::create(this, audioTrackVector);

    MediaStreamTrackVector videoTrackVector;
    size_t numberOfVideoTracks = m_descriptor->numberOfVideoComponents();
    videoTrackVector.reserveCapacity(numberOfVideoTracks);
    for (size_t i = 0; i < numberOfVideoTracks; i++)
        videoTrackVector.append(MediaStreamTrack::create(context, m_descriptor, m_descriptor->videoComponent(i)));
    m_videoTracks = MediaStreamTrackList::create(this, videoTrackVector);
}

MediaStream::~MediaStream()
{
    m_descriptor->setClient(0);
    m_audioTracks->detachOwner();
    m_videoTracks->detachOwner();
}

bool MediaStream::ended() const
{
    return m_descriptor->ended();
}

void MediaStream::streamEnded()
{
    if (ended())
        return;

    m_descriptor->setEnded();
    m_audioTracks->detachOwner();
    m_videoTracks->detachOwner();

    dispatchEvent(Event::create(eventNames().endedEvent, false, false));
}

const AtomicString& MediaStream::interfaceName() const
{
    return eventNames().interfaceForMediaStream;
}

ScriptExecutionContext* MediaStream::scriptExecutionContext() const
{
    return ContextDestructionObserver::scriptExecutionContext();
}

EventTargetData* MediaStream::eventTargetData()
{
    return &m_eventTargetData;
}

EventTargetData* MediaStream::ensureEventTargetData()
{
    return &m_eventTargetData;
}

void MediaStream::addTrack(MediaStreamComponent* component)
{
    RefPtr<MediaStreamTrack> track = MediaStreamTrack::create(scriptExecutionContext(), m_descriptor, component);
    ExceptionCode ec = 0;
    switch (component->source()->type()) {
    case MediaStreamSource::TypeAudio:
        m_audioTracks->add(track, ec);
        break;
    case MediaStreamSource::TypeVideo:
        m_videoTracks->add(track, ec);
        break;
    }
    ASSERT(!ec);
}

void MediaStream::removeTrack(MediaStreamComponent* component)
{
    switch (component->source()->type()) {
    case MediaStreamSource::TypeAudio:
        m_audioTracks->remove(component);
        break;
    case MediaStreamSource::TypeVideo:
        m_videoTracks->remove(component);
        break;
    }
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

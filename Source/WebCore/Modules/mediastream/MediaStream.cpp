/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2011 Ericsson AB. All rights reserved.
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
#include "ScriptExecutionContext.h"
#include "UUID.h"

namespace WebCore {

static void processTrackList(PassRefPtr<MediaStreamTrackList> prpTracks, const String& kind, MediaStreamSourceVector& sources, ExceptionCode& ec)
{
    RefPtr<MediaStreamTrackList> tracks = prpTracks;
    if (!tracks)
        return;

    for (unsigned i = 0; i < tracks->length(); ++i) {
        MediaStreamTrack* track = tracks->item(i);
        if (track->kind() != kind) {
            ec = SYNTAX_ERR;
            return;
        }
        MediaStreamSource* source = track->component()->source();
        bool isDuplicate = false;
        for (MediaStreamSourceVector::iterator j = sources.begin(); j < sources.end(); ++j) {
            if ((*j)->id() == source->id()) {
                isDuplicate = true;
                break;
            }
        }
        if (!isDuplicate)
            sources.append(source);
    }
}

PassRefPtr<MediaStream> MediaStream::create(ScriptExecutionContext* context, PassRefPtr<MediaStreamTrackList> audioTracks, PassRefPtr<MediaStreamTrackList> videoTracks, ExceptionCode& ec)
{
    MediaStreamSourceVector audioSources;
    processTrackList(audioTracks, "audio", audioSources, ec);
    if (ec)
        return 0;

    MediaStreamSourceVector videoSources;
    processTrackList(videoTracks, "video", videoSources, ec);
    if (ec)
        return 0;

    RefPtr<MediaStreamDescriptor> descriptor = MediaStreamDescriptor::create(createCanonicalUUIDString(), audioSources, videoSources);
    MediaStreamCenter::instance().didConstructMediaStream(descriptor.get());
    return adoptRef(new MediaStream(context, descriptor.release()));
}

PassRefPtr<MediaStream> MediaStream::create(ScriptExecutionContext* context, PassRefPtr<MediaStreamDescriptor> streamDescriptor)
{
    return adoptRef(new MediaStream(context, streamDescriptor));
}

MediaStream::MediaStream(ScriptExecutionContext* context, PassRefPtr<MediaStreamDescriptor> streamDescriptor)
    : m_scriptExecutionContext(context)
    , m_descriptor(streamDescriptor)
{
    m_descriptor->setOwner(this);

    MediaStreamTrackVector audioTrackVector;
    size_t numberOfAudioTracks = m_descriptor->numberOfAudioComponents();
    audioTrackVector.reserveCapacity(numberOfAudioTracks);
    for (size_t i = 0; i < numberOfAudioTracks; i++)
        audioTrackVector.append(MediaStreamTrack::create(m_descriptor, m_descriptor->audioComponent(i)));
    m_audioTracks = MediaStreamTrackList::create(audioTrackVector);

    MediaStreamTrackVector videoTrackVector;
    size_t numberOfVideoTracks = m_descriptor->numberOfVideoComponents();
    videoTrackVector.reserveCapacity(numberOfVideoTracks);
    for (size_t i = 0; i < numberOfVideoTracks; i++)
        videoTrackVector.append(MediaStreamTrack::create(m_descriptor, m_descriptor->videoComponent(i)));
    m_videoTracks = MediaStreamTrackList::create(videoTrackVector);
}

MediaStream::~MediaStream()
{
    m_descriptor->setOwner(0);
}

MediaStream::ReadyState MediaStream::readyState() const
{
    return m_descriptor->ended() ? ENDED : LIVE;
}

void MediaStream::streamEnded()
{
    if (readyState() == ENDED)
        return;

    m_descriptor->setEnded();

    dispatchEvent(Event::create(eventNames().endedEvent, false, false));
}

const AtomicString& MediaStream::interfaceName() const
{
    return eventNames().interfaceForMediaStream;
}

ScriptExecutionContext* MediaStream::scriptExecutionContext() const
{
    return m_scriptExecutionContext.get();
}

EventTargetData* MediaStream::eventTargetData()
{
    return &m_eventTargetData;
}

EventTargetData* MediaStream::ensureEventTargetData()
{
    return &m_eventTargetData;
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

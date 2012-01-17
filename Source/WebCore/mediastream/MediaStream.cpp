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
#include "MediaStreamCenter.h"
#include "ScriptExecutionContext.h"
#include "UUID.h"

namespace WebCore {

PassRefPtr<MediaStream> MediaStream::create(ScriptExecutionContext* context, PassRefPtr<MediaStreamTrackList> prpTracks)
{
    RefPtr<MediaStreamTrackList> tracks = prpTracks;
    MediaStreamSourceVector sources;
    for (unsigned i = 0; i < tracks->length(); ++i)
        sources.append(tracks->item(i)->component()->source());

    RefPtr<MediaStreamDescriptor> descriptor = MediaStreamDescriptor::create(createCanonicalUUIDString(), sources);
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

    MediaStreamTrackVector trackVector;
    size_t numberOfTracks = m_descriptor->numberOfComponents();

    trackVector.reserveCapacity(numberOfTracks);
    for (size_t i = 0; i < numberOfTracks; i++)
        trackVector.append(MediaStreamTrack::create(m_descriptor, i));

    m_tracks = MediaStreamTrackList::create(trackVector);
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

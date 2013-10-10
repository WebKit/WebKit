/*
 * Copyright (C) 2011 Ericsson AB. All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(MEDIA_STREAM)

#include "MediaStreamDescriptor.h"

#include "MediaStreamCenter.h"
#include "MediaStreamSource.h"
#include "UUID.h"
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>

namespace WebCore {

PassRefPtr<MediaStreamDescriptor> MediaStreamDescriptor::create(const MediaStreamSourceVector& audioSources, const MediaStreamSourceVector& videoSources, EndedAtCreationFlag flag)
{
    return adoptRef(new MediaStreamDescriptor(createCanonicalUUIDString(), audioSources, videoSources, flag == IsEnded));
}

MediaStreamDescriptor::~MediaStreamDescriptor()
{
    for (size_t i = 0; i < m_audioStreamSources.size(); i++)
        m_audioStreamSources[i]->setStream(0);
    
    for (size_t i = 0; i < m_videoStreamSources.size(); i++)
        m_videoStreamSources[i]->setStream(0);
}

void MediaStreamDescriptor::addSource(PassRefPtr<MediaStreamSource> source)
{
    switch (source->type()) {
    case MediaStreamSource::Audio:
        if (m_audioStreamSources.find(source) == notFound)
            m_audioStreamSources.append(source);
        break;
    case MediaStreamSource::Video:
        if (m_videoStreamSources.find(source) == notFound)
            m_videoStreamSources.append(source);
        break;
    }
}

void MediaStreamDescriptor::removeSource(PassRefPtr<MediaStreamSource> source)
{
    size_t pos = notFound;
    switch (source->type()) {
    case MediaStreamSource::Audio:
        pos = m_audioStreamSources.find(source);
        if (pos == notFound)
            return;
        m_audioStreamSources.remove(pos);
        break;
    case MediaStreamSource::Video:
        pos = m_videoStreamSources.find(source);
        if (pos == notFound)
            return;
        m_videoStreamSources.remove(pos);
        break;
    }

    source->setStream(0);
}

void MediaStreamDescriptor::addRemoteSource(MediaStreamSource* source)
{
    if (m_client)
        m_client->addRemoteSource(source);
    else
        addSource(source);
}

void MediaStreamDescriptor::removeRemoteSource(MediaStreamSource* source)
{
    if (m_client)
        m_client->removeRemoteSource(source);
    else
        removeSource(source);
}

MediaStreamDescriptor::MediaStreamDescriptor(const String& id, const MediaStreamSourceVector& audioSources, const MediaStreamSourceVector& videoSources, bool ended)
    : m_client(0)
    , m_id(id)
    , m_ended(ended)
{
    ASSERT(m_id.length());
    for (size_t i = 0; i < audioSources.size(); i++) {
        audioSources[i]->setStream(this);
        m_audioStreamSources.append(audioSources[i]);
    }

    for (size_t i = 0; i < videoSources.size(); i++) {
        videoSources[i]->setStream(this);
        m_videoStreamSources.append(videoSources[i]);
    }
}

void MediaStreamDescriptor::setEnded()
{
    if (m_client)
        m_client->streamDidEnd();
    m_ended = true;
    for (size_t i = 0; i < m_audioStreamSources.size(); i++)
        m_audioStreamSources[i]->setReadyState(MediaStreamSource::Ended);
    for (size_t i = 0; i < m_videoStreamSources.size(); i++)
        m_videoStreamSources[i]->setReadyState(MediaStreamSource::Ended);
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

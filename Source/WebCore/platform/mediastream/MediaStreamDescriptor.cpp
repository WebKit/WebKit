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

#include "MediaStreamSource.h"
#include "UUID.h"
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>

namespace WebCore {

PassRefPtr<MediaStreamDescriptor> MediaStreamDescriptor::create(const MediaStreamSourceVector& audioSources, const MediaStreamSourceVector& videoSources)
{
    return adoptRef(new MediaStreamDescriptor(createCanonicalUUIDString(), audioSources, videoSources));
}

PassRefPtr<MediaStreamDescriptor> MediaStreamDescriptor::create(const MediaStreamComponentVector& audioComponents, const MediaStreamComponentVector& videoComponents)
{
    return adoptRef(new MediaStreamDescriptor(createCanonicalUUIDString(), audioComponents, videoComponents));
}

void MediaStreamDescriptor::addComponent(PassRefPtr<MediaStreamComponent> component)
{
    switch (component->source()->type()) {
    case MediaStreamSource::TypeAudio:
        if (m_audioComponents.find(component) == notFound)
            m_audioComponents.append(component);
        break;
    case MediaStreamSource::TypeVideo:
        if (m_videoComponents.find(component) == notFound)
            m_videoComponents.append(component);
        break;
    }
}

void MediaStreamDescriptor::removeComponent(PassRefPtr<MediaStreamComponent> component)
{
    size_t pos = notFound;
    switch (component->source()->type()) {
    case MediaStreamSource::TypeAudio:
        pos = m_audioComponents.find(component);
        if (pos != notFound)
            m_audioComponents.remove(pos);
        break;
    case MediaStreamSource::TypeVideo:
        pos = m_videoComponents.find(component);
        if (pos != notFound)
            m_videoComponents.remove(pos);
        break;
    }
}

void MediaStreamDescriptor::addRemoteTrack(MediaStreamComponent* component)
{
    if (m_client)
        m_client->addRemoteTrack(component);
    else
        addComponent(component);
}

void MediaStreamDescriptor::removeRemoteTrack(MediaStreamComponent* component)
{
    if (m_client)
        m_client->removeRemoteTrack(component);
    else
        removeComponent(component);
}

MediaStreamDescriptor::MediaStreamDescriptor(const String& id, const MediaStreamSourceVector& audioSources, const MediaStreamSourceVector& videoSources)
    : m_client(0)
    , m_id(id)
    , m_ended(false)
{
    ASSERT(m_id.length());
    for (size_t i = 0; i < audioSources.size(); i++)
        m_audioComponents.append(MediaStreamComponent::create(this, audioSources[i]));

    for (size_t i = 0; i < videoSources.size(); i++)
        m_videoComponents.append(MediaStreamComponent::create(this, videoSources[i]));
}

MediaStreamDescriptor::MediaStreamDescriptor(const String& id, const MediaStreamComponentVector& audioComponents, const MediaStreamComponentVector& videoComponents)
    : m_client(0)
    , m_id(id)
    , m_ended(false)
{
    ASSERT(m_id.length());
    for (MediaStreamComponentVector::const_iterator iter = audioComponents.begin(); iter != audioComponents.end(); ++iter) {
        (*iter)->setStream(this);
        m_audioComponents.append((*iter));
    }
    for (MediaStreamComponentVector::const_iterator iter = videoComponents.begin(); iter != videoComponents.end(); ++iter) {
        (*iter)->setStream(this);
        m_videoComponents.append((*iter));
    }
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

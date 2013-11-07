/*
 * Copyright (C) 2011 Ericsson AB. All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
 * Copyright (C) 2013 Nokia Corporation and/or its subsidiary(-ies).
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

#include "MediaStreamPrivate.h"

#include "MediaStreamCenter.h"
#include "UUID.h"
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>

namespace WebCore {

PassRefPtr<MediaStreamPrivate> MediaStreamPrivate::create(const Vector<RefPtr<MediaStreamSource>>& audioSources, const Vector<RefPtr<MediaStreamSource>>& videoSources)
{
    return adoptRef(new MediaStreamPrivate(createCanonicalUUIDString(), audioSources, videoSources));
}

PassRefPtr<MediaStreamPrivate> MediaStreamPrivate::create(const Vector<RefPtr<MediaStreamTrackPrivate>>& audioPrivateTracks, const Vector<RefPtr<MediaStreamTrackPrivate>>& videoPrivateTracks)
{
    return adoptRef(new MediaStreamPrivate(createCanonicalUUIDString(), audioPrivateTracks, videoPrivateTracks));
}

void MediaStreamPrivate::addSource(PassRefPtr<MediaStreamSource> prpSource)
{
    RefPtr<MediaStreamSource> source = prpSource;
    switch (source->type()) {
    case MediaStreamSource::Audio:
        if (m_audioStreamSources.find(source) == notFound)
            m_audioStreamSources.append(source);
        break;
    case MediaStreamSource::Video:
        if (m_videoStreamSources.find(source) == notFound)
            m_videoStreamSources.append(source);
        break;
    case MediaStreamSource::None:
        ASSERT_NOT_REACHED();
        break;
    }
}

void MediaStreamPrivate::removeSource(PassRefPtr<MediaStreamSource> source)
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
    case MediaStreamSource::None:
        ASSERT_NOT_REACHED();
        break;
    }
}

void MediaStreamPrivate::addRemoteSource(MediaStreamSource* source)
{
    if (m_client)
        m_client->addRemoteSource(source);
    else
        addSource(source);
}

void MediaStreamPrivate::removeRemoteSource(MediaStreamSource* source)
{
    if (m_client)
        m_client->removeRemoteSource(source);
    else
        removeSource(source);
}

void MediaStreamPrivate::addRemoteTrack(MediaStreamTrackPrivate* track)
{
    if (m_client)
        m_client->addRemoteTrack(track);
    else
        addTrack(track);
}

void MediaStreamPrivate::removeRemoteTrack(MediaStreamTrackPrivate* track)
{
    if (m_client)
        m_client->removeRemoteTrack(track);
    else
        removeTrack(track);
}

MediaStreamPrivate::MediaStreamPrivate(const String& id, const Vector<RefPtr<MediaStreamSource>>& audioSources, const Vector<RefPtr<MediaStreamSource>>& videoSources)
    : m_client(0)
    , m_id(id)
    , m_ended(false)
{
    ASSERT(m_id.length());
    for (size_t i = 0; i < audioSources.size(); i++)
        addTrack(MediaStreamTrackPrivate::create(audioSources[i]));

    for (size_t i = 0; i < videoSources.size(); i++)
        addTrack(MediaStreamTrackPrivate::create(videoSources[i]));

    unsigned providedSourcesSize = audioSources.size() + videoSources.size();
    unsigned tracksSize = m_audioPrivateTracks.size() + m_videoPrivateTracks.size();
    // If sources were provided and no track was added to the MediaStreamPrivate's tracks, this means
    // that the tracks were all ended
    if (providedSourcesSize > 0 && !tracksSize)
        m_ended = true;
}

MediaStreamPrivate::MediaStreamPrivate(const String& id, const Vector<RefPtr<MediaStreamTrackPrivate>>& audioPrivateTracks, const Vector<RefPtr<MediaStreamTrackPrivate>>& videoPrivateTracks)
    : m_client(0)
    , m_id(id)
    , m_ended(false)
{
    ASSERT(m_id.length());
    for (size_t i = 0; i < audioPrivateTracks.size(); i++)
        addTrack(audioPrivateTracks[i]);

    for (size_t i = 0; i < videoPrivateTracks.size(); i++)
        addTrack(videoPrivateTracks[i]);

    unsigned providedTracksSize = audioPrivateTracks.size() + videoPrivateTracks.size();
    unsigned tracksSize = m_audioPrivateTracks.size() + m_videoPrivateTracks.size();
    // If tracks were provided and no one was added to the MediaStreamPrivate's tracks, this means
    // that the tracks were all ended
    if (providedTracksSize > 0 && !tracksSize)
        m_ended = true;
}

void MediaStreamPrivate::setEnded()
{
    if (m_client)
        m_client->streamDidEnd();

    m_ended = true;
}

void MediaStreamPrivate::addTrack(PassRefPtr<MediaStreamTrackPrivate> prpTrack)
{
    RefPtr<MediaStreamTrackPrivate> track = prpTrack;
    if (track->ended())
        return;

    Vector<RefPtr<MediaStreamTrackPrivate>>& tracks = track->type() == MediaStreamSource::Audio ? m_audioPrivateTracks : m_videoPrivateTracks;

    size_t pos = tracks.find(track);
    if (pos != notFound)
        return;

    tracks.append(track);
    if (track->source())
        addSource(track->source());
}

void MediaStreamPrivate::removeTrack(PassRefPtr<MediaStreamTrackPrivate> track)
{
    Vector<RefPtr<MediaStreamTrackPrivate>>& tracks = track->type() == MediaStreamSource::Audio ? m_audioPrivateTracks : m_videoPrivateTracks;

    size_t pos = tracks.find(track);
    if (pos == notFound)
        return;

    tracks.remove(pos);
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

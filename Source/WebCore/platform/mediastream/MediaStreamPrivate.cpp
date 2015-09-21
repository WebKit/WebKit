/*
 * Copyright (C) 2011, 2015 Ericsson AB. All rights reserved.
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

#include "UUID.h"
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>

namespace WebCore {

RefPtr<MediaStreamPrivate> MediaStreamPrivate::create(const Vector<RefPtr<RealtimeMediaSource>>& audioSources, const Vector<RefPtr<RealtimeMediaSource>>& videoSources)
{
    MediaStreamTrackPrivateVector tracks;
    tracks.reserveCapacity(audioSources.size() + videoSources.size());

    for (auto source : audioSources)
        tracks.append(MediaStreamTrackPrivate::create(WTF::move(source)));

    for (auto source : videoSources)
        tracks.append(MediaStreamTrackPrivate::create(WTF::move(source)));

    return MediaStreamPrivate::create(tracks);
}

RefPtr<MediaStreamPrivate> MediaStreamPrivate::create(const MediaStreamTrackPrivateVector& tracks)
{
    return adoptRef(new MediaStreamPrivate(createCanonicalUUIDString(), tracks));
}

MediaStreamPrivate::MediaStreamPrivate(const String& id, const MediaStreamTrackPrivateVector& tracks)
    : m_id(id)
{
    ASSERT(!m_id.isEmpty());

    for (auto& track : tracks)
        m_trackSet.add(track->id(), track);

    updateActiveState(NotifyClientOption::DontNotify);
}

MediaStreamPrivate::MediaStreamPrivate(MediaStreamPrivateClient& client)
    : m_client(&client)
{
}

MediaStreamPrivate::~MediaStreamPrivate()
{
    m_client = nullptr;
    m_isActive = false;
}

MediaStreamTrackPrivateVector MediaStreamPrivate::tracks() const
{
    MediaStreamTrackPrivateVector tracks;
    tracks.reserveCapacity(m_trackSet.size());
    copyValuesToVector(m_trackSet, tracks);

    return tracks;
}

void MediaStreamPrivate::updateActiveState(NotifyClientOption notifyClientOption)
{
    // A stream is active if it has at least one un-ended track.
    bool newActiveState = false;
    for (auto& track : m_trackSet.values()) {
        if (!track->ended()) {
            newActiveState = true;
            break;
        }
    }

    if (newActiveState == m_isActive)
        return;
    m_isActive = newActiveState;

    if (m_client && notifyClientOption == NotifyClientOption::Notify)
        m_client->activeStatusChanged();
}

void MediaStreamPrivate::addTrack(RefPtr<MediaStreamTrackPrivate>&& track, NotifyClientOption notifyClientOption)
{
    if (m_trackSet.contains(track->id()))
        return;

    track->addObserver(*this);
    m_trackSet.add(track->id(), track);

    if (m_client && notifyClientOption == NotifyClientOption::Notify)
        m_client->didAddTrack(*track.get());

    updateActiveState(notifyClientOption);
}

void MediaStreamPrivate::removeTrack(MediaStreamTrackPrivate& track, NotifyClientOption notifyClientOption)
{
    if (!m_trackSet.remove(track.id()))
        return;

    track.removeObserver(*this);

    if (m_client && notifyClientOption == NotifyClientOption::Notify)
        m_client->didRemoveTrack(track);

    updateActiveState(NotifyClientOption::Notify);
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

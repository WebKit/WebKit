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
    Vector<RefPtr<MediaStreamTrackPrivate>> tracks;
    tracks.reserveCapacity(audioSources.size() + videoSources.size());

    for (auto source : audioSources)
        tracks.append(MediaStreamTrackPrivate::create(WTF::move(source)));

    for (auto source : videoSources)
        tracks.append(MediaStreamTrackPrivate::create(WTF::move(source)));

    return MediaStreamPrivate::create(tracks);
}

RefPtr<MediaStreamPrivate> MediaStreamPrivate::create(const Vector<RefPtr<MediaStreamTrackPrivate>>& tracks)
{
    return adoptRef(new MediaStreamPrivate(createCanonicalUUIDString(), tracks));
}

RefPtr<MediaStreamPrivate> MediaStreamPrivate::create()
{
    return MediaStreamPrivate::create(Vector<RefPtr<MediaStreamTrackPrivate>>());
}

MediaStreamPrivate::MediaStreamPrivate(const String& id, const Vector<RefPtr<MediaStreamTrackPrivate>>& tracks)
    : m_client(0)
    , m_id(id)
    , m_isActive(false)
{
    ASSERT(m_id.length());

    for (auto& track : tracks)
        m_trackSet.add(track->id(), track);

    updateActiveState(NotifyClientOption::DontNotify);
}

Vector<RefPtr<MediaStreamTrackPrivate>> MediaStreamPrivate::tracks() const
{
    Vector<RefPtr<MediaStreamTrackPrivate>> tracks;
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

    m_trackSet.add(track->id(), track);

    if (m_client && notifyClientOption == NotifyClientOption::Notify)
        m_client->didAddTrackToPrivate(*track.get());

    updateActiveState(NotifyClientOption::Notify);
}

void MediaStreamPrivate::removeTrack(MediaStreamTrackPrivate& track, NotifyClientOption notifyClientOption)
{
    if (!m_trackSet.remove(track.id()))
        return;

    if (m_client && notifyClientOption == NotifyClientOption::Notify)
        m_client->didRemoveTrackFromPrivate(track);

    updateActiveState(NotifyClientOption::Notify);
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

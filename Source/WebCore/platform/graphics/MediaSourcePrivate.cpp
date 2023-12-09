/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "MediaSourcePrivate.h"

#if ENABLE(MEDIA_SOURCE)

#include "MediaSourcePrivateClient.h"
#include "PlatformTimeRanges.h"
#include "SourceBufferPrivate.h"

namespace WebCore {

bool MediaSourcePrivate::hasFutureTime(const MediaTime& currentTime) const
{
    if (currentTime >= duration())
        return false;

    auto& ranges = buffered();
    MediaTime nearest = ranges.nearest(currentTime);
    if (abs(nearest - currentTime) > timeFudgeFactor())
        return false;

    size_t found = ranges.find(nearest);
    if (found == notFound)
        return false;

    MediaTime localEnd = ranges.end(found);
    if (localEnd == duration())
        return true;

    return localEnd - currentTime > timeFudgeFactor();
}

MediaSourcePrivate::MediaSourcePrivate(MediaSourcePrivateClient& client)
    : m_client(client)
{
}

MediaSourcePrivate::~MediaSourcePrivate() = default;

RefPtr<MediaSourcePrivateClient> MediaSourcePrivate::client() const
{
    return m_client.get();
}

const MediaTime& MediaSourcePrivate::duration() const
{
    return m_duration;
}

Ref<MediaTimePromise> MediaSourcePrivate::waitForTarget(const SeekTarget& target)
{
    if (RefPtr client = this->client())
        return client->waitForTarget(target);
    return MediaTimePromise::createAndReject(PlatformMediaError::ClientDisconnected);
}

Ref<MediaPromise> MediaSourcePrivate::seekToTime(const MediaTime& time)
{
    if (RefPtr client = this->client())
        return client->seekToTime(time);
    return MediaPromise::createAndReject(PlatformMediaError::ClientDisconnected);
}

void MediaSourcePrivate::removeSourceBuffer(SourceBufferPrivate& sourceBuffer)
{
    ASSERT(m_sourceBuffers.contains(&sourceBuffer));

    size_t pos = m_activeSourceBuffers.find(&sourceBuffer);
    if (pos != notFound) {
        m_activeSourceBuffers.remove(pos);
        notifyActiveSourceBuffersChanged();
    }
    m_sourceBuffers.removeFirst(&sourceBuffer);
}

void MediaSourcePrivate::sourceBufferPrivateDidChangeActiveState(SourceBufferPrivate& sourceBuffer, bool active)
{
    size_t position = m_activeSourceBuffers.find(&sourceBuffer);
    if (active && position == notFound) {
        m_activeSourceBuffers.append(&sourceBuffer);
        notifyActiveSourceBuffersChanged();
        return;
    }

    if (active || position == notFound)
        return;

    m_activeSourceBuffers.remove(position);
    notifyActiveSourceBuffersChanged();
}

bool MediaSourcePrivate::hasAudio() const
{
    return std::any_of(m_activeSourceBuffers.begin(), m_activeSourceBuffers.end(), [] (SourceBufferPrivate* sourceBuffer) {
        return sourceBuffer->hasAudio();
    });
}

bool MediaSourcePrivate::hasVideo() const
{
    return std::any_of(m_activeSourceBuffers.begin(), m_activeSourceBuffers.end(), [] (SourceBufferPrivate* sourceBuffer) {
        return sourceBuffer->hasVideo();
    });
}

void MediaSourcePrivate::durationChanged(const MediaTime& duration)
{
    m_duration = duration;
    for (auto& sourceBuffer : m_sourceBuffers)
        sourceBuffer->setMediaSourceDuration(duration);
}

void MediaSourcePrivate::bufferedChanged(const PlatformTimeRanges& buffered)
{
    m_buffered = buffered;
}

const PlatformTimeRanges& MediaSourcePrivate::buffered() const
{
    return m_buffered;
}

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
void MediaSourcePrivate::setCDMSession(LegacyCDMSession* session)
{
    for (auto& sourceBuffer : m_sourceBuffers)
        sourceBuffer->setCDMSession(session);
}
#endif

} // namespace WebCore

#endif

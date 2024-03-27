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

#include "MediaPlayerPrivate.h"
#include "MediaSource.h"
#include "MediaSourcePrivateClient.h"
#include "PlatformTimeRanges.h"
#include "SourceBufferPrivate.h"

namespace WebCore {

bool MediaSourcePrivate::hasFutureTime(const MediaTime& currentTime) const
{
    return hasFutureTime(currentTime, futureDataThreshold());
}

bool MediaSourcePrivate::hasFutureTime(const MediaTime& currentTime, const MediaTime& threshold) const
{
    if (currentTime >= duration())
        return false;

    auto ranges = buffered();
    MediaTime nearest = ranges.nearest(currentTime);
    if (abs(nearest - currentTime) > timeFudgeFactor())
        return false;

    size_t found = ranges.find(nearest);
    if (found == notFound)
        return false;

    MediaTime localEnd = ranges.end(found);

    if (localEnd == duration())
        return true;

    // https://html.spec.whatwg.org/multipage/media.html#dom-media-have_future_data
    // "Data for the immediate current playback position is available, as well as enough data
    // for the user agent to advance the current playback position in the direction of playback
    // at least a little without immediately reverting to the HAVE_METADATA state."
    // So we check if currentTime could progress further from its current value by at least one
    // video frame if paused, or if currentTime could go still progress.
    return localEnd - currentTime > threshold;
}

MediaSourcePrivate::MediaSourcePrivate(MediaSourcePrivateClient& client)
    : MediaSourcePrivate(client, RunLoop::current())
{
}

MediaSourcePrivate::MediaSourcePrivate(MediaSourcePrivateClient& client, RefCountedSerialFunctionDispatcher& dispatcher)
    : m_readyState(MediaSourceReadyState::Closed)
    , m_dispatcher(dispatcher)
    , m_client(client)
{
}

MediaSourcePrivate::~MediaSourcePrivate() = default;

RefPtr<MediaSourcePrivateClient> MediaSourcePrivate::client() const
{
    return m_client.get();
}

MediaTime MediaSourcePrivate::duration() const
{
    Locker locker { m_lock };

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
    assertIsCurrent(m_dispatcher);

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
    assertIsCurrent(m_dispatcher);

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
    assertIsCurrent(m_dispatcher);

    return std::any_of(m_activeSourceBuffers.begin(), m_activeSourceBuffers.end(), [] (SourceBufferPrivate* sourceBuffer) {
        return sourceBuffer->hasAudio();
    });
}

bool MediaSourcePrivate::hasVideo() const
{
    assertIsCurrent(m_dispatcher);

    return std::any_of(m_activeSourceBuffers.begin(), m_activeSourceBuffers.end(), [] (SourceBufferPrivate* sourceBuffer) {
        return sourceBuffer->hasVideo();
    });
}

void MediaSourcePrivate::durationChanged(const MediaTime& duration)
{
    {
        Locker locker { m_lock };
        m_duration = duration;
    }
    ensureOnDispatcher([protectedThis = Ref { *this }, this, duration] {
        for (auto& sourceBuffer : m_sourceBuffers)
            sourceBuffer->setMediaSourceDuration(duration);
    });
}

void MediaSourcePrivate::bufferedChanged(const PlatformTimeRanges& buffered)
{
    Locker locker { m_lock };

    m_buffered = buffered;
}

void MediaSourcePrivate::trackBufferedChanged(SourceBufferPrivate&, Vector<PlatformTimeRanges>&&)
{
}

PlatformTimeRanges MediaSourcePrivate::buffered() const
{
    Locker locker { m_lock };

    return m_buffered;
}

bool MediaSourcePrivate::hasBufferedData() const
{
    Locker locker { m_lock };

    return m_buffered.length();
}

PlatformTimeRanges MediaSourcePrivate::seekable() const
{
    MediaTime duration;
    PlatformTimeRanges buffered;
    PlatformTimeRanges liveSeekable;
    {
        Locker locker { m_lock };
        duration = m_duration;
        buffered = m_buffered;
        liveSeekable = m_liveSeekable;
    }

    // 6. HTMLMediaElement Extensions, seekable
    // W3C Editor's Draft 16 September 2016
    // https://rawgit.com/w3c/media-source/45627646344eea0170dd1cbc5a3d508ca751abb8/media-source-respec.html#htmlmediaelement-extensions

    // ↳ If duration equals NaN:
    // Return an empty TimeRanges object.
    if (duration.isInvalid())
        return PlatformTimeRanges::emptyRanges();

    // ↳ If duration equals positive Infinity:
    if (duration.isPositiveInfinite()) {
        // If live seekable range is not empty:
        if (liveSeekable.length()) {
            // Let union ranges be the union of live seekable range and the HTMLMediaElement.buffered attribute.
            buffered.unionWith(liveSeekable);
            // Return a single range with a start time equal to the earliest start time in union ranges
            // and an end time equal to the highest end time in union ranges and abort these steps.
            buffered.add(buffered.start(0), buffered.maximumBufferedTime());
            return buffered;
        }

        // If the HTMLMediaElement.buffered attribute returns an empty TimeRanges object, then return
        // an empty TimeRanges object and abort these steps.
        if (!buffered.length())
            return PlatformTimeRanges::emptyRanges();

        // Return a single range with a start time of 0 and an end time equal to the highest end time
        // reported by the HTMLMediaElement.buffered attribute.
        return PlatformTimeRanges { MediaTime::zeroTime(), buffered.maximumBufferedTime() };
    }

    // ↳ Otherwise:
    // Return a single range with a start time of 0 and an end time equal to duration.
    return PlatformTimeRanges { MediaTime::zeroTime(), duration };
}

void MediaSourcePrivate::setLiveSeekableRange(const PlatformTimeRanges& ranges)
{
    Locker locker { m_lock };

    m_liveSeekable = ranges;
}

void MediaSourcePrivate::clearLiveSeekableRange()
{
    Locker locker { m_lock };

    m_liveSeekable.clear();
}

const PlatformTimeRanges& MediaSourcePrivate::liveSeekableRange() const
{
    Locker locker { m_lock };

    return m_liveSeekable;
}

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
void MediaSourcePrivate::setCDMSession(LegacyCDMSession* session)
{
    assertIsCurrent(m_dispatcher);

    for (auto& sourceBuffer : m_sourceBuffers)
        sourceBuffer->setCDMSession(session);
}
#endif

void MediaSourcePrivate::ensureOnDispatcher(Function<void()>&& function) const
{
    if (m_dispatcher->isCurrent()) {
        function();
        return;
    }
    m_dispatcher->dispatch(WTFMove(function));
}

MediaTime MediaSourcePrivate::currentTime() const
{
    if (RefPtr player = this->player())
        return player->currentOrPendingSeekTime();
    return MediaTime::invalidTime();
}

bool MediaSourcePrivate::timeIsProgressing() const
{
    if (RefPtr player = this->player())
        return player->timeIsProgressing();
    return false;
}

} // namespace WebCore

#endif

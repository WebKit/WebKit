/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "SourceBufferPrivate.h"

#if ENABLE(MEDIA_SOURCE)

#include "AudioTrackPrivate.h"
#include "Logging.h"
#include "MediaDescription.h"
#include "MediaSample.h"
#include "MediaSourcePrivate.h"
#include "PlatformTimeRanges.h"
#include "SampleMap.h"
#include "SharedBuffer.h"
#include "SourceBufferPrivateClient.h"
#include "TimeRanges.h"
#include "TrackBuffer.h"
#include "TrackInfo.h"
#include "VideoTrackPrivate.h"
#include <wtf/CheckedArithmetic.h>
#include <wtf/IteratorRange.h>
#include <wtf/MainThread.h>
#include <wtf/MediaTime.h>
#include <wtf/StringPrintStream.h>

namespace WebCore {

// Do not enqueue samples spanning a significant unbuffered gap.
// NOTE: one second is somewhat arbitrary. MediaSource::monitorSourceBuffers() is run
// on the playbackTimer, which is effectively every 350ms. Allowing > 350ms gap between
// enqueued samples allows for situations where we overrun the end of a buffered range
// but don't notice for 350ms of playback time, and the client can enqueue data for the
// new current time without triggering this early return.
// FIXME(135867): Make this gap detection logic less arbitrary.
static const MediaTime discontinuityTolerance = MediaTime(1, 1);
static const unsigned evictionAlgorithmInitialTimeChunk = 30000;
static const unsigned evictionAlgorithmTimeChunkLowThreshold = 3000;

SourceBufferPrivate::SourceBufferPrivate(MediaSourcePrivate& parent)
    : SourceBufferPrivate(parent, WorkQueue::mainSingleton())
{
}

SourceBufferPrivate::SourceBufferPrivate(MediaSourcePrivate& parent, WorkQueue& dispatcher)
    : m_mediaSource(&parent)
    , m_dispatcher(dispatcher)
#if ASSERT_ENABLED
    , m_creationThreadId(isMainThread() ? 0 : Thread::currentSingleton().uid())
#endif
{
}

SourceBufferPrivate::~SourceBufferPrivate() = default;

void SourceBufferPrivate::removedFromMediaSource()
{
    ALWAYS_LOG(LOGIDENTIFIER);

    ensureOnDispatcher([protectedThis = Ref { *this }, this] {
        // The SourceBufferClient holds a strong reference to SourceBufferPrivate at this stage
        // and can be safely removed from the MediaSourcePrivate which also holds a strong reference.
        if (RefPtr mediaSource = std::exchange(m_mediaSource, nullptr).get())
            mediaSource->removeSourceBuffer(*this);
    });
}

void SourceBufferPrivate::setClient(SourceBufferPrivateClient& client)
{
    // Called on SourceBufferClient creation, immediately after SourceBufferPrivate creation.
    m_client = client;
}

MediaTime SourceBufferPrivate::currentTime() const
{
    if (RefPtr mediaSource = m_mediaSource.get())
        return mediaSource->currentTime();
    return { };
}

void SourceBufferPrivate::setMediaSourceDuration(const MediaTime& duration)
{
    Locker locker { m_lock };
    m_mediaSourceDuration = duration;
}

MediaTime SourceBufferPrivate::mediaSourceDuration() const
{
    Locker locker { m_lock };
    return m_mediaSourceDuration;
}

void SourceBufferPrivate::setMode(SourceBufferAppendMode mode)
{
    ensureWeakOnDispatcher([mode](auto& buffer) {
        assertIsCurrent(buffer.m_dispatcher.get());
        buffer.m_appendMode = mode;
    });
}

void SourceBufferPrivate::resetTimestampOffsetInTrackBuffers()
{
    // Can be called on SourceBuffer's thread.
    ensureWeakOnDispatcher([](auto& buffer) {
        buffer.iterateTrackBuffers([&](auto& trackBuffer) {
            trackBuffer.resetTimestampOffset();
        });
    });
}

void SourceBufferPrivate::startChangingType()
{
    // Can be called on SourceBuffer's thread.
    ensureWeakOnDispatcher([](auto& buffer) {
        assertIsCurrent(buffer.m_dispatcher.get());
        buffer.m_pendingInitializationSegmentForChangeType = true;
    });
}

void SourceBufferPrivate::setTimestampOffset(const MediaTime& timestampOffset)
{
    // Called from the SourceBuffer's dispatcher
    Locker locker { m_lock };
    m_timestampOffset = timestampOffset;
}

MediaTime SourceBufferPrivate::timestampOffset() const
{
    Locker locker { m_lock };
    return m_timestampOffset;
}

void SourceBufferPrivate::resetTrackBuffers()
{
    // Can be called on SourceBuffer's thread.
    ASSERT(m_dispatcher->isCurrent() || isOnCreationThread());
    ensureWeakOnDispatcher([](auto& buffer) {
        buffer.iterateTrackBuffers([&](auto& trackBuffer) {
            trackBuffer.reset();
        });
    });
}

void SourceBufferPrivate::setAppendWindowStart(const MediaTime& appendWindowStart)
{
    // Called from the SourceBuffer's dispatcher
    ASSERT(isOnCreationThread());
    Locker locker { m_lock };
    m_appendWindowStart = appendWindowStart;
}

void SourceBufferPrivate::setAppendWindowEnd(const MediaTime& appendWindowEnd)
{
    // Called from the SourceBuffer's dispatcher
    ASSERT(isOnCreationThread());
    Locker locker { m_lock };
    m_appendWindowEnd = appendWindowEnd;
}

std::pair<MediaTime, MediaTime> SourceBufferPrivate::appendWindow() const
{
    Locker locker { m_lock };
    return { m_appendWindowStart, m_appendWindowEnd };
}

void SourceBufferPrivate::updateHighestPresentationTimestamp()
{
    assertIsCurrent(m_dispatcher.get());
    MediaTime highestTime;
    iterateTrackBuffers([&](auto& trackBuffer) {
        auto lastSampleIter = trackBuffer.samples().presentationOrder().rbegin();
        if (lastSampleIter != trackBuffer.samples().presentationOrder().rend())
            highestTime = std::max(highestTime, lastSampleIter->first);
    });

    if (m_highestPresentationTimestamp == highestTime)
        return;

    m_highestPresentationTimestamp = highestTime;
    if (RefPtr client = this->client())
        client->sourceBufferPrivateHighestPresentationTimestampChanged(m_highestPresentationTimestamp);
}

Ref<MediaPromise> SourceBufferPrivate::updateBuffered()
{
    assertIsCurrent(m_dispatcher);

    if (RefPtr mediaSource = m_mediaSource.get())
        mediaSource->trackBufferedChanged(*this, trackBuffersRanges());

    if (RefPtr client = this->client())
        return client->sourceBufferPrivateBufferedChanged(trackBuffersRanges());
    return MediaPromise::createAndReject(PlatformMediaError::BufferRemoved);
}

Vector<PlatformTimeRanges> SourceBufferPrivate::trackBuffersRanges() const
{
    assertIsCurrent(m_dispatcher.get());

    auto iteratorRange = makeSizedIteratorRange(m_trackBufferMap, m_trackBufferMap.begin(), m_trackBufferMap.end());
    return WTF::map(iteratorRange, [](auto& trackBuffer) {
        return trackBuffer.second->buffered();
    });
}

bool SourceBufferPrivate::hasReceivedFirstInitializationSegment() const
{
    assertIsCurrent(m_dispatcher.get());

    return m_receivedFirstInitializationSegment;
}

void SourceBufferPrivate::reenqueSamples(TrackID trackID, NeedsFlush needsFlush)
{
    assertIsCurrent(m_dispatcher.get());

    RefPtr client = this->client();
    if (!client)
        return;

    auto trackBuffer = m_trackBufferMap.find(trackID);
    if (trackBuffer == m_trackBufferMap.end())
        return;
    trackBuffer->second->setNeedsReenqueueing(true);
    reenqueueMediaForTime(trackBuffer->second, trackID, currentTime(), needsFlush);
}

Ref<SourceBufferPrivate::ComputeSeekPromise> SourceBufferPrivate::computeSeekTime(const SeekTarget& target)
{
    // Called on SourceBuffer's thread
    ASSERT(isOnCreationThread());
    return invokeAsync(m_dispatcher, [weakThis = ThreadSafeWeakPtr { *this }, target] {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return ComputeSeekPromise::createAndReject(PlatformMediaError::BufferRemoved);
        RefPtr client = protectedThis->client();
        if (!client)
            return ComputeSeekPromise::createAndReject(PlatformMediaError::BufferRemoved);

        auto seekTime = target.time;

        if (target.negativeThreshold || target.positiveThreshold) {
            protectedThis->iterateTrackBuffers([&](auto& trackBuffer) {
                // Find the sample which contains the target time.
                auto trackSeekTime = trackBuffer.findSeekTimeForTargetTime(target.time, target.negativeThreshold, target.positiveThreshold);

                if (trackSeekTime.isValid() && abs(target.time - trackSeekTime) > abs(target.time - seekTime))
                    seekTime = trackSeekTime;
            });
        }
        // When converting from a double-precision float to a MediaTime, a certain amount of precision is lost. If that
        // results in a round-trip between `float in -> MediaTime -> float out` where in != out, we will wait forever for
        // the time jump observer to fire.
        if (seekTime.hasDoubleValue())
            seekTime = MediaTime::createWithDouble(seekTime.toDouble(), MediaTime::DefaultTimeScale);

        protectedThis->computeEvictionData();

        return ComputeSeekPromise::createAndResolve(seekTime);
    });
}

void SourceBufferPrivate::seekToTime(const MediaTime& time)
{
    assertIsCurrent(m_dispatcher.get());

    for (auto& trackBufferPair : m_trackBufferMap) {
        TrackBuffer& trackBuffer = trackBufferPair.second;
        TrackID trackID = trackBufferPair.first;

        trackBuffer.setNeedsReenqueueing(true);
        reenqueueMediaForTime(trackBuffer, trackID, time);
    }

    computeEvictionData();
}

void SourceBufferPrivate::clearTrackBuffers(bool shouldReportToClient)
{
    // Called from SourceBuffer thread or on dispatcher from memoryPressure.
    ASSERT(m_dispatcher->isCurrent() || isOnCreationThread());
    ensureWeakOnDispatcher([shouldReportToClient](auto& buffer) {
        buffer.iterateTrackBuffers([&](auto& trackBuffer) {
            trackBuffer.clearSamples();
        });
        if (!shouldReportToClient)
            return;

        buffer.computeEvictionData();

        buffer.updateHighestPresentationTimestamp();

        buffer.updateBuffered();
    });
}

Ref<SourceBufferPrivate::SamplesPromise> SourceBufferPrivate::bufferedSamplesForTrackId(TrackID trackID)
{
    // Internals only.
    return invokeAsync(m_dispatcher, [protectedThis = Ref { *this }, this, trackID] {
        assertIsCurrent(m_dispatcher.get());
        auto trackBuffer = m_trackBufferMap.find(trackID);
        if (trackBuffer == m_trackBufferMap.end())
            return SamplesPromise::createAndResolve(Vector<String> { });

        return SamplesPromise::createAndResolve(WTF::map(trackBuffer->second->samples().decodeOrder(), [](auto& entry) {
            return toString(entry.second.get());
        }));
    });
}

Ref<SourceBufferPrivate::SamplesPromise> SourceBufferPrivate::enqueuedSamplesForTrackID(TrackID)
{
    return SamplesPromise::createAndResolve(Vector<String> { });
}

MediaTime SourceBufferPrivate::minimumUpcomingPresentationTimeForTrackID(TrackID trackID)
{
    // Called on SourceBuffer's thread for testing-only method.
    ASSERT(m_dispatcher->isCurrent() || isOnCreationThread());
    MediaTime minimum = MediaTime::invalidTime();
    ensureOnDispatcherSync([&] {
        assertIsCurrent(m_dispatcher.get());

        auto trackBuffer = m_trackBufferMap.find(trackID);
        if (trackBuffer == m_trackBufferMap.end())
            return;
        minimum = trackBuffer->second->minimumEnqueuedPresentationTime();
    });
    return minimum;
}

void SourceBufferPrivate::updateMinimumUpcomingPresentationTime(TrackBuffer& trackBuffer, TrackID trackID)
{
    assertIsCurrent(m_dispatcher);

    if (!canSetMinimumUpcomingPresentationTime(trackID))
        return;

    if (auto minimumTime = trackBuffer.minimumEnqueuedPresentationTime())
        setMinimumUpcomingPresentationTime(trackID, minimumTime);
}

void SourceBufferPrivate::setMediaSourceEnded(bool isEnded)
{
    // Called on SourceBuffer's thread.
    ASSERT(isOnCreationThread());
    ensureWeakOnDispatcher([isEnded](auto& buffer) {
        assertIsCurrent(buffer.m_dispatcher.get());

        if (std::exchange(buffer.m_isMediaSourceEnded, isEnded) == isEnded)
            return;

        if (buffer.m_isMediaSourceEnded) {
            for (auto& trackBufferPair : buffer.m_trackBufferMap) {
                TrackBuffer& trackBuffer = trackBufferPair.second;
                TrackID trackID = trackBufferPair.first;

                buffer.trySignalAllSamplesInTrackEnqueued(trackBuffer, trackID);
            }
        }
    });
}

void SourceBufferPrivate::trySignalAllSamplesInTrackEnqueued(TrackBuffer& trackBuffer, TrackID trackID)
{
    assertIsCurrent(m_dispatcher.get());

    if (m_isMediaSourceEnded && !trackBuffer.remainingSamples()) {
        DEBUG_LOG(LOGIDENTIFIER, "All samples in track \"", trackID, "\" enqueued.");
        allSamplesInTrackEnqueued(trackID);
    }
}

void SourceBufferPrivate::provideMediaData(TrackID trackID)
{
    assertIsCurrent(m_dispatcher.get());

    auto it = m_trackBufferMap.find(trackID);
    if (it == m_trackBufferMap.end())
        return;

    provideMediaData(it->second, trackID);
}

void SourceBufferPrivate::provideMediaData(TrackBuffer& trackBuffer, TrackID trackID)
{
    if (trackBuffer.needsReenqueueing() || isSeeking())
        return;
    RefPtr client = this->client();
    if (!client)
        return; // detached.

#if !RELEASE_LOG_DISABLED
    unsigned enqueuedSamples = 0;
#endif

    while (true) {
        if (!isReadyForMoreSamples(trackID)) {
            DEBUG_LOG(LOGIDENTIFIER, "bailing early, track id ", trackID, " is not ready for more data");
            notifyClientWhenReadyForMoreSamples(trackID);
            break;
        }

        RefPtr sample = trackBuffer.nextSample();
        if (!sample)
            break;
        enqueueSample(sample.releaseNonNull(), trackID);
#if !RELEASE_LOG_DISABLED
        ++enqueuedSamples;
#endif
    }

    updateMinimumUpcomingPresentationTime(trackBuffer, trackID);

#if !RELEASE_LOG_DISABLED
    DEBUG_LOG(LOGIDENTIFIER, "enqueued ", enqueuedSamples, " samples, ", trackBuffer.remainingSamples(), " remaining");
#endif

    trySignalAllSamplesInTrackEnqueued(trackBuffer, trackID);
}

void SourceBufferPrivate::reenqueueMediaForTime(TrackBuffer& trackBuffer, TrackID trackID, const MediaTime& time, NeedsFlush needsFlush)
{
    assertIsCurrent(m_dispatcher);

    if (needsFlush == NeedsFlush::Yes)
        flush(trackID);
    bool isEnded = false;
    if (RefPtr mediaSource = m_mediaSource.get())
        isEnded = mediaSource->isEnded();
    if (trackBuffer.reenqueueMediaForTime(time, timeFudgeFactor(), isEnded))
        provideMediaData(trackBuffer, trackID);
}

void SourceBufferPrivate::reenqueueMediaIfNeeded(const MediaTime& currentTime)
{
    // Can be called on SourceBuffer's thread.
    ASSERT(m_dispatcher->isCurrent() || isOnCreationThread());
    ensureWeakOnDispatcher([currentTime](auto& buffer) {
        assertIsCurrent(buffer.m_dispatcher.get());

        for (auto& trackBufferPair : buffer.m_trackBufferMap) {
            TrackBuffer& trackBuffer = trackBufferPair.second;
            TrackID trackID = trackBufferPair.first;

            if (trackBuffer.needsReenqueueing()) {
                DEBUG_LOG_WITH_THIS(&buffer, LOGIDENTIFIER_WITH_THIS(&buffer), "reenqueuing at time ", currentTime);
                buffer.reenqueueMediaForTime(trackBuffer, trackID, currentTime);
            } else
                buffer.provideMediaData(trackBuffer, trackID);
        }
    });
}

static PlatformTimeRanges removeSamplesFromTrackBuffer(const DecodeOrderSampleMap::MapType& samples, TrackBuffer& trackBuffer, ASCIILiteral logPrefix)
{
    return trackBuffer.removeSamples(samples, logPrefix);
}

MediaTime SourceBufferPrivate::findPreviousSyncSamplePresentationTime(const MediaTime& time)
{
    MediaTime previousSyncSamplePresentationTime = time;
    iterateTrackBuffers([&](auto& trackBuffer) {
        auto sampleIterator = trackBuffer.samples().decodeOrder().findSyncSamplePriorToPresentationTime(time);
        if (sampleIterator == trackBuffer.samples().decodeOrder().rend())
            return;
        const MediaTime& samplePresentationTime = sampleIterator->first.second;
        if (samplePresentationTime < time)
            previousSyncSamplePresentationTime = samplePresentationTime;
    });
    return previousSyncSamplePresentationTime;
}

Ref<MediaPromise> SourceBufferPrivate::removeCodedFrames(const MediaTime& start, const MediaTime& end, const MediaTime& currentTime)
{
    m_currentSourceBufferOperation = protectedCurrentSourceBufferOperation()->whenSettled(m_dispatcher, [weakThis = ThreadSafeWeakPtr { *this }, start, end, currentTime](auto result) mutable -> Ref<OperationPromise> {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis || !result)
            return OperationPromise::createAndReject(!result ? result.error() : PlatformMediaError::BufferRemoved);
        protectedThis->removeCodedFramesInternal(start, end, currentTime);
        protectedThis->computeEvictionData();
        return protectedThis->updateBuffered().get();
    });
    return m_currentSourceBufferOperation.get();
}

void SourceBufferPrivate::removeCodedFramesInternal(const MediaTime& start, const MediaTime& end, const MediaTime& currentTime)
{
    assertIsCurrent(m_dispatcher.get());

    ASSERT(start < end);
    if (start >= end)
        return;

    // 3.5.9 Coded Frame Removal Algorithm
    // https://w3c.github.io/media-source/#sourcebuffer-coded-frame-removal

    // 1. Let start be the starting presentation timestamp for the removal range.
    // 2. Let end be the end presentation timestamp for the removal range.
    // 3. For each track buffer in this source buffer, run the following steps:

    size_t removedSize = 0;
    iterateTrackBuffers([&](auto& trackBuffer) {
        removedSize += trackBuffer.removeCodedFrames(start, end, currentTime);
        // 3.4 If this object is in activeSourceBuffers, the current playback position is greater than or equal to start
        // and less than the remove end timestamp, and HTMLMediaElement.readyState is greater than HAVE_METADATA, then set
        // the HTMLMediaElement.readyState attribute to HAVE_METADATA and stall playback.
        // This step will be performed in SourceBuffer::sourceBufferPrivateBufferedChanged
    });

    {
        Locker locker { m_lock };
        ASSERT(m_evictionData.contentSize >= removedSize);
        m_evictionData.contentSize -= removedSize;
    }
    ASSERT(contentSize() == totalTrackBufferSizeInBytes());

    reenqueueMediaIfNeeded(currentTime);

    // 4. If buffer full flag equals true and this object is ready to accept more bytes, then set the buffer full flag to false.
    // No-op

    updateHighestPresentationTimestamp();
}

size_t SourceBufferPrivate::platformEvictionThreshold() const
{
    // Default implementation of the virtual function.
    return 0;
}

Ref<GenericPromise> SourceBufferPrivate::setMaximumBufferSize(size_t size)
{
    if (m_maximumBufferSize.exchange(size) == size)
        return GenericPromise::createAndResolve();

    return invokeAsync(m_dispatcher, [weakThis = ThreadSafeWeakPtr { *this }] {
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->computeEvictionData(ComputeEvictionDataRule::ForceNotification);
        return GenericPromise::createAndResolve();
    });
}

void SourceBufferPrivate::computeEvictionData(ComputeEvictionDataRule rule)
{
    assertIsCurrent(m_dispatcher.get());

    SourceBufferEvictionData evictionData {
        .contentSize = totalTrackBufferSizeInBytes(),
        .evictableSize = [&]() -> int64_t {
            RefPtr mediaSource = m_mediaSource.get();
            if (!mediaSource)
                return 0;
            size_t evictableSize = 0;
            auto currentTime = mediaSource->currentTime();

            // We can evict everything from the beginning of the buffer to a maximum of timeChunk (3s) before currentTime (or the previous sync sample whichever comes first).
            auto timeChunkAsMilliseconds = evictionAlgorithmTimeChunkLowThreshold;
            const auto timeChunk = MediaTime(timeChunkAsMilliseconds, 1000);

            const auto rangeStartBeforeCurrentTime = minimumBufferedTime();
            const auto rangeEndBeforeCurrentTime = std::min(currentTime - timeChunk, findPreviousSyncSamplePresentationTime(currentTime));

            if (rangeStartBeforeCurrentTime < rangeEndBeforeCurrentTime) {
                iterateTrackBuffers([&](auto& trackBuffer) {
                    evictableSize += trackBuffer.codedFramesIntervalSize(rangeStartBeforeCurrentTime, rangeEndBeforeCurrentTime);
                });
            }

            PlatformTimeRanges buffered { MediaTime::zeroTime(), MediaTime::positiveInfiniteTime() };
            iterateTrackBuffers([&](const TrackBuffer& trackBuffer) {
                buffered.intersectWith(trackBuffer.buffered());
            });

            // We can evict everything from currentTime+timeChunk (3s) to the end of the buffer, not contiguous in current range.
            auto rangeStartAfterCurrentTime = currentTime + timeChunk;
            const auto rangeEndAfterCurrentTime = buffered.maximumBufferedTime();

            if (rangeStartAfterCurrentTime >= rangeEndAfterCurrentTime)
                return evictableSize;

            // Do not evict data from the time range that contains currentTime.
            size_t currentTimeRange = buffered.find(currentTime);
            size_t startTimeRange = buffered.find(rangeStartAfterCurrentTime);
            if (currentTimeRange != notFound && startTimeRange == currentTimeRange) {
                currentTimeRange++;
                if (currentTimeRange == buffered.length())
                    return evictableSize;
                rangeStartAfterCurrentTime = buffered.start(currentTimeRange);
                if (rangeStartAfterCurrentTime >= rangeEndAfterCurrentTime)
                    return evictableSize;
            }

            iterateTrackBuffers([&](auto& trackBuffer) {
                evictableSize += trackBuffer.codedFramesIntervalSize(rangeStartAfterCurrentTime, rangeEndAfterCurrentTime);
            });
            return evictableSize;
        }(),
        .maximumBufferSize = m_maximumBufferSize,
        .numMediaSamples = [&]() -> size_t {
            const size_t evictionThreshold = platformEvictionThreshold();
            if (!evictionThreshold)
                return 0;
            size_t currentSize = 0;
            iterateTrackBuffers([&](auto& trackBuffer) {
                currentSize += trackBuffer.samples().size();
            });
            return currentSize;
        }()
    };

    bool changed = [&] {
        Locker locker { m_lock };
        changed = m_evictionData != evictionData;
        m_evictionData = evictionData;
        return changed;
    }();
    if (RefPtr client = this->client(); client && (rule == ComputeEvictionDataRule::ForceNotification || changed))
        client->sourceBufferPrivateEvictionDataChanged(evictionData);
}

bool SourceBufferPrivate::hasTooManySamples() const
{
    size_t evictionThreshold = platformEvictionThreshold();
    Locker locker { m_lock };
    return evictionThreshold && m_evictionData.numMediaSamples > evictionThreshold;
}

void SourceBufferPrivate::asyncEvictCodedFrames(uint64_t newDataSize, const MediaTime& currentTime)
{
    m_currentSourceBufferOperation = protectedCurrentSourceBufferOperation()->whenSettled(m_dispatcher, [weakThis = ThreadSafeWeakPtr { *this }, newDataSize, currentTime](auto result) mutable -> Ref<OperationPromise> {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis || !result)
            return OperationPromise::createAndReject(!result ? result.error() : PlatformMediaError::BufferRemoved);
        protectedThis->evictCodedFramesInternal(newDataSize, currentTime);
        return OperationPromise::createAndResolve();
    });
}

bool SourceBufferPrivate::evictCodedFrames(uint64_t newDataSize, const MediaTime& currentTime)
{
    // 3.5.13 Coded Frame Eviction Algorithm
    // http://www.w3.org/TR/media-source/#sourcebuffer-coded-frame-eviction

    RefPtr client = this->client();
    if (!client)
        return true;

    if (canAppend(newDataSize)) {
        if (!isBufferFullFor(newDataSize))
            return false;
        // The buffer is full, but we will be able to evict the content prior appending.
        ensureWeakOnDispatcher([newDataSize, currentTime](auto& buffer) {
            buffer.evictCodedFramesInternal(newDataSize, currentTime);
        });
        return false;
    }

    bool returnValue = false;
    ensureOnDispatcherSync([this, newDataSize, currentTime, &returnValue] {
        assertIsCurrent(m_dispatcher.get());
        returnValue = evictCodedFramesInternal(newDataSize, currentTime);
    });
    return returnValue;
}

bool SourceBufferPrivate::evictCodedFramesInternal(uint64_t newDataSize, const MediaTime& currentTime)
{
    // If the algorithm here is modified, computeEvictionData() must be updated accordingly.

    // This algorithm is run to free up space in this source buffer when new data is appended.
    // 1. Let new data equal the data that is about to be appended to this SourceBuffer.
    // 2. If the buffer full flag equals false, then abort these steps.
    bool isBufferFull = isBufferFullFor(newDataSize) || hasTooManySamples();
    if (!isBufferFull)
        return false;

    // 3. Let removal ranges equal a list of presentation time ranges that can be evicted from
    // the presentation to make room for the new data.

    // NOTE: begin by removing data from the beginning of the buffered ranges, timeChunk seconds at
    // a time, up to timeChunk seconds before currentTime.

#if !RELEASE_LOG_DISABLED
    uint64_t initialBufferedSize = evictionData().contentSize;
    DEBUG_LOG(LOGIDENTIFIER, "currentTime = ", currentTime, ", require ", initialBufferedSize + newDataSize, " bytes, maximum buffer size is ", m_maximumBufferSize.load());
#endif

    isBufferFull = evictFrames(newDataSize, currentTime);

    computeEvictionData();

    if (!isBufferFull) {
#if !RELEASE_LOG_DISABLED
        DEBUG_LOG(LOGIDENTIFIER, "evicted ", initialBufferedSize - evictionData().contentSize);
#endif
        return false;
    }

#if !RELEASE_LOG_DISABLED
    ERROR_LOG(LOGIDENTIFIER, "FAILED to free enough after evicting ", initialBufferedSize - evictionData().contentSize);
#endif
    return true;
}

bool SourceBufferPrivate::isBufferFullFor(uint64_t requiredSize) const
{
    auto totalRequired = checkedSum<uint64_t>(contentSize(), requiredSize);
    if (totalRequired.hasOverflowed())
        return true;

    return totalRequired >= m_maximumBufferSize.load();
}

bool SourceBufferPrivate::canAppend(uint64_t requiredSize) const
{
    Locker locker { m_lock };
    return m_evictionData.contentSize - m_evictionData.evictableSize + requiredSize <= m_maximumBufferSize.load();
}

SourceBufferEvictionData SourceBufferPrivate::evictionData() const
{
    Locker locker { m_lock };
    return m_evictionData;
}

uint64_t SourceBufferPrivate::totalTrackBufferSizeInBytes() const
{
    uint64_t totalSizeInBytes = 0;
    iterateTrackBuffers([&](auto& trackBuffer) {
        totalSizeInBytes += trackBuffer.samples().sizeInBytes();
    });

    return totalSizeInBytes;
}

uint64_t SourceBufferPrivate::contentSize() const
{
    Locker locker { m_lock };
    return m_evictionData.contentSize;
}

void SourceBufferPrivate::addTrackBuffer(TrackID trackId, RefPtr<MediaDescription>&& description)
{
    // Called on SourceBuffer's thread.
    ASSERT(isOnCreationThread());
    ensureWeakOnDispatcher([trackId, description = WTFMove(description)](auto& buffer) mutable {
        assertIsCurrent(buffer.m_dispatcher.get());
        ASSERT(buffer.m_trackBufferMap.find(trackId) == buffer.m_trackBufferMap.end());

        buffer.m_hasAudio = buffer.m_hasAudio || description->isAudio();
        buffer.m_hasVideo = buffer.m_hasVideo || description->isVideo();

        // 5.2.9 Add the track description for this track to the track buffer.
        auto trackBuffer = TrackBuffer::create(WTFMove(description), discontinuityTolerance);
#if !RELEASE_LOG_DISABLED
        // False positive see webkit.org/b/302520
        SUPPRESS_UNCOUNTED_ARG trackBuffer->setLogger(buffer.protectedLogger(), buffer.logIdentifier());
#endif
        buffer.m_trackBufferMap.try_emplace(trackId, WTFMove(trackBuffer));
        if (RefPtr mediaSource = buffer.m_mediaSource.get()) {
            MediaSourcePrivate::TracksType tracksType;
            if (buffer.m_hasAudio)
                tracksType |= TrackInfoTrackType::Audio;
            if (buffer.m_hasVideo)
                tracksType |= TrackInfoTrackType::Video;
            mediaSource->tracksTypeChanged(buffer, tracksType);
        }
    });
}

void SourceBufferPrivate::updateTrackIds(Vector<std::pair<TrackID, TrackID>>&& trackIdPairs)
{
    // Called on SourceBuffer's thread or on dispatcher from SourceBufferPrivate override.
    ASSERT(m_dispatcher->isCurrent() || isOnCreationThread());
    ensureWeakOnDispatcher([trackIdPairs = WTFMove(trackIdPairs)](auto& buffer) mutable {
        assertIsCurrent(buffer.m_dispatcher.get());

        auto trackBufferMap = std::exchange(buffer.m_trackBufferMap, { });
        for (auto& trackIdPair : trackIdPairs) {
            auto oldId = trackIdPair.first;
            auto newId = trackIdPair.second;
            ASSERT(oldId != newId);
            auto trackBufferNode = trackBufferMap.extract(oldId);
            if (!trackBufferNode)
                continue;
            trackBufferNode.key() = newId;
            buffer.m_trackBufferMap.insert(WTFMove(trackBufferNode));
        }
    });
}

void SourceBufferPrivate::setAllTrackBuffersNeedRandomAccess()
{
    // Called on SourceBuffer's thread.
    ASSERT(isOnCreationThread());
    ensureWeakOnDispatcher([](auto& buffer) {
        buffer.iterateTrackBuffers([&](auto& trackBuffer) {
            trackBuffer.setNeedRandomAccessFlag(true);
        });
    });
}

void SourceBufferPrivate::setGroupStartTimestamp(const MediaTime& mediaTime)
{
    // Called on SourceBuffer's thread.
    ASSERT(isOnCreationThread());
    ensureWeakOnDispatcher([mediaTime](auto& buffer) {
        assertIsCurrent(buffer.m_dispatcher.get());
        buffer.m_groupStartTimestamp = mediaTime;
    });
}

void SourceBufferPrivate::setGroupStartTimestampToEndTimestamp()
{
    // Called on SourceBuffer's thread.
    ASSERT(isOnCreationThread());
    ensureWeakOnDispatcher([](auto& buffer) {
        assertIsCurrent(buffer.m_dispatcher.get());
        buffer.m_groupStartTimestamp = buffer.m_groupEndTimestamp;
    });
}

void SourceBufferPrivate::setShouldGenerateTimestamps(bool flag)
{
    // Called on SourceBuffer's thread.
    ASSERT(isOnCreationThread());
    ensureWeakOnDispatcher([flag](auto& buffer) {
        assertIsCurrent(buffer.m_dispatcher.get());
        buffer.m_shouldGenerateTimestamps = flag;
    });
}

Ref<MediaPromise> SourceBufferPrivate::protectedCurrentAppendProcessing() const
{
    assertIsCurrent(m_dispatcher.get());
    return m_currentAppendProcessing;
}

void SourceBufferPrivate::didReceiveInitializationSegment(InitializationSegment&& segment)
{
    assertIsCurrent(m_dispatcher.get());

    processPendingMediaSamples();

    auto segmentCopy = segment;
    m_currentAppendProcessing = protectedCurrentAppendProcessing()->whenSettled(m_dispatcher, [segment = WTFMove(segment), weakThis = ThreadSafeWeakPtr { *this }, abortCount = m_abortCount.load()](auto result) mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return MediaPromise::createAndReject(PlatformMediaError::BufferRemoved);
        assertIsCurrent(protectedThis->m_dispatcher.get());
        RefPtr client = protectedThis->client();
        if (!client)
            return MediaPromise::createAndReject(PlatformMediaError::BufferRemoved);

        if (abortCount != protectedThis->m_abortCount) {
            protectedThis->processInitializationSegment({ });
            return MediaPromise::createAndResolve();
        }
        if (!result || ((protectedThis->m_receivedFirstInitializationSegment && !protectedThis->validateInitializationSegment(segment)) || !protectedThis->precheckInitializationSegment(segment))) {
            protectedThis->processInitializationSegment({ });
            return MediaPromise::createAndReject(!result ? result.error() : PlatformMediaError::ParsingError);
        }
        protectedThis->m_lastInitializationSegment = segment;
        return client->sourceBufferPrivateDidReceiveInitializationSegment(WTFMove(segment));
    })->whenSettled(m_dispatcher, [weakThis = ThreadSafeWeakPtr { *this }, segment = WTFMove(segmentCopy)] (auto result) mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return MediaPromise::createAndReject(PlatformMediaError::BufferRemoved);
        assertIsCurrent(protectedThis->m_dispatcher.get());

        // We don't check for abort here as we need to complete the already started initialization segment.
        protectedThis->m_receivedFirstInitializationSegment = true;
        protectedThis->m_pendingInitializationSegmentForChangeType = false;

        protectedThis->processInitializationSegment(!result ? std::nullopt : std::make_optional(WTFMove(segment)));

        return MediaPromise::createAndSettle(WTFMove(result));
    });
}

void SourceBufferPrivate::didUpdateFormatDescriptionForTrackId(Ref<TrackInfo>&& formatDescription, uint64_t trackId)
{
    assertIsCurrent(m_dispatcher.get());

    m_currentAppendProcessing = protectedCurrentAppendProcessing()->whenSettled(m_dispatcher, [weakThis = ThreadSafeWeakPtr { *this }, formatDescription = WTFMove(formatDescription), trackId] (auto result) mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis || !result)
            return MediaPromise::createAndReject(!result ? result.error() : PlatformMediaError::BufferRemoved);
        protectedThis->processFormatDescriptionForTrackId(WTFMove(formatDescription), trackId);
        return MediaPromise::createAndResolve();
    });
}

bool SourceBufferPrivate::validateInitializationSegment(const SourceBufferPrivateClient::InitializationSegment& segment)
{
    assertIsCurrent(m_dispatcher.get());

    //   * If more than one track for a single type are present (ie 2 audio tracks), then the Track
    //   IDs match the ones in the first initialization segment.
    if (segment.audioTracks.size() >= 2) {
        for (auto& audioTrackInfo : segment.audioTracks) {
            if (m_trackBufferMap.find(RefPtr { audioTrackInfo.track }->id()) == m_trackBufferMap.end())
                return false;
        }
    }

    if (segment.videoTracks.size() >= 2) {
        for (auto& videoTrackInfo : segment.videoTracks) {
            if (m_trackBufferMap.find(RefPtr { videoTrackInfo.track }->id()) == m_trackBufferMap.end())
                return false;
        }
    }

    if (segment.textTracks.size() >= 2) {
        for (auto& textTrackInfo : segment.videoTracks) {
            if (m_trackBufferMap.find(RefPtr { textTrackInfo.track }->id()) == m_trackBufferMap.end())
                return false;
        }
    }

    return true;
}

void SourceBufferPrivate::didReceiveSample(Ref<MediaSample>&& sample)
{
    assertIsCurrent(m_dispatcher.get());
    DEBUG_LOG(LOGIDENTIFIER, sample.get());

    m_pendingSamples.append(WTFMove(sample));
}

Ref<MediaPromise> SourceBufferPrivate::append(Ref<SharedBuffer>&& buffer)
{
    m_currentSourceBufferOperation = protectedCurrentSourceBufferOperation()->whenSettled(m_dispatcher, [weakThis = ThreadSafeWeakPtr { *this }, buffer = WTFMove(buffer), abortCount = m_abortCount.load()](auto result) mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis || !result)
            return MediaPromise::createAndReject(!result ? result.error() : PlatformMediaError::BufferRemoved);
        assertIsCurrent(protectedThis->m_dispatcher.get());

        // We have fully completed the previous append operation, we can start a new promise chain.
        protectedThis->m_currentAppendProcessing = MediaPromise::createAndResolve();

        if (buffer->isEmpty())
            return MediaPromise::createAndResolve();

        if (abortCount != protectedThis->m_abortCount)
            return MediaPromise::createAndResolve();

        // Before the promise returned by appendInternal is resolved, the various callbacks would have been called and populating m_currentAppendProcessing.
        return protectedThis->appendInternal(WTFMove(buffer));
    })->whenSettled(m_dispatcher, [weakThis = ThreadSafeWeakPtr { *this }](auto result) mutable -> Ref<OperationPromise> {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return OperationPromise::createAndReject(PlatformMediaError::BufferRemoved);

        protectedThis->processPendingMediaSamples();

        // We need to wait for m_currentAppendOperation to be settled (which will occur once all the init and media segments have been processed)
        return protectedThis->protectedCurrentAppendProcessing()->whenSettled(protectedThis->m_dispatcher, [previousResult = WTFMove(result)](auto result) {
            return (previousResult && result) ? OperationPromise::createAndResolve() : OperationPromise::createAndReject(!result ? result.error() : previousResult.error());
        });
    })->whenSettled(m_dispatcher, [weakThis = ThreadSafeWeakPtr { * this }, abortCount = m_abortCount.load()](auto result) mutable -> Ref<OperationPromise> {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis || !result)
            return OperationPromise::createAndReject(!result ? result.error() : PlatformMediaError::BufferRemoved);
        assertIsCurrent(protectedThis->m_dispatcher.get());

        protectedThis->computeEvictionData();

        if (abortCount != protectedThis->m_abortCount)
            return OperationPromise::createAndResolve();

        RefPtr client = protectedThis->client();
        if (!client)
            return OperationPromise::createAndReject(PlatformMediaError::BufferRemoved);

        // Resolve the changes in TrackBuffers' buffered ranges
        // into the SourceBuffer's buffered ranges
        Vector<Ref<MediaPromise>> promises;
        promises.append(protectedThis->updateBuffered());
        if (protectedThis->m_groupEndTimestamp > protectedThis->mediaSourceDuration()) {
            // https://w3c.github.io/media-source/#sourcebuffer-coded-frame-processing
            // 5. If the media segment contains data beyond the current duration, then run the duration change algorithm with new
            // duration set to the maximum of the current duration and the group end timestamp.
            promises.append(client->sourceBufferPrivateDurationChanged(protectedThis->m_groupEndTimestamp));
        }

        return MediaPromise::all(promises).get();
    });
    return m_currentSourceBufferOperation.get();
}

void SourceBufferPrivate::processPendingMediaSamples()
{
    assertIsCurrent(m_dispatcher.get());

    if (m_pendingSamples.isEmpty())
        return;
    auto samples = std::exchange(m_pendingSamples, { });
    m_currentAppendProcessing = protectedCurrentAppendProcessing()->whenSettled(m_dispatcher, [weakThis = ThreadSafeWeakPtr { *this }, samples = WTFMove(samples), abortCount = m_abortCount.load()](auto result) mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis || !result)
            return MediaPromise::createAndReject(!result ? result.error() : PlatformMediaError::BufferRemoved);
        if (abortCount != protectedThis->m_abortCount)
            return MediaPromise::createAndResolve();

        RefPtr client = protectedThis->client();
        if (!client)
            return MediaPromise::createAndReject(PlatformMediaError::BufferRemoved);

        for (auto& sample : samples) {
            if (!protectedThis->processMediaSample(*client, WTFMove(sample)))
                return MediaPromise::createAndReject(PlatformMediaError::ParsingError);
        }
        return MediaPromise::createAndResolve();
    });
}

bool SourceBufferPrivate::processMediaSample(SourceBufferPrivateClient& client, Ref<MediaSample>&& sample)
{
    assertIsCurrent(m_dispatcher.get());

    // 3.5.1 Segment Parser Loop
    // 6.1 If the first initialization segment received flag is false, (Note: Issue # 155 & changeType()
    // algorithm) or the  pending initialization segment for changeType flag  is true, (End note)
    // then run the append error algorithm
    //     with the decode error parameter set to true and abort this algorithm.
    // Note: current design makes SourceBuffer somehow ignorant of append state - it's more a thing
    //  of SourceBufferPrivate. That's why this check can't really be done in appendInternal.
    //  unless we force some kind of design with state machine switching.

    if (!m_receivedFirstInitializationSegment || m_pendingInitializationSegmentForChangeType)
        return false;

    if (!isMediaSampleAllowed(sample))
        return true;

    // 3.5.8 Coded Frame Processing
    // http://www.w3.org/TR/media-source/#sourcebuffer-coded-frame-processing

    // When complete coded frames have been parsed by the segment parser loop then the following steps
    // are run:
    // 1. For each coded frame in the media segment run the following steps:
    // 1.1. Loop Top

    do {
        MediaTime presentationTimestamp = MediaTime::zeroTime();
        MediaTime decodeTimestamp = MediaTime::zeroTime();

        // NOTE: this is out-of-order, but we need the timescale from the
        // sample's duration for timestamp generation.
        // 1.2 Let frame duration be a double precision floating point representation of the coded frame's
        // duration in seconds.
        MediaTime frameDuration = sample->duration();

        if (m_shouldGenerateTimestamps) {
            // ↳ If generate timestamps flag equals true:
            // 1. Let presentation timestamp equal 0.
            if (frameDuration.isValid()) {
                // NOTE: Use the duration timscale for the presentation timestamp, as this will eliminate
                // timescale rounding when generating timestamps.
                presentationTimestamp = { 0, frameDuration.timeScale() };

                // 2. Let decode timestamp equal 0.
                decodeTimestamp = { 0, frameDuration.timeScale() };
            }
        } else {
            // ↳ Otherwise:
            // 1. Let presentation timestamp be a double precision floating point representation of
            // the coded frame's presentation timestamp in seconds.
            presentationTimestamp = sample->presentationTime();

            // 2. Let decode timestamp be a double precision floating point representation of the coded frame's
            // decode timestamp in seconds.
            decodeTimestamp = sample->decodeTime();
        }
        ERROR_LOG_IF(presentationTimestamp.isInvalid(), LOGIDENTIFIER, "invalid sample time encountered:", sample.get());

        // 1.3 If mode equals "sequence" and group start timestamp is set, then run the following steps:
        if (m_appendMode == SourceBufferAppendMode::Sequence && m_groupStartTimestamp.isValid()) {
            // 1.3.1 Set timestampOffset equal to group start timestamp - presentation timestamp.
            setTimestampOffset(m_groupStartTimestamp - presentationTimestamp);

            iterateTrackBuffers([&](auto& trackBuffer) {
                trackBuffer.resetTimestampOffset();
            });

            // 1.3.2 Set group end timestamp equal to group start timestamp.
            m_groupEndTimestamp = m_groupStartTimestamp;

            // 1.3.3 Set the need random access point flag on all track buffers to true.
            iterateTrackBuffers([&](auto& trackBuffer) {
                trackBuffer.setNeedRandomAccessFlag(true);
            });

            // 1.3.4 Unset group start timestamp.
            m_groupStartTimestamp = MediaTime::invalidTime();
        }

        // NOTE: this is out-of-order, but we need TrackBuffer to be able to cache the results of timestamp offset rounding
        // 1.5 Let track buffer equal the track buffer that the coded frame will be added to.
        auto trackID = sample->trackID();
        auto it = m_trackBufferMap.find(trackID);
        if (it == m_trackBufferMap.end()) {
            // The client managed to append a sample with a trackID not present in the initialization
            // segment. This would be a good place to post an message to the developer console.
            client.sourceBufferPrivateDidDropSample();
            return true;
        }
        TrackBuffer& trackBuffer = it->second;

        MediaTime microsecond(1, 1000000);

        // 1.4 If timestampOffset is not 0, then run the following steps:
        if (auto timestampOffset = this->timestampOffset()) {
            if (!trackBuffer.roundedTimestampOffset().isValid())
                trackBuffer.setRoundedTimestampOffset(timestampOffset);
            if (presentationTimestamp.isValid() && presentationTimestamp.timeScale() != trackBuffer.lastFrameTimescale()) {
                trackBuffer.setLastFrameTimescale(presentationTimestamp.timeScale());
                trackBuffer.setRoundedTimestampOffset(timestampOffset, trackBuffer.lastFrameTimescale(), microsecond);
            }

            // 1.4.1 Add timestampOffset to the presentation timestamp.
            presentationTimestamp += trackBuffer.roundedTimestampOffset();

            // 1.4.2 Add timestampOffset to the decode timestamp.
            decodeTimestamp += trackBuffer.roundedTimestampOffset();
        }

        // 1.6 ↳ If last decode timestamp for track buffer is set and decode timestamp is less than last
        // decode timestamp:
        // OR
        // ↳ If last decode timestamp for track buffer is set and the difference between decode timestamp and
        // last decode timestamp is greater than 2 times last frame duration:
        if (trackBuffer.lastDecodeTimestamp().isValid() && (decodeTimestamp < trackBuffer.lastDecodeTimestamp()
            || (trackBuffer.greatestFrameDuration().isValid() && decodeTimestamp - trackBuffer.lastDecodeTimestamp() > (trackBuffer.greatestFrameDuration() * 2)))) {

            // 1.6.1:
            if (m_appendMode == SourceBufferAppendMode::Segments) {
                // ↳ If mode equals "segments":
                // Set group end timestamp to presentation timestamp.
                m_groupEndTimestamp = presentationTimestamp;
            } else {
                // ↳ If mode equals "sequence":
                // Set group start timestamp equal to the group end timestamp.
                m_groupStartTimestamp = m_groupEndTimestamp;
            }

            // 1.6.2 Unset the last decode timestamp on all track buffers.
            // 1.6.3 Unset the last frame duration on all track buffers.
            // 1.6.4 Unset the highest presentation timestamp on all track buffers.
            // 1.6.5 Set the need random access point flag on all track buffers to true.
            resetTrackBuffers();

            // 1.6.6 Jump to the Loop Top step above to restart processing of the current coded frame.
            continue;
        }

        if (m_appendMode == SourceBufferAppendMode::Sequence) {
            // Use the generated timestamps instead of the sample's timestamps.
            sample->setTimestamps(presentationTimestamp, decodeTimestamp);
        } else if (trackBuffer.roundedTimestampOffset()) {
            // Reflect the timestamp offset into the sample.
            sample->offsetTimestampsBy(trackBuffer.roundedTimestampOffset());
        }

        DEBUG_LOG(LOGIDENTIFIER, sample.get());

        // 1.7 Let frame end timestamp equal the sum of presentation timestamp and frame duration.
        MediaTime frameEndTimestamp = presentationTimestamp + frameDuration;

        // 1.8 If presentation timestamp is less than appendWindowStart, then set the need random access
        // point flag to true, drop the coded frame, and jump to the top of the loop to start processing
        // the next coded frame.
        // 1.9 If frame end timestamp is greater than appendWindowEnd, then set the need random access
        // point flag to true, drop the coded frame, and jump to the top of the loop to start processing
        // the next coded frame.
        auto [appendWindowStart, appendWindowEnd] = appendWindow();
        if (presentationTimestamp.isInvalid() || presentationTimestamp < appendWindowStart || frameEndTimestamp > appendWindowEnd) {
            // 1.8 Note.
            // Some implementations MAY choose to collect some of these coded frames with presentation
            // timestamp less than appendWindowStart and use them to generate a splice at the first coded
            // frame that has a presentation timestamp greater than or equal to appendWindowStart even if
            // that frame is not a random access point. Supporting this requires multiple decoders or
            // faster than real-time decoding so for now this behavior will not be a normative
            // requirement.
            // 1.9 Note.
            // Some implementations MAY choose to collect coded frames with presentation timestamp less
            // than appendWindowEnd and frame end timestamp greater than appendWindowEnd and use them to
            // generate a splice across the portion of the collected coded frames within the append
            // window at time of collection, and the beginning portion of later processed frames which
            // only partially overlap the end of the collected coded frames. Supporting this requires
            // multiple decoders or faster than real-time decoding so for now this behavior will not be a
            // normative requirement. In conjunction with collecting coded frames that span
            // appendWindowStart, implementations MAY thus support gapless audio splicing.
            // Audio MediaSamples are typically made of packed audio samples. Trim sample to make it fit within the appendWindow.
            if (sample->isDivisable()) {
                std::pair<RefPtr<MediaSample>, RefPtr<MediaSample>> replacementSamples = sample->divide(appendWindowStart);
                if (RefPtr endMediaSample = replacementSamples.second) {
                    ASSERT(endMediaSample->presentationTime() >= appendWindowStart);
                    replacementSamples = endMediaSample->divide(appendWindowEnd, MediaSample::UseEndTime::Use);
                    if (replacementSamples.first) {
                        sample = replacementSamples.first.releaseNonNull();
                        ASSERT(sample->presentationTime() >= appendWindowStart && sample->presentationTime() + sample->duration() <= appendWindowEnd);
                        if (m_appendMode != SourceBufferAppendMode::Sequence && trackBuffer.roundedTimestampOffset())
                            sample->offsetTimestampsBy(-trackBuffer.roundedTimestampOffset());
                        continue;
                    }
                }
            }
            trackBuffer.setNeedRandomAccessFlag(true);
            client.sourceBufferPrivateDidDropSample();
            return true;
        }

        // 1.10 If the need random access point flag on track buffer equals true, then run the following steps:
        if (trackBuffer.needRandomAccessFlag()) {
            // 1.11.1 If the coded frame is not a random access point, then drop the coded frame and jump
            // to the top of the loop to start processing the next coded frame.
            if (!sample->isSync()) {
                client.sourceBufferPrivateDidDropSample();
                return true;
            }

            // 1.11.2 Set the need random access point flag on track buffer to false.
            trackBuffer.setNeedRandomAccessFlag(false);
        }

        // 1.11 Let spliced audio frame be an unset variable for holding audio splice information
        // 1.12 Let spliced timed text frame be an unset variable for holding timed text splice information
        // FIXME: Add support for sample splicing.

        SampleMap erasedSamples;

        // 1.13 If last decode timestamp for track buffer is unset and presentation timestamp
        // falls within the presentation interval of a coded frame in track buffer, then run the
        // following steps:
        if (trackBuffer.lastDecodeTimestamp().isInvalid()) {
            auto iter = trackBuffer.samples().presentationOrder().findSampleContainingPresentationTime(presentationTimestamp);
            if (iter != trackBuffer.samples().presentationOrder().end()) {
                // 1.13.1 Let overlapped frame be the coded frame in track buffer that matches the condition above.
                Ref overlappedFrame = iter->second;

                // 1.13.2 If track buffer contains audio coded frames:
                // Run the audio splice frame algorithm and if a splice frame is returned, assign it to
                // spliced audio frame.
                // FIXME: Add support for sample splicing.

                // If track buffer contains video coded frames:
                if (RefPtr description = trackBuffer.description(); description && description->isVideo()) {
                    // 1.13.2.1 Let overlapped frame presentation timestamp equal the presentation timestamp
                    // of overlapped frame.
                    MediaTime overlappedFramePresentationTimestamp = overlappedFrame->presentationTime();

                    // 1.13.2.2 Let remove window timestamp equal overlapped frame presentation timestamp
                    // plus 1 microsecond.
                    MediaTime removeWindowTimestamp = overlappedFramePresentationTimestamp + microsecond;

                    // 1.13.2.3 If the presentation timestamp is less than the remove window timestamp,
                    // then remove overlapped frame and any coded frames that depend on it from track buffer.
                    if (presentationTimestamp < removeWindowTimestamp)
                        erasedSamples.addSample(iter->second.copyRef());
                }

                // If track buffer contains timed text coded frames:
                // Run the text splice frame algorithm and if a splice frame is returned, assign it to spliced timed text frame.
                // FIXME: Add support for sample splicing.
            }
        }

        // 1.14 Remove existing coded frames in track buffer:
        // If highest presentation timestamp for track buffer is not set:
        if (trackBuffer.highestPresentationTimestamp().isInvalid()) {
            // Remove all coded frames from track buffer that have a presentation timestamp greater than or
            // equal to presentation timestamp and less than frame end timestamp.
            auto iterPair = trackBuffer.samples().presentationOrder().findSamplesBetweenPresentationTimes(presentationTimestamp, frameEndTimestamp);
            if (iterPair.first != trackBuffer.samples().presentationOrder().end())
                erasedSamples.addRange(iterPair.first, iterPair.second);
        }

        // When appending media containing B-frames (media whose samples' presentation timestamps
        // do not increase monotonically, the prior erase steps could leave samples in the trackBuffer
        // which will be disconnected from its previous I-frame. If the incoming frame is an I-frame,
        // remove all samples in decode order between the incoming I-frame's decode timestamp and the
        // next I-frame that is presented after the incoming I-frame. See <https://github.com/w3c/media-source/issues/187>
        // for a discussion of how the MSE specification should handle this scenario.
        do {
            if (!sample->isSync())
                break;

            DecodeOrderSampleMap::KeyType decodeKey(sample->decodeTime(), sample->presentationTime());
            auto nextSampleInDecodeOrder = trackBuffer.samples().decodeOrder().findSampleAfterDecodeKey(decodeKey);
            if (nextSampleInDecodeOrder == trackBuffer.samples().decodeOrder().end())
                break;

            if (Ref second = nextSampleInDecodeOrder->second; second->isSync() && second->presentationTime() > sample->presentationTime())
                break;

            auto nextSyncSample = trackBuffer.samples().decodeOrder().findSyncSampleAfterDecodeIterator(nextSampleInDecodeOrder);
            while (nextSyncSample != trackBuffer.samples().decodeOrder().end() && Ref { nextSyncSample->second }->presentationTime() <= sample->presentationTime())
                nextSyncSample = trackBuffer.samples().decodeOrder().findSyncSampleAfterDecodeIterator(nextSyncSample);

            INFO_LOG(LOGIDENTIFIER, "Discovered out-of-order frames, from: ", nextSampleInDecodeOrder->second.get(), " to: ", (nextSyncSample == trackBuffer.samples().decodeOrder().end() ? "[end]"_s : toString(nextSyncSample->second.get())));
            erasedSamples.addRange(nextSampleInDecodeOrder, nextSyncSample);
        } while (false);

        // There are many files out there where the frame times are not perfectly contiguous and may have small overlaps
        // between the beginning of a frame and the end of the previous one; therefore a tolerance is needed whenever
        // durations are considered.
        // For instance, most WebM files are muxed rounded to the millisecond (the default TimecodeScale of the format)
        // but their durations use a finer timescale (causing a sub-millisecond overlap). More rarely, there are also
        // MP4 files with slightly off tfdt boxes, presenting a similar problem at the beginning of each fragment.
        const MediaTime contiguousFrameTolerance = MediaTime(1, 1000);

        // If highest presentation timestamp for track buffer is set and less than or equal to presentation timestamp
        if (trackBuffer.highestPresentationTimestamp().isValid() && trackBuffer.highestPresentationTimestamp() - contiguousFrameTolerance <= presentationTimestamp) {
            // Remove all coded frames from track buffer that have a presentation timestamp greater than highest
            // presentation timestamp and less than or equal to frame end timestamp.
            do {
                // NOTE: Searching from the end of the trackBuffer will be vastly more efficient if the search range is
                // near the end of the buffered range. Use a linear-backwards search if the search range is within one
                // frame duration of the end:
                if (!trackBuffer.buffered().length())
                    break;

                MediaTime highestBufferedTime = trackBuffer.maximumBufferedTime();
                MediaTime eraseBeginTime = trackBuffer.highestPresentationTimestamp();
                MediaTime eraseEndTime = frameEndTimestamp - contiguousFrameTolerance;

                if (eraseEndTime <= eraseBeginTime)
                    break;

                PresentationOrderSampleMap::iterator_range range;
                if (highestBufferedTime - trackBuffer.highestPresentationTimestamp() < trackBuffer.lastFrameDuration()) {
                    // If the new frame is at the end of the buffered ranges, perform a sequential scan from end (O(1)).
                    range = trackBuffer.samples().presentationOrder().findSamplesBetweenPresentationTimesFromEnd(eraseBeginTime, eraseEndTime);
                } else {
                    // In any other case, perform a binary search (O(log(n)).
                    range = trackBuffer.samples().presentationOrder().findSamplesBetweenPresentationTimes(eraseBeginTime, eraseEndTime);
                }

                if (range.first != trackBuffer.samples().presentationOrder().end())
                    erasedSamples.addRange(range.first, range.second);
            } while (false);
        }

        // 1.15 Remove decoding dependencies of the coded frames removed in the previous step:
        DecodeOrderSampleMap::MapType dependentSamples;
        if (!erasedSamples.empty()) {
            // If detailed information about decoding dependencies is available:
            // FIXME: Add support for detailed dependency information

            // Otherwise: Remove all coded frames between the coded frames removed in the previous step
            // and the next random access point after those removed frames.
            auto firstDecodeIter = trackBuffer.samples().decodeOrder().findSampleWithDecodeKey(erasedSamples.decodeOrder().begin()->first);
            auto lastDecodeIter = trackBuffer.samples().decodeOrder().findSampleWithDecodeKey(erasedSamples.decodeOrder().rbegin()->first);
            auto nextSyncIter = trackBuffer.samples().decodeOrder().findSyncSampleAfterDecodeIterator(lastDecodeIter);
            dependentSamples.insert(firstDecodeIter, nextSyncIter);

            // NOTE: in the case of b-frames, the previous step may leave in place samples whose presentation
            // timestamp < presentationTime, but whose decode timestamp >= decodeTime. These will eventually cause
            // a decode error if left in place, so remove these samples as well.
            DecodeOrderSampleMap::KeyType decodeKey(sample->decodeTime(), sample->presentationTime());
            if (auto samplesWithHigherDecodeTimes = trackBuffer.samples().decodeOrder().findSamplesBetweenDecodeKeys(decodeKey, erasedSamples.decodeOrder().begin()->first); samplesWithHigherDecodeTimes.size())
                dependentSamples.insert(samplesWithHigherDecodeTimes.begin(), samplesWithHigherDecodeTimes.end());

            PlatformTimeRanges erasedRanges = removeSamplesFromTrackBuffer(dependentSamples, trackBuffer, "didReceiveSample"_s);

            // Only force the TrackBuffer to re-enqueue if the removed ranges overlap with enqueued and possibly
            // not yet displayed samples.
            MediaTime currentTime = this->currentTime();
            if (trackBuffer.highestEnqueuedPresentationTime().isValid() && currentTime < trackBuffer.highestEnqueuedPresentationTime()) {
                PlatformTimeRanges possiblyEnqueuedRanges(currentTime, trackBuffer.highestEnqueuedPresentationTime());
                possiblyEnqueuedRanges.intersectWith(erasedRanges);
                if (possiblyEnqueuedRanges.length())
                    trackBuffer.setNeedsReenqueueing(true);
            }

            erasedRanges.invert();
            trackBuffer.buffered().intersectWith(erasedRanges);
        }

        // 1.16 If spliced audio frame is set:
        // Add spliced audio frame to the track buffer.
        // If spliced timed text frame is set:
        // Add spliced timed text frame to the track buffer.
        // FIXME: Add support for sample splicing.

        // Otherwise:
        // Add the coded frame with the presentation timestamp, decode timestamp, and frame duration to the track buffer.
        // 1.17 Set last decode timestamp for track buffer to decode timestamp.
        // 1.18 Set last frame duration for track buffer to frame duration.
        // 1.19 If highest presentation timestamp for track buffer is unset or frame end timestamp is greater
        // than highest presentation timestamp, then set highest presentation timestamp for track buffer
        // to frame end timestamp.
        trackBuffer.addSample(sample);

        // 1.20 If frame end timestamp is greater than group end timestamp, then set group end timestamp equal
        // to frame end timestamp.
        if (m_groupEndTimestamp.isInvalid() || frameEndTimestamp > m_groupEndTimestamp)
            m_groupEndTimestamp = frameEndTimestamp;

        // 1.21 If generate timestamps flag equals true, then set timestampOffset equal to frame end timestamp.
        if (m_shouldGenerateTimestamps) {
            setTimestampOffset(frameEndTimestamp);
            resetTimestampOffsetInTrackBuffers();
        }
        break;
    } while (true);

    // Steps 2-4 will be handled by MediaSource::monitorSourceBuffers()
    // Step 5 will be handlded by SourceBufferPrivate::appendCompleted()

    updateHighestPresentationTimestamp();
    return true;
}

void SourceBufferPrivate::abort()
{
    m_abortCount++;
}

void SourceBufferPrivate::resetParserState()
{
    m_currentSourceBufferOperation = protectedCurrentSourceBufferOperation()->whenSettled(m_dispatcher, [weakThis = ThreadSafeWeakPtr { *this }](auto result) mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return OperationPromise::createAndReject(PlatformMediaError::BufferRemoved);
        protectedThis->resetParserStateInternal();
        return OperationPromise::createAndSettle(WTFMove(result));
    });
}

void SourceBufferPrivate::memoryPressure(const MediaTime& currentTime)
{
    ALWAYS_LOG(LOGIDENTIFIER, "currentTime: ", currentTime);
    m_currentSourceBufferOperation = protectedCurrentSourceBufferOperation()->whenSettled(m_dispatcher, [weakThis = ThreadSafeWeakPtr { *this }, currentTime](auto result) mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return OperationPromise::createAndReject(PlatformMediaError::BufferRemoved);
        assertIsCurrent(protectedThis->m_dispatcher.get());
        ALWAYS_LOG_WITH_THIS(protectedThis, LOGIDENTIFIER_WITH_THIS(protectedThis), "isActive = ", protectedThis->m_isActive);
        if (protectedThis->m_isActive)
            protectedThis->evictFrames(protectedThis->m_maximumBufferSize, currentTime);
        else {
            protectedThis->resetTrackBuffers();
            protectedThis->clearTrackBuffers(true);
        }
        protectedThis->updateBuffered();
        protectedThis->computeEvictionData();
        return OperationPromise::createAndSettle(WTFMove(result));
    });
}

auto SourceBufferPrivate::protectedCurrentSourceBufferOperation() const -> Ref<OperationPromise>
{
    ASSERT(m_dispatcher.ptr() == &WorkQueue::mainSingleton() || !m_dispatcher->isCurrent());

    return m_currentSourceBufferOperation;
}

MediaTime SourceBufferPrivate::minimumBufferedTime() const
{
    assertIsCurrent(m_dispatcher);

    MediaTime minimumTime = MediaTime::positiveInfiniteTime();
    iterateTrackBuffers([&](const TrackBuffer& trackBuffer) {
        minimumTime = std::min(minimumTime, trackBuffer.buffered().minimumBufferedTime());
    });
    return minimumTime;
}

MediaTime SourceBufferPrivate::maximumBufferedTime() const
{
    assertIsCurrent(m_dispatcher);

    MediaTime maximumTime = MediaTime::negativeInfiniteTime();
    iterateTrackBuffers([&](const TrackBuffer& trackBuffer) {
        maximumTime = std::max(maximumTime, trackBuffer.buffered().maximumBufferedTime());
    });
    return maximumTime;
}

bool SourceBufferPrivate::evictFrames(uint64_t newDataSize, const MediaTime& currentTime)
{
    assertIsCurrent(m_dispatcher);

    auto isBufferFull = true;

    // FIXME: All this is nice but we should take into account negative playback rate and begin from after current time
    // and be more conservative with before current time.

    auto timeChunkAsMilliseconds = evictionAlgorithmInitialTimeChunk;
    do {
        const auto timeChunk = MediaTime(timeChunkAsMilliseconds, 1000);
        const auto maximumRangeEnd = std::min(currentTime - timeChunk, findPreviousSyncSamplePresentationTime(currentTime));

        do {
            auto rangeStartBeforeCurrentTime = minimumBufferedTime();
            auto rangeEndBeforeCurrentTime = std::min(rangeStartBeforeCurrentTime + timeChunk, maximumRangeEnd);

            if (rangeStartBeforeCurrentTime >= rangeEndBeforeCurrentTime)
                break;

            // 4. For each range in removal ranges, run the coded frame removal algorithm with start and
            // end equal to the removal range start and end timestamp respectively.
            removeCodedFramesInternal(rangeStartBeforeCurrentTime, rangeEndBeforeCurrentTime, currentTime);
            if (minimumBufferedTime() == rangeStartBeforeCurrentTime)
                break; // Nothing evicted.

            isBufferFull = isBufferFullFor(newDataSize);
        } while (isBufferFull);

        timeChunkAsMilliseconds /= 2;
    } while (isBufferFull && timeChunkAsMilliseconds >= evictionAlgorithmTimeChunkLowThreshold);

    if (!isBufferFull)
        return false;

    timeChunkAsMilliseconds = evictionAlgorithmInitialTimeChunk;
    do {
        const auto timeChunk = MediaTime(timeChunkAsMilliseconds, 1000);
        const auto minimumRangeStartAfterCurrentTime = currentTime + timeChunk;

        do {
            PlatformTimeRanges buffered { MediaTime::zeroTime(), MediaTime::positiveInfiniteTime() };
            iterateTrackBuffers([&](const TrackBuffer& trackBuffer) {
                buffered.intersectWith(trackBuffer.buffered());
            });

            auto rangeEndAfterCurrentTime = buffered.maximumBufferedTime();
            auto rangeStartAfterCurrentTime = std::max(minimumRangeStartAfterCurrentTime, rangeEndAfterCurrentTime - timeChunk);

            if (rangeStartAfterCurrentTime >= rangeEndAfterCurrentTime)
                break;

            // Do not evict data from the time range that contains currentTime.
            size_t currentTimeRange = buffered.find(currentTime);
            size_t startTimeRange = buffered.find(rangeStartAfterCurrentTime);
            if (currentTimeRange != notFound && startTimeRange == currentTimeRange) {
                currentTimeRange++;
                if (currentTimeRange == buffered.length())
                    break;
                rangeStartAfterCurrentTime = buffered.start(currentTimeRange);
                if (rangeStartAfterCurrentTime >= rangeEndAfterCurrentTime)
                    break;
            }

            // 4. For each range in removal ranges, run the coded frame removal algorithm with start and
            // end equal to the removal range start and end timestamp respectively.
            removeCodedFramesInternal(rangeStartAfterCurrentTime, rangeEndAfterCurrentTime, currentTime);
            if (maximumBufferedTime() == rangeEndAfterCurrentTime)
                break; // Nothing evicted.

            isBufferFull = isBufferFullFor(newDataSize);
        } while (isBufferFull);

        timeChunkAsMilliseconds /= 2;
    } while (isBufferFull && timeChunkAsMilliseconds >= evictionAlgorithmTimeChunkLowThreshold);

    return isBufferFull;
}

void SourceBufferPrivate::setActive(bool isActive)
{
    ensureWeakOnDispatcher([isActive](auto& buffer) {
        assertIsCurrent(buffer.m_dispatcher.get());
        ALWAYS_LOG_WITH_THIS(&buffer, LOGIDENTIFIER_WITH_THIS(&buffer), isActive);
        buffer.m_isActive = isActive;
        if (RefPtr mediaSource = buffer.m_mediaSource.get())
            mediaSource->sourceBufferPrivateDidChangeActiveState(buffer, isActive);
    });
}

void SourceBufferPrivate::iterateTrackBuffers(NOESCAPE const Function<void(TrackBuffer&)>& func)
{
    assertIsCurrent(m_dispatcher.get());
    for (auto& pair : m_trackBufferMap)
        func(pair.second);
}

void SourceBufferPrivate::iterateTrackBuffers(NOESCAPE const Function<void(const TrackBuffer&)>& func) const
{
    assertIsCurrent(m_dispatcher.get());
    for (auto& pair : m_trackBufferMap)
        func(pair.second);
}

RefPtr<SourceBufferPrivateClient> SourceBufferPrivate::client() const
{
    return m_client.get();
}

void SourceBufferPrivate::ensureOnDispatcher(Function<void()>&& function) const
{
    if (m_dispatcher->isCurrent()) {
        function();
        return;
    }
    m_dispatcher->dispatch(WTFMove(function));
}

void SourceBufferPrivate::ensureOnDispatcherSync(NOESCAPE Function<void()>&& function)
{
    if (m_dispatcher->isCurrent())
        function();
    else
        m_dispatcher->dispatchSync(WTFMove(function));
}

void SourceBufferPrivate::ensureWeakOnDispatcher(Function<void(SourceBufferPrivate&)>&& function)
{
    auto weakWrapper = [function = WTFMove(function), weakThis = ThreadSafeWeakPtr(*this)] mutable {
        if (RefPtr protectedThis = weakThis.get())
            function(*protectedThis);
    };
    ensureOnDispatcher(WTFMove(weakWrapper));
}

void SourceBufferPrivate::attach()
{
    ensureWeakOnDispatcher([](auto& buffer) {
        assertIsCurrent(buffer.m_dispatcher.get());

        if (!buffer.m_lastInitializationSegment)
            return;
        RefPtr client = buffer.client();
        if (!client)
            return;
        auto segment = *buffer.m_lastInitializationSegment;
        client->sourceBufferPrivateDidAttach(WTFMove(segment))
        ->whenSettled(buffer.m_dispatcher, [weakThis = ThreadSafeWeakPtr { buffer }, segment = *buffer.m_lastInitializationSegment] (auto&& result) mutable {
            RefPtr protectedThis = weakThis.get();
            if (!protectedThis || !result)
                return;

            protectedThis->processInitializationSegment(WTFMove(segment));

            // When a MediaSource is re-attached part of the loading the media resources algorithm (https://html.spec.whatwg.org/multipage/media.html#loading-the-media-resourceas)
            // the playback position is to be set back to 0.
            protectedThis->seekToTime(MediaTime::zeroTime());
        });
    });
}

#if ASSERT_ENABLED
bool SourceBufferPrivate::isOnCreationThread() const
{
    return m_creationThreadId ? m_creationThreadId == Thread::currentSingleton().uid() : isMainThread();
}
#endif

} // namespace WebCore

#endif

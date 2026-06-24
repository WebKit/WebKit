/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "TrackBuffer.h"

#if ENABLE(MEDIA_SOURCE)

#include "Logging.h"
#include <ranges>
#include <wtf/CryptographicallyRandomNumber.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(TrackBuffer);

// The maximum queue depth possible for out of order frames with either H264 or HEVC is 16, limit looking ahead of 16 frames.
static constexpr size_t MaximumSlidingWindowLength = 16;

static inline MediaTime roundTowardsTimeScaleWithRoundingMargin(const MediaTime& time, uint32_t timeScale, const MediaTime& roundingMargin)
{
    ASSERT(timeScale);
    if (!timeScale)
        return time;
    while (true) {
        MediaTime roundedTime = time.toTimeScale(timeScale);
        if (abs(roundedTime - time) < roundingMargin || timeScale >= MediaTime::MaximumTimeScale)
            return roundedTime;

        if (!WTF::safeMultiply(timeScale, 2, timeScale) || timeScale > MediaTime::MaximumTimeScale)
            timeScale = MediaTime::MaximumTimeScale;
    }
};

UniqueRef<TrackBuffer> TrackBuffer::create(RefPtr<MediaDescription>&& description)
{
    return create(WTF::move(description), MediaTime::zeroTime());
}

UniqueRef<TrackBuffer> TrackBuffer::create(RefPtr<MediaDescription>&& description, const MediaTime& discontinuityTolerance)
{
    return makeUniqueRef<TrackBuffer>(WTF::move(description), discontinuityTolerance);
}

TrackBuffer::TrackBuffer(RefPtr<MediaDescription>&& description, const MediaTime& discontinuityTolerance)
    : m_description(WTF::move(description))
    , m_enqueueDiscontinuityBoundary(discontinuityTolerance)
    , m_discontinuityTolerance(discontinuityTolerance)
{
}

MediaTime TrackBuffer::maximumBufferedTime() const
{
    if (!m_buffered.length())
        return MediaTime::zeroTime();
    return m_buffered.maximumBufferedTime();
}

void TrackBuffer::addBufferedRange(const MediaTime& start, const MediaTime& end, AddTimeRangeOption addTimeRangeOption)
{
    m_buffered.add(start, end, addTimeRangeOption);
}

void TrackBuffer::adjustSampleStartTime(MediaSample& original, const MediaTime& offset)
{
    // Replace an already-buffered sample with a copy whose presentation and
    // decode timestamps are shifted forward by `offset` (duration shrinks
    // correspondingly; presentationEndTime is preserved; payload is unchanged).
    //
    // Both the SampleMap and m_decodeQueue need to be kept consistent: later
    // removals look samples up by their current decode key, and leaving the
    // pre-adjustment entry in the queue would orphan it.
    Ref replacement = original.createCopyWithAdjustedStartTime(offset);

    MediaTime originalStart = original.presentationTime();
    MediaTime originalEnd = original.presentationEndTime();
    DecodeOrderSampleMap::KeyType originalDecodeKey(original.decodeTime(), original.presentationTime());

    MediaTime replacementStart = replacement->presentationTime();
    MediaTime replacementEnd = replacement->presentationEndTime();
    DecodeOrderSampleMap::KeyType replacementDecodeKey(replacement->decodeTime(), replacementStart);

    PlatformTimeRanges invertedRange(originalStart, originalEnd);
    invertedRange.invert();
    m_buffered.intersectWith(invertedRange);

    m_samples.replaceSample(original, replacement.copyRef());

    addBufferedRange(replacementStart, replacementEnd, AddTimeRangeOption::EliminateSmallGaps);

    // Since `offset` moves the key forward by at most timeFudgeFactor (much
    // smaller than the typical inter-sample DTS gap), the iterator returned by
    // erase() is a valid insertion-point hint for the adjusted entry.
    auto queueIt = m_decodeQueue.find(originalDecodeKey);
    if (queueIt != m_decodeQueue.end()) {
        auto hint = m_decodeQueue.erase(queueIt);
        m_decodeQueue.insert(hint, { replacementDecodeKey, WTF::move(replacement) });
    }
}

void TrackBuffer::addSample(MediaSample& sample)
{
    // The track buffer's main SampleMap must never receive a duplicate
    // (PT) / (DTS, PT) key: SampleMap accounts for sizeInBytes() unconditionally
    // (used by eviction), and a silent insert no-op would skew that accounting
    // and leave the presentation- and decode-order maps inconsistent. Callers
    // (notably SourceBufferPrivate::processMediaSample) are responsible for
    // erasing any colliding sample before adding the new one.
    ASSERT(m_samples.decodeOrder().findSampleWithDecodeKey(DecodeOrderSampleMap::KeyType(sample.decodeTime(), sample.presentationTime())) == m_samples.decodeOrder().end());

    m_samples.addSample(sample);

    // Note: The terminology here is confusing: "enqueuing" means providing a frame to the inner media framework.
    // First, frames are inserted in the decode queue; later, at the end of the append some of the frames in the
    // decode may be "enqueued" (sent to the inner media framework) in `provideMediaData()`.
    //
    // In order to check whether a frame should be added to the decode queue we check that it does not precede any
    // frame already enqueued.
    //
    // Note that adding a frame to the decode queue is no guarantee that it will be actually enqueued at that point.
    // If the frame is after the discontinuity boundary, the enqueueing algorithm will hold it there until samples
    // with earlier timestamps are enqueued. The decode queue is not FIFO, but rather an ordered map.
    DecodeOrderSampleMap::KeyType decodeKey(sample.decodeTime(), sample.presentationTime());
    if (lastEnqueuedDecodeKey().first.isInvalid() || decodeKey > lastEnqueuedDecodeKey()) {
        auto result = decodeQueue().insert(DecodeOrderSampleMap::MapType::value_type(decodeKey, sample));
        auto it = result.first;
        if (it == decodeQueue().begin())
            m_minimumEnqueuedPresentationTime = sample.presentationTime();
        else {
            m_minimumEnqueuedPresentationTime = std::min(m_minimumEnqueuedPresentationTime, sample.presentationTime());
            Ref previousSample = (--it)->second;
            if (sample.presentationTime() < previousSample->presentationTime())
                m_hasOutOfOrderFrames = true;
        }

        // Track reorder depth in decode order. We can't publish a trustworthy
        // minimum upcoming PTS until we've seen at least m_maxObservedReorderDepth
        // + 1 samples past the head — a B-frame with lower PTS may still be on
        // its way.
        if (m_maxPresentationTimeSeenInDecodeOrder.isInvalid() || sample.presentationTime() > m_maxPresentationTimeSeenInDecodeOrder) {
            m_maxPresentationTimeSeenInDecodeOrder = sample.presentationTime();
            m_samplesSinceMaxPresentationTime = 0;
        } else {
            ++m_samplesSinceMaxPresentationTime;
            m_maxObservedReorderDepth = std::max(m_maxObservedReorderDepth, m_samplesSinceMaxPresentationTime);
        }
    }

    // NOTE: the spec considers the need to check the last frame duration but doesn't specify if that last frame
    // is the one prior in presentation or decode order.
    // So instead, as a workaround we use the largest frame duration seen in the current coded frame group (as defined in https://www.w3.org/TR/media-source/#coded-frame-group.
    if (lastDecodeTimestamp().isValid()) {
        MediaTime lastDecodeDuration = sample.decodeTime() - lastDecodeTimestamp();
        if (!greatestFrameDuration().isValid())
            setGreatestFrameDuration(std::max(lastDecodeDuration, sample.duration()));
        else
            setGreatestFrameDuration(std::max({ greatestFrameDuration(), sample.duration(), lastDecodeDuration }));
    }

    // 1.17 Set last decode timestamp for track buffer to decode timestamp.
    setLastDecodeTimestamp(sample.decodeTime());

    // 1.18 Set last frame duration for track buffer to frame duration.
    setLastFrameDuration(sample.duration());

    // 1.19 If highest presentation timestamp for track buffer is unset or frame end timestamp is greater
    // than highest presentation timestamp, then set highest presentation timestamp for track buffer
    // to frame end timestamp.
    if (highestPresentationTimestamp().isInvalid() || sample.presentationEndTime() > highestPresentationTimestamp())
        setHighestPresentationTimestamp(sample.presentationEndTime());

    addBufferedRange(sample.presentationTime(), sample.presentationEndTime(), AddTimeRangeOption::EliminateSmallGaps);
}

RefPtr<MediaSample> TrackBuffer::nextSample()
{
    if (m_decodeQueue.empty())
        return { };

    Ref sample = decodeQueue().begin()->second;

    if (sample->decodeTime() > enqueueDiscontinuityBoundary()) {
        WARNING_LOG(LOGIDENTIFIER, "bailing early because of unbuffered gap, new sample DTS: ", sample->decodeTime(), " >= the current discontinuity boundary: ", enqueueDiscontinuityBoundary());
        return { };
    }

    // Remove the sample from the decode queue now.
    decodeQueue().erase(decodeQueue().begin());

    MediaTime samplePresentationEnd = sample->presentationEndTime();
    if (highestEnqueuedPresentationTime().isInvalid() || samplePresentationEnd > highestEnqueuedPresentationTime())
        setHighestEnqueuedPresentationTime(samplePresentationEnd);

    setLastEnqueuedDecodeKey({ sample->decodeTime(), sample->presentationTime() });
    auto decodeEnd = std::max(sample->decodeTime() + sample->duration(), samplePresentationEnd);
    setEnqueueDiscontinuityBoundary(decodeEnd + m_discontinuityTolerance);

    m_minimumEnqueuedPresentationTime = MediaTime::invalidTime();
    if (m_hasOutOfOrderFrames)
        updateMinimumUpcomingPresentationTime();
    else {
        // Next upcoming time is the next displayed sample.
        for (auto it = m_decodeQueue.begin(); it != m_decodeQueue.end(); ++it) {
            Ref sample = it->second;
            if (sample->isNonDisplaying())
                continue;
            m_minimumEnqueuedPresentationTime = sample->presentationTime();
            break;
        }
    }

    return sample;
}

void TrackBuffer::updateMinimumUpcomingPresentationTime()
{
    if (m_decodeQueue.empty()) {
        m_minimumEnqueuedPresentationTime = MediaTime::invalidTime();
        return;
    }

    // For streams with B-frames we can't trust the sliding-window minimum until
    // we've seen enough samples past the head to cover the observed reorder
    // depth — a later-arriving B-frame could have a lower PTS than what's
    // currently in the queue, and publishing the too-high minimum to the
    // renderer triggers UpcomingPTSExpectation warnings for every B-frame.
    if (m_hasOutOfOrderFrames && m_decodeQueue.size() < m_maxObservedReorderDepth + 1) {
        m_minimumEnqueuedPresentationTime = MediaTime::invalidTime();
        return;
    }

    size_t forwardIndex = 0;
    m_minimumEnqueuedPresentationTime = MediaTime::positiveInfiniteTime();
    for (auto it = m_decodeQueue.begin(); it != m_decodeQueue.end() && forwardIndex < MaximumSlidingWindowLength; ++forwardIndex, ++it) {
        Ref sample = it->second;
        if (!sample->isNonDisplaying())
            m_minimumEnqueuedPresentationTime = std::min(m_minimumEnqueuedPresentationTime, sample->presentationTime());
    }
    if (m_minimumEnqueuedPresentationTime.isPositiveInfinite())
        m_minimumEnqueuedPresentationTime = MediaTime::invalidTime();
}

bool TrackBuffer::reenqueueMediaForTime(const MediaTime& time, const MediaTime& timeFudgeFactor, bool isEnded)
{
    clearDecodeQueue();
    m_enqueueDiscontinuityBoundary = time + m_discontinuityTolerance;

    m_needsReenqueueing = false;

    if (m_samples.empty())
        return false;

    // Find the sample which contains the current presentation time.
    auto currentSamplePTSIterator = m_samples.presentationOrder().findSampleContainingPresentationTime(time);

    // Find the next sample, so long as its presentation start time is within the timeFudgeFactor.
    if (currentSamplePTSIterator == m_samples.presentationOrder().end()) {
        auto nextSampleIterator = m_samples.presentationOrder().findSampleStartingOnOrAfterPresentationTime(time);
        if ((nextSampleIterator->first - time) <= timeFudgeFactor)
            currentSamplePTSIterator = nextSampleIterator;
    }

    // Find the last sample, so long as the track is ended, and the presentation time is after the last sample.
    if (currentSamplePTSIterator == m_samples.presentationOrder().end() && isEnded) {
        auto lastSampleIterator = std::prev(currentSamplePTSIterator);
        if (time >= Ref { lastSampleIterator->second }->presentationEndTime())
            currentSamplePTSIterator = lastSampleIterator;
    }

    if (currentSamplePTSIterator == m_samples.presentationOrder().end())
        return false;

    // Search backward for the previous sync sample.
    Ref sample = currentSamplePTSIterator->second;
    DecodeOrderSampleMap::KeyType decodeKey(sample->decodeTime(), sample->presentationTime());
    auto currentSampleDTSIterator = m_samples.decodeOrder().findSampleWithDecodeKey(decodeKey);
    ASSERT(currentSampleDTSIterator != m_samples.decodeOrder().end());

    auto reverseCurrentSampleIter = --DecodeOrderSampleMap::reverse_iterator(currentSampleDTSIterator);
    auto reverseLastSyncSampleIter = m_samples.decodeOrder().findSyncSamplePriorToDecodeIterator(reverseCurrentSampleIter);
    if (reverseLastSyncSampleIter == m_samples.decodeOrder().rend())
        return false;

    // Fill the decode queue with the non-displaying samples.
    for (auto iter = reverseLastSyncSampleIter; iter != reverseCurrentSampleIter; --iter) {
        Ref copy = Ref { iter->second }->createNonDisplayingCopy();
        DecodeOrderSampleMap::KeyType decodeKey(copy->decodeTime(), copy->presentationTime());
        m_decodeQueue.insert(DecodeOrderSampleMap::MapType::value_type(decodeKey, WTF::move(copy)));
    }

    MediaTime previousSampleTime;

    // Fill the decode queue with the remaining samples.
    if (currentSampleDTSIterator != m_samples.decodeOrder().end()) {
        Ref sample = currentSampleDTSIterator->second;
        if (sample->isDivisable() && sample->presentationTime() < time && time < sample->presentationEndTime()) {
            // Avoid enqueueing content before the current playback position: split the sample
            // straddling `time` and keep only the tail (sub-entries ending after `time`).
            auto [head, tail] = sample->divide(time, MediaSample::UseEndTime::Use);
            if (tail)
                sample = tail.releaseNonNull();
        }
        DecodeOrderSampleMap::KeyType decodeKey(sample->decodeTime(), sample->presentationTime());
        m_minimumEnqueuedPresentationTime = sample->presentationTime();
        previousSampleTime = m_minimumEnqueuedPresentationTime;
        m_decodeQueue.insert(DecodeOrderSampleMap::MapType::value_type(decodeKey, WTF::move(sample)));
    }
    for (auto iter = ++currentSampleDTSIterator; iter != m_samples.decodeOrder().end(); ++iter) {
        Ref sample = iter->second;
        if (sample->presentationTime() < time) {
            Ref copy = sample->createNonDisplayingCopy();
            DecodeOrderSampleMap::KeyType decodeKey(copy->decodeTime(), copy->presentationTime());
            m_decodeQueue.insert(DecodeOrderSampleMap::MapType::value_type(decodeKey, WTF::move(copy)));
        } else {
            m_decodeQueue.insert(*iter);
            if (sample->presentationTime() < m_minimumEnqueuedPresentationTime)
                m_minimumEnqueuedPresentationTime = sample->presentationTime();
            if (std::exchange(previousSampleTime, sample->presentationTime()) > sample->presentationTime())
                m_hasOutOfOrderFrames = true;
        }
    }

    return true;
}

MediaTime TrackBuffer::findSeekTimeForTargetTime(const MediaTime& targetTime, const MediaTime& negativeThreshold, const MediaTime& positiveThreshold)
{
    auto futureSyncSampleIterator = m_samples.decodeOrder().findSyncSampleAfterPresentationTime(targetTime, positiveThreshold);
    auto pastSyncSampleIterator = m_samples.decodeOrder().findSyncSamplePriorToPresentationTime(targetTime, negativeThreshold);
    auto upperBound = m_samples.decodeOrder().end();
    auto lowerBound = m_samples.decodeOrder().rend();

    if (futureSyncSampleIterator == upperBound && pastSyncSampleIterator == lowerBound)
        return MediaTime::invalidTime();

    auto futureSeekTime = MediaTime::positiveInfiniteTime();
    if (futureSyncSampleIterator != upperBound) {
        auto& sample = futureSyncSampleIterator->second;
        futureSeekTime = sample->presentationTime();
    }

    auto pastSeekTime = MediaTime::negativeInfiniteTime();
    if (pastSyncSampleIterator != lowerBound) {
        auto& sample = pastSyncSampleIterator->second;
        pastSeekTime = sample->presentationTime();
    }

    return abs(targetTime - futureSeekTime) < abs(targetTime - pastSeekTime) ? futureSeekTime : pastSeekTime;
}

PlatformTimeRanges TrackBuffer::removeSamples(const DecodeOrderSampleMap::MapType& samples, ASCIILiteral logPrefix)
{
#if !RELEASE_LOG_DISABLED
    auto logId = Logger::LogSiteIdentifier(logClassName(), logPrefix, logIdentifier());
    MediaTime earliestSample = MediaTime::positiveInfiniteTime();
    MediaTime latestSample = MediaTime::zeroTime();
    uint64_t bytesRemoved = 0;
#else
    UNUSED_PARAM(logPrefix);
#endif

#if !RELEASE_LOG_DISABLED
    uint64_t startBufferSize = m_samples.sizeInBytes();
#endif
    PlatformTimeRanges erasedRanges;
    for (const auto& sampleIt : samples) {
        const DecodeOrderSampleMap::KeyType& decodeKey = sampleIt.first;

        Ref sample = sampleIt.second;

#if !RELEASE_LOG_DISABLED
        DEBUG_LOG_IF(m_logger, logId, "removing sample ", sampleIt.second.get());
#endif

        // Remove the erased samples from the TrackBuffer sample map.
        m_samples.removeSample(sample);

        // Also remove the erased samples from the TrackBuffer decodeQueue.
        m_decodeQueue.erase(decodeKey);

        auto startTime = sample->presentationTime();
        auto endTime = startTime + sample->duration();
        erasedRanges.add(startTime, endTime, AddTimeRangeOption::EliminateSmallGaps);

#if !RELEASE_LOG_DISABLED
        if (startTime < earliestSample)
            earliestSample = startTime;
        if (endTime > latestSample)
            latestSample = endTime;
#endif
    }

#if !RELEASE_LOG_DISABLED
    bytesRemoved += startBufferSize - m_samples.sizeInBytes();
#endif

    // Walk each disjoint erased range and consult its retained neighbour on
    // each side. The neighbour can be in one of four states, handled
    // symmetrically at both boundaries:
    //   - no neighbour: extend erasedRanges out to 0 or +inf so the
    //     surrounding unbuffered area is erased too.
    //   - gap (neighbour doesn't reach the erased range): pad the gap so
    //     artificial padding added during append() is removed here as well.
    //   - contiguous: nothing to do.
    //   - overlap (neighbour's range reaches inside the erased range):
    //     clip the erased range so m_buffered isn't stripped of coverage
    //     that a retained sample still holds (e.g. WebM sub-ms overlaps
    //     allowed by contiguousFrameTolerance).
    PlatformTimeRanges clippedErasedRanges;
    PlatformTimeRanges additionalErasedRanges;
    for (unsigned i = 0; i < erasedRanges.length(); ++i) {
        auto erasedStart = erasedRanges.start(i);
        auto erasedEnd = erasedRanges.end(i);

        auto startIterator = m_samples.presentationOrder().reverseFindSampleBeforePresentationTime(erasedStart);
        if (startIterator == m_samples.presentationOrder().rend())
            additionalErasedRanges.add(MediaTime::zeroTime(), erasedStart);
        else {
            Ref previousSample = startIterator->second.get();
            auto previousEnd = previousSample->presentationTime() + previousSample->duration();
            if (previousEnd < erasedStart)
                additionalErasedRanges.add(previousEnd, erasedStart);
            else if (previousEnd > erasedStart)
                erasedStart = std::min(previousEnd, erasedEnd);
        }

        auto endIterator = m_samples.presentationOrder().findSampleStartingAfterPresentationTime(erasedStart);
        if (endIterator == m_samples.presentationOrder().end())
            additionalErasedRanges.add(erasedEnd, MediaTime::positiveInfiniteTime());
        else {
            Ref nextSample = endIterator->second.get();
            auto nextStart = nextSample->presentationTime();
            if (nextStart > erasedEnd)
                additionalErasedRanges.add(erasedEnd, nextStart);
            else if (nextStart < erasedEnd)
                erasedEnd = std::max(nextStart, erasedStart);
        }

        if (erasedStart < erasedEnd)
            clippedErasedRanges.add(erasedStart, erasedEnd, AddTimeRangeOption::EliminateSmallGaps);
    }
    erasedRanges = WTF::move(clippedErasedRanges);
    if (additionalErasedRanges.length())
        erasedRanges.unionWith(additionalErasedRanges);

#if !RELEASE_LOG_DISABLED
    if (bytesRemoved)
        DEBUG_LOG_IF(m_logger, logId, "removed ", bytesRemoved, ", start = ", earliestSample, ", end = ", latestSample);
#endif

    updateMinimumUpcomingPresentationTime();

    return erasedRanges;
}

[[nodiscard]] static bool decodeTimeComparator(const PresentationOrderSampleMap::MapType::value_type& a, const PresentationOrderSampleMap::MapType::value_type& b)
{
    return Ref { a.second }->decodeTime() < Ref { b.second }->decodeTime();
};

int64_t TrackBuffer::removeCodedFrames(const MediaTime& start, const MediaTime& end, const MediaTime& currentTime)
{
    ASSERT(start.isValid());
    ASSERT(end.isValid());
    // 3.5.9 Coded Frame Removal Algorithm
    // https://dvcs.w3.org/hg/html-media/raw-file/tip/media-source/media-source.html#sourcebuffer-coded-frame-removal
    
    // 3.1. Let remove end timestamp be the current value of duration
    // 3.2 If this track buffer has a random access point timestamp that is greater than or equal to end, then update
    // remove end timestamp to that random access point timestamp.
    // NOTE: Step 3.2 will be incorrect for any random access point timestamp whose decode time is later than the sample at end,
    // but whose presentation time is less than the sample at end. Skip this step until step 3.3 below.

    size_t framesSizeBefore = samples().sizeInBytes();

    // NOTE: To handle MediaSamples which may be an amalgamation of multiple shorter samples, find samples whose presentation
    // interval straddles the start and end times, and divide them if possible:
    // Per spec 3.5.9 step 3.3, only samples with starting PTS >= start should be removed.
    // If the sample whose range contains `start` was successfully split, the "after" piece is the
    // first sample to erase — find it by its actual PTS (which may differ slightly from `start`
    // due to timescale rounding, in either direction). Otherwise the original sample (with PTS <
    // start) must be retained per spec; use findSampleStartingOnOrAfter to skip it.
    auto splitAtStart = tryDivideSampleAtTime(start, ApplyDivide::Yes);
    tryDivideSampleAtTime(end, ApplyDivide::Yes);

    auto removePresentationStart = splitAtStart.afterSplitPresentationTime.isValid()
        ? m_samples.presentationOrder().findSampleStartingOnOrAfterPresentationTime(splitAtStart.afterSplitPresentationTime)
        : m_samples.presentationOrder().findSampleStartingOnOrAfterPresentationTime(start);
    auto removePresentationEnd = m_samples.presentationOrder().findSampleStartingOnOrAfterPresentationTime(end);
    if (removePresentationStart == m_samples.presentationOrder().end() || removePresentationStart == removePresentationEnd)
        return framesSizeBefore - samples().sizeInBytes(); // This could be negative if new frames were created above.

    // 3.3 Remove all media data, from this track buffer, that contain starting timestamps greater than or equal to
    // start and less than the remove end timestamp.
    // NOTE: frames must be removed in decode order, so that all dependant frames between the frame to be removed
    // and the next sync sample frame are removed. But we must start from the first sample in decode order, not
    // presentation order.
    auto minmaxDecodeTimeIterPair = std::minmax_element(removePresentationStart, removePresentationEnd, decodeTimeComparator);
    Ref firstSample = minmaxDecodeTimeIterPair.first->second.get();
    Ref lastSample = minmaxDecodeTimeIterPair.second->second.get();
    auto removeDecodeStart = m_samples.decodeOrder().findSampleWithDecodeKey({ firstSample->decodeTime(), firstSample->presentationTime() });
    auto removeDecodeLast = m_samples.decodeOrder().findSampleWithDecodeKey({ lastSample->decodeTime(), lastSample->presentationTime() });
    auto removeDecodeEnd = m_samples.decodeOrder().findSyncSampleAfterDecodeIterator(removeDecodeLast);

    DecodeOrderSampleMap::MapType erasedSamples(removeDecodeStart, removeDecodeEnd);

    PlatformTimeRanges erasedRanges = removeSamples(erasedSamples, "removeCodedFrames"_s);

    // Only force the TrackBuffer to re-enqueue if the removed ranges overlap with enqueued and possibly
    // not yet displayed samples.
    if (m_highestEnqueuedPresentationTime.isValid() && currentTime < m_highestEnqueuedPresentationTime) {
        PlatformTimeRanges possiblyEnqueuedRanges(currentTime, m_highestEnqueuedPresentationTime);
        possiblyEnqueuedRanges.intersectWith(erasedRanges);
        if (possiblyEnqueuedRanges.length()) {
            m_needsReenqueueing = true;
            DEBUG_LOG_IF(m_logger, LOGIDENTIFIER, "the range in removeCodedFrames() includes already enqueued samples, reenqueueing from ", currentTime);
        }
    }

    erasedRanges.invert();
    m_buffered.intersectWith(erasedRanges);

    return framesSizeBefore - samples().sizeInBytes();
}

int64_t TrackBuffer::codedFramesIntervalSize(const MediaTime& start, const MediaTime& end)
{
    ASSERT(start.isValid());
    ASSERT(end.isValid());

    // Mirror removeCodedFrames' iterator selection. Compute split sizes without mutating the
    // sample map (ApplyDivide::No).
    auto splitAtStart = tryDivideSampleAtTime(start, ApplyDivide::No);
    auto splitAtEnd = tryDivideSampleAtTime(end, ApplyDivide::No);

    auto removePresentationStart = splitAtStart.afterSplitPresentationTime.isValid()
        ? m_samples.presentationOrder().findSampleStartingOnOrAfterPresentationTime(splitAtStart.afterSplitPresentationTime)
        : m_samples.presentationOrder().findSampleStartingOnOrAfterPresentationTime(start);
    auto removePresentationEnd = m_samples.presentationOrder().findSampleStartingOnOrAfterPresentationTime(end);
    if (removePresentationStart == m_samples.presentationOrder().end() || removePresentationStart == removePresentationEnd)
        return 0;

    int64_t framesSize = 0;
    // Subtract the "before" piece at start (kept) and the "after" piece at end (kept) from the
    // total below; everything between is summed.
    framesSize -= splitAtStart.beforeSplitSize;
    framesSize -= splitAtEnd.afterSplitSize;

    auto minmaxDecodeTimeIterPair = std::minmax_element(removePresentationStart, removePresentationEnd, decodeTimeComparator);
    Ref firstSample = minmaxDecodeTimeIterPair.first->second.get();
    Ref lastSample = minmaxDecodeTimeIterPair.second->second.get();
    auto removeDecodeStart = m_samples.decodeOrder().findSampleWithDecodeKey({ firstSample->decodeTime(), firstSample->presentationTime() });
    auto removeDecodeLast = m_samples.decodeOrder().findSampleWithDecodeKey({ lastSample->decodeTime(), lastSample->presentationTime() });
    auto removeDecodeEnd = m_samples.decodeOrder().findSyncSampleAfterDecodeIterator(removeDecodeLast);

    DecodeOrderSampleMap::MapType erasedSamples(removeDecodeStart, removeDecodeEnd);

    for (auto& erasedPair : erasedSamples)
        framesSize += Ref { erasedPair.second }->sizeInBytes();

    return framesSize;
}

TrackBuffer::DivideResult TrackBuffer::tryDivideSampleAtTime(const MediaTime& time, ApplyDivide applyDivide)
{
    auto sampleIterator = m_samples.presentationOrder().findSampleContainingPresentationTime(time);
    if (sampleIterator == m_samples.presentationOrder().end())
        return { };
    Ref sample = sampleIterator->second;
    if (!sample->isDivisable())
        return { };
    MediaTime microsecond(1, 1000000);
    MediaTime roundedTime = roundTowardsTimeScaleWithRoundingMargin(time, sample->presentationTime().timeScale(), microsecond);
    std::pair<RefPtr<MediaSample>, RefPtr<MediaSample>> replacementSamples = sample->divide(roundedTime);
    if (!replacementSamples.first || !replacementSamples.second)
        return { };
    DivideResult result {
        .afterSplitPresentationTime = protect(replacementSamples.second)->presentationTime(),
        .beforeSplitSize = static_cast<int64_t>(protect(replacementSamples.first)->sizeInBytes()),
        .afterSplitSize = static_cast<int64_t>(protect(replacementSamples.second)->sizeInBytes()),
    };
    if (applyDivide == ApplyDivide::Yes) {
        DEBUG_LOG_IF(m_logger, LOGIDENTIFIER, "splitting sample ", sample.get(), " into ", Ref { *replacementSamples.first }.get(), " and ", Ref { *replacementSamples.second }.get());
        m_samples.removeSample(sample);
        m_samples.addSample(replacementSamples.first.releaseNonNull());
        m_samples.addSample(replacementSamples.second.releaseNonNull());
    }
    return result;
}

void TrackBuffer::resetTimestampOffset()
{
    m_lastFrameTimescale = 0;
    m_roundedTimestampOffset = MediaTime::invalidTime();
}

void TrackBuffer::reset()
{
    m_lastDecodeTimestamp = MediaTime::invalidTime();
    m_greatestFrameDuration = MediaTime::invalidTime();
    m_lastFrameDuration = MediaTime::invalidTime();
    m_highestPresentationTimestamp = MediaTime::invalidTime();
    m_needRandomAccessFlag = true;
}

void TrackBuffer::clearSamples()
{
    m_samples.clear();
    clearDecodeQueue();
    m_buffered = PlatformTimeRanges();
}

void TrackBuffer::clearDecodeQueue()
{
    m_decodeQueue.clear();
    m_hasOutOfOrderFrames = false;
    m_minimumEnqueuedPresentationTime = MediaTime::invalidTime();
    m_highestEnqueuedPresentationTime = MediaTime::invalidTime();
    m_lastEnqueuedDecodeKey = { MediaTime::invalidTime(), MediaTime::invalidTime() };
    // Reset the running reorder observation but keep m_maxObservedReorderDepth —
    // the codec-declared / previously-seen reorder depth remains valid across a
    // decode-queue flush (same stream, same codec config).
    m_maxPresentationTimeSeenInDecodeOrder = MediaTime::invalidTime();
    m_samplesSinceMaxPresentationTime = 0;
}

void TrackBuffer::setRoundedTimestampOffset(const MediaTime& time, uint32_t timeScale, const MediaTime& roundingMargin)
{
    m_roundedTimestampOffset = roundTowardsTimeScaleWithRoundingMargin(time, timeScale, roundingMargin);
}

#if !RELEASE_LOG_DISABLED
void TrackBuffer::setLogger(const Logger& newLogger, uint64_t newLogIdentifier)
{
    m_logger = newLogger;
    m_logIdentifier = childLogIdentifier(newLogIdentifier, cryptographicallyRandomNumber<uint32_t>());
    ALWAYS_LOG(LOGIDENTIFIER);
}

WTFLogChannel& TrackBuffer::logChannel() const
{
    return JOIN_LOG_CHANNEL_WITH_PREFIX(LOG_CHANNEL_PREFIX, MediaSource);
}
#endif

} // namespace WebCore

#endif // ENABLE(MEDIA_SOURCE)

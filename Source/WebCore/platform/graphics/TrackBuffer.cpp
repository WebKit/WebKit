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

static inline MediaTime virtualDecodeEnd(const MediaSample& sample)
{
    // Clipping to presentationEndTime is a workaround for audio streams with encoder priming,
    // see: https://bugs.webkit.org/show_bug.cgi?id=317485
    return std::max(sample.decodeTime() + sample.duration(), sample.presentationEndTime());
}

UniqueRef<TrackBuffer> TrackBuffer::create(RefPtr<MediaDescription>&& description, IsAcceptableEnqueueGapFn&& isAcceptableEnqueueGap)
{
    return makeUniqueRef<TrackBuffer>(WTF::move(description), WTF::move(isAcceptableEnqueueGap));
}

TrackBuffer::TrackBuffer(RefPtr<MediaDescription>&& description, IsAcceptableEnqueueGapFn&& isAcceptableEnqueueGap)
    : m_description(WTF::move(description))
    , m_enqueueDiscontinuityBoundary(PlatformTimeRanges::timeFudgeFactor())
    , m_isAcceptableEnqueueGap(WTF::move(isAcceptableEnqueueGap))
    , m_furthestContiguousSample(m_decodeQueue.end())
{
}

bool TrackBuffer::isAcceptableEnqueueGap(const MediaTime& fromTime, const MediaTime& toTime) const
{
    if (toTime - fromTime <= PlatformTimeRanges::timeFudgeFactor())
        return true;
    return m_isAcceptableEnqueueGap && m_isAcceptableEnqueueGap(fromTime, toTime);
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
        updateFurthestContiguousSampleBeforeErase(queueIt);

        auto hint = m_decodeQueue.erase(queueIt);
        m_decodeQueue.insert(hint, { replacementDecodeKey, WTF::move(replacement) });

        advanceFurthestContiguousSample();
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

    DecodeOrderSampleMap::KeyType decodeKey = { sample.decodeTime(), sample.presentationTime() };

    if (sample.isSync()) {
        m_appendGroupDecodeKey = decodeKey;
        if (m_lastEnqueuedDecodeKey.first.isInvalid() || decodeKey > m_lastEnqueuedDecodeKey) {
            if (m_isCatchingUpForSmoothSwitch && sample.presentationTime() < m_highestEnqueuedPresentationTime) {
                INFO_LOG(LOGIDENTIFIER, "Smooth switch: New GOP during catch-up, discarding previous catch-up samples");
                m_furthestContiguousSample = decodeQueue().end();
                m_decodeQueue.erase(m_decodeQueue.begin(), m_decodeQueue.upper_bound(decodeKey));
            } else
                DEBUG_LOG(LOGIDENTIFIER, "This GOP can be forwarded to the decode queue.");

            m_isWithholdingSamples = false;
        } else {
            INFO_LOG(LOGIDENTIFIER, "Smooth switch: This GOP will be withheld from the decode queue, at least for now. Future discontinuity boundary: ", futureDiscontinuityBoundary().toDouble());
            m_isWithholdingSamples = true;
        }
    }

    ASSERT(futureDiscontinuityBoundary().isValid());
    if (!sample.isSync() && m_isWithholdingSamples && sample.decodeTime() > futureDiscontinuityBoundary()) {
        INFO_LOG(LOGIDENTIFIER, "Smooth switch: Completing with non-displaying samples (expensive case). "
            "Is decode queue empty: ", m_decodeQueue.empty(), ". New sample: ", sample);

        auto newGroupStart = m_samples.decodeOrder().findSyncSamplePriorToDecodeKey(decodeKey);
        auto newGroupDecodeKey = newGroupStart->first;

        // Delete everything remaining in the decodeQueue that will be replaced by the new GOP. Decoding those
        // samples would be pointless since we already have replacements for the same time range.
        // See: media/media-source/media-source-smooth-switch-expensive-filling-gap.html
        m_decodeQueue.erase(m_decodeQueue.lower_bound(newGroupDecodeKey), m_decodeQueue.lower_bound({ futureDiscontinuityBoundary(), MediaTime::negativeInfiniteTime() }));

        // Insert the dependent samples of the new GOP into the decode queue.
        for (auto it = newGroupStart; it != m_samples.decodeOrder().end() && it->first < decodeKey; ++it) {
            Ref<MediaSample> dependentSample = it->second;
            bool needsNonDisplaying = dependentSample->presentationEndTime()
                < m_highestEnqueuedPresentationTime + PlatformTimeRanges::timeFudgeFactor();
            DEBUG_LOG(LOGIDENTIFIER, "Inserting dependent ", needsNonDisplaying ? "non-" : "" , "displaying sample with PTS=", dependentSample->presentationTime().toDouble());
            Ref<MediaSample> dependentSampleToInsert = needsNonDisplaying ?
                dependentSample->createNonDisplayingCopy() : dependentSample;
            auto result = m_decodeQueue.insert({ it->first, dependentSampleToInsert });

            if (dependentSampleToInsert->isSync())
                m_furthestContiguousSample = result.first;
        }

        advanceFurthestContiguousSample();

        m_isWithholdingSamples = false;
        // Enter a catch-up state until we are done enqueueing non-displaying samples.
        // Being aware of this state is important for correctly handling a potential second smooth switch that
        // occurs while we are still catching up.
        m_isCatchingUpForSmoothSwitch = true;
    }

    // If the current sample needs to be in the decode queue, insert it.
    //
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
    if (!m_isWithholdingSamples && (lastEnqueuedDecodeKey().first.isInvalid() || decodeKey > lastEnqueuedDecodeKey())) {
        // If we get a smooth switch interrupted by a third overlapping attempt, the latter may need some samples
        // to be non-displaying, as they can be after lastEnqueuedDecodeKey but have PTS <= highestEnqueuedPresentationTime.
        // See: media/media-source/media-source-smooth-switch-twice.html
        bool needsNonDisplaying = m_isCatchingUpForSmoothSwitch
            && sample.presentationEndTime() < m_highestEnqueuedPresentationTime + PlatformTimeRanges::timeFudgeFactor();
        Ref<MediaSample> sampleToInsert = needsNonDisplaying ?
            sample.createNonDisplayingCopy() : Ref(sample);
        DEBUG_LOG(LOGIDENTIFIER, "Inserting ", sampleToInsert->isNonDisplaying() ? "non-" : "", "displaying sample in decodeQueue with decodeKey: DTS=", decodeKey.first.toDouble(), " PTS=", decodeKey.second.toDouble());

        // Due to a previous smooth switch, there may remain samples in the decode queue that are no longer part
        // of the official samples in the track. If the new sample overlaps with such a sample, it must be erased.
        DecodeOrderSampleMap::iterator it = decodeQueue().find(decodeKey);
        if (it != decodeQueue().end()) {
            updateFurthestContiguousSampleBeforeErase(it);
            it->second = sampleToInsert;

            DEBUG_LOG(LOGIDENTIFIER, "Overwrote sample from previous smooth switch with identical decodeKey: DTS=", decodeKey.first.toDouble(), " PTS=", decodeKey.second.toDouble());
        } else
            it = decodeQueue().insert({ decodeKey, sampleToInsert }).first;

        advanceFurthestContiguousSample();

        if (it == decodeQueue().begin())
            m_minimumEnqueuedPresentationTime = sample.presentationTime();
        else {
            m_minimumEnqueuedPresentationTime = std::min(m_minimumEnqueuedPresentationTime, sample.presentationTime());
            Ref previousSample = std::prev(it)->second;
            if (sample.presentationTime() < previousSample->presentationTime())
                m_hasOutOfOrderFrames = true;
        }

        if (!sample.isSync()) {
            // If the new sample got ahead of a unrelated sync sample, erase it, as it is a leftover from a smooth switch.
            // See media/media-source/media-source-smooth-switch-replacement-has-bigger-gop-misaligned.html for an example of why that's necessary.
            auto prevSyncSample = std::make_reverse_iterator(it);
            while (prevSyncSample != decodeQueue().rend() && !protect(prevSyncSample->second)->isSync())
                ++prevSyncSample;
            if (prevSyncSample != decodeQueue().rend() && prevSyncSample->first != m_appendGroupDecodeKey) {
                auto forwardIt = std::prev(prevSyncSample.base());
                DEBUG_LOG(LOGIDENTIFIER, "Erasing stale interleaved sync sample with DTS=", prevSyncSample->first.first.toDouble(), " current group start DTS=", m_appendGroupDecodeKey.first.toDouble());
                updateFurthestContiguousSampleBeforeErase(forwardIt);
                decodeQueue().erase(forwardIt);
            }
        }

        // Delete any following samples in the decode queue that the new sample makes undecodable.
        ++it;
        while (it != decodeQueue().end() && !protect(it->second)->isSync()) {
            DEBUG_LOG(LOGIDENTIFIER, "Erasing newly orphaned sample from decodeQueue: ", protect(it->second).get());
            updateFurthestContiguousSampleBeforeErase(it);
            it = decodeQueue().erase(it);
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

    // Bail out if there is a discontinuity, except in the one case where it's necessary: catching up with a nested smooth switch.
    if (sample->decodeTime() > enqueueDiscontinuityBoundary()
        && (!m_isAcceptableEnqueueGap || !m_isAcceptableEnqueueGap(m_lastEnqueueDecodeEnd, sample->decodeTime()))
        && !(m_isCatchingUpForSmoothSwitch && sample->isSync() && sample->presentationTime() <= highestEnqueuedPresentationTime()))
    {
        WARNING_LOG(LOGIDENTIFIER, "bailing early because of unbuffered gap, new sample DTS: ", sample->decodeTime(), " >= the current discontinuity boundary: ", enqueueDiscontinuityBoundary());
        return { };
    }

    // Remove the sample from the decode queue now.
    if (m_furthestContiguousSample == decodeQueue().begin())
        m_furthestContiguousSample = decodeQueue().end();
    decodeQueue().erase(decodeQueue().begin());

    MediaTime samplePresentationEnd = sample->presentationEndTime();

    if (highestEnqueuedPresentationTime().isInvalid() || samplePresentationEnd > highestEnqueuedPresentationTime()) {
        setHighestEnqueuedPresentationTime(samplePresentationEnd);

        if (m_isCatchingUpForSmoothSwitch) {
            INFO_LOG(LOGIDENTIFIER, "Smooth switch: Completed catch-up with first displaying sample, PTS=", sample->presentationTime().toDouble());
            m_isCatchingUpForSmoothSwitch = false;
        }
    }

    DecodeOrderSampleMap::KeyType decodeKey { sample->decodeTime(), sample->presentationTime() };
    setLastEnqueuedDecodeKey(decodeKey);
    auto decodeEnd = virtualDecodeEnd(sample);
    m_lastEnqueueDecodeEnd = decodeEnd;
    setEnqueueDiscontinuityBoundary(decodeEnd + PlatformTimeRanges::timeFudgeFactor());

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

    DEBUG_LOG(LOGIDENTIFIER, "Enqueued sample DTS=", decodeKey.first.toDouble(), " PTS=", decodeKey.second.toDouble(),
        " Leader DTS=", m_appendGroupDecodeKey.first.toDouble(), " PTS=", m_appendGroupDecodeKey.second.toDouble(),
        " m_isWithholdingSamples: ", m_isWithholdingSamples, " isSync: ", sample->isSync());
    if (m_appendGroupDecodeKey.first.isValid() && !m_isWithholdingSamples && sample->isSync() && decodeKey > m_appendGroupDecodeKey) {
        // Prevent future non-sync samples of the currently appended GOP from mixing with the unrelated GOP just enqueued now.
        // Test case: LayoutTests/media/media-source/media-source-smooth-switch-sandwiched-replacement.html
        INFO_LOG(LOGIDENTIFIER, "Smooth switch: The GOP currently being appended needs to be withheld again.");
        m_isWithholdingSamples = true;
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

bool TrackBuffer::reenqueueMediaForTime(const MediaTime& time, bool isEnded)
{
    clearDecodeQueue();
    m_lastEnqueueDecodeEnd = time;
    m_enqueueDiscontinuityBoundary = time + PlatformTimeRanges::timeFudgeFactor();

    m_needsReenqueueing = false;

    if (m_samples.empty())
        return false;

    // Find the sample which contains the current presentation time.
    auto currentSamplePTSIterator = m_samples.presentationOrder().findSampleContainingPresentationTime(time);

    // Find the next sample, so long as its presentation start time is within
    // the gap-skipping policy from the seek target.
    if (currentSamplePTSIterator == m_samples.presentationOrder().end()) {
        auto nextSampleIterator = m_samples.presentationOrder().findSampleStartingOnOrAfterPresentationTime(time);
        if (nextSampleIterator != m_samples.presentationOrder().end() && isAcceptableEnqueueGap(time, nextSampleIterator->first))
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

    m_furthestContiguousSample = m_decodeQueue.begin();
    advanceFurthestContiguousSample();

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

PlatformTimeRanges TrackBuffer::removeSamplesFromMap(const DecodeOrderSampleMap::MapType& samples, ASCIILiteral logPrefix)
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
        Ref sample = sampleIt.second;

#if !RELEASE_LOG_DISABLED
        DEBUG_LOG_IF(m_logger, logId, "removing sample from the sample map ", sampleIt.second.get());
#endif

        // Remove the erased samples from the TrackBuffer sample map.
        m_samples.removeSample(sample);

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
    for (auto& range : erasedRanges.span()) {
        auto erasedStart = range.start;
        auto erasedEnd = range.end;

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

    return erasedRanges;
}

void TrackBuffer::removeSamplesFromDecodeQueue(const DecodeOrderSampleMap::MapType& samples, ASCIILiteral logPrefix)
{
#if !RELEASE_LOG_DISABLED
    auto logId = Logger::LogSiteIdentifier(logClassName(), logPrefix, logIdentifier());
#else
    UNUSED_PARAM(logPrefix);
#endif

    for (const auto& sampleIt : samples) {
        const DecodeOrderSampleMap::KeyType& decodeKey = sampleIt.first;

#if !RELEASE_LOG_DISABLED
        DEBUG_LOG_IF(m_logger, logId, "removing sample from decode queue ", sampleIt.second.get());
#endif

        auto decodeQueueIt = decodeQueue().find(decodeKey);
        if (decodeQueueIt != decodeQueue().end()) {
            updateFurthestContiguousSampleBeforeErase(decodeQueueIt);
            m_decodeQueue.erase(decodeQueueIt);
        }
    }

    updateMinimumUpcomingPresentationTime();
}

[[nodiscard]] static bool decodeTimeComparator(const PresentationOrderSampleMap::MapType::value_type& a, const PresentationOrderSampleMap::MapType::value_type& b)
{
    return Ref { a.second }->decodeTime() < Ref { b.second }->decodeTime();
};

int64_t TrackBuffer::removeCodedFrames(const MediaTime& startIn, const MediaTime& end, const MediaTime& currentTime)
{
    MediaTime start = startIn;
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
    auto splitAtStart = tryDivideSampleAtTime(start, ApplyDivide::No);
    if (splitAtStart.afterSplitPresentationTime.isValid()) {
        // NOTE: When the sample at `start` is divisible, `tryDivideSampleAtTime()` snaps the
        // split point UP to the next sub-sample boundary at or after `start`. That boundary
        // can land beyond `end` when the requested removal range is shorter than one sub-sample
        // (e.g. shorter than one AAC frame inside a bundled CMSampleBuffer). If another sample
        // happens to sit at PTS in [end, afterSplitPresentationTime), the lower_bound calls
        // below would invert iterator order and walk std::minmax_element off the end of the map.
        if (splitAtStart.afterSplitPresentationTime >= end)
            return 0;
    }

    splitAtStart = tryDivideSampleAtTime(start, ApplyDivide::Yes);
    tryDivideSampleAtTime(end, ApplyDivide::Yes);
    auto removePresentationStart = m_samples.presentationOrder().findSampleStartingOnOrAfterPresentationTime(start);
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

    PlatformTimeRanges erasedRanges = removeSamplesFromMap(erasedSamples, "removeCodedFrames"_s);
    removeSamplesFromDecodeQueue(erasedSamples, "removeCodedFrames"_s);

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

int64_t TrackBuffer::codedFramesIntervalSize(const MediaTime& startIn, const MediaTime& end)
{
    ASSERT(startIn.isValid());
    ASSERT(end.isValid());

    MediaTime start = startIn;

    // Mirror removeCodedFrames' iterator selection. Compute split sizes without mutating the
    // sample map (ApplyDivide::No).
    auto splitAtStart = tryDivideSampleAtTime(start, ApplyDivide::No);
    if (splitAtStart.afterSplitPresentationTime.isValid()) {
        start = splitAtStart.afterSplitPresentationTime;
        // See removeCodedFrames() for why this guard is required.
        if (start >= end)
            return 0;
    }

    auto splitAtEnd = tryDivideSampleAtTime(end, ApplyDivide::No);
    auto removePresentationStart = m_samples.presentationOrder().findSampleStartingOnOrAfterPresentationTime(start);
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

MediaTime TrackBuffer::futureDiscontinuityBoundary() const
{
    if (m_furthestContiguousSample == decodeQueue().end())
        return m_enqueueDiscontinuityBoundary;

    return virtualDecodeEnd(m_furthestContiguousSample->second) + PlatformTimeRanges::timeFudgeFactor();
}

void TrackBuffer::advanceFurthestContiguousSample()
{
    if (m_furthestContiguousSample == decodeQueue().end()) {
        auto begin = decodeQueue().begin();
        if (begin == decodeQueue().end() || !isAcceptableEnqueueGap(MediaTime::zeroTime(), protect(begin->second)->decodeTime()))
            return;

        m_furthestContiguousSample = begin;
    }

    while (true) {
        MediaTime decodeEnd = virtualDecodeEnd(m_furthestContiguousSample->second);

        auto next = std::next(m_furthestContiguousSample);
        if (next == decodeQueue().end() || !isAcceptableEnqueueGap(decodeEnd, protect(next->second)->decodeTime()))
            break;

        m_furthestContiguousSample = next;
    }
}

void TrackBuffer::updateFurthestContiguousSampleBeforeErase(DecodeOrderSampleMap::iterator sample)
{
    if (m_furthestContiguousSample == decodeQueue().end() || sample == decodeQueue().end() || sample->first > m_furthestContiguousSample->first)
        return;

    MediaTime decodeEnd = m_lastEnqueueDecodeEnd;
    if (sample != decodeQueue().begin())
        decodeEnd = virtualDecodeEnd(std::prev(sample)->second);

    auto next = std::next(sample);
    if (next == decodeQueue().end() || m_furthestContiguousSample == sample || !isAcceptableEnqueueGap(decodeEnd, protect(next->second)->decodeTime())) {
        if (sample == decodeQueue().begin())
            m_furthestContiguousSample = decodeQueue().end();
        else
            m_furthestContiguousSample = std::prev(sample);
    }
}

void TrackBuffer::clearDecodeQueue()
{
    m_decodeQueue.clear();
    m_hasOutOfOrderFrames = false;
    m_minimumEnqueuedPresentationTime = MediaTime::invalidTime();
    m_highestEnqueuedPresentationTime = MediaTime::invalidTime();
    m_lastEnqueuedDecodeKey = { MediaTime::invalidTime(), MediaTime::invalidTime() };
    m_furthestContiguousSample = m_decodeQueue.end();
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

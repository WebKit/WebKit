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
    return create(WTFMove(description), MediaTime::zeroTime());
}

UniqueRef<TrackBuffer> TrackBuffer::create(RefPtr<MediaDescription>&& description, const MediaTime& discontinuityTolerance)
{
    return makeUniqueRef<TrackBuffer>(WTFMove(description), discontinuityTolerance);
}

TrackBuffer::TrackBuffer(RefPtr<MediaDescription>&& description, const MediaTime& discontinuityTolerance)
    : m_description(WTFMove(description))
    , m_enqueueDiscontinuityBoundary(discontinuityTolerance)
    , m_discontinuityTolerance(discontinuityTolerance)
{
}

MediaTime TrackBuffer::maximumBufferedTime() const
{
    return m_buffered.maximumBufferedTime();
}

void TrackBuffer::addBufferedRange(const MediaTime& start, const MediaTime& end, AddTimeRangeOption addTimeRangeOption)
{
    m_buffered.add(start, end, addTimeRangeOption);
}

void TrackBuffer::addSample(MediaSample& sample)
{
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
        DEBUG_LOG(LOGIDENTIFIER, "bailing early because of unbuffered gap, new sample: ", sample->decodeTime(), " >= the current discontinuity boundary: ", enqueueDiscontinuityBoundary());
        return { };
    }

    // Remove the sample from the decode queue now.
    decodeQueue().erase(decodeQueue().begin());

    MediaTime samplePresentationEnd = sample->presentationEndTime();
    if (highestEnqueuedPresentationTime().isInvalid() || samplePresentationEnd > highestEnqueuedPresentationTime())
        setHighestEnqueuedPresentationTime(WTFMove(samplePresentationEnd));

    setLastEnqueuedDecodeKey({ sample->decodeTime(), sample->presentationTime() });
    setEnqueueDiscontinuityBoundary(sample->decodeTime() + sample->duration() + m_discontinuityTolerance);

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
        m_decodeQueue.insert(DecodeOrderSampleMap::MapType::value_type(decodeKey, WTFMove(copy)));
    }

    MediaTime previousSampleTime;

    // Fill the decode queue with the remaining samples.
    if (currentSampleDTSIterator != m_samples.decodeOrder().end()) {
        m_decodeQueue.insert(*currentSampleDTSIterator);
        m_minimumEnqueuedPresentationTime = Ref { currentSampleDTSIterator->second }->presentationTime();
        previousSampleTime = m_minimumEnqueuedPresentationTime;
    }
    for (auto iter = ++currentSampleDTSIterator; iter != m_samples.decodeOrder().end(); ++iter) {
        Ref sample = iter->second;
        if (sample->presentationTime() < time) {
            Ref copy = sample->createNonDisplayingCopy();
            DecodeOrderSampleMap::KeyType decodeKey(copy->decodeTime(), copy->presentationTime());
            m_decodeQueue.insert(DecodeOrderSampleMap::MapType::value_type(decodeKey, WTFMove(copy)));
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

    // Because we may have added artificial padding in the buffered ranges when adding samples, we may
    // need to remove that padding when removing those same samples. Walk over the erased ranges looking
    // for unbuffered areas and expand erasedRanges to encompass those areas.
    PlatformTimeRanges additionalErasedRanges;
    for (unsigned i = 0; i < erasedRanges.length(); ++i) {
        auto erasedStart = erasedRanges.start(i);
        auto erasedEnd = erasedRanges.end(i);
        auto startIterator = m_samples.presentationOrder().reverseFindSampleBeforePresentationTime(erasedStart);
        if (startIterator == m_samples.presentationOrder().rend())
            additionalErasedRanges.add(MediaTime::zeroTime(), erasedStart);
        else {
            Ref previousSample = startIterator->second.get();
            if (previousSample->presentationTime() + previousSample->duration() < erasedStart)
                additionalErasedRanges.add(previousSample->presentationTime() + previousSample->duration(), erasedStart);
        }

        auto endIterator = m_samples.presentationOrder().findSampleStartingAfterPresentationTime(erasedStart);
        if (endIterator == m_samples.presentationOrder().end())
            additionalErasedRanges.add(erasedEnd, MediaTime::positiveInfiniteTime());
        else {
            Ref nextSample = endIterator->second.get();
            if (nextSample->presentationTime() > erasedEnd)
                additionalErasedRanges.add(erasedEnd, nextSample->presentationTime());
        }
    }
    if (additionalErasedRanges.length())
        erasedRanges.unionWith(additionalErasedRanges);

#if !RELEASE_LOG_DISABLED
    if (bytesRemoved)
        DEBUG_LOG_IF(m_logger, logId, "removed ", bytesRemoved, ", start = ", earliestSample, ", end = ", latestSample);
#endif

    updateMinimumUpcomingPresentationTime();

    return erasedRanges;
}

static WARN_UNUSED_RETURN bool decodeTimeComparator(const PresentationOrderSampleMap::MapType::value_type& a, const PresentationOrderSampleMap::MapType::value_type& b)
{
    return Ref { a.second }->decodeTime() < Ref { b.second }->decodeTime();
};

int64_t TrackBuffer::removeCodedFrames(const MediaTime& start, const MediaTime& end, const MediaTime& currentTime)
{
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
    auto divideSampleIfPossibleAtPresentationTime = [&] (const MediaTime& time) {
        auto sampleIterator = m_samples.presentationOrder().findSampleContainingPresentationTime(time);
        if (sampleIterator == m_samples.presentationOrder().end())
            return;
        Ref sample = sampleIterator->second;
        if (!sample->isDivisable())
            return;
        MediaTime microsecond(1, 1000000);
        MediaTime roundedTime = roundTowardsTimeScaleWithRoundingMargin(time, sample->presentationTime().timeScale(), microsecond);
        std::pair<RefPtr<MediaSample>, RefPtr<MediaSample>> replacementSamples = sample->divide(roundedTime);
        if (!replacementSamples.first || !replacementSamples.second)
            return;
        DEBUG_LOG_IF(m_logger, LOGIDENTIFIER, "splitting sample ", sample.get(), " into ", Ref { *replacementSamples.first }.get(), " and ", Ref { *replacementSamples.second }.get());
        m_samples.removeSample(sample);
        m_samples.addSample(replacementSamples.first.releaseNonNull());
        m_samples.addSample(replacementSamples.second.releaseNonNull());
    };
    divideSampleIfPossibleAtPresentationTime(start);
    divideSampleIfPossibleAtPresentationTime(end);

    auto removePresentationStart = m_samples.presentationOrder().findSampleContainingOrAfterPresentationTime(start);
    auto removePresentationEnd = m_samples.presentationOrder().findSampleStartingOnOrAfterPresentationTime(end);
    if (removePresentationStart == removePresentationEnd)
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
    auto removePresentationStart = m_samples.presentationOrder().findSampleContainingOrAfterPresentationTime(start);
    auto removePresentationEnd = m_samples.presentationOrder().findSampleStartingOnOrAfterPresentationTime(end);
    if (removePresentationStart == removePresentationEnd)
        return 0;

    auto divideSampleIfPossibleAtPresentationTime = [&] (const MediaTime& time, bool dropFirstPart) -> int64_t  {
        auto sampleIterator = m_samples.presentationOrder().findSampleContainingPresentationTime(time);
        if (sampleIterator == m_samples.presentationOrder().end())
            return 0;
        Ref sample = sampleIterator->second;
        if (!sample->isDivisable())
            return 0;
        MediaTime microsecond(1, 1000000);
        MediaTime roundedTime = roundTowardsTimeScaleWithRoundingMargin(time, sample->presentationTime().timeScale(), microsecond);
        std::pair<RefPtr<MediaSample>, RefPtr<MediaSample>> replacementSamples = sample->divide(roundedTime);
        if (!replacementSamples.first || !replacementSamples.second)
            return 0;
        return dropFirstPart ? Ref { *replacementSamples.first }->sizeInBytes() : Ref { *replacementSamples.second }->sizeInBytes();
    };

    int64_t framesSize = 0;
    framesSize -= divideSampleIfPossibleAtPresentationTime(start, true);
    framesSize -= divideSampleIfPossibleAtPresentationTime(end, false);

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
    return JOIN_LOG_CHANNEL_WITH_PREFIX(LOG_CHANNEL_PREFIX, Media);
}
#endif

} // namespace WebCore

#endif // ENABLE(MEDIA_SOURCE)

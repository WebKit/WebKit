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
#include <wtf/CryptographicallyRandomNumber.h>

namespace WebCore {

static inline MediaTime roundTowardsTimeScaleWithRoundingMargin(const MediaTime& time, uint32_t timeScale, const MediaTime& roundingMargin)
{
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

void TrackBuffer::addBufferedRange(const MediaTime& start, const MediaTime& end)
{
    m_buffered.add(start, end);
}

void TrackBuffer::addSample(MediaSample& sample)
{
    m_samples.addSample(sample);
}

bool TrackBuffer::updateMinimumUpcomingPresentationTime()
{
    if (m_decodeQueue.empty()) {
        m_minimumEnqueuedPresentationTime = MediaTime::invalidTime();
        return false;
    }

    auto minPts = std::min_element(m_decodeQueue.begin(), m_decodeQueue.end(), [](auto& left, auto& right) -> bool {
        return left.second->presentationTime() < right.second->presentationTime();
    });

    if (minPts == m_decodeQueue.end()) {
        m_minimumEnqueuedPresentationTime = MediaTime::invalidTime();
        return false;
    }

    m_minimumEnqueuedPresentationTime = minPts->second->presentationTime();
    return true;
}

bool TrackBuffer::reenqueueMediaForTime(const MediaTime& time, const MediaTime& timeFudgeFactor)
{
    m_decodeQueue.clear();

    m_highestEnqueuedPresentationTime = MediaTime::invalidTime();
    m_lastEnqueuedDecodeKey = { MediaTime::invalidTime(), MediaTime::invalidTime() };
    m_enqueueDiscontinuityBoundary = time + m_discontinuityTolerance;

    // Find the sample which contains the current presentation time.
    auto currentSamplePTSIterator = m_samples.presentationOrder().findSampleContainingPresentationTime(time);

    if (currentSamplePTSIterator == m_samples.presentationOrder().end())
        currentSamplePTSIterator = m_samples.presentationOrder().findSampleStartingOnOrAfterPresentationTime(time);

    if (currentSamplePTSIterator == m_samples.presentationOrder().end()
        || (currentSamplePTSIterator->first - time) > timeFudgeFactor)
        return false;

    // Search backward for the previous sync sample.
    DecodeOrderSampleMap::KeyType decodeKey(currentSamplePTSIterator->second->decodeTime(), currentSamplePTSIterator->second->presentationTime());
    auto currentSampleDTSIterator = m_samples.decodeOrder().findSampleWithDecodeKey(decodeKey);
    ASSERT(currentSampleDTSIterator != m_samples.decodeOrder().end());

    auto reverseCurrentSampleIter = --DecodeOrderSampleMap::reverse_iterator(currentSampleDTSIterator);
    auto reverseLastSyncSampleIter = m_samples.decodeOrder().findSyncSamplePriorToDecodeIterator(reverseCurrentSampleIter);
    if (reverseLastSyncSampleIter == m_samples.decodeOrder().rend())
        return false;

    // Fill the decode queue with the non-displaying samples.
    for (auto iter = reverseLastSyncSampleIter; iter != reverseCurrentSampleIter; --iter) {
        auto copy = iter->second->createNonDisplayingCopy();
        DecodeOrderSampleMap::KeyType decodeKey(copy->decodeTime(), copy->presentationTime());
        m_decodeQueue.insert(DecodeOrderSampleMap::MapType::value_type(decodeKey, WTFMove(copy)));
    }

    // Fill the decode queue with the remaining samples.
    for (auto iter = currentSampleDTSIterator; iter != m_samples.decodeOrder().end(); ++iter)
        m_decodeQueue.insert(*iter);

    m_needsReenqueueing = false;
    
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
        RefPtr<MediaSample>& sample = futureSyncSampleIterator->second;
        futureSeekTime = sample->presentationTime();
    }

    auto pastSeekTime = MediaTime::negativeInfiniteTime();
    if (pastSyncSampleIterator != lowerBound) {
        RefPtr<MediaSample>& sample = pastSyncSampleIterator->second;
        pastSeekTime = sample->presentationTime();
    }

    return abs(targetTime - futureSeekTime) < abs(targetTime - pastSeekTime) ? futureSeekTime : pastSeekTime;
}

PlatformTimeRanges TrackBuffer::removeSamples(const DecodeOrderSampleMap::MapType& samples, const char* logPrefix)
{
#if !RELEASE_LOG_DISABLED
    auto logId = Logger::LogSiteIdentifier(logClassName(), logPrefix, logIdentifier());
    MediaTime earliestSample = MediaTime::positiveInfiniteTime();
    MediaTime latestSample = MediaTime::zeroTime();
    uint64_t bytesRemoved = 0;
#else
    UNUSED_PARAM(logPrefix);
#endif

    PlatformTimeRanges erasedRanges;
    for (const auto& sampleIt : samples) {
        const DecodeOrderSampleMap::KeyType& decodeKey = sampleIt.first;
#if !RELEASE_LOG_DISABLED
        uint64_t startBufferSize = m_samples.sizeInBytes();
#endif

        const RefPtr<MediaSample>& sample = sampleIt.second;

#if !RELEASE_LOG_DISABLED
        DEBUG_LOG_IF(m_logger, logId, "removing sample ", *sampleIt.second);
#endif

        // Remove the erased samples from the TrackBuffer sample map.
        m_samples.removeSample(sample.get());

        // Also remove the erased samples from the TrackBuffer decodeQueue.
        m_decodeQueue.erase(decodeKey);

        auto startTime = sample->presentationTime();
        auto endTime = startTime + sample->duration();
        erasedRanges.add(startTime, endTime);

#if !RELEASE_LOG_DISABLED
        bytesRemoved += startBufferSize - m_samples.sizeInBytes();
        if (startTime < earliestSample)
            earliestSample = startTime;
        if (endTime > latestSample)
            latestSample = endTime;
#endif
    }

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
            auto& previousSample = *startIterator->second;
            if (previousSample.presentationTime() + previousSample.duration() < erasedStart)
                additionalErasedRanges.add(previousSample.presentationTime() + previousSample.duration(), erasedStart);
        }

        auto endIterator = m_samples.presentationOrder().findSampleStartingAfterPresentationTime(erasedStart);
        if (endIterator == m_samples.presentationOrder().end())
            additionalErasedRanges.add(erasedEnd, MediaTime::positiveInfiniteTime());
        else {
            auto& nextSample = *endIterator->second;
            if (nextSample.presentationTime() > erasedEnd)
                additionalErasedRanges.add(erasedEnd, nextSample.presentationTime());
        }
    }
    if (additionalErasedRanges.length())
        erasedRanges.unionWith(additionalErasedRanges);

#if !RELEASE_LOG_DISABLED
    if (bytesRemoved)
        DEBUG_LOG_IF(m_logger, logId, "removed ", bytesRemoved, ", start = ", earliestSample, ", end = ", latestSample);
#endif

    return erasedRanges;
}

static WARN_UNUSED_RETURN bool decodeTimeComparator(const PresentationOrderSampleMap::MapType::value_type& a, const PresentationOrderSampleMap::MapType::value_type& b)
{
    return a.second->decodeTime() < b.second->decodeTime();
};

bool TrackBuffer::removeCodedFrames(const MediaTime& start, const MediaTime& end, const MediaTime& currentTime)
{
    // 3.5.9 Coded Frame Removal Algorithm
    // https://dvcs.w3.org/hg/html-media/raw-file/tip/media-source/media-source.html#sourcebuffer-coded-frame-removal
    
    // 3.1. Let remove end timestamp be the current value of duration
    // 3.2 If this track buffer has a random access point timestamp that is greater than or equal to end, then update
    // remove end timestamp to that random access point timestamp.
    // NOTE: Step 3.2 will be incorrect for any random access point timestamp whose decode time is later than the sample at end,
    // but whose presentation time is less than the sample at end. Skip this step until step 3.3 below.

    // NOTE: To handle MediaSamples which may be an amalgamation of multiple shorter samples, find samples whose presentation
    // interval straddles the start and end times, and divide them if possible:
    auto divideSampleIfPossibleAtPresentationTime = [&] (const MediaTime& time) {
        auto sampleIterator = m_samples.presentationOrder().findSampleContainingPresentationTime(time);
        if (sampleIterator == m_samples.presentationOrder().end())
            return;
        RefPtr<MediaSample> sample = sampleIterator->second;
        if (!sample->isDivisable())
            return;
        MediaTime microsecond(1, 1000000);
        MediaTime roundedTime = roundTowardsTimeScaleWithRoundingMargin(time, sample->presentationTime().timeScale(), microsecond);
        std::pair<RefPtr<MediaSample>, RefPtr<MediaSample>> replacementSamples = sample->divide(roundedTime);
        if (!replacementSamples.first || !replacementSamples.second)
            return;
        DEBUG_LOG_IF(m_logger, LOGIDENTIFIER, "splitting sample ", *sample, " into ", *replacementSamples.first, " and ", *replacementSamples.second);
        m_samples.removeSample(sample.get());
        m_samples.addSample(*replacementSamples.first);
        m_samples.addSample(*replacementSamples.second);
    };
    divideSampleIfPossibleAtPresentationTime(start);
    divideSampleIfPossibleAtPresentationTime(end);

    auto removePresentationStart = m_samples.presentationOrder().findSampleContainingOrAfterPresentationTime(start);
    auto removePresentationEnd = m_samples.presentationOrder().findSampleStartingOnOrAfterPresentationTime(end);
    if (removePresentationStart == removePresentationEnd)
        return false;

    // 3.3 Remove all media data, from this track buffer, that contain starting timestamps greater than or equal to
    // start and less than the remove end timestamp.
    // NOTE: frames must be removed in decode order, so that all dependant frames between the frame to be removed
    // and the next sync sample frame are removed. But we must start from the first sample in decode order, not
    // presentation order.
    auto minmaxDecodeTimeIterPair = std::minmax_element(removePresentationStart, removePresentationEnd, decodeTimeComparator);
    auto& firstSample = *minmaxDecodeTimeIterPair.first->second;
    auto& lastSample = *minmaxDecodeTimeIterPair.second->second;
    auto removeDecodeStart = m_samples.decodeOrder().findSampleWithDecodeKey({ firstSample.decodeTime(), firstSample.presentationTime() });
    auto removeDecodeLast = m_samples.decodeOrder().findSampleWithDecodeKey({ lastSample.decodeTime(), lastSample.presentationTime() });
    auto removeDecodeEnd = m_samples.decodeOrder().findSyncSampleAfterDecodeIterator(removeDecodeLast);

    DecodeOrderSampleMap::MapType erasedSamples(removeDecodeStart, removeDecodeEnd);
    
    PlatformTimeRanges erasedRanges = removeSamples(erasedSamples, "removeCodedFrames");

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
    return true;
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
    m_decodeQueue.clear();
}

void TrackBuffer::setRoundedTimestampOffset(const MediaTime& time, uint32_t timeScale, const MediaTime& roundingMargin)
{
    m_roundedTimestampOffset = roundTowardsTimeScaleWithRoundingMargin(time, timeScale, roundingMargin);
}

#if !RELEASE_LOG_DISABLED
void TrackBuffer::setLogger(const Logger& newLogger, const void* newLogIdentifier)
{
    m_logger = &newLogger;
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

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

#include "Logging.h"
#include "MediaDescription.h"
#include "MediaSample.h"
#include "PlatformTimeRanges.h"
#include "SampleMap.h"
#include "SourceBufferPrivateClient.h"
#include "TimeRanges.h"
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

SourceBufferPrivate::TrackBuffer::TrackBuffer()
    : lastDecodeTimestamp(MediaTime::invalidTime())
    , greatestDecodeDuration(MediaTime::invalidTime())
    , lastFrameDuration(MediaTime::invalidTime())
    , highestPresentationTimestamp(MediaTime::invalidTime())
    , highestEnqueuedPresentationTime(MediaTime::invalidTime())
    , lastEnqueuedDecodeKey({MediaTime::invalidTime(), MediaTime::invalidTime()})
    , enqueueDiscontinuityBoundary(discontinuityTolerance)
{
}

SourceBufferPrivate::SourceBufferPrivate()
    : m_buffered(TimeRanges::create())
{
}

SourceBufferPrivate::~SourceBufferPrivate() = default;

void SourceBufferPrivate::resetTimestampOffsetInTrackBuffers()
{
    for (auto& trackBuffer : m_trackBufferMap.values()) {
        trackBuffer.lastFrameTimescale = 0;
        trackBuffer.roundedTimestampOffset = MediaTime::invalidTime();
    }
}

void SourceBufferPrivate::setBufferedDirty(bool flag)
{
    if (m_client)
        m_client->sourceBufferPrivateBufferedDirtyChanged(flag);
}

void SourceBufferPrivate::resetTrackBuffers()
{
    for (auto& trackBufferPair : m_trackBufferMap.values()) {
        trackBufferPair.lastDecodeTimestamp = MediaTime::invalidTime();
        trackBufferPair.greatestDecodeDuration = MediaTime::invalidTime();
        trackBufferPair.lastFrameDuration = MediaTime::invalidTime();
        trackBufferPair.highestPresentationTimestamp = MediaTime::invalidTime();
        trackBufferPair.needRandomAccessFlag = true;
    }
}

void SourceBufferPrivate::updateHighestPresentationTimestamp()
{
    MediaTime highestTime;
    for (auto& trackBuffer : m_trackBufferMap.values()) {
        auto lastSampleIter = trackBuffer.samples.presentationOrder().rbegin();
        if (lastSampleIter == trackBuffer.samples.presentationOrder().rend())
            continue;
        highestTime = std::max(highestTime, lastSampleIter->first);
    }

    if (m_highestPresentationTimestamp == highestTime)
        return;

    m_highestPresentationTimestamp = highestTime;
    if (m_client)
        m_client->sourceBufferPrivateHighestPresentationTimestampChanged(m_highestPresentationTimestamp);
}

void SourceBufferPrivate::setBufferedRanges(const PlatformTimeRanges& timeRanges)
{
    m_buffered->ranges() = timeRanges;
    if (m_client)
        m_client->sourceBufferPrivateBufferedRangesChanged(m_buffered->ranges());
}

void SourceBufferPrivate::updateBufferedFromTrackBuffers(bool sourceIsEnded)
{
    // 3.1 Attributes, buffered
    // https://rawgit.com/w3c/media-source/45627646344eea0170dd1cbc5a3d508ca751abb8/media-source-respec.html#dom-sourcebuffer-buffered

    // 2. Let highest end time be the largest track buffer ranges end time across all the track buffers managed by this SourceBuffer object.
    MediaTime highestEndTime = MediaTime::negativeInfiniteTime();
    for (auto& trackBuffer : m_trackBufferMap.values()) {
        if (!trackBuffer.buffered.length())
            continue;
        highestEndTime = std::max(highestEndTime, trackBuffer.buffered.maximumBufferedTime());
    }

    // NOTE: Short circuit the following if none of the TrackBuffers have buffered ranges to avoid generating
    // a single range of {0, 0}.
    if (highestEndTime.isNegativeInfinite()) {
        setBufferedRanges(PlatformTimeRanges());
        return;
    }

    // 3. Let intersection ranges equal a TimeRange object containing a single range from 0 to highest end time.
    PlatformTimeRanges intersectionRanges { MediaTime::zeroTime(), highestEndTime };

    // 4. For each audio and video track buffer managed by this SourceBuffer, run the following steps:
    for (auto& trackBuffer : m_trackBufferMap.values()) {
        // 4.1 Let track ranges equal the track buffer ranges for the current track buffer.
        PlatformTimeRanges trackRanges = trackBuffer.buffered;
        if (!trackRanges.length())
            continue;

        // 4.2 If readyState is "ended", then set the end time on the last range in track ranges to highest end time.
        if (sourceIsEnded)
            trackRanges.add(trackRanges.maximumBufferedTime(), highestEndTime);

        // 4.3 Let new intersection ranges equal the intersection between the intersection ranges and the track ranges.
        // 4.4 Replace the ranges in intersection ranges with the new intersection ranges.
        intersectionRanges.intersectWith(trackRanges);
    }

    // 5. If intersection ranges does not contain the exact same range information as the current value of this attribute,
    //    then update the current value of this attribute to intersection ranges.
    setBufferedRanges(intersectionRanges);
    setBufferedDirty(true);
}

void SourceBufferPrivate::appendCompleted(bool parsingSucceeded, bool isEnded)
{
    DEBUG_LOG(LOGIDENTIFIER);

    // Resolve the changes in TrackBuffers' buffered ranges
    // into the SourceBuffer's buffered ranges
    updateBufferedFromTrackBuffers(isEnded);

    if (m_client) {
        m_client->sourceBufferPrivateAppendComplete(parsingSucceeded ? SourceBufferPrivateClient::AppendResult::AppendSucceeded : SourceBufferPrivateClient::AppendResult::ParsingFailed);
        m_client->sourceBufferPrivateReportExtraMemoryCost(totalTrackBufferSizeInBytes());
    }
}

void SourceBufferPrivate::reenqueSamples(const AtomString& trackID)
{
    if (!m_isAttached)
        return;

    auto it = m_trackBufferMap.find(trackID);
    if (it == m_trackBufferMap.end())
        return;

    auto& trackBuffer = it->value;
    trackBuffer.needsReenqueueing = true;
    reenqueueMediaForTime(trackBuffer, trackID, currentMediaTime());
}

void SourceBufferPrivate::seekToTime(const MediaTime& time)
{
    for (auto& trackBufferPair : m_trackBufferMap) {
        TrackBuffer& trackBuffer = trackBufferPair.value;
        const AtomString& trackID = trackBufferPair.key;

        trackBuffer.needsReenqueueing = true;
        reenqueueMediaForTime(trackBuffer, trackID, time);
    }
}

void SourceBufferPrivate::clearTrackBuffers()
{
    for (auto& trackBufferPair : m_trackBufferMap.values()) {
        trackBufferPair.samples.clear();
        trackBufferPair.decodeQueue.clear();
    }
}

void SourceBufferPrivate::bufferedSamplesForTrackId(const AtomString& trackId, CompletionHandler<void(Vector<String>&&)>&& completionHandler)
{
    auto it = m_trackBufferMap.find(trackId);
    if (it == m_trackBufferMap.end())
        completionHandler({ });

    auto& trackBuffer = it->value;
    Vector<String> sampleDescriptions;
    for (auto& pair : trackBuffer.samples.decodeOrder())
        sampleDescriptions.append(toString(*pair.second));

    completionHandler(WTFMove(sampleDescriptions));
}

MediaTime SourceBufferPrivate::fastSeekTimeForMediaTime(const MediaTime& targetTime, const MediaTime& negativeThreshold, const MediaTime& positiveThreshold)
{
    if (!m_client)
        return targetTime;

    MediaTime seekTime = targetTime;

    for (auto& trackBuffer : m_trackBufferMap.values()) {
        // Find the sample which contains the target time time.
        auto futureSyncSampleIterator = trackBuffer.samples.decodeOrder().findSyncSampleAfterPresentationTime(targetTime, positiveThreshold);
        auto pastSyncSampleIterator = trackBuffer.samples.decodeOrder().findSyncSamplePriorToPresentationTime(targetTime, negativeThreshold);
        auto upperBound = trackBuffer.samples.decodeOrder().end();
        auto lowerBound = trackBuffer.samples.decodeOrder().rend();

        if (futureSyncSampleIterator == upperBound && pastSyncSampleIterator == lowerBound)
            continue;

        MediaTime futureSeekTime = MediaTime::positiveInfiniteTime();
        if (futureSyncSampleIterator != upperBound) {
            RefPtr<MediaSample>& sample = futureSyncSampleIterator->second;
            futureSeekTime = sample->presentationTime();
        }

        MediaTime pastSeekTime = MediaTime::negativeInfiniteTime();
        if (pastSyncSampleIterator != lowerBound) {
            RefPtr<MediaSample>& sample = pastSyncSampleIterator->second;
            pastSeekTime = sample->presentationTime();
        }

        MediaTime trackSeekTime = abs(targetTime - futureSeekTime) < abs(targetTime - pastSeekTime) ? futureSeekTime : pastSeekTime;
        if (abs(targetTime - trackSeekTime) > abs(targetTime - seekTime))
            seekTime = trackSeekTime;
    }

    return seekTime;
}

void SourceBufferPrivate::updateMinimumUpcomingPresentationTime(TrackBuffer& trackBuffer, const AtomString& trackID)
{
    if (!canSetMinimumUpcomingPresentationTime(trackID))
        return;

    if (trackBuffer.decodeQueue.empty()) {
        trackBuffer.minimumEnqueuedPresentationTime = MediaTime::invalidTime();
        clearMinimumUpcomingPresentationTime(trackID);
        return;
    }

    auto minPts = std::min_element(trackBuffer.decodeQueue.begin(), trackBuffer.decodeQueue.end(), [](auto& left, auto& right) -> bool {
        return left.second->presentationTime() < right.second->presentationTime();
    });

    if (minPts == trackBuffer.decodeQueue.end()) {
        trackBuffer.minimumEnqueuedPresentationTime = MediaTime::invalidTime();
        clearMinimumUpcomingPresentationTime(trackID);
        return;
    }

    trackBuffer.minimumEnqueuedPresentationTime = minPts->second->presentationTime();
    setMinimumUpcomingPresentationTime(trackID, trackBuffer.minimumEnqueuedPresentationTime);
}

void SourceBufferPrivate::setMediaSourceEnded(bool isEnded)
{
    if (m_isMediaSourceEnded == isEnded)
        return;

    m_isMediaSourceEnded = isEnded;

    if (m_isMediaSourceEnded) {
        for (auto& trackBufferPair : m_trackBufferMap) {
            TrackBuffer& trackBuffer = trackBufferPair.value;
            const AtomString& trackID = trackBufferPair.key;

            trySignalAllSamplesInTrackEnqueued(trackBuffer, trackID);
        }
    }
}

void SourceBufferPrivate::trySignalAllSamplesInTrackEnqueued(TrackBuffer& trackBuffer, const AtomString& trackID)
{
    if (m_isMediaSourceEnded && trackBuffer.decodeQueue.empty()) {
        DEBUG_LOG(LOGIDENTIFIER, "All samples in track \"", trackID, "\" enqueued.");
        allSamplesInTrackEnqueued(trackID);
    }
}

void SourceBufferPrivate::provideMediaData(const AtomString& trackID)
{
    auto it = m_trackBufferMap.find(trackID);
    if (it == m_trackBufferMap.end())
        return;

    provideMediaData(it->value, trackID);
}

void SourceBufferPrivate::provideMediaData(TrackBuffer& trackBuffer, const AtomString& trackID)
{
    if (!m_isAttached || isSeeking())
        return;

#if !RELEASE_LOG_DISABLED
    unsigned enqueuedSamples = 0;
#endif

    if (trackBuffer.needsMinimumUpcomingPresentationTimeUpdating && canSetMinimumUpcomingPresentationTime(trackID)) {
        trackBuffer.minimumEnqueuedPresentationTime = MediaTime::invalidTime();
        clearMinimumUpcomingPresentationTime(trackID);
    }

    while (!trackBuffer.decodeQueue.empty()) {
        if (!isReadyForMoreSamples(trackID)) {
            DEBUG_LOG(LOGIDENTIFIER, "bailing early, track id ", trackID, " is not ready for more data");
            notifyClientWhenReadyForMoreSamples(trackID);
            break;
        }

        // FIXME(rdar://problem/20635969): Remove this re-entrancy protection when the aforementioned radar is resolved; protecting
        // against re-entrancy introduces a small inefficency when removing appended samples from the decode queue one at a time
        // rather than when all samples have been enqueued.
        auto sample = trackBuffer.decodeQueue.begin()->second;

        if (sample->decodeTime() > trackBuffer.enqueueDiscontinuityBoundary) {
            DEBUG_LOG(LOGIDENTIFIER, "bailing early because of unbuffered gap, new sample: ", sample->decodeTime(), " >= the current discontinuity boundary: ", trackBuffer.enqueueDiscontinuityBoundary);
            break;
        }

        // Remove the sample from the decode queue now.
        trackBuffer.decodeQueue.erase(trackBuffer.decodeQueue.begin());

        MediaTime samplePresentationEnd = sample->presentationTime() + sample->duration();
        if (trackBuffer.highestEnqueuedPresentationTime.isInvalid() || samplePresentationEnd > trackBuffer.highestEnqueuedPresentationTime)
            trackBuffer.highestEnqueuedPresentationTime = samplePresentationEnd;

        trackBuffer.lastEnqueuedDecodeKey = {sample->decodeTime(), sample->presentationTime()};
        trackBuffer.enqueueDiscontinuityBoundary = sample->decodeTime() + sample->duration() + discontinuityTolerance;

        enqueueSample(sample.releaseNonNull(), trackID);
#if !RELEASE_LOG_DISABLED
        ++enqueuedSamples;
#endif
    }

    updateMinimumUpcomingPresentationTime(trackBuffer, trackID);

#if !RELEASE_LOG_DISABLED
    DEBUG_LOG(LOGIDENTIFIER, "enqueued ", enqueuedSamples, " samples, ", static_cast<uint64_t>(trackBuffer.decodeQueue.size()), " remaining");
#endif

    trySignalAllSamplesInTrackEnqueued(trackBuffer, trackID);
}

void SourceBufferPrivate::reenqueueMediaForTime(TrackBuffer& trackBuffer, const AtomString& trackID, const MediaTime& time)
{
    flush(trackID);
    trackBuffer.decodeQueue.clear();

    trackBuffer.highestEnqueuedPresentationTime = MediaTime::invalidTime();
    trackBuffer.lastEnqueuedDecodeKey = {MediaTime::invalidTime(), MediaTime::invalidTime()};
    trackBuffer.enqueueDiscontinuityBoundary = time + discontinuityTolerance;

    // Find the sample which contains the current presentation time.
    auto currentSamplePTSIterator = trackBuffer.samples.presentationOrder().findSampleContainingPresentationTime(time);

    if (currentSamplePTSIterator == trackBuffer.samples.presentationOrder().end())
        currentSamplePTSIterator = trackBuffer.samples.presentationOrder().findSampleStartingOnOrAfterPresentationTime(time);

    if (currentSamplePTSIterator == trackBuffer.samples.presentationOrder().end()
        || (currentSamplePTSIterator->first - time) > timeFudgeFactor())
        return;

    // Seach backward for the previous sync sample.
    DecodeOrderSampleMap::KeyType decodeKey(currentSamplePTSIterator->second->decodeTime(), currentSamplePTSIterator->second->presentationTime());
    auto currentSampleDTSIterator = trackBuffer.samples.decodeOrder().findSampleWithDecodeKey(decodeKey);
    ASSERT(currentSampleDTSIterator != trackBuffer.samples.decodeOrder().end());

    auto reverseCurrentSampleIter = --DecodeOrderSampleMap::reverse_iterator(currentSampleDTSIterator);
    auto reverseLastSyncSampleIter = trackBuffer.samples.decodeOrder().findSyncSamplePriorToDecodeIterator(reverseCurrentSampleIter);
    if (reverseLastSyncSampleIter == trackBuffer.samples.decodeOrder().rend())
        return;

    // Fill the decode queue with the non-displaying samples.
    for (auto iter = reverseLastSyncSampleIter; iter != reverseCurrentSampleIter; --iter) {
        auto copy = iter->second->createNonDisplayingCopy();
        DecodeOrderSampleMap::KeyType decodeKey(copy->decodeTime(), copy->presentationTime());
        trackBuffer.decodeQueue.insert(DecodeOrderSampleMap::MapType::value_type(decodeKey, WTFMove(copy)));
    }

    // Fill the decode queue with the remaining samples.
    for (auto iter = currentSampleDTSIterator; iter != trackBuffer.samples.decodeOrder().end(); ++iter)
        trackBuffer.decodeQueue.insert(*iter);
    provideMediaData(trackBuffer, trackID);

    trackBuffer.needsReenqueueing = false;
}

void SourceBufferPrivate::reenqueueMediaIfNeeded(const MediaTime& currentTime, uint64_t pendingAppendDataCapacity, uint64_t maximumBufferSize)
{
    for (auto& trackBufferPair : m_trackBufferMap) {
        TrackBuffer& trackBuffer = trackBufferPair.value;
        const AtomString& trackID = trackBufferPair.key;

        if (trackBuffer.needsReenqueueing) {
            DEBUG_LOG(LOGIDENTIFIER, "reenqueuing at time ", currentTime);
            reenqueueMediaForTime(trackBuffer, trackID, currentTime);
        } else
            provideMediaData(trackBuffer, trackID);
    }

    if (totalTrackBufferSizeInBytes() + pendingAppendDataCapacity > maximumBufferSize)
        m_bufferFull = true;
}

static WARN_UNUSED_RETURN bool decodeTimeComparator(const PresentationOrderSampleMap::MapType::value_type& a, const PresentationOrderSampleMap::MapType::value_type& b)
{
    return a.second->decodeTime() < b.second->decodeTime();
}

#if !RELEASE_LOG_DISABLED
static PlatformTimeRanges removeSamplesFromTrackBuffer(const DecodeOrderSampleMap::MapType& samples, SourceBufferPrivate::TrackBuffer& trackBuffer, const SourceBufferPrivate* sourceBufferPrivate, const char* logPrefix)
#else
static PlatformTimeRanges removeSamplesFromTrackBuffer(const DecodeOrderSampleMap::MapType& samples, SourceBufferPrivate::TrackBuffer& trackBuffer)
#endif
{
#if !RELEASE_LOG_DISABLED
    MediaTime earliestSample = MediaTime::positiveInfiniteTime();
    MediaTime latestSample = MediaTime::zeroTime();
    uint64_t bytesRemoved = 0;
    auto logIdentifier = WTF::Logger::LogSiteIdentifier(sourceBufferPrivate->logClassName(), logPrefix, sourceBufferPrivate->logIdentifier());
    auto& logger = sourceBufferPrivate->logger();
    auto willLog = logger.willLog(sourceBufferPrivate->logChannel(), WTFLogLevel::Debug);
#endif

    PlatformTimeRanges erasedRanges;
    for (const auto& sampleIt : samples) {
        const DecodeOrderSampleMap::KeyType& decodeKey = sampleIt.first;
#if !RELEASE_LOG_DISABLED
        uint64_t startBufferSize = trackBuffer.samples.sizeInBytes();
#endif

        const RefPtr<MediaSample>& sample = sampleIt.second;

#if !RELEASE_LOG_DISABLED
        if (willLog)
            logger.debug(sourceBufferPrivate->logChannel(), logIdentifier, "removing sample ", *sampleIt.second);
#endif

        // Remove the erased samples from the TrackBuffer sample map.
        trackBuffer.samples.removeSample(sample.get());

        // Also remove the erased samples from the TrackBuffer decodeQueue.
        trackBuffer.decodeQueue.erase(decodeKey);

        auto startTime = sample->presentationTime();
        auto endTime = startTime + sample->duration();
        erasedRanges.add(startTime, endTime);

#if !RELEASE_LOG_DISABLED
        bytesRemoved += startBufferSize - trackBuffer.samples.sizeInBytes();
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
        auto startIterator = trackBuffer.samples.presentationOrder().reverseFindSampleBeforePresentationTime(erasedStart);
        if (startIterator == trackBuffer.samples.presentationOrder().rend())
            additionalErasedRanges.add(MediaTime::zeroTime(), erasedStart);
        else {
            auto& previousSample = *startIterator->second;
            if (previousSample.presentationTime() + previousSample.duration() < erasedStart)
                additionalErasedRanges.add(previousSample.presentationTime() + previousSample.duration(), erasedStart);
        }

        auto endIterator = trackBuffer.samples.presentationOrder().findSampleStartingOnOrAfterPresentationTime(erasedEnd);
        if (endIterator == trackBuffer.samples.presentationOrder().end())
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
    if (bytesRemoved && willLog)
        logger.debug(sourceBufferPrivate->logChannel(), logIdentifier, "removed ", bytesRemoved, ", start = ", earliestSample, ", end = ", latestSample);
#endif

    return erasedRanges;
}

void SourceBufferPrivate::removeCodedFrames(const MediaTime& start, const MediaTime& end, const MediaTime& currentTime, bool isEnded, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(start < end);
    if (start >= end) {
        completionHandler();
        return;
    }

    // 3.5.9 Coded Frame Removal Algorithm
    // https://dvcs.w3.org/hg/html-media/raw-file/tip/media-source/media-source.html#sourcebuffer-coded-frame-removal

    // 1. Let start be the starting presentation timestamp for the removal range.
    // 2. Let end be the end presentation timestamp for the removal range.
    // 3. For each track buffer in this source buffer, run the following steps:
    for (auto& trackBufferKeyValue : m_trackBufferMap) {
        TrackBuffer& trackBuffer = trackBufferKeyValue.value;
        AtomString trackID = trackBufferKeyValue.key;

        // 3.1. Let remove end timestamp be the current value of duration
        // 3.2 If this track buffer has a random access point timestamp that is greater than or equal to end, then update
        // remove end timestamp to that random access point timestamp.
        // NOTE: Step 3.2 will be incorrect for any random access point timestamp whose decode time is later than the sample at end,
        // but whose presentation time is less than the sample at end. Skip this step until step 3.3 below.

        // NOTE: To handle MediaSamples which may be an amalgamation of multiple shorter samples, find samples whose presentation
        // interval straddles the start and end times, and divide them if possible:
        auto divideSampleIfPossibleAtPresentationTime = [&] (const MediaTime& time) {
            auto sampleIterator = trackBuffer.samples.presentationOrder().findSampleContainingPresentationTime(time);
            if (sampleIterator == trackBuffer.samples.presentationOrder().end())
                return;
            RefPtr<MediaSample> sample = sampleIterator->second;
            if (!sample->isDivisable())
                return;
            std::pair<RefPtr<MediaSample>, RefPtr<MediaSample>> replacementSamples = sample->divide(time);
            if (!replacementSamples.first || !replacementSamples.second)
                return;
            DEBUG_LOG(LOGIDENTIFIER, "splitting sample ", *sample, " into ", *replacementSamples.first, " and ", *replacementSamples.second);
            trackBuffer.samples.removeSample(sample.get());
            trackBuffer.samples.addSample(*replacementSamples.first);
            trackBuffer.samples.addSample(*replacementSamples.second);
        };
        divideSampleIfPossibleAtPresentationTime(start);
        divideSampleIfPossibleAtPresentationTime(end);

        auto removePresentationStart = trackBuffer.samples.presentationOrder().findSampleContainingOrAfterPresentationTime(start);
        auto removePresentationEnd = trackBuffer.samples.presentationOrder().findSampleStartingOnOrAfterPresentationTime(end);
        if (removePresentationStart == removePresentationEnd)
            continue;

        // 3.3 Remove all media data, from this track buffer, that contain starting timestamps greater than or equal to
        // start and less than the remove end timestamp.
        // NOTE: frames must be removed in decode order, so that all dependant frames between the frame to be removed
        // and the next sync sample frame are removed. But we must start from the first sample in decode order, not
        // presentation order.
        auto minmaxDecodeTimeIterPair = std::minmax_element(removePresentationStart, removePresentationEnd, decodeTimeComparator);
        auto& firstSample = *minmaxDecodeTimeIterPair.first->second;
        auto& lastSample = *minmaxDecodeTimeIterPair.second->second;
        auto removeDecodeStart = trackBuffer.samples.decodeOrder().findSampleWithDecodeKey({firstSample.decodeTime(), firstSample.presentationTime()});
        auto removeDecodeLast = trackBuffer.samples.decodeOrder().findSampleWithDecodeKey({lastSample.decodeTime(), lastSample.presentationTime()});
        auto removeDecodeEnd = trackBuffer.samples.decodeOrder().findSyncSampleAfterDecodeIterator(removeDecodeLast);

        DecodeOrderSampleMap::MapType erasedSamples(removeDecodeStart, removeDecodeEnd);
#if !RELEASE_LOG_DISABLED
        PlatformTimeRanges erasedRanges = removeSamplesFromTrackBuffer(erasedSamples, trackBuffer, this, "removeCodedFrames");
#else
        PlatformTimeRanges erasedRanges = removeSamplesFromTrackBuffer(erasedSamples, trackBuffer);
#endif

        // Only force the TrackBuffer to re-enqueue if the removed ranges overlap with enqueued and possibly
        // not yet displayed samples.
        if (trackBuffer.highestEnqueuedPresentationTime.isValid() && currentTime < trackBuffer.highestEnqueuedPresentationTime) {
            PlatformTimeRanges possiblyEnqueuedRanges(currentTime, trackBuffer.highestEnqueuedPresentationTime);
            possiblyEnqueuedRanges.intersectWith(erasedRanges);
            if (possiblyEnqueuedRanges.length()) {
                trackBuffer.needsReenqueueing = true;
                DEBUG_LOG(LOGIDENTIFIER, "the range in removeCodedFrames() includes already enqueued samples, reenqueueing from ", currentTime);
                reenqueueMediaForTime(trackBuffer, trackID, currentTime);
            }
        }

        erasedRanges.invert();
        trackBuffer.buffered.intersectWith(erasedRanges);
        setBufferedDirty(true);

        // 3.4 If this object is in activeSourceBuffers, the current playback position is greater than or equal to start
        // and less than the remove end timestamp, and HTMLMediaElement.readyState is greater than HAVE_METADATA, then set
        // the HTMLMediaElement.readyState attribute to HAVE_METADATA and stall playback.
        if (isActive() && currentTime >= start && currentTime < end && readyState() > MediaPlayer::ReadyState::HaveMetadata)
            setReadyState(MediaPlayer::ReadyState::HaveMetadata);
    }

    updateBufferedFromTrackBuffers(isEnded);

    // 4. If buffer full flag equals true and this object is ready to accept more bytes, then set the buffer full flag to false.
    // No-op

    updateHighestPresentationTimestamp();

    LOG(Media, "SourceBuffer::removeCodedFrames(%p) - buffered = %s", this, toString(m_buffered->ranges()).utf8().data());

    completionHandler();
}

void SourceBufferPrivate::evictCodedFrames(uint64_t newDataSize, uint64_t pendingAppendDataCapacity, uint64_t maximumBufferSize, const MediaTime& currentTime, const MediaTime& duration, bool isEnded)
{
    // 3.5.13 Coded Frame Eviction Algorithm
    // http://www.w3.org/TR/media-source/#sourcebuffer-coded-frame-eviction

    if (!m_isAttached)
        return;

    // This algorithm is run to free up space in this source buffer when new data is appended.
    // 1. Let new data equal the data that is about to be appended to this SourceBuffer.
    // 2. If the buffer full flag equals false, then abort these steps.
    if (!m_bufferFull)
        return;

    // 3. Let removal ranges equal a list of presentation time ranges that can be evicted from
    // the presentation to make room for the new data.

    // NOTE: begin by removing data from the beginning of the buffered ranges, 30 seconds at
    // a time, up to 30 seconds before currentTime.
    MediaTime thirtySeconds = MediaTime(30, 1);
    MediaTime maximumRangeEnd = currentTime - thirtySeconds;

#if !RELEASE_LOG_DISABLED
    uint64_t initialBufferedSize = totalTrackBufferSizeInBytes();
    DEBUG_LOG(LOGIDENTIFIER, "currentTime = ", currentTime, ", require ", initialBufferedSize + newDataSize, " bytes, maximum buffer size is ", maximumBufferSize);
#endif

    MediaTime rangeStart = MediaTime::zeroTime();
    MediaTime rangeEnd = rangeStart + thirtySeconds;
    while (rangeStart < maximumRangeEnd) {
        // 4. For each range in removal ranges, run the coded frame removal algorithm with start and
        // end equal to the removal range start and end timestamp respectively.
        removeCodedFrames(rangeStart, std::min(rangeEnd, maximumRangeEnd), currentTime, isEnded);
        if (totalTrackBufferSizeInBytes() + pendingAppendDataCapacity + newDataSize < maximumBufferSize) {
            m_bufferFull = false;
            break;
        }

        rangeStart += thirtySeconds;
        rangeEnd += thirtySeconds;
    }

    if (!m_bufferFull) {
#if !RELEASE_LOG_DISABLED
        DEBUG_LOG(LOGIDENTIFIER, "evicted ", initialBufferedSize - totalTrackBufferSizeInBytes());
#endif
        return;
    }

    // If there still isn't enough free space and there buffers in time ranges after the current range (ie. there is a gap after
    // the current buffered range), delete 30 seconds at a time from duration back to the current time range or 30 seconds after
    // currenTime whichever we hit first.
    auto buffered = m_buffered->ranges();
    uint64_t currentTimeRange = buffered.find(currentTime);
    if (currentTimeRange == buffered.length() - 1) {
#if !RELEASE_LOG_DISABLED
        ERROR_LOG(LOGIDENTIFIER, "FAILED to free enough after evicting ", initialBufferedSize - totalTrackBufferSizeInBytes());
#endif
        return;
    }

    MediaTime minimumRangeStart = currentTime + thirtySeconds;

    rangeEnd = duration;
    if (!rangeEnd.isFinite()) {
        rangeEnd = buffered.maximumBufferedTime();
#if !RELEASE_LOG_DISABLED
        DEBUG_LOG(LOGIDENTIFIER, "MediaSource duration is not a finite value, using maximum buffered time: ", rangeEnd);
#endif
    }

    rangeStart = rangeEnd - thirtySeconds;
    while (rangeStart > minimumRangeStart) {
        // Do not evict data from the time range that contains currentTime.
        uint64_t startTimeRange = buffered.find(rangeStart);
        if (currentTimeRange != notFound && startTimeRange == currentTimeRange) {
            uint64_t endTimeRange = buffered.find(rangeEnd);
            if (currentTimeRange != notFound && endTimeRange == currentTimeRange)
                break;

            rangeEnd = buffered.start(endTimeRange);
        }

        // 4. For each range in removal ranges, run the coded frame removal algorithm with start and
        // end equal to the removal range start and end timestamp respectively.
        removeCodedFrames(std::max(minimumRangeStart, rangeStart), rangeEnd, currentTime, isEnded);

        if (totalTrackBufferSizeInBytes() + pendingAppendDataCapacity + newDataSize < maximumBufferSize) {
            m_bufferFull = false;
            break;
        }

        rangeStart -= thirtySeconds;
        rangeEnd -= thirtySeconds;
    }

#if !RELEASE_LOG_DISABLED
    if (m_bufferFull)
        ERROR_LOG(LOGIDENTIFIER, "FAILED to free enough after evicting ", initialBufferedSize - totalTrackBufferSizeInBytes());
    else
        DEBUG_LOG(LOGIDENTIFIER, "evicted ", initialBufferedSize - totalTrackBufferSizeInBytes());
#endif
}

uint64_t SourceBufferPrivate::totalTrackBufferSizeInBytes() const
{
    uint64_t totalSizeInBytes = 0;
    for (auto& trackBuffer : m_trackBufferMap.values())
        totalSizeInBytes += trackBuffer.samples.sizeInBytes();

    return totalSizeInBytes;
}

void SourceBufferPrivate::addTrackBuffer(const AtomString& trackId, RefPtr<MediaDescription>&& description)
{
    ASSERT(!m_trackBufferMap.contains(trackId));
    auto& trackBuffer = m_trackBufferMap.add(trackId, TrackBuffer()).iterator->value;

    // 5.2.9 Add the track description for this track to the track buffer.
    trackBuffer.description = description;

    m_hasAudio = m_hasAudio || trackBuffer.description->isAudio();
    m_hasVideo = m_hasVideo || trackBuffer.description->isVideo();
}

void SourceBufferPrivate::updateTrackIds(Vector<std::pair<AtomString, AtomString>>&& trackIdPairs)
{
    for (auto& trackIdPair : trackIdPairs) {
        auto oldId = trackIdPair.first;
        auto newId = trackIdPair.second;
        ASSERT(oldId != newId);
        auto trackBuffer = m_trackBufferMap.take(oldId);
        m_trackBufferMap.add(newId, WTFMove(trackBuffer));
    }
}

void SourceBufferPrivate::setAllTrackBuffersNeedRandomAccess()
{
    for (auto& trackBuffer : m_trackBufferMap.values())
        trackBuffer.needRandomAccessFlag = true;
}

void SourceBufferPrivate::didReceiveInitializationSegment(SourceBufferPrivateClient::InitializationSegment&& segment, CompletionHandler<void()>&& completionHandler)
{
    if (!m_client) {
        completionHandler();
        return;
    }

    if (m_receivedFirstInitializationSegment && !validateInitializationSegment(segment)) {
        m_client->sourceBufferPrivateAppendError(true);
        return;
    }

    m_client->sourceBufferPrivateDidReceiveInitializationSegment(WTFMove(segment), WTFMove(completionHandler));

    m_receivedFirstInitializationSegment = true;
    m_pendingInitializationSegmentForChangeType = false;
}

bool SourceBufferPrivate::validateInitializationSegment(const SourceBufferPrivateClient::InitializationSegment& segment)
{
    //   * If more than one track for a single type are present (ie 2 audio tracks), then the Track
    //   IDs match the ones in the first initialization segment.
    if (segment.audioTracks.size() >= 2) {
        for (auto& audioTrackInfo : segment.audioTracks) {
            if (!m_trackBufferMap.contains(audioTrackInfo.track->id()))
                return false;
        }
    }

    if (segment.videoTracks.size() >= 2) {
        for (auto& videoTrackInfo : segment.videoTracks) {
            if (!m_trackBufferMap.contains(videoTrackInfo.track->id()))
                return false;
        }
    }

    if (segment.textTracks.size() >= 2) {
        for (auto& textTrackInfo : segment.videoTracks) {
            if (!m_trackBufferMap.contains(textTrackInfo.track->id()))
                return false;
        }
    }

    return true;
}

void SourceBufferPrivate::didReceiveSample(Ref<MediaSample>&& originalSample)
{
    if (!m_isAttached)
        return;

    // 3.5.1 Segment Parser Loop
    // 6.1 If the first initialization segment received flag is false, (Note: Issue # 155 & changeType()
    // algorithm) or the  pending initialization segment for changeType flag  is true, (End note)
    // then run the append error algorithm
    //     with the decode error parameter set to true and abort this algorithm.
    // Note: current design makes SourceBuffer somehow ignorant of append state - it's more a thing
    //  of SourceBufferPrivate. That's why this check can't really be done in appendInternal.
    //  unless we force some kind of design with state machine switching.

    if ((!m_receivedFirstInitializationSegment || m_pendingInitializationSegmentForChangeType) && m_client) {
        m_client->sourceBufferPrivateAppendError(true);
        return;
    }

    // 3.5.8 Coded Frame Processing
    // http://www.w3.org/TR/media-source/#sourcebuffer-coded-frame-processing

    // When complete coded frames have been parsed by the segment parser loop then the following steps
    // are run:
    // 1. For each coded frame in the media segment run the following steps:
    // 1.1. Loop Top

    Ref<MediaSample> sample = WTFMove(originalSample);

    do {
        MediaTime presentationTimestamp;
        MediaTime decodeTimestamp;

        // NOTE: this is out-of-order, but we need the timescale from the
        // sample's duration for timestamp generation.
        // 1.2 Let frame duration be a double precision floating point representation of the coded frame's
        // duration in seconds.
        MediaTime frameDuration = sample->duration();

        if (m_shouldGenerateTimestamps) {
            // ↳ If generate timestamps flag equals true:
            // 1. Let presentation timestamp equal 0.
            // NOTE: Use the duration timscale for the presentation timestamp, as this will eliminate
            // timescale rounding when generating timestamps.
            presentationTimestamp = { 0, frameDuration.timeScale() };

            // 2. Let decode timestamp equal 0.
            decodeTimestamp = { 0, frameDuration.timeScale() };
        } else {
            // ↳ Otherwise:
            // 1. Let presentation timestamp be a double precision floating point representation of
            // the coded frame's presentation timestamp in seconds.
            presentationTimestamp = sample->presentationTime();

            // 2. Let decode timestamp be a double precision floating point representation of the coded frame's
            // decode timestamp in seconds.
            decodeTimestamp = sample->decodeTime();
        }

        // 1.3 If mode equals "sequence" and group start timestamp is set, then run the following steps:
        if (m_appendMode == SourceBufferAppendMode::Sequence && m_groupStartTimestamp.isValid()) {
            // 1.3.1 Set timestampOffset equal to group start timestamp - presentation timestamp.
            m_timestampOffset = m_groupStartTimestamp;

            for (auto& trackBuffer : m_trackBufferMap.values()) {
                trackBuffer.lastFrameTimescale = 0;
                trackBuffer.roundedTimestampOffset = MediaTime::invalidTime();
            }

            // 1.3.2 Set group end timestamp equal to group start timestamp.
            m_groupEndTimestamp = m_groupStartTimestamp;

            // 1.3.3 Set the need random access point flag on all track buffers to true.
            for (auto& trackBuffer : m_trackBufferMap.values())
                trackBuffer.needRandomAccessFlag = true;

            // 1.3.4 Unset group start timestamp.
            m_groupStartTimestamp = MediaTime::invalidTime();
        }

        // NOTE: this is out-of-order, but we need TrackBuffer to be able to cache the results of timestamp offset rounding
        // 1.5 Let track buffer equal the track buffer that the coded frame will be added to.
        AtomString trackID = sample->trackID();
        auto it = m_trackBufferMap.find(trackID);
        if (it == m_trackBufferMap.end()) {
            // The client managed to append a sample with a trackID not present in the initialization
            // segment. This would be a good place to post an message to the developer console.
            m_client->sourceBufferPrivateDidDropSample();
            return;
        }
        TrackBuffer& trackBuffer = it->value;

        MediaTime microsecond(1, 1000000);

        auto roundTowardsTimeScaleWithRoundingMargin = [] (const MediaTime& time, uint32_t timeScale, const MediaTime& roundingMargin) {
            while (true) {
                MediaTime roundedTime = time.toTimeScale(timeScale);
                if (abs(roundedTime - time) < roundingMargin || timeScale >= MediaTime::MaximumTimeScale)
                    return roundedTime;

                if (!WTF::safeMultiply(timeScale, 2, timeScale) || timeScale > MediaTime::MaximumTimeScale)
                    timeScale = MediaTime::MaximumTimeScale;
            }
        };

        // 1.4 If timestampOffset is not 0, then run the following steps:
        if (m_timestampOffset) {
            if (!trackBuffer.roundedTimestampOffset.isValid() || presentationTimestamp.timeScale() != trackBuffer.lastFrameTimescale) {
                trackBuffer.lastFrameTimescale = presentationTimestamp.timeScale();
                trackBuffer.roundedTimestampOffset = roundTowardsTimeScaleWithRoundingMargin(m_timestampOffset, trackBuffer.lastFrameTimescale, microsecond);
            }

            // 1.4.1 Add timestampOffset to the presentation timestamp.
            presentationTimestamp += trackBuffer.roundedTimestampOffset;

            // 1.4.2 Add timestampOffset to the decode timestamp.
            decodeTimestamp += trackBuffer.roundedTimestampOffset;
        }

        // 1.6 ↳ If last decode timestamp for track buffer is set and decode timestamp is less than last
        // decode timestamp:
        // OR
        // ↳ If last decode timestamp for track buffer is set and the difference between decode timestamp and
        // last decode timestamp is greater than 2 times last frame duration:
        MediaTime decodeDurationToCheck = trackBuffer.greatestDecodeDuration;

        if (decodeDurationToCheck.isValid() && trackBuffer.lastFrameDuration.isValid()
            && (trackBuffer.lastFrameDuration > decodeDurationToCheck))
            decodeDurationToCheck = trackBuffer.lastFrameDuration;

        if (trackBuffer.lastDecodeTimestamp.isValid() && (decodeTimestamp < trackBuffer.lastDecodeTimestamp
            || (decodeDurationToCheck.isValid() && abs(decodeTimestamp - trackBuffer.lastDecodeTimestamp) > (decodeDurationToCheck * 2)))) {

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

            for (auto& trackBuffer : m_trackBufferMap.values()) {
                // 1.6.2 Unset the last decode timestamp on all track buffers.
                trackBuffer.lastDecodeTimestamp = MediaTime::invalidTime();
                // 1.6.3 Unset the last frame duration on all track buffers.
                trackBuffer.greatestDecodeDuration = MediaTime::invalidTime();
                trackBuffer.lastFrameDuration = MediaTime::invalidTime();
                // 1.6.4 Unset the highest presentation timestamp on all track buffers.
                trackBuffer.highestPresentationTimestamp = MediaTime::invalidTime();
                // 1.6.5 Set the need random access point flag on all track buffers to true.
                trackBuffer.needRandomAccessFlag = true;
            }

            // 1.6.6 Jump to the Loop Top step above to restart processing of the current coded frame.
            continue;
        }

        if (m_appendMode == SourceBufferAppendMode::Sequence) {
            // Use the generated timestamps instead of the sample's timestamps.
            sample->setTimestamps(presentationTimestamp, decodeTimestamp);
        } else if (trackBuffer.roundedTimestampOffset) {
            // Reflect the timestamp offset into the sample.
            sample->offsetTimestampsBy(trackBuffer.roundedTimestampOffset);
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
        if (presentationTimestamp < m_appendWindowStart || frameEndTimestamp > m_appendWindowEnd) {
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
                std::pair<RefPtr<MediaSample>, RefPtr<MediaSample>> replacementSamples = sample->divide(m_appendWindowStart);
                if (replacementSamples.second) {
                    replacementSamples = replacementSamples.second->divide(m_appendWindowEnd);
                    if (replacementSamples.first) {
                        sample = replacementSamples.first.releaseNonNull();
                        if (m_appendMode != SourceBufferAppendMode::Sequence && trackBuffer.roundedTimestampOffset)
                            sample->offsetTimestampsBy(-trackBuffer.roundedTimestampOffset);
                        continue;
                    }
                }
            }
            trackBuffer.needRandomAccessFlag = true;
            m_client->sourceBufferPrivateDidDropSample();
            return;
        }

        // If the decode timestamp is less than the presentation start time, then run the end of stream
        // algorithm with the error parameter set to "decode", and abort these steps.
        // NOTE: Until <https://www.w3.org/Bugs/Public/show_bug.cgi?id=27487> is resolved, we will only check
        // the presentation timestamp.
        MediaTime presentationStartTime = MediaTime::zeroTime();
        if (presentationTimestamp < presentationStartTime) {
            ERROR_LOG(LOGIDENTIFIER, "failing because presentationTimestamp (", presentationTimestamp, ") < presentationStartTime (", presentationStartTime, ")");
            m_client->sourceBufferPrivateStreamEndedWithDecodeError();
            return;
        }

        // 1.10 If the need random access point flag on track buffer equals true, then run the following steps:
        if (trackBuffer.needRandomAccessFlag) {
            // 1.11.1 If the coded frame is not a random access point, then drop the coded frame and jump
            // to the top of the loop to start processing the next coded frame.
            if (!sample->isSync()) {
                m_client->sourceBufferPrivateDidDropSample();
                return;
            }

            // 1.11.2 Set the need random access point flag on track buffer to false.
            trackBuffer.needRandomAccessFlag = false;
        }

        // 1.11 Let spliced audio frame be an unset variable for holding audio splice information
        // 1.12 Let spliced timed text frame be an unset variable for holding timed text splice information
        // FIXME: Add support for sample splicing.

        SampleMap erasedSamples;

        // 1.13 If last decode timestamp for track buffer is unset and presentation timestamp
        // falls within the presentation interval of a coded frame in track buffer, then run the
        // following steps:
        if (trackBuffer.lastDecodeTimestamp.isInvalid()) {
            auto iter = trackBuffer.samples.presentationOrder().findSampleContainingPresentationTime(presentationTimestamp);
            if (iter != trackBuffer.samples.presentationOrder().end()) {
                // 1.13.1 Let overlapped frame be the coded frame in track buffer that matches the condition above.
                RefPtr<MediaSample> overlappedFrame = iter->second;

                // 1.13.2 If track buffer contains audio coded frames:
                // Run the audio splice frame algorithm and if a splice frame is returned, assign it to
                // spliced audio frame.
                // FIXME: Add support for sample splicing.

                // If track buffer contains video coded frames:
                if (trackBuffer.description && trackBuffer.description->isVideo()) {
                    // 1.13.2.1 Let overlapped frame presentation timestamp equal the presentation timestamp
                    // of overlapped frame.
                    MediaTime overlappedFramePresentationTimestamp = overlappedFrame->presentationTime();

                    // 1.13.2.2 Let remove window timestamp equal overlapped frame presentation timestamp
                    // plus 1 microsecond.
                    MediaTime removeWindowTimestamp = overlappedFramePresentationTimestamp + microsecond;

                    // 1.13.2.3 If the presentation timestamp is less than the remove window timestamp,
                    // then remove overlapped frame and any coded frames that depend on it from track buffer.
                    if (presentationTimestamp < removeWindowTimestamp)
                        erasedSamples.addSample(*iter->second);
                }

                // If track buffer contains timed text coded frames:
                // Run the text splice frame algorithm and if a splice frame is returned, assign it to spliced timed text frame.
                // FIXME: Add support for sample splicing.
            }
        }

        // 1.14 Remove existing coded frames in track buffer:
        // If highest presentation timestamp for track buffer is not set:
        if (trackBuffer.highestPresentationTimestamp.isInvalid()) {
            // Remove all coded frames from track buffer that have a presentation timestamp greater than or
            // equal to presentation timestamp and less than frame end timestamp.
            auto iterPair = trackBuffer.samples.presentationOrder().findSamplesBetweenPresentationTimes(presentationTimestamp, frameEndTimestamp);
            if (iterPair.first != trackBuffer.samples.presentationOrder().end())
                erasedSamples.addRange(iterPair.first, iterPair.second);
        }

        // When appending media containing B-frames (media whose samples' presentation timestamps
        // do not increase monotonically, the prior erase steps could leave a sample in the trackBuffer
        // which will be disconnected from its previous I-frame. If the incoming frame is an I-frame,
        // remove all samples in decode order between the incoming I-frame's decode timestamp and the
        // next I-frame. See <https://github.com/w3c/media-source/issues/187> for a discussion of what
        // the how the MSE specification should handlie this secnario.
        do {
            if (!sample->isSync())
                break;

            DecodeOrderSampleMap::KeyType decodeKey(sample->decodeTime(), sample->presentationTime());
            auto nextSampleInDecodeOrder = trackBuffer.samples.decodeOrder().findSampleAfterDecodeKey(decodeKey);
            if (nextSampleInDecodeOrder == trackBuffer.samples.decodeOrder().end())
                break;

            if (nextSampleInDecodeOrder->second->isSync())
                break;

            auto nextSyncSample = trackBuffer.samples.decodeOrder().findSyncSampleAfterDecodeIterator(nextSampleInDecodeOrder);
            INFO_LOG(LOGIDENTIFIER, "Discovered out-of-order frames, from: ", *nextSampleInDecodeOrder->second, " to: ", (nextSyncSample == trackBuffer.samples.decodeOrder().end() ? "[end]"_s : toString(*nextSyncSample->second)));
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
        if (trackBuffer.highestPresentationTimestamp.isValid() && trackBuffer.highestPresentationTimestamp - contiguousFrameTolerance <= presentationTimestamp) {
            // Remove all coded frames from track buffer that have a presentation timestamp greater than highest
            // presentation timestamp and less than or equal to frame end timestamp.
            do {
                // NOTE: Searching from the end of the trackBuffer will be vastly more efficient if the search range is
                // near the end of the buffered range. Use a linear-backwards search if the search range is within one
                // frame duration of the end:
                unsigned bufferedLength = trackBuffer.buffered.length();
                if (!bufferedLength)
                    break;

                MediaTime highestBufferedTime = trackBuffer.buffered.maximumBufferedTime();
                MediaTime eraseBeginTime = trackBuffer.highestPresentationTimestamp - contiguousFrameTolerance;
                MediaTime eraseEndTime = frameEndTimestamp - contiguousFrameTolerance;

                PresentationOrderSampleMap::iterator_range range;
                if (highestBufferedTime - trackBuffer.highestPresentationTimestamp < trackBuffer.lastFrameDuration) {
                    // If the new frame is at the end of the buffered ranges, perform a sequential scan from end (O(1)).
                    range = trackBuffer.samples.presentationOrder().findSamplesBetweenPresentationTimesFromEnd(eraseBeginTime, eraseEndTime);
                } else {
                    // In any other case, perform a binary search (O(log(n)).
                    range = trackBuffer.samples.presentationOrder().findSamplesBetweenPresentationTimes(eraseBeginTime, eraseEndTime);
                }

                if (range.first != trackBuffer.samples.presentationOrder().end())
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
            auto firstDecodeIter = trackBuffer.samples.decodeOrder().findSampleWithDecodeKey(erasedSamples.decodeOrder().begin()->first);
            auto lastDecodeIter = trackBuffer.samples.decodeOrder().findSampleWithDecodeKey(erasedSamples.decodeOrder().rbegin()->first);
            auto nextSyncIter = trackBuffer.samples.decodeOrder().findSyncSampleAfterDecodeIterator(lastDecodeIter);
            dependentSamples.insert(firstDecodeIter, nextSyncIter);

            // NOTE: in the case of b-frames, the previous step may leave in place samples whose presentation
            // timestamp < presentationTime, but whose decode timestamp >= decodeTime. These will eventually cause
            // a decode error if left in place, so remove these samples as well.
            DecodeOrderSampleMap::KeyType decodeKey(sample->decodeTime(), sample->presentationTime());
            auto samplesWithHigherDecodeTimes = trackBuffer.samples.decodeOrder().findSamplesBetweenDecodeKeys(decodeKey, erasedSamples.decodeOrder().begin()->first);
            if (samplesWithHigherDecodeTimes.first != samplesWithHigherDecodeTimes.second)
                dependentSamples.insert(samplesWithHigherDecodeTimes.first, samplesWithHigherDecodeTimes.second);

#if !RELEASE_LOG_DISABLED
            PlatformTimeRanges erasedRanges = removeSamplesFromTrackBuffer(dependentSamples, trackBuffer, this, "didReceiveSample");
#else
            PlatformTimeRanges erasedRanges = removeSamplesFromTrackBuffer(dependentSamples, trackBuffer);
#endif

            // Only force the TrackBuffer to re-enqueue if the removed ranges overlap with enqueued and possibly
            // not yet displayed samples.
            MediaTime currentTime = currentMediaTime();
            if (trackBuffer.highestEnqueuedPresentationTime.isValid() && currentTime < trackBuffer.highestEnqueuedPresentationTime) {
                PlatformTimeRanges possiblyEnqueuedRanges(currentTime, trackBuffer.highestEnqueuedPresentationTime);
                possiblyEnqueuedRanges.intersectWith(erasedRanges);
                if (possiblyEnqueuedRanges.length())
                    trackBuffer.needsReenqueueing = true;
            }

            erasedRanges.invert();
            trackBuffer.buffered.intersectWith(erasedRanges);
            setBufferedDirty(true);
        }

        // 1.16 If spliced audio frame is set:
        // Add spliced audio frame to the track buffer.
        // If spliced timed text frame is set:
        // Add spliced timed text frame to the track buffer.
        // FIXME: Add support for sample splicing.

        // Otherwise:
        // Add the coded frame with the presentation timestamp, decode timestamp, and frame duration to the track buffer.
        trackBuffer.samples.addSample(sample);

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
        DecodeOrderSampleMap::KeyType decodeKey(sample->decodeTime(), sample->presentationTime());
        if (trackBuffer.lastEnqueuedDecodeKey.first.isInvalid() || decodeKey > trackBuffer.lastEnqueuedDecodeKey) {
            trackBuffer.decodeQueue.insert(DecodeOrderSampleMap::MapType::value_type(decodeKey, &sample.get()));

            if (trackBuffer.minimumEnqueuedPresentationTime.isValid() && sample->presentationTime() < trackBuffer.minimumEnqueuedPresentationTime)
                trackBuffer.needsMinimumUpcomingPresentationTimeUpdating = true;
        }

        // NOTE: the spec considers "Coded Frame Duration" to be the presentation duration, but this is not necessarily equal
        // to the decoded duration. When comparing deltas between decode timestamps, the decode duration, not the presentation.
        if (trackBuffer.lastDecodeTimestamp.isValid()) {
            MediaTime lastDecodeDuration = decodeTimestamp - trackBuffer.lastDecodeTimestamp;
            if (!trackBuffer.greatestDecodeDuration.isValid() || lastDecodeDuration > trackBuffer.greatestDecodeDuration)
                trackBuffer.greatestDecodeDuration = lastDecodeDuration;
        }

        // 1.17 Set last decode timestamp for track buffer to decode timestamp.
        trackBuffer.lastDecodeTimestamp = decodeTimestamp;

        // 1.18 Set last frame duration for track buffer to frame duration.
        trackBuffer.lastFrameDuration = frameDuration;

        // 1.19 If highest presentation timestamp for track buffer is unset or frame end timestamp is greater
        // than highest presentation timestamp, then set highest presentation timestamp for track buffer
        // to frame end timestamp.
        if (trackBuffer.highestPresentationTimestamp.isInvalid() || frameEndTimestamp > trackBuffer.highestPresentationTimestamp)
            trackBuffer.highestPresentationTimestamp = frameEndTimestamp;

        // 1.20 If frame end timestamp is greater than group end timestamp, then set group end timestamp equal
        // to frame end timestamp.
        if (m_groupEndTimestamp.isInvalid() || frameEndTimestamp > m_groupEndTimestamp)
            m_groupEndTimestamp = frameEndTimestamp;

        // 1.21 If generate timestamps flag equals true, then set timestampOffset equal to frame end timestamp.
        if (m_shouldGenerateTimestamps) {
            m_timestampOffset = frameEndTimestamp;
            for (auto& trackBuffer : m_trackBufferMap.values()) {
                trackBuffer.lastFrameTimescale = 0;
                trackBuffer.roundedTimestampOffset = MediaTime::invalidTime();
            }
        }

        // Eliminate small gaps between buffered ranges by coalescing
        // disjoint ranges separated by less than a "fudge factor".
        auto presentationEndTime = presentationTimestamp + frameDuration;
        auto nearestToPresentationStartTime = trackBuffer.buffered.nearest(presentationTimestamp);
        if (nearestToPresentationStartTime.isValid() && (presentationTimestamp - nearestToPresentationStartTime).isBetween(MediaTime::zeroTime(), timeFudgeFactor()))
            presentationTimestamp = nearestToPresentationStartTime;

        auto nearestToPresentationEndTime = trackBuffer.buffered.nearest(presentationEndTime);
        if (nearestToPresentationEndTime.isValid() && (nearestToPresentationEndTime - presentationEndTime).isBetween(MediaTime::zeroTime(), timeFudgeFactor()))
            presentationEndTime = nearestToPresentationEndTime;

        trackBuffer.buffered.add(presentationTimestamp, presentationEndTime);
        m_client->sourceBufferPrivateDidParseSample(frameDuration.toDouble());
        setBufferedDirty(true);

        break;
    } while (true);

    // Steps 2-4 will be handled by MediaSource::monitorSourceBuffers()

    // 5. If the media segment contains data beyond the current duration, then run the duration change algorithm with new
    // duration set to the maximum of the current duration and the group end timestamp.
    if (m_groupEndTimestamp > duration())
        m_client->sourceBufferPrivateDurationChanged(m_groupEndTimestamp);

    updateHighestPresentationTimestamp();
}

} // namespace WebCore

#endif

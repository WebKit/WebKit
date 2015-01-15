/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 * Copyright (C) 2013-2014 Apple Inc. All rights reserved.
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
#include "SourceBuffer.h"

#if ENABLE(MEDIA_SOURCE)

#include "AudioTrackList.h"
#include "Event.h"
#include "ExceptionCodePlaceholder.h"
#include "GenericEventQueue.h"
#include "HTMLMediaElement.h"
#include "InbandTextTrack.h"
#include "Logging.h"
#include "MediaDescription.h"
#include "MediaSample.h"
#include "MediaSource.h"
#include "SampleMap.h"
#include "SourceBufferPrivate.h"
#include "TextTrackList.h"
#include "TimeRanges.h"
#include "VideoTrackList.h"
#include <limits>
#include <map>
#include <runtime/JSCInlines.h>
#include <runtime/JSLock.h>
#include <runtime/VM.h>
#include <wtf/CurrentTime.h>
#include <wtf/NeverDestroyed.h>
#if !LOG_DISABLED
#include <wtf/text/StringBuilder.h>
#endif

namespace WebCore {

static double ExponentialMovingAverageCoefficient = 0.1;

// Allow hasCurrentTime() to be off by as much as the length of a 24fps video frame
static const MediaTime& currentTimeFudgeFactor()
{
    static NeverDestroyed<MediaTime> fudgeFactor(1, 24);
    return fudgeFactor;
}

const AtomicString& SourceBuffer::segmentsKeyword()
{
    static NeverDestroyed<AtomicString> segments("segments");
    return segments.get();
}

const AtomicString& SourceBuffer::sequenceKeyword()
{
    static NeverDestroyed<AtomicString> segments("sequence");
    return segments.get();
}

struct SourceBuffer::TrackBuffer {
    MediaTime lastDecodeTimestamp;
    MediaTime lastFrameDuration;
    MediaTime highestPresentationTimestamp;
    MediaTime lastEnqueuedPresentationTime;
    MediaTime lastEnqueuedDecodeEndTime;
    bool needRandomAccessFlag;
    bool enabled;
    bool needsReenqueueing;
    SampleMap samples;
    DecodeOrderSampleMap::MapType decodeQueue;
    RefPtr<MediaDescription> description;

    TrackBuffer()
        : lastDecodeTimestamp(MediaTime::invalidTime())
        , lastFrameDuration(MediaTime::invalidTime())
        , highestPresentationTimestamp(MediaTime::invalidTime())
        , lastEnqueuedPresentationTime(MediaTime::invalidTime())
        , lastEnqueuedDecodeEndTime(MediaTime::invalidTime())
        , needRandomAccessFlag(true)
        , enabled(false)
        , needsReenqueueing(false)
    {
    }
};

PassRef<SourceBuffer> SourceBuffer::create(PassRef<SourceBufferPrivate> sourceBufferPrivate, MediaSource* source)
{
    RefPtr<SourceBuffer> sourceBuffer(adoptRef(new SourceBuffer(WTF::move(sourceBufferPrivate), source)));
    sourceBuffer->suspendIfNeeded();
    return sourceBuffer.releaseNonNull();
}

SourceBuffer::SourceBuffer(PassRef<SourceBufferPrivate> sourceBufferPrivate, MediaSource* source)
    : ActiveDOMObject(source->scriptExecutionContext())
    , m_private(WTF::move(sourceBufferPrivate))
    , m_source(source)
    , m_asyncEventQueue(*this)
    , m_mode(segmentsKeyword())
    , m_appendBufferTimer(this, &SourceBuffer::appendBufferTimerFired)
    , m_groupStartTimestamp(MediaTime::invalidTime())
    , m_groupEndTimestamp(MediaTime::zeroTime())
    , m_buffered(TimeRanges::create())
    , m_appendState(WaitingForSegment)
    , m_timeOfBufferingMonitor(monotonicallyIncreasingTime())
    , m_bufferedSinceLastMonitor(0)
    , m_averageBufferRate(0)
    , m_reportedExtraMemoryCost(0)
    , m_pendingRemoveStart(MediaTime::invalidTime())
    , m_pendingRemoveEnd(MediaTime::invalidTime())
    , m_removeTimer(this, &SourceBuffer::removeTimerFired)
    , m_updating(false)
    , m_receivedFirstInitializationSegment(false)
    , m_active(false)
    , m_bufferFull(false)
    , m_shouldGenerateTimestamps(false)
{
    ASSERT(m_source);

    m_private->setClient(this);
}

SourceBuffer::~SourceBuffer()
{
    ASSERT(isRemoved());

    m_private->setClient(0);
}

PassRefPtr<TimeRanges> SourceBuffer::buffered(ExceptionCode& ec) const
{
    // Section 3.1 buffered attribute steps.
    // https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#attributes-1
    // 1. If this object has been removed from the sourceBuffers attribute of the parent media source then throw an
    //    INVALID_STATE_ERR exception and abort these steps.
    if (isRemoved()) {
        ec = INVALID_STATE_ERR;
        return nullptr;
    }

    // 2. Return a new static normalized TimeRanges object for the media segments buffered.
    return m_buffered->copy();
}

const RefPtr<TimeRanges>& SourceBuffer::buffered() const
{
    return m_buffered;
}

double SourceBuffer::timestampOffset() const
{
    return m_timestampOffset.toDouble();
}

void SourceBuffer::setTimestampOffset(double offset, ExceptionCode& ec)
{
    // Section 3.1 timestampOffset attribute setter steps.
    // https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#attributes-1
    // 1. Let new timestamp offset equal the new value being assigned to this attribute.
    // 2. If this object has been removed from the sourceBuffers attribute of the parent media source, then throw an
    //    INVALID_STATE_ERR exception and abort these steps.
    // 3. If the updating attribute equals true, then throw an INVALID_STATE_ERR exception and abort these steps.
    if (isRemoved() || m_updating) {
        ec = INVALID_STATE_ERR;
        return;
    }

    // 4. If the readyState attribute of the parent media source is in the "ended" state then run the following steps:
    // 4.1 Set the readyState attribute of the parent media source to "open"
    // 4.2 Queue a task to fire a simple event named sourceopen at the parent media source.
    m_source->openIfInEndedState();

    // 5. If the append state equals PARSING_MEDIA_SEGMENT, then throw an INVALID_STATE_ERR and abort these steps.
    if (m_appendState == ParsingMediaSegment) {
        ec = INVALID_STATE_ERR;
        return;
    }

    MediaTime newTimestampOffset = MediaTime::createWithDouble(offset);

    // 6. If the mode attribute equals "sequence", then set the group start timestamp to new timestamp offset.
    if (m_mode == sequenceKeyword())
        m_groupStartTimestamp = newTimestampOffset;

    // 7. Update the attribute to the new value.
    m_timestampOffset = newTimestampOffset;
}

void SourceBuffer::appendBuffer(PassRefPtr<ArrayBuffer> data, ExceptionCode& ec)
{
    // Section 3.2 appendBuffer()
    // https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#widl-SourceBuffer-appendBuffer-void-ArrayBufferView-data
    // 1. If data is null then throw an INVALID_ACCESS_ERR exception and abort these steps.
    if (!data) {
        ec = INVALID_ACCESS_ERR;
        return;
    }

    appendBufferInternal(static_cast<unsigned char*>(data->data()), data->byteLength(), ec);
}

void SourceBuffer::appendBuffer(PassRefPtr<ArrayBufferView> data, ExceptionCode& ec)
{
    // Section 3.2 appendBuffer()
    // https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#widl-SourceBuffer-appendBuffer-void-ArrayBufferView-data
    // 1. If data is null then throw an INVALID_ACCESS_ERR exception and abort these steps.
    if (!data) {
        ec = INVALID_ACCESS_ERR;
        return;
    }

    appendBufferInternal(static_cast<unsigned char*>(data->baseAddress()), data->byteLength(), ec);
}

void SourceBuffer::abort(ExceptionCode& ec)
{
    // Section 3.2 abort() method steps.
    // https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#widl-SourceBuffer-abort-void
    // 1. If this object has been removed from the sourceBuffers attribute of the parent media source
    //    then throw an INVALID_STATE_ERR exception and abort these steps.
    // 2. If the readyState attribute of the parent media source is not in the "open" state
    //    then throw an INVALID_STATE_ERR exception and abort these steps.
    if (isRemoved() || !m_source->isOpen()) {
        ec = INVALID_STATE_ERR;
        return;
    }

    // 3. If the sourceBuffer.updating attribute equals true, then run the following steps: ...
    abortIfUpdating();

    // 4. Run the reset parser state algorithm.
    m_private->abort();

    // FIXME(229408) Add steps 5-6 update appendWindowStart & appendWindowEnd.
}

void SourceBuffer::remove(double start, double end, ExceptionCode& ec)
{
    remove(MediaTime::createWithDouble(start), MediaTime::createWithDouble(end), ec);
}

void SourceBuffer::remove(const MediaTime& start, const MediaTime& end, ExceptionCode& ec)
{
    LOG(MediaSource, "SourceBuffer::remove(%p) - start(%lf), end(%lf)", this, start.toDouble(), end.toDouble());

    // Section 3.2 remove() method steps.
    // 1. If start is negative or greater than duration, then throw an InvalidAccessError exception and abort these steps.
    // 2. If end is less than or equal to start, then throw an InvalidAccessError exception and abort these steps.
    if (start < MediaTime::zeroTime() || (m_source && (!m_source->duration().isValid() || start > m_source->duration())) || end <= start) {
        ec = INVALID_ACCESS_ERR;
        return;
    }

    // 3. If this object has been removed from the sourceBuffers attribute of the parent media source then throw an
    //    InvalidStateError exception and abort these steps.
    // 4. If the updating attribute equals true, then throw an InvalidStateError exception and abort these steps.
    if (isRemoved() || m_updating) {
        ec = INVALID_STATE_ERR;
        return;
    }

    // 5. If the readyState attribute of the parent media source is in the "ended" state then run the following steps:
    // 5.1. Set the readyState attribute of the parent media source to "open"
    // 5.2. Queue a task to fire a simple event named sourceopen at the parent media source .
    m_source->openIfInEndedState();

    // 6. Set the updating attribute to true.
    m_updating = true;

    // 7. Queue a task to fire a simple event named updatestart at this SourceBuffer object.
    scheduleEvent(eventNames().updatestartEvent);

    // 8. Return control to the caller and run the rest of the steps asynchronously.
    m_pendingRemoveStart = start;
    m_pendingRemoveEnd = end;
    m_removeTimer.startOneShot(0);
}

void SourceBuffer::abortIfUpdating()
{
    // Section 3.2 abort() method step 3 substeps.
    // https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#widl-SourceBuffer-abort-void

    if (!m_updating)
        return;

    // 3.1. Abort the buffer append and stream append loop algorithms if they are running.
    m_appendBufferTimer.stop();
    m_pendingAppendData.clear();

    m_removeTimer.stop();
    m_pendingRemoveStart = MediaTime::invalidTime();
    m_pendingRemoveEnd = MediaTime::invalidTime();

    // 3.2. Set the updating attribute to false.
    m_updating = false;

    // 3.3. Queue a task to fire a simple event named abort at this SourceBuffer object.
    scheduleEvent(eventNames().abortEvent);

    // 3.4. Queue a task to fire a simple event named updateend at this SourceBuffer object.
    scheduleEvent(eventNames().updateendEvent);
}

void SourceBuffer::removedFromMediaSource()
{
    if (isRemoved())
        return;

    abortIfUpdating();

    for (auto& trackBufferPair : m_trackBufferMap.values()) {
        trackBufferPair.samples.clear();
        trackBufferPair.decodeQueue.clear();
    }

    m_private->removedFromMediaSource();
    m_source = 0;
}

void SourceBuffer::seekToTime(const MediaTime& time)
{
    LOG(MediaSource, "SourceBuffer::seekToTime(%p) - time(%s)", this, toString(time).utf8().data());

    for (auto& trackBufferPair : m_trackBufferMap) {
        TrackBuffer& trackBuffer = trackBufferPair.value;
        const AtomicString& trackID = trackBufferPair.key;

        trackBuffer.needsReenqueueing = true;
        reenqueueMediaForTime(trackBuffer, trackID, time);
    }
}

MediaTime SourceBuffer::sourceBufferPrivateFastSeekTimeForMediaTime(SourceBufferPrivate*, const MediaTime& targetTime, const MediaTime& negativeThreshold, const MediaTime& positiveThreshold)
{
    MediaTime seekTime = targetTime;
    MediaTime lowerBoundTime = targetTime - negativeThreshold;
    MediaTime upperBoundTime = targetTime + positiveThreshold;

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

bool SourceBuffer::hasPendingActivity() const
{
    return m_source || m_asyncEventQueue.hasPendingEvents();
}

void SourceBuffer::stop()
{
    m_appendBufferTimer.stop();
    m_removeTimer.stop();
}

bool SourceBuffer::isRemoved() const
{
    return !m_source;
}

void SourceBuffer::scheduleEvent(const AtomicString& eventName)
{
    RefPtr<Event> event = Event::create(eventName, false, false);
    event->setTarget(this);

    m_asyncEventQueue.enqueueEvent(event.release());
}

void SourceBuffer::appendBufferInternal(unsigned char* data, unsigned size, ExceptionCode& ec)
{
    // Section 3.2 appendBuffer()
    // https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#widl-SourceBuffer-appendBuffer-void-ArrayBufferView-data

    // Step 1 is enforced by the caller.
    // 2. Run the prepare append algorithm.
    // Section 3.5.4 Prepare AppendAlgorithm

    // 1. If the SourceBuffer has been removed from the sourceBuffers attribute of the parent media source
    // then throw an INVALID_STATE_ERR exception and abort these steps.
    // 2. If the updating attribute equals true, then throw an INVALID_STATE_ERR exception and abort these steps.
    if (isRemoved() || m_updating) {
        ec = INVALID_STATE_ERR;
        return;
    }

    // 3. If the readyState attribute of the parent media source is in the "ended" state then run the following steps:
    // 3.1. Set the readyState attribute of the parent media source to "open"
    // 3.2. Queue a task to fire a simple event named sourceopen at the parent media source .
    m_source->openIfInEndedState();

    // 4. Run the coded frame eviction algorithm.
    evictCodedFrames(size);

    // FIXME: enable this code when MSE libraries have been updated to support it.
#if 0
    // 5. If the buffer full flag equals true, then throw a QUOTA_EXCEEDED_ERR exception and abort these step.
    if (m_bufferFull) {
        LOG(MediaSource, "SourceBuffer::appendBufferInternal(%p) -  buffer full, failing with QUOTA_EXCEEDED_ERR error", this);
        ec = QUOTA_EXCEEDED_ERR;
        return;
    }
#endif

    // NOTE: Return to 3.2 appendBuffer()
    // 3. Add data to the end of the input buffer.
    m_pendingAppendData.append(data, size);

    // 4. Set the updating attribute to true.
    m_updating = true;

    // 5. Queue a task to fire a simple event named updatestart at this SourceBuffer object.
    scheduleEvent(eventNames().updatestartEvent);

    // 6. Asynchronously run the buffer append algorithm.
    m_appendBufferTimer.startOneShot(0);

    reportExtraMemoryCost();
}

void SourceBuffer::appendBufferTimerFired(Timer&)
{
    if (isRemoved())
        return;

    ASSERT(m_updating);

    // Section 3.5.5 Buffer Append Algorithm
    // https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#sourcebuffer-buffer-append

    // 1. Run the segment parser loop algorithm.
    size_t appendSize = m_pendingAppendData.size();
    if (!appendSize) {
        // Resize buffer for 0 byte appends so we always have a valid pointer.
        // We need to convey all appends, even 0 byte ones to |m_private| so
        // that it can clear its end of stream state if necessary.
        m_pendingAppendData.resize(1);
    }

    // Section 3.5.1 Segment Parser Loop
    // https://dvcs.w3.org/hg/html-media/raw-file/tip/media-source/media-source.html#sourcebuffer-segment-parser-loop
    // When the segment parser loop algorithm is invoked, run the following steps:

    // 1. Loop Top: If the input buffer is empty, then jump to the need more data step below.
    if (!m_pendingAppendData.size()) {
        sourceBufferPrivateAppendComplete(&m_private.get(), AppendSucceeded);
        return;
    }

    m_private->append(m_pendingAppendData.data(), appendSize);
    m_pendingAppendData.clear();
}

void SourceBuffer::sourceBufferPrivateAppendComplete(SourceBufferPrivate*, AppendResult result)
{
    if (isRemoved())
        return;

    // Section 3.5.5 Buffer Append Algorithm, ctd.
    // https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#sourcebuffer-buffer-append

    // 2. If the input buffer contains bytes that violate the SourceBuffer byte stream format specification,
    // then run the end of stream algorithm with the error parameter set to "decode" and abort this algorithm.
    if (result == ParsingFailed) {
        m_source->streamEndedWithError(decodeError(), IgnorableExceptionCode());
        return;
    }

    // NOTE: Steps 3 - 6 enforced by sourceBufferPrivateDidReceiveInitializationSegment() and
    // sourceBufferPrivateDidReceiveSample below.

    // 7. Need more data: Return control to the calling algorithm.

    // NOTE: return to Section 3.5.5
    // 2.If the segment parser loop algorithm in the previous step was aborted, then abort this algorithm.
    if (result != AppendSucceeded)
        return;

    // 3. Set the updating attribute to false.
    m_updating = false;

    // 4. Queue a task to fire a simple event named update at this SourceBuffer object.
    scheduleEvent(eventNames().updateEvent);

    // 5. Queue a task to fire a simple event named updateend at this SourceBuffer object.
    scheduleEvent(eventNames().updateendEvent);

    if (m_source)
        m_source->monitorSourceBuffers();

    MediaTime currentMediaTime = m_source->currentTime();
    for (auto& trackBufferPair : m_trackBufferMap) {
        TrackBuffer& trackBuffer = trackBufferPair.value;
        const AtomicString& trackID = trackBufferPair.key;

        if (trackBuffer.needsReenqueueing) {
            LOG(MediaSource, "SourceBuffer::sourceBufferPrivateAppendComplete(%p) - reenqueuing at time (%s)", this, toString(currentMediaTime).utf8().data());
            reenqueueMediaForTime(trackBuffer, trackID, currentMediaTime);
        } else
            provideMediaData(trackBuffer, trackID);
    }

    reportExtraMemoryCost();
    if (extraMemoryCost() > this->maximumBufferSize())
        m_bufferFull = true;

    LOG(Media, "SourceBuffer::sourceBufferPrivateAppendComplete(%p) - buffered = %s", this, toString(m_buffered->ranges()).utf8().data());
}

void SourceBuffer::sourceBufferPrivateDidReceiveRenderingError(SourceBufferPrivate*, int)
{
    if (!isRemoved())
        m_source->streamEndedWithError(decodeError(), IgnorableExceptionCode());
}

static bool decodeTimeComparator(const PresentationOrderSampleMap::MapType::value_type& a, const PresentationOrderSampleMap::MapType::value_type& b)
{
    return a.second->decodeTime() < b.second->decodeTime();
}

static PassRefPtr<TimeRanges> removeSamplesFromTrackBuffer(const DecodeOrderSampleMap::MapType& samples, SourceBuffer::TrackBuffer& trackBuffer, const SourceBuffer* buffer, const char* logPrefix)
{
#if !LOG_DISABLED
    double earliestSample = std::numeric_limits<double>::infinity();
    double latestSample = 0;
    size_t bytesRemoved = 0;
#else
    UNUSED_PARAM(logPrefix);
    UNUSED_PARAM(buffer);
#endif

    RefPtr<TimeRanges> erasedRanges = TimeRanges::create();
    MediaTime microsecond(1, 1000000);
    for (auto sampleIt : samples) {
        const DecodeOrderSampleMap::KeyType& decodeKey = sampleIt.first;
#if !LOG_DISABLED
        size_t startBufferSize = trackBuffer.samples.sizeInBytes();
#endif

        RefPtr<MediaSample>& sample = sampleIt.second;
        LOG(MediaSource, "SourceBuffer::%s(%p) - removing sample(%s)", logPrefix, buffer, toString(*sampleIt.second).utf8().data());

        // Remove the erased samples from the TrackBuffer sample map.
        trackBuffer.samples.removeSample(sample.get());

        // Also remove the erased samples from the TrackBuffer decodeQueue.
        trackBuffer.decodeQueue.erase(decodeKey);

        double startTime = sample->presentationTime().toDouble();
        double endTime = startTime + (sample->duration() + microsecond).toDouble();
        erasedRanges->add(startTime, endTime);

#if !LOG_DISABLED
        bytesRemoved += startBufferSize - trackBuffer.samples.sizeInBytes();
        if (startTime < earliestSample)
            earliestSample = startTime;
        if (endTime > latestSample)
            latestSample = endTime;
#endif
    }

#if !LOG_DISABLED
    if (bytesRemoved)
        LOG(MediaSource, "SourceBuffer::%s(%p) removed %zu bytes, start(%lf), end(%lf)", logPrefix, buffer, bytesRemoved, earliestSample, latestSample);
#endif

    return erasedRanges.release();
}

void SourceBuffer::removeCodedFrames(const MediaTime& start, const MediaTime& end)
{
    LOG(MediaSource, "SourceBuffer::removeCodedFrames(%p) - start(%s), end(%s)", this, toString(start).utf8().data(), toString(end).utf8().data());

    // 3.5.9 Coded Frame Removal Algorithm
    // https://dvcs.w3.org/hg/html-media/raw-file/tip/media-source/media-source.html#sourcebuffer-coded-frame-removal

    // 1. Let start be the starting presentation timestamp for the removal range.
    MediaTime durationMediaTime = m_source->duration();
    MediaTime currentMediaTime = m_source->currentTime();

    // 2. Let end be the end presentation timestamp for the removal range.
    // 3. For each track buffer in this source buffer, run the following steps:
    for (auto& iter : m_trackBufferMap) {
        TrackBuffer& trackBuffer = iter.value;

        // 3.1. Let remove end timestamp be the current value of duration
        // 3.2 If this track buffer has a random access point timestamp that is greater than or equal to end, then update
        // remove end timestamp to that random access point timestamp.
        // NOTE: findSyncSampleAfterPresentationTime will return the next sync sample on or after the presentation time
        // or decodeOrder().end() if no sync sample exists after that presentation time.
        DecodeOrderSampleMap::iterator removeDecodeEnd = trackBuffer.samples.decodeOrder().findSyncSampleAfterPresentationTime(end);
        PresentationOrderSampleMap::iterator removePresentationEnd;
        if (removeDecodeEnd == trackBuffer.samples.decodeOrder().end())
            removePresentationEnd = trackBuffer.samples.presentationOrder().end();
        else
            removePresentationEnd = trackBuffer.samples.presentationOrder().findSampleWithPresentationTime(removeDecodeEnd->second->presentationTime());

        PresentationOrderSampleMap::iterator removePresentationStart = trackBuffer.samples.presentationOrder().findSampleOnOrAfterPresentationTime(start);
        if (removePresentationStart == removePresentationEnd)
            continue;

        // 3.3 Remove all media data, from this track buffer, that contain starting timestamps greater than or equal to
        // start and less than the remove end timestamp.
        // NOTE: frames must be removed in decode order, so that all dependant frames between the frame to be removed
        // and the next sync sample frame are removed. But we must start from the first sample in decode order, not
        // presentation order.
        PresentationOrderSampleMap::iterator minDecodeTimeIter = std::min_element(removePresentationStart, removePresentationEnd, decodeTimeComparator);
        DecodeOrderSampleMap::KeyType decodeKey(minDecodeTimeIter->second->decodeTime(), minDecodeTimeIter->second->presentationTime());
        DecodeOrderSampleMap::iterator removeDecodeStart = trackBuffer.samples.decodeOrder().findSampleWithDecodeKey(decodeKey);

        DecodeOrderSampleMap::MapType erasedSamples(removeDecodeStart, removeDecodeEnd);
        RefPtr<TimeRanges> erasedRanges = removeSamplesFromTrackBuffer(erasedSamples, trackBuffer, this, "removeCodedFrames");

        // Only force the TrackBuffer to re-enqueue if the removed ranges overlap with enqueued and possibly
        // not yet displayed samples.
        if (currentMediaTime < trackBuffer.lastEnqueuedPresentationTime) {
            PlatformTimeRanges possiblyEnqueuedRanges(currentMediaTime, trackBuffer.lastEnqueuedPresentationTime);
            possiblyEnqueuedRanges.intersectWith(erasedRanges->ranges());
            if (possiblyEnqueuedRanges.length())
                trackBuffer.needsReenqueueing = true;
        }

        // Only force the TrackBuffer to re-enqueue if the removed ranges overlap with enqueued and possibly
        // not yet displayed samples.
        PlatformTimeRanges possiblyEnqueuedRanges(currentMediaTime, trackBuffer.lastEnqueuedPresentationTime);
        possiblyEnqueuedRanges.intersectWith(erasedRanges->ranges());
        if (possiblyEnqueuedRanges.length())
            trackBuffer.needsReenqueueing = true;

        erasedRanges->invert();
        m_buffered->intersectWith(*erasedRanges);

        // 3.4 If this object is in activeSourceBuffers, the current playback position is greater than or equal to start
        // and less than the remove end timestamp, and HTMLMediaElement.readyState is greater than HAVE_METADATA, then set
        // the HTMLMediaElement.readyState attribute to HAVE_METADATA and stall playback.
        if (m_active && currentMediaTime >= start && currentMediaTime < end && m_private->readyState() > MediaPlayer::HaveMetadata)
            m_private->setReadyState(MediaPlayer::HaveMetadata);
    }

    // 4. If buffer full flag equals true and this object is ready to accept more bytes, then set the buffer full flag to false.
    // No-op

    LOG(Media, "SourceBuffer::removeCodedFrames(%p) - buffered = %s", this, toString(m_buffered->ranges()).utf8().data());
}

void SourceBuffer::removeTimerFired(Timer*)
{
    ASSERT(m_updating);
    ASSERT(m_pendingRemoveStart.isValid());
    ASSERT(m_pendingRemoveStart < m_pendingRemoveEnd);

    // Section 3.2 remove() method steps
    // https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#widl-SourceBuffer-remove-void-double-start-double-end

    // 9. Run the coded frame removal algorithm with start and end as the start and end of the removal range.
    removeCodedFrames(m_pendingRemoveStart, m_pendingRemoveEnd);

    // 10. Set the updating attribute to false.
    m_updating = false;
    m_pendingRemoveStart = MediaTime::invalidTime();
    m_pendingRemoveEnd = MediaTime::invalidTime();

    // 11. Queue a task to fire a simple event named update at this SourceBuffer object.
    scheduleEvent(eventNames().updateEvent);

    // 12. Queue a task to fire a simple event named updateend at this SourceBuffer object.
    scheduleEvent(eventNames().updateendEvent);
}

void SourceBuffer::evictCodedFrames(size_t newDataSize)
{
    // 3.5.13 Coded Frame Eviction Algorithm
    // http://www.w3.org/TR/media-source/#sourcebuffer-coded-frame-eviction

    if (isRemoved())
        return;

    // This algorithm is run to free up space in this source buffer when new data is appended.
    // 1. Let new data equal the data that is about to be appended to this SourceBuffer.
    // 2. If the buffer full flag equals false, then abort these steps.
    if (!m_bufferFull)
        return;

    size_t maximumBufferSize = this->maximumBufferSize();

    // 3. Let removal ranges equal a list of presentation time ranges that can be evicted from
    // the presentation to make room for the new data.

    // NOTE: begin by removing data from the beginning of the buffered ranges, 30 seconds at
    // a time, up to 30 seconds before currentTime.
    MediaTime thirtySeconds = MediaTime(30, 1);
    MediaTime currentTime = m_source->currentTime();
    MediaTime maximumRangeEnd = currentTime - thirtySeconds;

#if !LOG_DISABLED
    LOG(MediaSource, "SourceBuffer::evictCodedFrames(%p) - currentTime = %lf, require %zu bytes, maximum buffer size is %zu", this, m_source->currentTime().toDouble(), extraMemoryCost() + newDataSize, maximumBufferSize);
    size_t initialBufferedSize = extraMemoryCost();
#endif

    MediaTime rangeStart = MediaTime::zeroTime();
    MediaTime rangeEnd = rangeStart + thirtySeconds;
    while (rangeStart < maximumRangeEnd) {
        // 4. For each range in removal ranges, run the coded frame removal algorithm with start and
        // end equal to the removal range start and end timestamp respectively.
        removeCodedFrames(rangeStart, std::min(rangeEnd, maximumRangeEnd));
        if (extraMemoryCost() + newDataSize < maximumBufferSize) {
            m_bufferFull = false;
            break;
        }

        rangeStart += thirtySeconds;
        rangeEnd += thirtySeconds;
    }

    if (!m_bufferFull) {
        LOG(MediaSource, "SourceBuffer::evictCodedFrames(%p) - evicted %zu bytes", this, initialBufferedSize - extraMemoryCost());
        return;
    }

    // If there still isn't enough free space and there buffers in time ranges after the current range (ie. there is a gap after
    // the current buffered range), delete 30 seconds at a time from duration back to the current time range or 30 seconds after
    // currenTime whichever we hit first.
    auto buffered = m_buffered->ranges();
    size_t currentTimeRange = buffered.find(currentTime);
    if (currentTimeRange == notFound || currentTimeRange == buffered.length() - 1) {
        LOG(MediaSource, "SourceBuffer::evictCodedFrames(%p) - evicted %zu bytes but FAILED to free enough", this, initialBufferedSize - extraMemoryCost());
        return;
    }

    MediaTime minimumRangeStart = currentTime + thirtySeconds;

    rangeEnd = m_source->duration();
    rangeStart = rangeEnd - thirtySeconds;
    while (rangeStart > minimumRangeStart) {

        // Do not evict data from the time range that contains currentTime.
        size_t startTimeRange = buffered.find(rangeStart);
        if (startTimeRange == currentTimeRange) {
            size_t endTimeRange = buffered.find(rangeEnd);
            if (endTimeRange == currentTimeRange)
                break;

            rangeEnd = buffered.start(endTimeRange);
        }

        // 4. For each range in removal ranges, run the coded frame removal algorithm with start and
        // end equal to the removal range start and end timestamp respectively.
        removeCodedFrames(std::max(minimumRangeStart, rangeStart), rangeEnd);
        if (extraMemoryCost() + newDataSize < maximumBufferSize) {
            m_bufferFull = false;
            break;
        }

        rangeStart -= thirtySeconds;
        rangeEnd -= thirtySeconds;
    }

    LOG(MediaSource, "SourceBuffer::evictCodedFrames(%p) - evicted %zu bytes%s", this, initialBufferedSize - extraMemoryCost(), m_bufferFull ? "" : " but FAILED to free enough");
}

size_t SourceBuffer::maximumBufferSize() const
{
    if (isRemoved())
        return 0;

    HTMLMediaElement* element = m_source->mediaElement();
    if (!element)
        return 0;

    return element->maximumSourceBufferSize(*this);
}

const AtomicString& SourceBuffer::decodeError()
{
    static NeverDestroyed<AtomicString> decode("decode", AtomicString::ConstructFromLiteral);
    return decode;
}

const AtomicString& SourceBuffer::networkError()
{
    static NeverDestroyed<AtomicString> network("network", AtomicString::ConstructFromLiteral);
    return network;
}

VideoTrackList* SourceBuffer::videoTracks()
{
    if (!m_source || !m_source->mediaElement())
        return nullptr;

    if (!m_videoTracks)
        m_videoTracks = VideoTrackList::create(m_source->mediaElement(), ActiveDOMObject::scriptExecutionContext());

    return m_videoTracks.get();
}

AudioTrackList* SourceBuffer::audioTracks()
{
    if (!m_source || !m_source->mediaElement())
        return nullptr;

    if (!m_audioTracks)
        m_audioTracks = AudioTrackList::create(m_source->mediaElement(), ActiveDOMObject::scriptExecutionContext());

    return m_audioTracks.get();
}

TextTrackList* SourceBuffer::textTracks()
{
    if (!m_source || !m_source->mediaElement())
        return nullptr;

    if (!m_textTracks)
        m_textTracks = TextTrackList::create(m_source->mediaElement(), ActiveDOMObject::scriptExecutionContext());

    return m_textTracks.get();
}

void SourceBuffer::setActive(bool active)
{
    if (m_active == active)
        return;

    m_active = active;
    m_private->setActive(active);
    if (!isRemoved())
        m_source->sourceBufferDidChangeAcitveState(this, active);
}

void SourceBuffer::sourceBufferPrivateDidEndStream(SourceBufferPrivate*, const WTF::AtomicString& error)
{
    if (!isRemoved())
        m_source->streamEndedWithError(error, IgnorableExceptionCode());
}

void SourceBuffer::sourceBufferPrivateDidReceiveInitializationSegment(SourceBufferPrivate*, const InitializationSegment& segment)
{
    if (isRemoved())
        return;

    // 3.5.7 Initialization Segment Received
    // https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#sourcebuffer-init-segment-received
    // 1. Update the duration attribute if it currently equals NaN:
    if (m_source->duration().isInvalid()) {
        // ↳ If the initialization segment contains a duration:
        //   Run the duration change algorithm with new duration set to the duration in the initialization segment.
        // ↳ Otherwise:
        //   Run the duration change algorithm with new duration set to positive Infinity.
        MediaTime newDuration = segment.duration.isValid() ? segment.duration : MediaTime::positiveInfiniteTime();
        m_source->setDurationInternal(newDuration);
    }

    // 2. If the initialization segment has no audio, video, or text tracks, then run the end of stream
    // algorithm with the error parameter set to "decode" and abort these steps.
    if (!segment.audioTracks.size() && !segment.videoTracks.size() && !segment.textTracks.size())
        m_source->streamEndedWithError(decodeError(), IgnorableExceptionCode());


    // 3. If the first initialization segment flag is true, then run the following steps:
    if (m_receivedFirstInitializationSegment) {
        if (!validateInitializationSegment(segment)) {
            m_source->streamEndedWithError(decodeError(), IgnorableExceptionCode());
            return;
        }
        // 3.2 Add the appropriate track descriptions from this initialization segment to each of the track buffers.
        ASSERT(segment.audioTracks.size() == audioTracks()->length());
        for (auto& audioTrackInfo : segment.audioTracks) {
            if (audioTracks()->length() == 1) {
                audioTracks()->item(0)->setPrivate(audioTrackInfo.track);
                break;
            }

            auto audioTrack = audioTracks()->getTrackById(audioTrackInfo.track->id());
            ASSERT(audioTrack);
            audioTrack->setPrivate(audioTrackInfo.track);
        }

        ASSERT(segment.videoTracks.size() == videoTracks()->length());
        for (auto& videoTrackInfo : segment.videoTracks) {
            if (videoTracks()->length() == 1) {
                videoTracks()->item(0)->setPrivate(videoTrackInfo.track);
                break;
            }

            auto videoTrack = videoTracks()->getTrackById(videoTrackInfo.track->id());
            ASSERT(videoTrack);
            videoTrack->setPrivate(videoTrackInfo.track);
        }

        ASSERT(segment.textTracks.size() == textTracks()->length());
        for (auto& textTrackInfo : segment.textTracks) {
            if (textTracks()->length() == 1) {
                toInbandTextTrack(textTracks()->item(0))->setPrivate(textTrackInfo.track);
                break;
            }

            auto textTrack = textTracks()->getTrackById(textTrackInfo.track->id());
            ASSERT(textTrack);
            toInbandTextTrack(textTrack)->setPrivate(textTrackInfo.track);
        }

        for (auto& trackBuffer : m_trackBufferMap.values())
            trackBuffer.needRandomAccessFlag = true;
    }

    // 4. Let active track flag equal false.
    bool activeTrackFlag = false;

    // 5. If the first initialization segment flag is false, then run the following steps:
    if (!m_receivedFirstInitializationSegment) {
        // 5.1 If the initialization segment contains tracks with codecs the user agent does not support,
        // then run the end of stream algorithm with the error parameter set to "decode" and abort these steps.
        // NOTE: This check is the responsibility of the SourceBufferPrivate.

        // 5.2 For each audio track in the initialization segment, run following steps:
        for (auto& audioTrackInfo : segment.audioTracks) {
            AudioTrackPrivate* audioTrackPrivate = audioTrackInfo.track.get();

            // 5.2.1 Let new audio track be a new AudioTrack object.
            // 5.2.2 Generate a unique ID and assign it to the id property on new video track.
            RefPtr<AudioTrack> newAudioTrack = AudioTrack::create(this, audioTrackPrivate);
            newAudioTrack->setSourceBuffer(this);

            // 5.2.3 If audioTracks.length equals 0, then run the following steps:
            if (!audioTracks()->length()) {
                // 5.2.3.1 Set the enabled property on new audio track to true.
                newAudioTrack->setEnabled(true);

                // 5.2.3.2 Set active track flag to true.
                activeTrackFlag = true;
            }

            // 5.2.4 Add new audio track to the audioTracks attribute on this SourceBuffer object.
            // 5.2.5 Queue a task to fire a trusted event named addtrack, that does not bubble and is
            // not cancelable, and that uses the TrackEvent interface, at the AudioTrackList object
            // referenced by the audioTracks attribute on this SourceBuffer object.
            audioTracks()->append(newAudioTrack);

            // 5.2.6 Add new audio track to the audioTracks attribute on the HTMLMediaElement.
            // 5.2.7 Queue a task to fire a trusted event named addtrack, that does not bubble and is
            // not cancelable, and that uses the TrackEvent interface, at the AudioTrackList object
            // referenced by the audioTracks attribute on the HTMLMediaElement.
            m_source->mediaElement()->audioTracks()->append(newAudioTrack);

            // 5.2.8 Create a new track buffer to store coded frames for this track.
            ASSERT(!m_trackBufferMap.contains(newAudioTrack->id()));
            TrackBuffer& trackBuffer = m_trackBufferMap.add(newAudioTrack->id(), TrackBuffer()).iterator->value;

            // 5.2.9 Add the track description for this track to the track buffer.
            trackBuffer.description = audioTrackInfo.description;

            m_audioCodecs.append(trackBuffer.description->codec());
        }

        // 5.3 For each video track in the initialization segment, run following steps:
        for (auto& videoTrackInfo : segment.videoTracks) {
            VideoTrackPrivate* videoTrackPrivate = videoTrackInfo.track.get();

            // 5.3.1 Let new video track be a new VideoTrack object.
            // 5.3.2 Generate a unique ID and assign it to the id property on new video track.
            RefPtr<VideoTrack> newVideoTrack = VideoTrack::create(this, videoTrackPrivate);
            newVideoTrack->setSourceBuffer(this);

            // 5.3.3 If videoTracks.length equals 0, then run the following steps:
            if (!videoTracks()->length()) {
                // 5.3.3.1 Set the selected property on new video track to true.
                newVideoTrack->setSelected(true);

                // 5.3.3.2 Set active track flag to true.
                activeTrackFlag = true;
            }

            // 5.3.4 Add new video track to the videoTracks attribute on this SourceBuffer object.
            // 5.3.5 Queue a task to fire a trusted event named addtrack, that does not bubble and is
            // not cancelable, and that uses the TrackEvent interface, at the VideoTrackList object
            // referenced by the videoTracks attribute on this SourceBuffer object.
            videoTracks()->append(newVideoTrack);

            // 5.3.6 Add new video track to the videoTracks attribute on the HTMLMediaElement.
            // 5.3.7 Queue a task to fire a trusted event named addtrack, that does not bubble and is
            // not cancelable, and that uses the TrackEvent interface, at the VideoTrackList object
            // referenced by the videoTracks attribute on the HTMLMediaElement.
            m_source->mediaElement()->videoTracks()->append(newVideoTrack);

            // 5.3.8 Create a new track buffer to store coded frames for this track.
            ASSERT(!m_trackBufferMap.contains(newVideoTrack->id()));
            TrackBuffer& trackBuffer = m_trackBufferMap.add(newVideoTrack->id(), TrackBuffer()).iterator->value;

            // 5.3.9 Add the track description for this track to the track buffer.
            trackBuffer.description = videoTrackInfo.description;

            m_videoCodecs.append(trackBuffer.description->codec());
        }

        // 5.4 For each text track in the initialization segment, run following steps:
        for (auto& textTrackInfo : segment.textTracks) {
            InbandTextTrackPrivate* textTrackPrivate = textTrackInfo.track.get();

            // 5.4.1 Let new text track be a new TextTrack object with its properties populated with the
            // appropriate information from the initialization segment.
            RefPtr<InbandTextTrack> newTextTrack = InbandTextTrack::create(scriptExecutionContext(), this, textTrackPrivate);

            // 5.4.2 If the mode property on new text track equals "showing" or "hidden", then set active
            // track flag to true.
            if (textTrackPrivate->mode() != InbandTextTrackPrivate::Disabled)
                activeTrackFlag = true;

            // 5.4.3 Add new text track to the textTracks attribute on this SourceBuffer object.
            // 5.4.4 Queue a task to fire a trusted event named addtrack, that does not bubble and is
            // not cancelable, and that uses the TrackEvent interface, at textTracks attribute on this
            // SourceBuffer object.
            textTracks()->append(newTextTrack);

            // 5.4.5 Add new text track to the textTracks attribute on the HTMLMediaElement.
            // 5.4.6 Queue a task to fire a trusted event named addtrack, that does not bubble and is
            // not cancelable, and that uses the TrackEvent interface, at the TextTrackList object
            // referenced by the textTracks attribute on the HTMLMediaElement.
            m_source->mediaElement()->textTracks()->append(newTextTrack);

            // 5.4.7 Create a new track buffer to store coded frames for this track.
            ASSERT(!m_trackBufferMap.contains(textTrackPrivate->id()));
            TrackBuffer& trackBuffer = m_trackBufferMap.add(textTrackPrivate->id(), TrackBuffer()).iterator->value;

            // 5.4.8 Add the track description for this track to the track buffer.
            trackBuffer.description = textTrackInfo.description;

            m_textCodecs.append(trackBuffer.description->codec());
        }

        // 5.5 If active track flag equals true, then run the following steps:
        if (activeTrackFlag) {
            // 5.5.1 Add this SourceBuffer to activeSourceBuffers.
            setActive(true);
        }

        // 5.6 Set first initialization segment flag to true.
        m_receivedFirstInitializationSegment = true;
    }

    // 6. If the HTMLMediaElement.readyState attribute is HAVE_NOTHING, then run the following steps:
    if (m_private->readyState() == MediaPlayer::HaveNothing) {
        // 6.1 If one or more objects in sourceBuffers have first initialization segment flag set to false, then abort these steps.
        for (auto& sourceBuffer : *m_source->sourceBuffers()) {
            if (!sourceBuffer->m_receivedFirstInitializationSegment)
                return;
        }

        // 6.2 Set the HTMLMediaElement.readyState attribute to HAVE_METADATA.
        // 6.3 Queue a task to fire a simple event named loadedmetadata at the media element.
        m_private->setReadyState(MediaPlayer::HaveMetadata);
    }

    // 7. If the active track flag equals true and the HTMLMediaElement.readyState
    // attribute is greater than HAVE_CURRENT_DATA, then set the HTMLMediaElement.readyState
    // attribute to HAVE_METADATA.
    if (activeTrackFlag && m_private->readyState() > MediaPlayer::HaveCurrentData)
        m_private->setReadyState(MediaPlayer::HaveMetadata);
}

bool SourceBuffer::validateInitializationSegment(const InitializationSegment& segment)
{
    // 3.5.7 Initialization Segment Received (ctd)
    // https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#sourcebuffer-init-segment-received

    // 3.1. Verify the following properties. If any of the checks fail then run the end of stream
    // algorithm with the error parameter set to "decode" and abort these steps.
    //   * The number of audio, video, and text tracks match what was in the first initialization segment.
    if (segment.audioTracks.size() != audioTracks()->length()
        || segment.videoTracks.size() != videoTracks()->length()
        || segment.textTracks.size() != textTracks()->length())
        return false;

    //   * The codecs for each track, match what was specified in the first initialization segment.
    for (auto& audioTrackInfo : segment.audioTracks) {
        if (!m_audioCodecs.contains(audioTrackInfo.description->codec()))
            return false;
    }

    for (auto& videoTrackInfo : segment.videoTracks) {
        if (!m_videoCodecs.contains(videoTrackInfo.description->codec()))
            return false;
    }

    for (auto& textTrackInfo : segment.textTracks) {
        if (!m_textCodecs.contains(textTrackInfo.description->codec()))
            return false;
    }

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

class SampleLessThanComparator {
public:
    bool operator()(std::pair<MediaTime, RefPtr<MediaSample>> value1, std::pair<MediaTime, RefPtr<MediaSample>> value2)
    {
        return value1.first < value2.first;
    }

    bool operator()(MediaTime value1, std::pair<MediaTime, RefPtr<MediaSample>> value2)
    {
        return value1 < value2.first;
    }

    bool operator()(std::pair<MediaTime, RefPtr<MediaSample>> value1, MediaTime value2)
    {
        return value1.first < value2;
    }
};

void SourceBuffer::sourceBufferPrivateDidReceiveSample(SourceBufferPrivate*, PassRefPtr<MediaSample> prpSample)
{
    if (isRemoved())
        return;

    RefPtr<MediaSample> sample = prpSample;

    // 3.5.8 Coded Frame Processing
    // http://www.w3.org/TR/media-source/#sourcebuffer-coded-frame-processing

    // When complete coded frames have been parsed by the segment parser loop then the following steps
    // are run:
    // 1. For each coded frame in the media segment run the following steps:
    // 1.1. Loop Top
    do {
        MediaTime presentationTimestamp;
        MediaTime decodeTimestamp;

        if (m_shouldGenerateTimestamps) {
            // ↳ If generate timestamps flag equals true:
            // 1. Let presentation timestamp equal 0.
            presentationTimestamp = MediaTime::zeroTime();

            // 2. Let decode timestamp equal 0.
            decodeTimestamp = MediaTime::zeroTime();
        } else {
            // ↳ Otherwise:
            // 1. Let presentation timestamp be a double precision floating point representation of
            // the coded frame's presentation timestamp in seconds.
            presentationTimestamp = sample->presentationTime();

            // 2. Let decode timestamp be a double precision floating point representation of the coded frame's
            // decode timestamp in seconds.
            decodeTimestamp = sample->decodeTime();
        }

        // 1.2 Let frame duration be a double precision floating point representation of the coded frame's
        // duration in seconds.
        MediaTime frameDuration = sample->duration();

        // 1.3 If mode equals "sequence" and group start timestamp is set, then run the following steps:
        if (m_mode == sequenceKeyword()) {
            // 1.3.1 Set timestampOffset equal to group start timestamp - presentation timestamp.
            m_timestampOffset = m_groupStartTimestamp;

            // 1.3.2 Set group end timestamp equal to group start timestamp.
            m_groupEndTimestamp = m_groupStartTimestamp;

            // 1.3.3 Set the need random access point flag on all track buffers to true.
            for (auto& trackBuffer : m_trackBufferMap.values())
                trackBuffer.needRandomAccessFlag = true;

            // 1.3.4 Unset group start timestamp.
            m_groupStartTimestamp = MediaTime::invalidTime();
        }

        // 1.4 If timestampOffset is not 0, then run the following steps:
        if (m_timestampOffset) {
            // 1.4.1 Add timestampOffset to the presentation timestamp.
            presentationTimestamp += m_timestampOffset;

            // 1.4.2 Add timestampOffset to the decode timestamp.
            decodeTimestamp += m_timestampOffset;
        }

        // 1.5 Let track buffer equal the track buffer that the coded frame will be added to.
        AtomicString trackID = sample->trackID();
        auto it = m_trackBufferMap.find(trackID);
        if (it == m_trackBufferMap.end())
            it = m_trackBufferMap.add(trackID, TrackBuffer()).iterator;
        TrackBuffer& trackBuffer = it->value;

        // 1.6 ↳ If last decode timestamp for track buffer is set and decode timestamp is less than last
        // decode timestamp:
        // OR
        // ↳ If last decode timestamp for track buffer is set and the difference between decode timestamp and
        // last decode timestamp is greater than 2 times last frame duration:
        if (trackBuffer.lastDecodeTimestamp.isValid() && (decodeTimestamp < trackBuffer.lastDecodeTimestamp
            || abs(decodeTimestamp - trackBuffer.lastDecodeTimestamp) > (trackBuffer.lastFrameDuration * 2))) {

            // 1.6.1:
            if (m_mode == segmentsKeyword()) {
                // ↳ If mode equals "segments":
                // Set group end timestamp to presentation timestamp.
                m_groupEndTimestamp = presentationTimestamp;
            } else if (m_mode == sequenceKeyword()) {
                // ↳ If mode equals "sequence":
                // Set group start timestamp equal to the group end timestamp.
                m_groupStartTimestamp = m_groupEndTimestamp;
            }

            for (auto& trackBuffer : m_trackBufferMap.values()) {
                // 1.6.2 Unset the last decode timestamp on all track buffers.
                trackBuffer.lastDecodeTimestamp = MediaTime::invalidTime();
                // 1.6.3 Unset the last frame duration on all track buffers.
                trackBuffer.lastFrameDuration = MediaTime::invalidTime();
                // 1.6.4 Unset the highest presentation timestamp on all track buffers.
                trackBuffer.highestPresentationTimestamp = MediaTime::invalidTime();
                // 1.6.5 Set the need random access point flag on all track buffers to true.
                trackBuffer.needRandomAccessFlag = true;
            }

            // 1.6.6 Jump to the Loop Top step above to restart processing of the current coded frame.
            continue;
        }

        // 1.7 Let frame end timestamp equal the sum of presentation timestamp and frame duration.
        MediaTime frameEndTimestamp = presentationTimestamp + frameDuration;

        // 1.8 If presentation timestamp is less than appendWindowStart, then set the need random access
        // point flag to true, drop the coded frame, and jump to the top of the loop to start processing
        // the next coded frame.
        // 1.9 If frame end timestamp is greater than appendWindowEnd, then set the need random access
        // point flag to true, drop the coded frame, and jump to the top of the loop to start processing
        // the next coded frame.
        // FIXME: implement append windows

        // 1.10 If the decode timestamp is less than the presentation start time, then run the end of stream
        // algorithm with the error parameter set to "decode", and abort these steps.
        // NOTE: Until <https://www.w3.org/Bugs/Public/show_bug.cgi?id=27487> is resolved, we will only check
        // the presentation timestamp.
        MediaTime presentationStartTime = MediaTime::zeroTime();
        if (presentationTimestamp < presentationStartTime) {
            LOG(MediaSource, "SourceBuffer::sourceBufferPrivateDidReceiveSample(%p) - failing because presentationTimestamp < presentationStartTime", this);
            m_source->streamEndedWithError(decodeError(), IgnorableExceptionCode());
            return;
        }

        // 1.11 If the need random access point flag on track buffer equals true, then run the following steps:
        if (trackBuffer.needRandomAccessFlag) {
            // 1.11.1 If the coded frame is not a random access point, then drop the coded frame and jump
            // to the top of the loop to start processing the next coded frame.
            if (!sample->isSync()) {
                didDropSample();
                return;
            }

            // 1.11.2 Set the need random access point flag on track buffer to false.
            trackBuffer.needRandomAccessFlag = false;
        }

        // 1.12 Let spliced audio frame be an unset variable for holding audio splice information
        // 1.13 Let spliced timed text frame be an unset variable for holding timed text splice information
        // FIXME: Add support for sample splicing.

        SampleMap erasedSamples;
        MediaTime microsecond(1, 1000000);

        // 1.14 If last decode timestamp for track buffer is unset and presentation timestamp falls
        // falls within the presentation interval of a coded frame in track buffer, then run the
        // following steps:
        if (trackBuffer.lastDecodeTimestamp.isInvalid()) {
            auto iter = trackBuffer.samples.presentationOrder().findSampleContainingPresentationTime(presentationTimestamp);
            if (iter != trackBuffer.samples.presentationOrder().end()) {
                // 1.14.1 Let overlapped frame be the coded frame in track buffer that matches the condition above.
                RefPtr<MediaSample> overlappedFrame = iter->second;

                // 1.14.2 If track buffer contains audio coded frames:
                // Run the audio splice frame algorithm and if a splice frame is returned, assign it to
                // spliced audio frame.
                // FIXME: Add support for sample splicing.

                // If track buffer contains video coded frames:
                if (trackBuffer.description->isVideo()) {
                    // 1.14.2.1 Let overlapped frame presentation timestamp equal the presentation timestamp
                    // of overlapped frame.
                    MediaTime overlappedFramePresentationTimestamp = overlappedFrame->presentationTime();

                    // 1.14.2.2 Let remove window timestamp equal overlapped frame presentation timestamp
                    // plus 1 microsecond.
                    MediaTime removeWindowTimestamp = overlappedFramePresentationTimestamp + microsecond;

                    // 1.14.2.3 If the presentation timestamp is less than the remove window timestamp,
                    // then remove overlapped frame and any coded frames that depend on it from track buffer.
                    if (presentationTimestamp < removeWindowTimestamp)
                        erasedSamples.addSample(iter->second);
                }

                // If track buffer contains timed text coded frames:
                // Run the text splice frame algorithm and if a splice frame is returned, assign it to spliced timed text frame.
                // FIXME: Add support for sample splicing.
            }
        }

        // 1.15 Remove existing coded frames in track buffer:
        // If highest presentation timestamp for track buffer is not set:
        if (trackBuffer.highestPresentationTimestamp.isInvalid()) {
            // Remove all coded frames from track buffer that have a presentation timestamp greater than or
            // equal to presentation timestamp and less than frame end timestamp.
            auto iter_pair = trackBuffer.samples.presentationOrder().findSamplesBetweenPresentationTimes(presentationTimestamp, frameEndTimestamp);
            if (iter_pair.first != trackBuffer.samples.presentationOrder().end())
                erasedSamples.addRange(iter_pair.first, iter_pair.second);
        }

        // If highest presentation timestamp for track buffer is set and less than or equal to presentation timestamp
        if (trackBuffer.highestPresentationTimestamp.isValid() && trackBuffer.highestPresentationTimestamp <= presentationTimestamp) {
            // Remove all coded frames from track buffer that have a presentation timestamp greater than highest
            // presentation timestamp and less than or equal to frame end timestamp.
            do {
                // NOTE: Searching from the end of the trackBuffer will be vastly more efficient if the search range is
                // near the end of the buffered range. Use a linear-backwards search if the search range is within one
                // frame duration of the end:
                if (!m_buffered)
                    break;

                unsigned bufferedLength = m_buffered->ranges().length();
                if (!bufferedLength)
                    break;

                MediaTime highestBufferedTime = m_buffered->ranges().maximumBufferedTime();

                PresentationOrderSampleMap::iterator_range range;
                if (highestBufferedTime - trackBuffer.highestPresentationTimestamp < trackBuffer.lastFrameDuration)
                    range = trackBuffer.samples.presentationOrder().findSamplesWithinPresentationRangeFromEnd(trackBuffer.highestPresentationTimestamp, frameEndTimestamp);
                else
                    range = trackBuffer.samples.presentationOrder().findSamplesWithinPresentationRange(trackBuffer.highestPresentationTimestamp, frameEndTimestamp);

                if (range.first != trackBuffer.samples.presentationOrder().end())
                    erasedSamples.addRange(range.first, range.second);
            } while(false);
        }

        // 1.16 Remove decoding dependencies of the coded frames removed in the previous step:
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

            RefPtr<TimeRanges> erasedRanges = removeSamplesFromTrackBuffer(dependentSamples, trackBuffer, this, "sourceBufferPrivateDidReceiveSample");

            // Only force the TrackBuffer to re-enqueue if the removed ranges overlap with enqueued and possibly
            // not yet displayed samples.
            MediaTime currentMediaTime = m_source->currentTime();
            if (currentMediaTime < trackBuffer.lastEnqueuedPresentationTime) {
                PlatformTimeRanges possiblyEnqueuedRanges(currentMediaTime, trackBuffer.lastEnqueuedPresentationTime);
                possiblyEnqueuedRanges.intersectWith(erasedRanges->ranges());
                if (possiblyEnqueuedRanges.length())
                    trackBuffer.needsReenqueueing = true;
            }

            erasedRanges->invert();
            m_buffered->intersectWith(*erasedRanges);
        }

        // 1.17 If spliced audio frame is set:
        // Add spliced audio frame to the track buffer.
        // If spliced timed text frame is set:
        // Add spliced timed text frame to the track buffer.
        // FIXME: Add support for sample splicing.

        // Otherwise:
        // Add the coded frame with the presentation timestamp, decode timestamp, and frame duration to the track buffer.
        trackBuffer.samples.addSample(sample);

        if (trackBuffer.lastEnqueuedDecodeEndTime.isInvalid() || decodeTimestamp >= trackBuffer.lastEnqueuedDecodeEndTime) {
            DecodeOrderSampleMap::KeyType decodeKey(decodeTimestamp, presentationTimestamp);
            trackBuffer.decodeQueue.insert(DecodeOrderSampleMap::MapType::value_type(decodeKey, sample));
        }

        // 1.18 Set last decode timestamp for track buffer to decode timestamp.
        trackBuffer.lastDecodeTimestamp = decodeTimestamp;

        // 1.19 Set last frame duration for track buffer to frame duration.
        trackBuffer.lastFrameDuration = frameDuration;

        // 1.20 If highest presentation timestamp for track buffer is unset or frame end timestamp is greater
        // than highest presentation timestamp, then set highest presentation timestamp for track buffer
        // to frame end timestamp.
        if (trackBuffer.highestPresentationTimestamp.isInvalid() || frameEndTimestamp > trackBuffer.highestPresentationTimestamp)
            trackBuffer.highestPresentationTimestamp = frameEndTimestamp;

        // 1.21 If frame end timestamp is greater than group end timestamp, then set group end timestamp equal
        // to frame end timestamp.
        if (m_groupEndTimestamp.isInvalid() || frameEndTimestamp > m_groupEndTimestamp)
            m_groupEndTimestamp = frameEndTimestamp;

        // 1.22 If generate timestamps flag equals true, then set timestampOffset equal to frame end timestamp.
        if (m_shouldGenerateTimestamps)
            m_timestampOffset = frameEndTimestamp;

        m_buffered->add(presentationTimestamp.toDouble(), (presentationTimestamp + frameDuration + microsecond).toDouble());
        m_bufferedSinceLastMonitor += frameDuration.toDouble();

        break;
    } while (1);

    // Steps 2-4 will be handled by MediaSource::monitorSourceBuffers()

    // 5. If the media segment contains data beyond the current duration, then run the duration change algorithm with new
    // duration set to the maximum of the current duration and the group end timestamp.
    if (m_groupEndTimestamp > m_source->duration())
        m_source->setDurationInternal(m_groupEndTimestamp);
}

bool SourceBuffer::hasAudio() const
{
    return m_audioTracks && m_audioTracks->length();
}

bool SourceBuffer::hasVideo() const
{
    return m_videoTracks && m_videoTracks->length();
}

bool SourceBuffer::sourceBufferPrivateHasAudio(const SourceBufferPrivate*) const
{
    return hasAudio();
}

bool SourceBuffer::sourceBufferPrivateHasVideo(const SourceBufferPrivate*) const
{
    return hasVideo();
}

void SourceBuffer::videoTrackSelectedChanged(VideoTrack* track)
{
    // 2.4.5 Changes to selected/enabled track state
    // If the selected video track changes, then run the following steps:
    // 1. If the SourceBuffer associated with the previously selected video track is not associated with
    // any other enabled tracks, run the following steps:
    if (track->selected()
        && (!m_videoTracks || !m_videoTracks->isAnyTrackEnabled())
        && (!m_audioTracks || !m_audioTracks->isAnyTrackEnabled())
        && (!m_textTracks || !m_textTracks->isAnyTrackEnabled())) {
        // 1.1 Remove the SourceBuffer from activeSourceBuffers.
        // 1.2 Queue a task to fire a simple event named removesourcebuffer at activeSourceBuffers
        setActive(false);
    } else if (!track->selected()) {
        // 2. If the SourceBuffer associated with the newly selected video track is not already in activeSourceBuffers,
        // run the following steps:
        // 2.1 Add the SourceBuffer to activeSourceBuffers.
        // 2.2 Queue a task to fire a simple event named addsourcebuffer at activeSourceBuffers
        setActive(true);
    }

    if (!isRemoved())
        m_source->mediaElement()->videoTrackSelectedChanged(track);
}

void SourceBuffer::audioTrackEnabledChanged(AudioTrack* track)
{
    // 2.4.5 Changes to selected/enabled track state
    // If an audio track becomes disabled and the SourceBuffer associated with this track is not
    // associated with any other enabled or selected track, then run the following steps:
    if (track->enabled()
        && (!m_videoTracks || !m_videoTracks->isAnyTrackEnabled())
        && (!m_audioTracks || !m_audioTracks->isAnyTrackEnabled())
        && (!m_textTracks || !m_textTracks->isAnyTrackEnabled())) {
        // 1. Remove the SourceBuffer associated with the audio track from activeSourceBuffers
        // 2. Queue a task to fire a simple event named removesourcebuffer at activeSourceBuffers
        setActive(false);
    } else if (!track->enabled()) {
        // If an audio track becomes enabled and the SourceBuffer associated with this track is
        // not already in activeSourceBuffers, then run the following steps:
        // 1. Add the SourceBuffer associated with the audio track to activeSourceBuffers
        // 2. Queue a task to fire a simple event named addsourcebuffer at activeSourceBuffers
        setActive(true);
    }

    if (!isRemoved())
        m_source->mediaElement()->audioTrackEnabledChanged(track);
}

void SourceBuffer::textTrackModeChanged(TextTrack* track)
{
    // 2.4.5 Changes to selected/enabled track state
    // If a text track mode becomes "disabled" and the SourceBuffer associated with this track is not
    // associated with any other enabled or selected track, then run the following steps:
    if (track->mode() == TextTrack::disabledKeyword()
        && (!m_videoTracks || !m_videoTracks->isAnyTrackEnabled())
        && (!m_audioTracks || !m_audioTracks->isAnyTrackEnabled())
        && (!m_textTracks || !m_textTracks->isAnyTrackEnabled())) {
        // 1. Remove the SourceBuffer associated with the audio track from activeSourceBuffers
        // 2. Queue a task to fire a simple event named removesourcebuffer at activeSourceBuffers
        setActive(false);
    } else {
        // If a text track mode becomes "showing" or "hidden" and the SourceBuffer associated with this
        // track is not already in activeSourceBuffers, then run the following steps:
        // 1. Add the SourceBuffer associated with the text track to activeSourceBuffers
        // 2. Queue a task to fire a simple event named addsourcebuffer at activeSourceBuffers
        setActive(true);
    }

    if (!isRemoved())
        m_source->mediaElement()->textTrackModeChanged(track);
}

void SourceBuffer::textTrackAddCue(TextTrack* track, WTF::PassRefPtr<TextTrackCue> cue)
{
    if (!isRemoved())
        m_source->mediaElement()->textTrackAddCue(track, cue);
}

void SourceBuffer::textTrackAddCues(TextTrack* track, TextTrackCueList const* cueList)
{
    if (!isRemoved())
        m_source->mediaElement()->textTrackAddCues(track, cueList);
}

void SourceBuffer::textTrackRemoveCue(TextTrack* track, WTF::PassRefPtr<TextTrackCue> cue)
{
    if (!isRemoved())
        m_source->mediaElement()->textTrackRemoveCue(track, cue);
}

void SourceBuffer::textTrackRemoveCues(TextTrack* track, TextTrackCueList const* cueList)
{
    if (!isRemoved())
        m_source->mediaElement()->textTrackRemoveCues(track, cueList);
}

void SourceBuffer::textTrackKindChanged(TextTrack* track)
{
    if (!isRemoved())
        m_source->mediaElement()->textTrackKindChanged(track);
}

void SourceBuffer::sourceBufferPrivateDidBecomeReadyForMoreSamples(SourceBufferPrivate*, AtomicString trackID)
{
    LOG(MediaSource, "SourceBuffer::sourceBufferPrivateDidBecomeReadyForMoreSamples(%p)", this);
    auto it = m_trackBufferMap.find(trackID);
    if (it == m_trackBufferMap.end())
        return;

    TrackBuffer& trackBuffer = it->value;
    if (!trackBuffer.needsReenqueueing && !m_source->isSeeking())
        provideMediaData(trackBuffer, trackID);
}

void SourceBuffer::provideMediaData(TrackBuffer& trackBuffer, AtomicString trackID)
{
#if !LOG_DISABLED
    unsigned enqueuedSamples = 0;
#endif

    auto sampleIt = trackBuffer.decodeQueue.begin();
    for (auto sampleEnd = trackBuffer.decodeQueue.end(); sampleIt != sampleEnd; ++sampleIt) {
        if (!m_private->isReadyForMoreSamples(trackID)) {
            m_private->notifyClientWhenReadyForMoreSamples(trackID);
            break;
        }

        RefPtr<MediaSample> sample = sampleIt->second;
        // Do not enqueue samples spanning a significant unbuffered gap.
        // NOTE: one second is somewhat arbitrary. MediaSource::monitorSourceBuffers() is run
        // on the playbackTimer, which is effectively every 350ms. Allowing > 350ms gap between
        // enqueued samples allows for situations where we overrun the end of a buffered range
        // but don't notice for 350s of playback time, and the client can enqueue data for the
        // new current time without triggering this early return.
        // FIXME(135867): Make this gap detection logic less arbitrary.
        MediaTime oneSecond(1, 1);
        if (trackBuffer.lastEnqueuedDecodeEndTime.isValid() && sample->decodeTime() - trackBuffer.lastEnqueuedDecodeEndTime > oneSecond)
            break;

        trackBuffer.lastEnqueuedPresentationTime = sample->presentationTime();
        trackBuffer.lastEnqueuedDecodeEndTime = sample->decodeTime() + sample->duration();
        m_private->enqueueSample(sample.release(), trackID);
#if !LOG_DISABLED
        ++enqueuedSamples;
#endif

    }
    trackBuffer.decodeQueue.erase(trackBuffer.decodeQueue.begin(), sampleIt);

    LOG(MediaSource, "SourceBuffer::provideMediaData(%p) - Enqueued %u samples", this, enqueuedSamples);
}

void SourceBuffer::reenqueueMediaForTime(TrackBuffer& trackBuffer, AtomicString trackID, const MediaTime& time)
{
    // Find the sample which contains the current presentation time.
    auto currentSamplePTSIterator = trackBuffer.samples.presentationOrder().findSampleContainingPresentationTime(time);

    if (currentSamplePTSIterator == trackBuffer.samples.presentationOrder().end()) {
        trackBuffer.decodeQueue.clear();
        m_private->flushAndEnqueueNonDisplayingSamples(Vector<RefPtr<MediaSample>>(), trackID);
        return;
    }

    // Seach backward for the previous sync sample.
    DecodeOrderSampleMap::KeyType decodeKey(currentSamplePTSIterator->second->decodeTime(), currentSamplePTSIterator->second->presentationTime());
    auto currentSampleDTSIterator = trackBuffer.samples.decodeOrder().findSampleWithDecodeKey(decodeKey);
    ASSERT(currentSampleDTSIterator != trackBuffer.samples.decodeOrder().end());

    auto reverseCurrentSampleIter = --DecodeOrderSampleMap::reverse_iterator(currentSampleDTSIterator);
    auto reverseLastSyncSampleIter = trackBuffer.samples.decodeOrder().findSyncSamplePriorToDecodeIterator(reverseCurrentSampleIter);
    if (reverseLastSyncSampleIter == trackBuffer.samples.decodeOrder().rend()) {
        trackBuffer.decodeQueue.clear();
        m_private->flushAndEnqueueNonDisplayingSamples(Vector<RefPtr<MediaSample>>(), trackID);
        return;
    }

    Vector<RefPtr<MediaSample>> nonDisplayingSamples;
    for (auto iter = reverseLastSyncSampleIter; iter != reverseCurrentSampleIter; --iter)
        nonDisplayingSamples.append(iter->second);

    m_private->flushAndEnqueueNonDisplayingSamples(nonDisplayingSamples, trackID);

    if (!nonDisplayingSamples.isEmpty()) {
        trackBuffer.lastEnqueuedPresentationTime = nonDisplayingSamples.last()->presentationTime();
        trackBuffer.lastEnqueuedDecodeEndTime = nonDisplayingSamples.last()->decodeTime();
    } else {
        trackBuffer.lastEnqueuedPresentationTime = MediaTime::invalidTime();
        trackBuffer.lastEnqueuedDecodeEndTime = MediaTime::invalidTime();
    }

    // Fill the decode queue with the remaining samples.
    trackBuffer.decodeQueue.clear();
    for (auto iter = currentSampleDTSIterator; iter != trackBuffer.samples.decodeOrder().end(); ++iter)
        trackBuffer.decodeQueue.insert(*iter);
    provideMediaData(trackBuffer, trackID);

    trackBuffer.needsReenqueueing = false;
}


void SourceBuffer::didDropSample()
{
    if (!isRemoved())
        m_source->mediaElement()->incrementDroppedFrameCount();
}

void SourceBuffer::monitorBufferingRate()
{
    if (!m_bufferedSinceLastMonitor)
        return;

    double now = monotonicallyIncreasingTime();
    double interval = now - m_timeOfBufferingMonitor;
    double rateSinceLastMonitor = m_bufferedSinceLastMonitor / interval;

    m_timeOfBufferingMonitor = now;
    m_bufferedSinceLastMonitor = 0;

    m_averageBufferRate = m_averageBufferRate * (1 - ExponentialMovingAverageCoefficient) + rateSinceLastMonitor * ExponentialMovingAverageCoefficient;

    LOG(MediaSource, "SourceBuffer::monitorBufferingRate(%p) - m_avegareBufferRate: %lf", this, m_averageBufferRate);
}

std::unique_ptr<PlatformTimeRanges> SourceBuffer::bufferedAccountingForEndOfStream() const
{
    // FIXME: Revisit this method once the spec bug <https://www.w3.org/Bugs/Public/show_bug.cgi?id=26436> is resolved.
    std::unique_ptr<PlatformTimeRanges> virtualRanges = PlatformTimeRanges::create(m_buffered->ranges());
    if (m_source->isEnded()) {
        MediaTime start = virtualRanges->maximumBufferedTime();
        MediaTime end = m_source->duration();
        if (start <= end)
            virtualRanges->add(start, end);
    }
    return virtualRanges;
}

bool SourceBuffer::hasCurrentTime() const
{
    if (isRemoved() || !m_buffered->length())
        return false;

    MediaTime currentTime = m_source->currentTime();
    MediaTime duration = m_source->duration();
    if (currentTime >= duration)
        return true;

    std::unique_ptr<PlatformTimeRanges> ranges = bufferedAccountingForEndOfStream();
    return abs(ranges->nearest(currentTime) - currentTime) <= currentTimeFudgeFactor();
}

bool SourceBuffer::hasFutureTime() const
{
    if (isRemoved())
        return false;

    std::unique_ptr<PlatformTimeRanges> ranges = bufferedAccountingForEndOfStream();
    if (!ranges->length())
        return false;

    MediaTime currentTime = m_source->currentTime();
    MediaTime duration = m_source->duration();
    if (currentTime >= duration)
        return true;

    MediaTime nearest = ranges->nearest(currentTime);
    if (abs(nearest - currentTime) > currentTimeFudgeFactor())
        return false;

    size_t found = ranges->find(nearest);
    if (found == notFound)
        return false;

    MediaTime localEnd = ranges->end(found);
    if (localEnd == duration)
        return true;

    return localEnd - currentTime > currentTimeFudgeFactor();
}

bool SourceBuffer::canPlayThrough()
{
    if (isRemoved())
        return false;

    monitorBufferingRate();

    // Assuming no fluctuations in the buffering rate, loading 1 second per second or greater
    // means indefinite playback. This could be improved by taking jitter into account.
    if (m_averageBufferRate > 1)
        return true;

    // Add up all the time yet to be buffered.
    MediaTime currentTime = m_source->currentTime();
    MediaTime duration = m_source->duration();

    std::unique_ptr<PlatformTimeRanges> unbufferedRanges = bufferedAccountingForEndOfStream();
    unbufferedRanges->invert();
    unbufferedRanges->intersectWith(PlatformTimeRanges(currentTime, std::max(currentTime, duration)));
    MediaTime unbufferedTime = unbufferedRanges->totalDuration();
    if (!unbufferedTime.isValid())
        return true;

    MediaTime timeRemaining = duration - currentTime;
    return unbufferedTime.toDouble() / m_averageBufferRate < timeRemaining.toDouble();
}

size_t SourceBuffer::extraMemoryCost() const
{
    size_t extraMemoryCost = m_pendingAppendData.capacity();
    for (auto& trackBuffer : m_trackBufferMap.values())
        extraMemoryCost += trackBuffer.samples.sizeInBytes();

    return extraMemoryCost;
}

void SourceBuffer::reportExtraMemoryCost()
{
    size_t extraMemoryCost = this->extraMemoryCost();
    if (extraMemoryCost < m_reportedExtraMemoryCost)
        return;

    size_t extraMemoryCostDelta = extraMemoryCost - m_reportedExtraMemoryCost;
    m_reportedExtraMemoryCost = extraMemoryCost;

    JSC::JSLockHolder lock(scriptExecutionContext()->vm());
    if (extraMemoryCostDelta > 0)
        scriptExecutionContext()->vm().heap.reportExtraMemoryCost(extraMemoryCostDelta);
}

Vector<String> SourceBuffer::bufferedSamplesForTrackID(const AtomicString& trackID)
{
    auto it = m_trackBufferMap.find(trackID);
    if (it == m_trackBufferMap.end())
        return Vector<String>();

    TrackBuffer& trackBuffer = it->value;
    Vector<String> sampleDescriptions;
    for (auto& pair : trackBuffer.samples.decodeOrder())
        sampleDescriptions.append(toString(*pair.second));

    return sampleDescriptions;
}

Document& SourceBuffer::document() const
{
    ASSERT(scriptExecutionContext()->isDocument());
    return *static_cast<Document*>(scriptExecutionContext());
}

void SourceBuffer::setMode(const AtomicString& newMode, ExceptionCode& ec)
{
    // 3.1 Attributes - mode
    // http://www.w3.org/TR/media-source/#widl-SourceBuffer-mode

    // On setting, run the following steps:

    // 1. Let new mode equal the new value being assigned to this attribute.
    // 2. If generate timestamps flag equals true and new mode equals "segments", then throw an INVALID_ACCESS_ERR exception and abort these steps.
    if (m_shouldGenerateTimestamps && newMode == segmentsKeyword()) {
        ec = INVALID_ACCESS_ERR;
        return;
    }

    // 3. If this object has been removed from the sourceBuffers attribute of the parent media source, then throw an INVALID_STATE_ERR exception and abort these steps.
    // 4. If the updating attribute equals true, then throw an INVALID_STATE_ERR exception and abort these steps.
    if (isRemoved() || m_updating) {
        ec = INVALID_STATE_ERR;
        return;
    }

    // 5. If the readyState attribute of the parent media source is in the "ended" state then run the following steps:
    if (m_source->readyState() == MediaSource::endedKeyword()) {
        // 5.1. Set the readyState attribute of the parent media source to "open"
        // 5.2. Queue a task to fire a simple event named sourceopen at the parent media source.
        m_source->openIfInEndedState();
    }

    // 6. If the append state equals PARSING_MEDIA_SEGMENT, then throw an INVALID_STATE_ERR and abort these steps.
    if (m_appendState == ParsingMediaSegment) {
        ec = INVALID_STATE_ERR;
        return;
    }

    // 7. If the new mode equals "sequence", then set the group start timestamp to the group end timestamp.
    if (newMode == sequenceKeyword())
        m_groupStartTimestamp = m_groupEndTimestamp;

    // 8. Update the attribute to new mode.
    m_mode = newMode;
}

} // namespace WebCore

#endif

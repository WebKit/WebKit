/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 * Copyright (C) 2013-2019 Apple Inc. All rights reserved.
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
#include "BufferSource.h"
#include "Event.h"
#include "EventNames.h"
#include "GenericEventQueue.h"
#include "HTMLMediaElement.h"
#include "InbandTextTrack.h"
#include "Logging.h"
#include "MediaDescription.h"
#include "MediaSample.h"
#include "MediaSource.h"
#include "SampleMap.h"
#include "SourceBufferList.h"
#include "SourceBufferPrivate.h"
#include "TextTrackList.h"
#include "TimeRanges.h"
#include "VideoTrackList.h"
#include <JavaScriptCore/JSCInlines.h>
#include <JavaScriptCore/JSLock.h>
#include <JavaScriptCore/VM.h>
#include <limits>
#include <wtf/CheckedArithmetic.h>

namespace WebCore {

static const double ExponentialMovingAverageCoefficient = 0.1;

struct SourceBuffer::TrackBuffer {
    MediaTime lastDecodeTimestamp;
    MediaTime greatestDecodeDuration;
    MediaTime lastFrameDuration;
    MediaTime highestPresentationTimestamp;
    MediaTime lastEnqueuedPresentationTime;
    DecodeOrderSampleMap::KeyType lastEnqueuedDecodeKey;
    MediaTime lastEnqueuedDecodeDuration;
    MediaTime roundedTimestampOffset;
    uint32_t lastFrameTimescale { 0 };
    bool needRandomAccessFlag { true };
    bool enabled { false };
    bool needsReenqueueing { false };
    SampleMap samples;
    DecodeOrderSampleMap::MapType decodeQueue;
    RefPtr<MediaDescription> description;
    PlatformTimeRanges buffered;

    TrackBuffer()
        : lastDecodeTimestamp(MediaTime::invalidTime())
        , greatestDecodeDuration(MediaTime::invalidTime())
        , lastFrameDuration(MediaTime::invalidTime())
        , highestPresentationTimestamp(MediaTime::invalidTime())
        , lastEnqueuedPresentationTime(MediaTime::invalidTime())
        , lastEnqueuedDecodeKey({MediaTime::invalidTime(), MediaTime::invalidTime()})
        , lastEnqueuedDecodeDuration(MediaTime::invalidTime())
    {
    }
};

Ref<SourceBuffer> SourceBuffer::create(Ref<SourceBufferPrivate>&& sourceBufferPrivate, MediaSource* source)
{
    auto sourceBuffer = adoptRef(*new SourceBuffer(WTFMove(sourceBufferPrivate), source));
    sourceBuffer->suspendIfNeeded();
    return sourceBuffer;
}

SourceBuffer::SourceBuffer(Ref<SourceBufferPrivate>&& sourceBufferPrivate, MediaSource* source)
    : ActiveDOMObject(source->scriptExecutionContext())
    , m_private(WTFMove(sourceBufferPrivate))
    , m_source(source)
    , m_asyncEventQueue(*this)
    , m_appendBufferTimer(*this, &SourceBuffer::appendBufferTimerFired)
    , m_appendWindowStart(MediaTime::zeroTime())
    , m_appendWindowEnd(MediaTime::positiveInfiniteTime())
    , m_groupStartTimestamp(MediaTime::invalidTime())
    , m_groupEndTimestamp(MediaTime::zeroTime())
    , m_buffered(TimeRanges::create())
    , m_appendState(WaitingForSegment)
    , m_timeOfBufferingMonitor(MonotonicTime::now())
    , m_pendingRemoveStart(MediaTime::invalidTime())
    , m_pendingRemoveEnd(MediaTime::invalidTime())
    , m_removeTimer(*this, &SourceBuffer::removeTimerFired)
#if !RELEASE_LOG_DISABLED
    , m_logger(m_private->sourceBufferLogger())
    , m_logIdentifier(m_private->sourceBufferLogIdentifier())
#endif
{
    ASSERT(m_source);
    ALWAYS_LOG(LOGIDENTIFIER);

    m_private->setClient(this);
}

SourceBuffer::~SourceBuffer()
{
    ASSERT(isRemoved());
    ALWAYS_LOG(LOGIDENTIFIER);

    m_private->setClient(nullptr);
}

ExceptionOr<Ref<TimeRanges>> SourceBuffer::buffered() const
{
    // Section 3.1 buffered attribute steps.
    // https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#attributes-1
    // 1. If this object has been removed from the sourceBuffers attribute of the parent media source then throw an
    //    InvalidStateError exception and abort these steps.
    if (isRemoved())
        return Exception { InvalidStateError };

    // 2. Return a new static normalized TimeRanges object for the media segments buffered.
    return m_buffered->copy();
}

double SourceBuffer::timestampOffset() const
{
    return m_timestampOffset.toDouble();
}

ExceptionOr<void> SourceBuffer::setTimestampOffset(double offset)
{
    // Section 3.1 timestampOffset attribute setter steps.
    // https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#attributes-1
    // 1. Let new timestamp offset equal the new value being assigned to this attribute.
    // 2. If this object has been removed from the sourceBuffers attribute of the parent media source, then throw an
    //    InvalidStateError exception and abort these steps.
    // 3. If the updating attribute equals true, then throw an InvalidStateError exception and abort these steps.
    if (isRemoved() || m_updating)
        return Exception { InvalidStateError };

    // 4. If the readyState attribute of the parent media source is in the "ended" state then run the following steps:
    // 4.1 Set the readyState attribute of the parent media source to "open"
    // 4.2 Queue a task to fire a simple event named sourceopen at the parent media source.
    m_source->openIfInEndedState();

    // 5. If the append state equals PARSING_MEDIA_SEGMENT, then throw an InvalidStateError and abort these steps.
    if (m_appendState == ParsingMediaSegment)
        return Exception { InvalidStateError };

    MediaTime newTimestampOffset = MediaTime::createWithDouble(offset);

    // 6. If the mode attribute equals "sequence", then set the group start timestamp to new timestamp offset.
    if (m_mode == AppendMode::Sequence)
        m_groupStartTimestamp = newTimestampOffset;

    // 7. Update the attribute to the new value.
    m_timestampOffset = newTimestampOffset;

    for (auto& trackBuffer : m_trackBufferMap.values()) {
        trackBuffer.lastFrameTimescale = 0;
        trackBuffer.roundedTimestampOffset = MediaTime::invalidTime();
    }

    return { };
}

double SourceBuffer::appendWindowStart() const
{
    return m_appendWindowStart.toDouble();
}

ExceptionOr<void> SourceBuffer::setAppendWindowStart(double newValue)
{
    // Section 3.1 appendWindowStart attribute setter steps.
    // W3C Editor's Draft 16 September 2016
    // https://rawgit.com/w3c/media-source/45627646344eea0170dd1cbc5a3d508ca751abb8/media-source-respec.html#dom-sourcebuffer-appendwindowstart
    // 1. If this object has been removed from the sourceBuffers attribute of the parent media source,
    //    then throw an InvalidStateError  exception and abort these steps.
    // 2. If the updating attribute equals true, then throw an InvalidStateError exception and abort these steps.
    if (isRemoved() || m_updating)
        return Exception { InvalidStateError };

    // 3. If the new value is less than 0 or greater than or equal to appendWindowEnd then
    //    throw an TypeError exception and abort these steps.
    if (newValue < 0 || newValue >= m_appendWindowEnd.toDouble())
        return Exception { TypeError };

    // 4. Update the attribute to the new value.
    m_appendWindowStart = MediaTime::createWithDouble(newValue);

    return { };
}

double SourceBuffer::appendWindowEnd() const
{
    return m_appendWindowEnd.toDouble();
}

ExceptionOr<void> SourceBuffer::setAppendWindowEnd(double newValue)
{
    // Section 3.1 appendWindowEnd attribute setter steps.
    // W3C Editor's Draft 16 September 2016
    // https://rawgit.com/w3c/media-source/45627646344eea0170dd1cbc5a3d508ca751abb8/media-source-respec.html#dom-sourcebuffer-appendwindowend
    // 1. If this object has been removed from the sourceBuffers attribute of the parent media source,
    //    then throw an InvalidStateError exception and abort these steps.
    // 2. If the updating attribute equals true, then throw an InvalidStateError exception and abort these steps.
    if (isRemoved() || m_updating)
        return Exception { InvalidStateError };

    // 3. If the new value equals NaN, then throw an TypeError and abort these steps.
    // 4. If the new value is less than or equal to appendWindowStart then throw an TypeError exception
    //    and abort these steps.
    if (std::isnan(newValue) || newValue <= m_appendWindowStart.toDouble())
        return Exception { TypeError };

    // 5.. Update the attribute to the new value.
    m_appendWindowEnd = MediaTime::createWithDouble(newValue);

    return { };
}

ExceptionOr<void> SourceBuffer::appendBuffer(const BufferSource& data)
{
    return appendBufferInternal(static_cast<const unsigned char*>(data.data()), data.length());
}

void SourceBuffer::resetParserState()
{
    // Section 3.5.2 Reset Parser State algorithm steps.
    // http://www.w3.org/TR/2014/CR-media-source-20140717/#sourcebuffer-reset-parser-state
    // 1. If the append state equals PARSING_MEDIA_SEGMENT and the input buffer contains some complete coded frames,
    //    then run the coded frame processing algorithm until all of these complete coded frames have been processed.
    // FIXME: If any implementation will work in pulling mode (instead of async push to SourceBufferPrivate, and forget)
    //     this should be handled somehow either here, or in m_private->abort();

    // 2. Unset the last decode timestamp on all track buffers.
    // 3. Unset the last frame duration on all track buffers.
    // 4. Unset the highest presentation timestamp on all track buffers.
    // 5. Set the need random access point flag on all track buffers to true.
    for (auto& trackBufferPair : m_trackBufferMap.values()) {
        trackBufferPair.lastDecodeTimestamp = MediaTime::invalidTime();
        trackBufferPair.greatestDecodeDuration = MediaTime::invalidTime();
        trackBufferPair.lastFrameDuration = MediaTime::invalidTime();
        trackBufferPair.highestPresentationTimestamp = MediaTime::invalidTime();
        trackBufferPair.needRandomAccessFlag = true;
    }
    // 6. Remove all bytes from the input buffer.
    // Note: this is handled by abortIfUpdating()
    // 7. Set append state to WAITING_FOR_SEGMENT.
    m_appendState = WaitingForSegment;

    m_private->resetParserState();
}

ExceptionOr<void> SourceBuffer::abort()
{
    // Section 3.2 abort() method steps.
    // https://rawgit.com/w3c/media-source/45627646344eea0170dd1cbc5a3d508ca751abb8/media-source-respec.html#dom-sourcebuffer-abort
    // 1. If this object has been removed from the sourceBuffers attribute of the parent media source
    //    then throw an InvalidStateError exception and abort these steps.
    // 2. If the readyState attribute of the parent media source is not in the "open" state
    //    then throw an InvalidStateError exception and abort these steps.
    if (isRemoved() || !m_source->isOpen())
        return Exception { InvalidStateError };

    // 3. If the range removal algorithm is running, then throw an InvalidStateError exception and abort these steps.
    if (m_removeTimer.isActive())
        return Exception { InvalidStateError };

    // 4. If the sourceBuffer.updating attribute equals true, then run the following steps: ...
    abortIfUpdating();

    // 5. Run the reset parser state algorithm.
    resetParserState();

    // 6. Set appendWindowStart to the presentation start time.
    m_appendWindowStart = MediaTime::zeroTime();

    // 7. Set appendWindowEnd to positive Infinity.
    m_appendWindowEnd = MediaTime::positiveInfiniteTime();

    return { };
}

ExceptionOr<void> SourceBuffer::remove(double start, double end)
{
    return remove(MediaTime::createWithDouble(start), MediaTime::createWithDouble(end));
}

ExceptionOr<void> SourceBuffer::remove(const MediaTime& start, const MediaTime& end)
{
    DEBUG_LOG(LOGIDENTIFIER, "start = ", start, ", end = ", end);

    // https://rawgit.com/w3c/media-source/45627646344eea0170dd1cbc5a3d508ca751abb8/media-source-respec.html#dom-sourcebuffer-remove
    // Section 3.2 remove() method steps.
    // 1. If this object has been removed from the sourceBuffers attribute of the parent media source then throw
    //    an InvalidStateError exception and abort these steps.
    // 2. If the updating attribute equals true, then throw an InvalidStateError exception and abort these steps.
    if (isRemoved() || m_updating)
        return Exception { InvalidStateError };

    // 3. If duration equals NaN, then throw a TypeError exception and abort these steps.
    // 4. If start is negative or greater than duration, then throw a TypeError exception and abort these steps.
    // 5. If end is less than or equal to start or end equals NaN, then throw a TypeError exception and abort these steps.
    if (m_source->duration().isInvalid()
        || end.isInvalid()
        || start.isInvalid()
        || start < MediaTime::zeroTime()
        || start > m_source->duration()
        || end <= start) {
        return Exception { TypeError };
    }

    // 6. If the readyState attribute of the parent media source is in the "ended" state then run the following steps:
    // 6.1. Set the readyState attribute of the parent media source to "open"
    // 6.2. Queue a task to fire a simple event named sourceopen at the parent media source .
    m_source->openIfInEndedState();

    // 7. Run the range removal algorithm with start and end as the start and end of the removal range.
    rangeRemoval(start, end);

    return { };
}

void SourceBuffer::rangeRemoval(const MediaTime& start, const MediaTime& end)
{
    // 3.5.7 Range Removal
    // https://rawgit.com/w3c/media-source/7bbe4aa33c61ec025bc7acbd80354110f6a000f9/media-source.html#sourcebuffer-range-removal
    // 1. Let start equal the starting presentation timestamp for the removal range.
    // 2. Let end equal the end presentation timestamp for the removal range.
    // 3. Set the updating attribute to true.
    m_updating = true;

    // 4. Queue a task to fire a simple event named updatestart at this SourceBuffer object.
    scheduleEvent(eventNames().updatestartEvent);

    // 5. Return control to the caller and run the rest of the steps asynchronously.
    m_pendingRemoveStart = start;
    m_pendingRemoveEnd = end;
    m_removeTimer.startOneShot(0_s);
}

ExceptionOr<void> SourceBuffer::changeType(const String& type)
{
    // changeType() proposed API. See issue #155: <https://github.com/w3c/media-source/issues/155>
    // https://rawgit.com/wicg/media-source/codec-switching/index.html#dom-sourcebuffer-changetype

    // 1. If type is an empty string then throw a TypeError exception and abort these steps.
    if (type.isEmpty())
        return Exception { TypeError };

    // 2. If this object has been removed from the sourceBuffers attribute of the parent media source,
    // then throw an InvalidStateError exception and abort these steps.
    // 3. If the updating attribute equals true, then throw an InvalidStateError exception and abort these steps.
    if (isRemoved() || m_updating)
        return Exception { InvalidStateError };

    // 4. If type contains a MIME type that is not supported or contains a MIME type that is not supported with
    // the types specified (currently or previously) of SourceBuffer objects in the sourceBuffers attribute of
    // the parent media source, then throw a NotSupportedError exception and abort these steps.
    ContentType contentType(type);
    if (!m_private->canSwitchToType(contentType))
        return Exception { NotSupportedError };

    // 5. If the readyState attribute of the parent media source is in the "ended" state then run the following
    // steps:
    // 5.1. Set the readyState attribute of the parent media source to "open"
    // 5.2. Queue a task to fire a simple event named sourceopen at the parent media source.
    m_source->openIfInEndedState();

    // 6. Run the reset parser state algorithm.
    resetParserState();

    // 7. Update the generate timestamps flag on this SourceBuffer object to the value in the "Generate Timestamps
    // Flag" column of the byte stream format registry [MSE-REGISTRY] entry that is associated with type.
    setShouldGenerateTimestamps(MediaSource::contentTypeShouldGenerateTimestamps(contentType));

    // ↳ If the generate timestamps flag equals true:
    // Set the mode attribute on this SourceBuffer object to "sequence", including running the associated steps
    // for that attribute being set.
    if (m_shouldGenerateTimestamps)
        setMode(AppendMode::Sequence);

    // ↳ Otherwise:
    // Keep the previous value of the mode attribute on this SourceBuffer object, without running any associated
    // steps for that attribute being set.
    // NOTE: No-op.

    // 9. Set pending initialization segment for changeType flag to true.
    m_pendingInitializationSegmentForChangeType = true;

    return { };
}

void SourceBuffer::abortIfUpdating()
{
    // Section 3.2 abort() method step 4 substeps.
    // https://rawgit.com/w3c/media-source/45627646344eea0170dd1cbc5a3d508ca751abb8/media-source-respec.html#dom-sourcebuffer-abort

    if (!m_updating)
        return;

    // 4.1. Abort the buffer append algorithm if it is running.
    m_appendBufferTimer.stop();
    m_pendingAppendData.clear();
    m_private->abort();

    // 4.2. Set the updating attribute to false.
    m_updating = false;

    // 4.3. Queue a task to fire a simple event named abort at this SourceBuffer object.
    scheduleEvent(eventNames().abortEvent);

    // 4.4. Queue a task to fire a simple event named updateend at this SourceBuffer object.
    scheduleEvent(eventNames().updateendEvent);
}

MediaTime SourceBuffer::highestPresentationTimestamp() const
{
    MediaTime highestTime;
    for (auto& trackBuffer : m_trackBufferMap.values()) {
        auto lastSampleIter = trackBuffer.samples.presentationOrder().rbegin();
        if (lastSampleIter == trackBuffer.samples.presentationOrder().rend())
            continue;
        highestTime = std::max(highestTime, lastSampleIter->first);
    }
    return highestTime;
}

void SourceBuffer::readyStateChanged()
{
    updateBufferedFromTrackBuffers();
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
    m_source = nullptr;
}

void SourceBuffer::seekToTime(const MediaTime& time)
{
    ALWAYS_LOG(LOGIDENTIFIER, time);

    for (auto& trackBufferPair : m_trackBufferMap) {
        TrackBuffer& trackBuffer = trackBufferPair.value;
        const AtomicString& trackID = trackBufferPair.key;

        trackBuffer.needsReenqueueing = true;
        reenqueueMediaForTime(trackBuffer, trackID, time);
    }
}

MediaTime SourceBuffer::sourceBufferPrivateFastSeekTimeForMediaTime(const MediaTime& targetTime, const MediaTime& negativeThreshold, const MediaTime& positiveThreshold)
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

void SourceBuffer::suspend(ReasonForSuspension reason)
{
    switch (reason) {
    case ReasonForSuspension::PageCache:
    case ReasonForSuspension::PageWillBeSuspended:
        m_asyncEventQueue.suspend();
        break;
    case ReasonForSuspension::JavaScriptDebuggerPaused:
    case ReasonForSuspension::WillDeferLoading:
        // Do nothing, we don't pause media playback in these cases.
        break;
    }
}

void SourceBuffer::resume()
{
    m_asyncEventQueue.resume();
}

void SourceBuffer::stop()
{
    m_asyncEventQueue.close();
    m_appendBufferTimer.stop();
    m_removeTimer.stop();
}

bool SourceBuffer::canSuspendForDocumentSuspension() const
{
    return !hasPendingActivity();
}

const char* SourceBuffer::activeDOMObjectName() const
{
    return "SourceBuffer";
}

bool SourceBuffer::isRemoved() const
{
    return !m_source;
}

void SourceBuffer::scheduleEvent(const AtomicString& eventName)
{
    auto event = Event::create(eventName, Event::CanBubble::No, Event::IsCancelable::No);
    event->setTarget(this);

    m_asyncEventQueue.enqueueEvent(WTFMove(event));
}

ExceptionOr<void> SourceBuffer::appendBufferInternal(const unsigned char* data, unsigned size)
{
    // Section 3.2 appendBuffer()
    // https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#widl-SourceBuffer-appendBuffer-void-ArrayBufferView-data

    // Step 1 is enforced by the caller.
    // 2. Run the prepare append algorithm.
    // Section 3.5.4 Prepare AppendAlgorithm

    // 1. If the SourceBuffer has been removed from the sourceBuffers attribute of the parent media source
    // then throw an InvalidStateError exception and abort these steps.
    // 2. If the updating attribute equals true, then throw an InvalidStateError exception and abort these steps.
    if (isRemoved() || m_updating)
        return Exception { InvalidStateError };

    // 3. If the readyState attribute of the parent media source is in the "ended" state then run the following steps:
    // 3.1. Set the readyState attribute of the parent media source to "open"
    // 3.2. Queue a task to fire a simple event named sourceopen at the parent media source .
    m_source->openIfInEndedState();

    // 4. Run the coded frame eviction algorithm.
    evictCodedFrames(size);

    // FIXME: enable this code when MSE libraries have been updated to support it.
#if USE(GSTREAMER)
    // 5. If the buffer full flag equals true, then throw a QuotaExceededError exception and abort these step.
    if (m_bufferFull) {
        ERROR_LOG(LOGIDENTIFIER, "buffer full, failing with QuotaExceededError error");
        return Exception { QuotaExceededError };
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
    m_appendBufferTimer.startOneShot(0_s);

    reportExtraMemoryAllocated();

    return { };
}

void SourceBuffer::appendBufferTimerFired()
{
    if (isRemoved())
        return;

    ASSERT(m_updating);

    // Section 3.5.5 Buffer Append Algorithm
    // https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#sourcebuffer-buffer-append

    // 1. Run the segment parser loop algorithm.

    // Section 3.5.1 Segment Parser Loop
    // https://dvcs.w3.org/hg/html-media/raw-file/tip/media-source/media-source.html#sourcebuffer-segment-parser-loop
    // When the segment parser loop algorithm is invoked, run the following steps:

    // 1. Loop Top: If the input buffer is empty, then jump to the need more data step below.
    if (!m_pendingAppendData.size()) {
        sourceBufferPrivateAppendComplete(AppendSucceeded);
        return;
    }

    // Manually clear out the m_pendingAppendData Vector, in case the platform implementation
    // rejects appending the buffer for whatever reason.
    // FIXME: The implementation should guarantee the move from this Vector, and we should
    // assert here to confirm that. See https://bugs.webkit.org/show_bug.cgi?id=178003.
    m_private->append(WTFMove(m_pendingAppendData));
    m_pendingAppendData.clear();
}

void SourceBuffer::sourceBufferPrivateAppendComplete(AppendResult result)
{
    if (isRemoved())
        return;

    // Resolve the changes it TrackBuffers' buffered ranges
    // into the SourceBuffer's buffered ranges
    updateBufferedFromTrackBuffers();

    // Section 3.5.5 Buffer Append Algorithm, ctd.
    // https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#sourcebuffer-buffer-append

    // 2. If the input buffer contains bytes that violate the SourceBuffer byte stream format specification,
    // then run the append error algorithm with the decode error parameter set to true and abort this algorithm.
    if (result == ParsingFailed) {
        ERROR_LOG(LOGIDENTIFIER, "ParsingFailed");
        appendError(true);
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
            DEBUG_LOG(LOGIDENTIFIER, "reenqueuing at time ", currentMediaTime);
            reenqueueMediaForTime(trackBuffer, trackID, currentMediaTime);
        } else
            provideMediaData(trackBuffer, trackID);
    }

    reportExtraMemoryAllocated();
    if (extraMemoryCost() > this->maximumBufferSize())
        m_bufferFull = true;

    DEBUG_LOG(LOGIDENTIFIER);
}

void SourceBuffer::sourceBufferPrivateDidReceiveRenderingError(int error)
{
#if RELEASE_LOG_DISABLED
    UNUSED_PARAM(error);
#endif

    ERROR_LOG(LOGIDENTIFIER, error);

    if (!isRemoved())
        m_source->streamEndedWithError(MediaSource::EndOfStreamError::Decode);
}

static bool decodeTimeComparator(const PresentationOrderSampleMap::MapType::value_type& a, const PresentationOrderSampleMap::MapType::value_type& b)
{
    return a.second->decodeTime() < b.second->decodeTime();
}

static PlatformTimeRanges removeSamplesFromTrackBuffer(const DecodeOrderSampleMap::MapType& samples, SourceBuffer::TrackBuffer& trackBuffer, const SourceBuffer* buffer, const char* logPrefix)
{
#if !RELEASE_LOG_DISABLED
    MediaTime earliestSample = MediaTime::positiveInfiniteTime();
    MediaTime latestSample = MediaTime::zeroTime();
    size_t bytesRemoved = 0;
    auto logIdentifier = WTF::Logger::LogSiteIdentifier(buffer->logClassName(), logPrefix, buffer->logIdentifier());
    auto& logger = buffer->logger();
    auto willLog = logger.willLog(buffer->logChannel(), WTFLogLevelDebug);
#else
    UNUSED_PARAM(logPrefix);
    UNUSED_PARAM(buffer);
#endif

    PlatformTimeRanges erasedRanges;
    for (const auto& sampleIt : samples) {
        const DecodeOrderSampleMap::KeyType& decodeKey = sampleIt.first;
#if !RELEASE_LOG_DISABLED
        size_t startBufferSize = trackBuffer.samples.sizeInBytes();
#endif

        const RefPtr<MediaSample>& sample = sampleIt.second;

#if !RELEASE_LOG_DISABLED
        if (willLog)
            logger.debug(buffer->logChannel(), logIdentifier, "removing sample ", *sampleIt.second);
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
        logger.debug(buffer->logChannel(), logIdentifier, "removed ", bytesRemoved, ", start = ", earliestSample, ", end = ", latestSample);
#endif

    return erasedRanges;
}

void SourceBuffer::removeCodedFrames(const MediaTime& start, const MediaTime& end)
{
    DEBUG_LOG(LOGIDENTIFIER, "start = ", start, ", end = ", end);

    // 3.5.9 Coded Frame Removal Algorithm
    // https://dvcs.w3.org/hg/html-media/raw-file/tip/media-source/media-source.html#sourcebuffer-coded-frame-removal

    // 1. Let start be the starting presentation timestamp for the removal range.
    MediaTime durationMediaTime = m_source->duration();
    MediaTime currentMediaTime = m_source->currentTime();

    // 2. Let end be the end presentation timestamp for the removal range.
    // 3. For each track buffer in this source buffer, run the following steps:
    for (auto& trackBuffer : m_trackBufferMap.values()) {
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
        PlatformTimeRanges erasedRanges = removeSamplesFromTrackBuffer(erasedSamples, trackBuffer, this, "removeCodedFrames");

        // Only force the TrackBuffer to re-enqueue if the removed ranges overlap with enqueued and possibly
        // not yet displayed samples.
        if (trackBuffer.lastEnqueuedPresentationTime.isValid() && currentMediaTime < trackBuffer.lastEnqueuedPresentationTime) {
            PlatformTimeRanges possiblyEnqueuedRanges(currentMediaTime, trackBuffer.lastEnqueuedPresentationTime);
            possiblyEnqueuedRanges.intersectWith(erasedRanges);
            if (possiblyEnqueuedRanges.length())
                trackBuffer.needsReenqueueing = true;
        }

        erasedRanges.invert();
        trackBuffer.buffered.intersectWith(erasedRanges);
        setBufferedDirty(true);

        // 3.4 If this object is in activeSourceBuffers, the current playback position is greater than or equal to start
        // and less than the remove end timestamp, and HTMLMediaElement.readyState is greater than HAVE_METADATA, then set
        // the HTMLMediaElement.readyState attribute to HAVE_METADATA and stall playback.
        if (m_active && currentMediaTime >= start && currentMediaTime < end && m_private->readyState() > MediaPlayer::HaveMetadata)
            m_private->setReadyState(MediaPlayer::HaveMetadata);
    }
    
    updateBufferedFromTrackBuffers();

    // 4. If buffer full flag equals true and this object is ready to accept more bytes, then set the buffer full flag to false.
    // No-op

    LOG(Media, "SourceBuffer::removeCodedFrames(%p) - buffered = %s", this, toString(m_buffered->ranges()).utf8().data());
}

void SourceBuffer::removeTimerFired()
{
    if (isRemoved())
        return;

    ASSERT(m_updating);
    ASSERT(m_pendingRemoveStart.isValid());
    ASSERT(m_pendingRemoveStart < m_pendingRemoveEnd);

    // Section 3.5.7 Range Removal
    // http://w3c.github.io/media-source/#sourcebuffer-range-removal

    // 6. Run the coded frame removal algorithm with start and end as the start and end of the removal range.
    removeCodedFrames(m_pendingRemoveStart, m_pendingRemoveEnd);

    // 7. Set the updating attribute to false.
    m_updating = false;
    m_pendingRemoveStart = MediaTime::invalidTime();
    m_pendingRemoveEnd = MediaTime::invalidTime();

    // 8. Queue a task to fire a simple event named update at this SourceBuffer object.
    scheduleEvent(eventNames().updateEvent);

    // 9. Queue a task to fire a simple event named updateend at this SourceBuffer object.
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

#if !RELEASE_LOG_DISABLED
    DEBUG_LOG(LOGIDENTIFIER, "currentTime = ", m_source->currentTime(), ", require ", extraMemoryCost() + newDataSize, " bytes, maximum buffer size is ", maximumBufferSize);
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

#if !RELEASE_LOG_DISABLED
    if (!m_bufferFull) {
        DEBUG_LOG(LOGIDENTIFIER, "evicted ", initialBufferedSize - extraMemoryCost());
        return;
    }
#endif

    // If there still isn't enough free space and there buffers in time ranges after the current range (ie. there is a gap after
    // the current buffered range), delete 30 seconds at a time from duration back to the current time range or 30 seconds after
    // currenTime whichever we hit first.
    auto buffered = m_buffered->ranges();
    size_t currentTimeRange = buffered.find(currentTime);
    if (currentTimeRange == notFound || currentTimeRange == buffered.length() - 1) {
#if !RELEASE_LOG_DISABLED
        ERROR_LOG(LOGIDENTIFIER, "FAILED to free enough after evicting ", initialBufferedSize - extraMemoryCost());
#endif
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

#if !RELEASE_LOG_DISABLED
    if (m_bufferFull)
        ERROR_LOG(LOGIDENTIFIER, "FAILED to free enough after evicting ", initialBufferedSize - extraMemoryCost());
    else
        DEBUG_LOG(LOGIDENTIFIER, "evicted ", initialBufferedSize - extraMemoryCost());
#endif
}

size_t SourceBuffer::maximumBufferSize() const
{
    if (isRemoved())
        return 0;

    auto* element = m_source->mediaElement();
    if (!element)
        return 0;

    return element->maximumSourceBufferSize(*this);
}

VideoTrackList& SourceBuffer::videoTracks()
{
    if (!m_videoTracks)
        m_videoTracks = VideoTrackList::create(m_source->mediaElement(), scriptExecutionContext());
    return *m_videoTracks;
}

AudioTrackList& SourceBuffer::audioTracks()
{
    if (!m_audioTracks)
        m_audioTracks = AudioTrackList::create(m_source->mediaElement(), scriptExecutionContext());
    return *m_audioTracks;
}

TextTrackList& SourceBuffer::textTracks()
{
    if (!m_textTracks)
        m_textTracks = TextTrackList::create(m_source->mediaElement(), scriptExecutionContext());
    return *m_textTracks;
}

void SourceBuffer::setActive(bool active)
{
    if (m_active == active)
        return;

    m_active = active;
    m_private->setActive(active);
    if (!isRemoved())
        m_source->sourceBufferDidChangeActiveState(*this, active);
}

void SourceBuffer::sourceBufferPrivateDidReceiveInitializationSegment(const InitializationSegment& segment)
{
    if (isRemoved())
        return;

    ALWAYS_LOG(LOGIDENTIFIER);

    // 3.5.8 Initialization Segment Received (ctd)
    // https://rawgit.com/w3c/media-source/c3ad59c7a370d04430969ba73d18dc9bcde57a33/index.html#sourcebuffer-init-segment-received [Editor's Draft 09 January 2015]

    // 1. Update the duration attribute if it currently equals NaN:
    if (m_source->duration().isInvalid()) {
        // ↳ If the initialization segment contains a duration:
        //   Run the duration change algorithm with new duration set to the duration in the initialization segment.
        // ↳ Otherwise:
        //   Run the duration change algorithm with new duration set to positive Infinity.
        if (segment.duration.isValid() && !segment.duration.isIndefinite())
            m_source->setDurationInternal(segment.duration);
        else
            m_source->setDurationInternal(MediaTime::positiveInfiniteTime());
    }

    // 2. If the initialization segment has no audio, video, or text tracks, then run the append error algorithm
    // with the decode error parameter set to true and abort these steps.
    if (segment.audioTracks.isEmpty() && segment.videoTracks.isEmpty() && segment.textTracks.isEmpty()) {
        appendError(true);
        return;
    }

    // 3. If the first initialization segment flag is true, then run the following steps:
    if (m_receivedFirstInitializationSegment) {

        // 3.1. Verify the following properties. If any of the checks fail then run the append error algorithm
        // with the decode error parameter set to true and abort these steps.
        if (!validateInitializationSegment(segment)) {
            appendError(true);
            return;
        }
        // 3.2 Add the appropriate track descriptions from this initialization segment to each of the track buffers.
        ASSERT(segment.audioTracks.size() == audioTracks().length());
        for (auto& audioTrackInfo : segment.audioTracks) {
            if (audioTracks().length() == 1) {
                audioTracks().item(0)->setPrivate(*audioTrackInfo.track);
                break;
            }

            auto audioTrack = audioTracks().getTrackById(audioTrackInfo.track->id());
            ASSERT(audioTrack);
            audioTrack->setPrivate(*audioTrackInfo.track);
        }

        ASSERT(segment.videoTracks.size() == videoTracks().length());
        for (auto& videoTrackInfo : segment.videoTracks) {
            if (videoTracks().length() == 1) {
                videoTracks().item(0)->setPrivate(*videoTrackInfo.track);
                break;
            }

            auto videoTrack = videoTracks().getTrackById(videoTrackInfo.track->id());
            ASSERT(videoTrack);
            videoTrack->setPrivate(*videoTrackInfo.track);
        }

        ASSERT(segment.textTracks.size() == textTracks().length());
        for (auto& textTrackInfo : segment.textTracks) {
            if (textTracks().length() == 1) {
                downcast<InbandTextTrack>(*textTracks().item(0)).setPrivate(*textTrackInfo.track);
                break;
            }

            auto textTrack = textTracks().getTrackById(textTrackInfo.track->id());
            ASSERT(textTrack);
            downcast<InbandTextTrack>(*textTrack).setPrivate(*textTrackInfo.track);
        }

        // 3.3 Set the need random access point flag on all track buffers to true.
        for (auto& trackBuffer : m_trackBufferMap.values())
            trackBuffer.needRandomAccessFlag = true;
    }

    // 4. Let active track flag equal false.
    bool activeTrackFlag = false;

    // 5. If the first initialization segment flag is false, then run the following steps:
    if (!m_receivedFirstInitializationSegment) {
        // 5.1 If the initialization segment contains tracks with codecs the user agent does not support,
        // then run the append error algorithm with the decode error parameter set to true and abort these steps.
        // NOTE: This check is the responsibility of the SourceBufferPrivate.

        // 5.2 For each audio track in the initialization segment, run following steps:
        for (auto& audioTrackInfo : segment.audioTracks) {
            // FIXME: Implement steps 5.2.1-5.2.8.1 as per Editor's Draft 09 January 2015, and reorder this
            // 5.2.1 Let new audio track be a new AudioTrack object.
            // 5.2.2 Generate a unique ID and assign it to the id property on new video track.
            auto newAudioTrack = AudioTrack::create(*this, *audioTrackInfo.track);
            newAudioTrack->setSourceBuffer(this);

            // 5.2.3 If audioTracks.length equals 0, then run the following steps:
            if (!audioTracks().length()) {
                // 5.2.3.1 Set the enabled property on new audio track to true.
                newAudioTrack->setEnabled(true);

                // 5.2.3.2 Set active track flag to true.
                activeTrackFlag = true;
            }

            // 5.2.4 Add new audio track to the audioTracks attribute on this SourceBuffer object.
            // 5.2.5 Queue a task to fire a trusted event named addtrack, that does not bubble and is
            // not cancelable, and that uses the TrackEvent interface, at the AudioTrackList object
            // referenced by the audioTracks attribute on this SourceBuffer object.
            audioTracks().append(newAudioTrack.copyRef());

            // 5.2.6 Add new audio track to the audioTracks attribute on the HTMLMediaElement.
            // 5.2.7 Queue a task to fire a trusted event named addtrack, that does not bubble and is
            // not cancelable, and that uses the TrackEvent interface, at the AudioTrackList object
            // referenced by the audioTracks attribute on the HTMLMediaElement.
            m_source->mediaElement()->ensureAudioTracks().append(newAudioTrack.copyRef());

            // 5.2.8 Create a new track buffer to store coded frames for this track.
            ASSERT(!m_trackBufferMap.contains(newAudioTrack->id()));
            auto& trackBuffer = m_trackBufferMap.add(newAudioTrack->id(), TrackBuffer()).iterator->value;

            // 5.2.9 Add the track description for this track to the track buffer.
            trackBuffer.description = audioTrackInfo.description;

            m_audioCodecs.append(trackBuffer.description->codec());
        }

        // 5.3 For each video track in the initialization segment, run following steps:
        for (auto& videoTrackInfo : segment.videoTracks) {
            // FIXME: Implement steps 5.3.1-5.3.8.1 as per Editor's Draft 09 January 2015, and reorder this
            // 5.3.1 Let new video track be a new VideoTrack object.
            // 5.3.2 Generate a unique ID and assign it to the id property on new video track.
            auto newVideoTrack = VideoTrack::create(*this, *videoTrackInfo.track);
            newVideoTrack->setSourceBuffer(this);

            // 5.3.3 If videoTracks.length equals 0, then run the following steps:
            if (!videoTracks().length()) {
                // 5.3.3.1 Set the selected property on new video track to true.
                newVideoTrack->setSelected(true);

                // 5.3.3.2 Set active track flag to true.
                activeTrackFlag = true;
            }

            // 5.3.4 Add new video track to the videoTracks attribute on this SourceBuffer object.
            // 5.3.5 Queue a task to fire a trusted event named addtrack, that does not bubble and is
            // not cancelable, and that uses the TrackEvent interface, at the VideoTrackList object
            // referenced by the videoTracks attribute on this SourceBuffer object.
            videoTracks().append(newVideoTrack.copyRef());

            // 5.3.6 Add new video track to the videoTracks attribute on the HTMLMediaElement.
            // 5.3.7 Queue a task to fire a trusted event named addtrack, that does not bubble and is
            // not cancelable, and that uses the TrackEvent interface, at the VideoTrackList object
            // referenced by the videoTracks attribute on the HTMLMediaElement.
            m_source->mediaElement()->ensureVideoTracks().append(newVideoTrack.copyRef());

            // 5.3.8 Create a new track buffer to store coded frames for this track.
            ASSERT(!m_trackBufferMap.contains(newVideoTrack->id()));
            auto& trackBuffer = m_trackBufferMap.add(newVideoTrack->id(), TrackBuffer()).iterator->value;

            // 5.3.9 Add the track description for this track to the track buffer.
            trackBuffer.description = videoTrackInfo.description;

            m_videoCodecs.append(trackBuffer.description->codec());
        }

        // 5.4 For each text track in the initialization segment, run following steps:
        for (auto& textTrackInfo : segment.textTracks) {
            auto& textTrackPrivate = *textTrackInfo.track;

            // FIXME: Implement steps 5.4.1-5.4.8.1 as per Editor's Draft 09 January 2015, and reorder this
            // 5.4.1 Let new text track be a new TextTrack object with its properties populated with the
            // appropriate information from the initialization segment.
            auto newTextTrack = InbandTextTrack::create(*scriptExecutionContext(), *this, textTrackPrivate);

            // 5.4.2 If the mode property on new text track equals "showing" or "hidden", then set active
            // track flag to true.
            if (textTrackPrivate.mode() != InbandTextTrackPrivate::Disabled)
                activeTrackFlag = true;

            // 5.4.3 Add new text track to the textTracks attribute on this SourceBuffer object.
            // 5.4.4 Queue a task to fire a trusted event named addtrack, that does not bubble and is
            // not cancelable, and that uses the TrackEvent interface, at textTracks attribute on this
            // SourceBuffer object.
            textTracks().append(newTextTrack.get());

            // 5.4.5 Add new text track to the textTracks attribute on the HTMLMediaElement.
            // 5.4.6 Queue a task to fire a trusted event named addtrack, that does not bubble and is
            // not cancelable, and that uses the TrackEvent interface, at the TextTrackList object
            // referenced by the textTracks attribute on the HTMLMediaElement.
            m_source->mediaElement()->ensureTextTracks().append(WTFMove(newTextTrack));

            // 5.4.7 Create a new track buffer to store coded frames for this track.
            ASSERT(!m_trackBufferMap.contains(textTrackPrivate.id()));
            auto& trackBuffer = m_trackBufferMap.add(textTrackPrivate.id(), TrackBuffer()).iterator->value;

            // 5.4.8 Add the track description for this track to the track buffer.
            trackBuffer.description = textTrackInfo.description;

            m_textCodecs.append(trackBuffer.description->codec());
        }

        // 5.5 If active track flag equals true, then run the following steps:
        if (activeTrackFlag) {
            // 5.5.1 Add this SourceBuffer to activeSourceBuffers.
            // 5.5.2 Queue a task to fire a simple event named addsourcebuffer at activeSourceBuffers
            setActive(true);
        }

        // 5.6 Set first initialization segment flag to true.
        m_receivedFirstInitializationSegment = true;
    }

    // (Note: Issue #155 adds this step after step 5:)
    // 6. Set  pending initialization segment for changeType flag  to false.
    m_pendingInitializationSegmentForChangeType = false;

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
    // FIXME: ordering of all 3.5.X (X>=7) functions needs to be updated to post-[24 July 2014 Editor's Draft] version
    // 3.5.8 Initialization Segment Received (ctd)
    // https://rawgit.com/w3c/media-source/c3ad59c7a370d04430969ba73d18dc9bcde57a33/index.html#sourcebuffer-init-segment-received [Editor's Draft 09 January 2015]

    // Note: those are checks from step 3.1
    //   * The number of audio, video, and text tracks match what was in the first initialization segment.
    if (segment.audioTracks.size() != audioTracks().length()
        || segment.videoTracks.size() != videoTracks().length()
        || segment.textTracks.size() != textTracks().length())
        return false;

    //   * The codecs for each track, match what was specified in the first initialization segment.
    // (Note: Issue #155 strikes out this check. For broad compatibility when this experimental feature
    // is not enabled, only perform this check if the "pending initialization segment for changeType flag"
    // is not set.)
    for (auto& audioTrackInfo : segment.audioTracks) {
        if (m_audioCodecs.contains(audioTrackInfo.description->codec()))
            continue;

        if (!m_pendingInitializationSegmentForChangeType)
            return false;

        m_audioCodecs.append(audioTrackInfo.description->codec());
    }

    for (auto& videoTrackInfo : segment.videoTracks) {
        if (m_videoCodecs.contains(videoTrackInfo.description->codec()))
            continue;

        if (!m_pendingInitializationSegmentForChangeType)
            return false;

        m_videoCodecs.append(videoTrackInfo.description->codec());
    }

    for (auto& textTrackInfo : segment.textTracks) {
        if (m_textCodecs.contains(textTrackInfo.description->codec()))
            continue;

        if (!m_pendingInitializationSegmentForChangeType)
            return false;

        m_textCodecs.append(textTrackInfo.description->codec());
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

void SourceBuffer::appendError(bool decodeErrorParam)
{
    // 3.5.3 Append Error Algorithm
    // https://rawgit.com/w3c/media-source/c3ad59c7a370d04430969ba73d18dc9bcde57a33/index.html#sourcebuffer-append-error [Editor's Draft 09 January 2015]

    ASSERT(m_updating);
    // 1. Run the reset parser state algorithm.
    resetParserState();

    // 2. Set the updating attribute to false.
    m_updating = false;

    // 3. Queue a task to fire a simple event named error at this SourceBuffer object.
    scheduleEvent(eventNames().errorEvent);

    // 4. Queue a task to fire a simple event named updateend at this SourceBuffer object.
    scheduleEvent(eventNames().updateendEvent);

    // 5. If decode error is true, then run the end of stream algorithm with the error parameter set to "decode".
    if (decodeErrorParam)
        m_source->streamEndedWithError(MediaSource::EndOfStreamError::Decode);
}

void SourceBuffer::sourceBufferPrivateDidReceiveSample(MediaSample& sample)
{
    if (isRemoved())
        return;

    // 3.5.1 Segment Parser Loop
    // 6.1 If the first initialization segment received flag is false, (Note: Issue # 155 & changeType()
    // algorithm) or the  pending initialization segment for changeType flag  is true, (End note)
    // then run the append error algorithm
    //     with the decode error parameter set to true and abort this algorithm.
    // Note: current design makes SourceBuffer somehow ignorant of append state - it's more a thing
    //  of SourceBufferPrivate. That's why this check can't really be done in appendInternal.
    //  unless we force some kind of design with state machine switching.
    if (!m_receivedFirstInitializationSegment || m_pendingInitializationSegmentForChangeType) {
        appendError(true);
        return;
    }

    // 3.5.8 Coded Frame Processing
    // http://www.w3.org/TR/media-source/#sourcebuffer-coded-frame-processing

    // When complete coded frames have been parsed by the segment parser loop then the following steps
    // are run:
    // 1. For each coded frame in the media segment run the following steps:
    // 1.1. Loop Top
    do {
        MediaTime presentationTimestamp;
        MediaTime decodeTimestamp;

        // NOTE: this is out-of-order, but we need the timescale from the
        // sample's duration for timestamp generation.
        // 1.2 Let frame duration be a double precision floating point representation of the coded frame's
        // duration in seconds.
        MediaTime frameDuration = sample.duration();

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
            presentationTimestamp = sample.presentationTime();

            // 2. Let decode timestamp be a double precision floating point representation of the coded frame's
            // decode timestamp in seconds.
            decodeTimestamp = sample.decodeTime();
        }

        // 1.3 If mode equals "sequence" and group start timestamp is set, then run the following steps:
        if (m_mode == AppendMode::Sequence && m_groupStartTimestamp.isValid()) {
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
        AtomicString trackID = sample.trackID();
        auto it = m_trackBufferMap.find(trackID);
        if (it == m_trackBufferMap.end()) {
            // The client managed to append a sample with a trackID not present in the initialization
            // segment. This would be a good place to post an message to the developer console.
            didDropSample();
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
        if (trackBuffer.lastDecodeTimestamp.isValid() && (decodeTimestamp < trackBuffer.lastDecodeTimestamp
            || (trackBuffer.greatestDecodeDuration.isValid() && abs(decodeTimestamp - trackBuffer.lastDecodeTimestamp) > (trackBuffer.greatestDecodeDuration * 2)))) {

            // 1.6.1:
            if (m_mode == AppendMode::Segments) {
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

        if (m_mode == AppendMode::Sequence) {
            // Use the generated timestamps instead of the sample's timestamps.
            sample.setTimestamps(presentationTimestamp, decodeTimestamp);
        } else if (trackBuffer.roundedTimestampOffset) {
            // Reflect the timestamp offset into the sample.
            sample.offsetTimestampsBy(trackBuffer.roundedTimestampOffset);
        }

        DEBUG_LOG(LOGIDENTIFIER, sample);

        // 1.7 Let frame end timestamp equal the sum of presentation timestamp and frame duration.
        MediaTime frameEndTimestamp = presentationTimestamp + frameDuration;

        // 1.8 If presentation timestamp is less than appendWindowStart, then set the need random access
        // point flag to true, drop the coded frame, and jump to the top of the loop to start processing
        // the next coded frame.
        // 1.9 If frame end timestamp is greater than appendWindowEnd, then set the need random access
        // point flag to true, drop the coded frame, and jump to the top of the loop to start processing
        // the next coded frame.
        if (presentationTimestamp < m_appendWindowStart || frameEndTimestamp > m_appendWindowEnd) {
            trackBuffer.needRandomAccessFlag = true;
            didDropSample();
            return;
        }


        // 1.10 If the decode timestamp is less than the presentation start time, then run the end of stream
        // algorithm with the error parameter set to "decode", and abort these steps.
        // NOTE: Until <https://www.w3.org/Bugs/Public/show_bug.cgi?id=27487> is resolved, we will only check
        // the presentation timestamp.
        MediaTime presentationStartTime = MediaTime::zeroTime();
        if (presentationTimestamp < presentationStartTime) {
            ERROR_LOG(LOGIDENTIFIER, "failing because presentationTimestamp (", presentationTimestamp, ") < presentationStartTime (", presentationStartTime, ")");
            m_source->streamEndedWithError(MediaSource::EndOfStreamError::Decode);
            return;
        }

        // 1.11 If the need random access point flag on track buffer equals true, then run the following steps:
        if (trackBuffer.needRandomAccessFlag) {
            // 1.11.1 If the coded frame is not a random access point, then drop the coded frame and jump
            // to the top of the loop to start processing the next coded frame.
            if (!sample.isSync()) {
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
                if (trackBuffer.description && trackBuffer.description->isVideo()) {
                    // 1.14.2.1 Let overlapped frame presentation timestamp equal the presentation timestamp
                    // of overlapped frame.
                    MediaTime overlappedFramePresentationTimestamp = overlappedFrame->presentationTime();

                    // 1.14.2.2 Let remove window timestamp equal overlapped frame presentation timestamp
                    // plus 1 microsecond.
                    MediaTime removeWindowTimestamp = overlappedFramePresentationTimestamp + microsecond;

                    // 1.14.2.3 If the presentation timestamp is less than the remove window timestamp,
                    // then remove overlapped frame and any coded frames that depend on it from track buffer.
                    if (presentationTimestamp < removeWindowTimestamp)
                        erasedSamples.addSample(*iter->second);
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

        // There are many files out there where the frame times are not perfectly contiguous and may have small overlaps
        // between the beginning of a frame and the end of the previous one; therefore a tolerance is needed whenever
        // durations are considered.
        // For instance, most WebM files are muxed rounded to the millisecond (the default TimecodeScale of the format)
        // but their durations use a finer timescale (causing a sub-millisecond overlap). More rarely, there are also
        // MP4 files with slightly off tfdt boxes, presenting a similar problem at the beginning of each fragment.
        const MediaTime contiguousFrameTolerance = MediaTime(1, 1000);

        // If highest presentation timestamp for track buffer is set and less than or equal to presentation timestamp
        if (trackBuffer.highestPresentationTimestamp.isValid() && trackBuffer.highestPresentationTimestamp <= presentationTimestamp) {
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
                MediaTime eraseBeginTime = trackBuffer.highestPresentationTimestamp;
                MediaTime eraseEndTime = frameEndTimestamp - contiguousFrameTolerance;

                PresentationOrderSampleMap::iterator_range range;
                if (highestBufferedTime - trackBuffer.highestPresentationTimestamp < trackBuffer.lastFrameDuration)
                    // If the new frame is at the end of the buffered ranges, perform a sequential scan from end (O(1)).
                    range = trackBuffer.samples.presentationOrder().findSamplesBetweenPresentationTimesFromEnd(eraseBeginTime, eraseEndTime);
                else
                    // In any other case, perform a binary search (O(log(n)).
                    range = trackBuffer.samples.presentationOrder().findSamplesBetweenPresentationTimes(eraseBeginTime, eraseEndTime);

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

            // NOTE: in the case of b-frames, the previous step may leave in place samples whose presentation
            // timestamp < presentationTime, but whose decode timestamp >= decodeTime. These will eventually cause
            // a decode error if left in place, so remove these samples as well.
            DecodeOrderSampleMap::KeyType decodeKey(sample.decodeTime(), sample.presentationTime());
            auto samplesWithHigherDecodeTimes = trackBuffer.samples.decodeOrder().findSamplesBetweenDecodeKeys(decodeKey, erasedSamples.decodeOrder().begin()->first);
            if (samplesWithHigherDecodeTimes.first != samplesWithHigherDecodeTimes.second)
                dependentSamples.insert(samplesWithHigherDecodeTimes.first, samplesWithHigherDecodeTimes.second);

            PlatformTimeRanges erasedRanges = removeSamplesFromTrackBuffer(dependentSamples, trackBuffer, this, "sourceBufferPrivateDidReceiveSample");

            // Only force the TrackBuffer to re-enqueue if the removed ranges overlap with enqueued and possibly
            // not yet displayed samples.
            MediaTime currentMediaTime = m_source->currentTime();
            if (trackBuffer.lastEnqueuedPresentationTime.isValid() && currentMediaTime < trackBuffer.lastEnqueuedPresentationTime) {
                PlatformTimeRanges possiblyEnqueuedRanges(currentMediaTime, trackBuffer.lastEnqueuedPresentationTime);
                possiblyEnqueuedRanges.intersectWith(erasedRanges);
                if (possiblyEnqueuedRanges.length())
                    trackBuffer.needsReenqueueing = true;
            }

            erasedRanges.invert();
            trackBuffer.buffered.intersectWith(erasedRanges);
            setBufferedDirty(true);
        }

        // 1.17 If spliced audio frame is set:
        // Add spliced audio frame to the track buffer.
        // If spliced timed text frame is set:
        // Add spliced timed text frame to the track buffer.
        // FIXME: Add support for sample splicing.

        // Otherwise:
        // Add the coded frame with the presentation timestamp, decode timestamp, and frame duration to the track buffer.
        trackBuffer.samples.addSample(sample);

        // Note: The terminology here is confusing: "enqueuing" means providing a frame to the inner media framework.
        // First, frames are inserted in the decode queue; later, at the end of the append all the frames in the decode
        // queue are "enqueued" (sent to the inner media framework) in `provideMediaData()`.
        //
        // In order to check whether a frame should be added to the decode queue we check whether it starts after the
        // lastEnqueuedDecodeKey.
        DecodeOrderSampleMap::KeyType decodeKey(sample.decodeTime(), sample.presentationTime());
        if (trackBuffer.lastEnqueuedDecodeKey.first.isInvalid() || decodeKey > trackBuffer.lastEnqueuedDecodeKey) {
            trackBuffer.decodeQueue.insert(DecodeOrderSampleMap::MapType::value_type(decodeKey, &sample));
        }

        // NOTE: the spec considers "Coded Frame Duration" to be the presentation duration, but this is not necessarily equal
        // to the decoded duration. When comparing deltas between decode timestamps, the decode duration, not the presentation.
        if (trackBuffer.lastDecodeTimestamp.isValid()) {
            MediaTime lastDecodeDuration = decodeTimestamp - trackBuffer.lastDecodeTimestamp;
            if (!trackBuffer.greatestDecodeDuration.isValid() || lastDecodeDuration > trackBuffer.greatestDecodeDuration)
                trackBuffer.greatestDecodeDuration = lastDecodeDuration;
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
        if (nearestToPresentationStartTime.isValid() && (presentationTimestamp - nearestToPresentationStartTime).isBetween(MediaTime::zeroTime(), MediaSource::currentTimeFudgeFactor()))
            presentationTimestamp = nearestToPresentationStartTime;

        auto nearestToPresentationEndTime = trackBuffer.buffered.nearest(presentationEndTime);
        if (nearestToPresentationEndTime.isValid() && (nearestToPresentationEndTime - presentationEndTime).isBetween(MediaTime::zeroTime(), MediaSource::currentTimeFudgeFactor()))
            presentationEndTime = nearestToPresentationEndTime;

        trackBuffer.buffered.add(presentationTimestamp, presentationEndTime);
        m_bufferedSinceLastMonitor += frameDuration.toDouble();
        setBufferedDirty(true);

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

bool SourceBuffer::sourceBufferPrivateHasAudio() const
{
    return hasAudio();
}

bool SourceBuffer::sourceBufferPrivateHasVideo() const
{
    return hasVideo();
}

void SourceBuffer::videoTrackSelectedChanged(VideoTrack& track)
{
    // 2.4.5 Changes to selected/enabled track state
    // If the selected video track changes, then run the following steps:
    // 1. If the SourceBuffer associated with the previously selected video track is not associated with
    // any other enabled tracks, run the following steps:
    if (!track.selected()
        && (!m_videoTracks || !m_videoTracks->isAnyTrackEnabled())
        && (!m_audioTracks || !m_audioTracks->isAnyTrackEnabled())
        && (!m_textTracks || !m_textTracks->isAnyTrackEnabled())) {
        // 1.1 Remove the SourceBuffer from activeSourceBuffers.
        // 1.2 Queue a task to fire a simple event named removesourcebuffer at activeSourceBuffers
        setActive(false);
    } else if (track.selected()) {
        // 2. If the SourceBuffer associated with the newly selected video track is not already in activeSourceBuffers,
        // run the following steps:
        // 2.1 Add the SourceBuffer to activeSourceBuffers.
        // 2.2 Queue a task to fire a simple event named addsourcebuffer at activeSourceBuffers
        setActive(true);
    }

    if (m_videoTracks && m_videoTracks->contains(track))
        m_videoTracks->scheduleChangeEvent();

    if (!isRemoved())
        m_source->mediaElement()->videoTrackSelectedChanged(track);
}

void SourceBuffer::audioTrackEnabledChanged(AudioTrack& track)
{
    // 2.4.5 Changes to selected/enabled track state
    // If an audio track becomes disabled and the SourceBuffer associated with this track is not
    // associated with any other enabled or selected track, then run the following steps:
    if (!track.enabled()
        && (!m_videoTracks || !m_videoTracks->isAnyTrackEnabled())
        && (!m_audioTracks || !m_audioTracks->isAnyTrackEnabled())
        && (!m_textTracks || !m_textTracks->isAnyTrackEnabled())) {
        // 1. Remove the SourceBuffer associated with the audio track from activeSourceBuffers
        // 2. Queue a task to fire a simple event named removesourcebuffer at activeSourceBuffers
        setActive(false);
    } else if (track.enabled()) {
        // If an audio track becomes enabled and the SourceBuffer associated with this track is
        // not already in activeSourceBuffers, then run the following steps:
        // 1. Add the SourceBuffer associated with the audio track to activeSourceBuffers
        // 2. Queue a task to fire a simple event named addsourcebuffer at activeSourceBuffers
        setActive(true);
    }

    if (m_audioTracks && m_audioTracks->contains(track))
        m_audioTracks->scheduleChangeEvent();

    if (!isRemoved())
        m_source->mediaElement()->audioTrackEnabledChanged(track);
}

void SourceBuffer::textTrackModeChanged(TextTrack& track)
{
    // 2.4.5 Changes to selected/enabled track state
    // If a text track mode becomes "disabled" and the SourceBuffer associated with this track is not
    // associated with any other enabled or selected track, then run the following steps:
    if (track.mode() == TextTrack::Mode::Disabled
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

    if (m_textTracks && m_textTracks->contains(track))
        m_textTracks->scheduleChangeEvent();

    if (!isRemoved())
        m_source->mediaElement()->textTrackModeChanged(track);
}

void SourceBuffer::textTrackAddCue(TextTrack& track, TextTrackCue& cue)
{
    if (!isRemoved())
        m_source->mediaElement()->textTrackAddCue(track, cue);
}

void SourceBuffer::textTrackAddCues(TextTrack& track, const TextTrackCueList& cueList)
{
    if (!isRemoved())
        m_source->mediaElement()->textTrackAddCues(track, cueList);
}

void SourceBuffer::textTrackRemoveCue(TextTrack& track, TextTrackCue& cue)
{
    if (!isRemoved())
        m_source->mediaElement()->textTrackRemoveCue(track, cue);
}

void SourceBuffer::textTrackRemoveCues(TextTrack& track, const TextTrackCueList& cueList)
{
    if (!isRemoved())
        m_source->mediaElement()->textTrackRemoveCues(track, cueList);
}

void SourceBuffer::textTrackKindChanged(TextTrack& track)
{
    if (!isRemoved())
        m_source->mediaElement()->textTrackKindChanged(track);
}

void SourceBuffer::sourceBufferPrivateReenqueSamples(const AtomicString& trackID)
{
    if (isRemoved())
        return;

    DEBUG_LOG(LOGIDENTIFIER);
    auto it = m_trackBufferMap.find(trackID);
    if (it == m_trackBufferMap.end())
        return;

    auto& trackBuffer = it->value;
    trackBuffer.needsReenqueueing = true;
    reenqueueMediaForTime(trackBuffer, trackID, m_source->currentTime());
}

void SourceBuffer::sourceBufferPrivateDidBecomeReadyForMoreSamples(const AtomicString& trackID)
{
    if (isRemoved())
        return;

    DEBUG_LOG(LOGIDENTIFIER);
    auto it = m_trackBufferMap.find(trackID);
    if (it == m_trackBufferMap.end())
        return;

    auto& trackBuffer = it->value;
    if (!trackBuffer.needsReenqueueing && !m_source->isSeeking())
        provideMediaData(trackBuffer, trackID);
}

void SourceBuffer::provideMediaData(TrackBuffer& trackBuffer, const AtomicString& trackID)
{
    if (m_source->isSeeking())
        return;

#if !RELEASE_LOG_DISABLED
    unsigned enqueuedSamples = 0;
#endif

    while (!trackBuffer.decodeQueue.empty()) {
        if (!m_private->isReadyForMoreSamples(trackID)) {
            m_private->notifyClientWhenReadyForMoreSamples(trackID);
            break;
        }

        // FIXME(rdar://problem/20635969): Remove this re-entrancy protection when the aforementioned radar is resolved; protecting
        // against re-entrancy introduces a small inefficency when removing appended samples from the decode queue one at a time
        // rather than when all samples have been enqueued.
        auto sample = trackBuffer.decodeQueue.begin()->second;

        // Do not enqueue samples spanning a significant unbuffered gap.
        // NOTE: one second is somewhat arbitrary. MediaSource::monitorSourceBuffers() is run
        // on the playbackTimer, which is effectively every 350ms. Allowing > 350ms gap between
        // enqueued samples allows for situations where we overrun the end of a buffered range
        // but don't notice for 350s of playback time, and the client can enqueue data for the
        // new current time without triggering this early return.
        // FIXME(135867): Make this gap detection logic less arbitrary.
        MediaTime oneSecond(1, 1);
        if (trackBuffer.lastEnqueuedDecodeKey.first.isValid()
            && trackBuffer.lastEnqueuedDecodeDuration.isValid()
            && sample->decodeTime() - trackBuffer.lastEnqueuedDecodeKey.first > oneSecond + trackBuffer.lastEnqueuedDecodeDuration)
            break;

        // Remove the sample from the decode queue now.
        trackBuffer.decodeQueue.erase(trackBuffer.decodeQueue.begin());

        trackBuffer.lastEnqueuedPresentationTime = sample->presentationTime();
        trackBuffer.lastEnqueuedDecodeKey = {sample->decodeTime(), sample->presentationTime()};
        trackBuffer.lastEnqueuedDecodeDuration = sample->duration();
        m_private->enqueueSample(sample.releaseNonNull(), trackID);
#if !RELEASE_LOG_DISABLED
        ++enqueuedSamples;
#endif
    }

#if !RELEASE_LOG_DISABLED
    DEBUG_LOG(LOGIDENTIFIER, "enqueued ", enqueuedSamples, " samples, ", static_cast<size_t>(trackBuffer.decodeQueue.size()), " remaining");
#endif

    trySignalAllSamplesInTrackEnqueued(trackID);
}

void SourceBuffer::trySignalAllSamplesInTrackEnqueued(const AtomicString& trackID)
{
    if (m_source->isEnded() && m_trackBufferMap.get(trackID).decodeQueue.empty()) {
        DEBUG_LOG(LOGIDENTIFIER, "enqueued all samples from track ", trackID);
        m_private->allSamplesInTrackEnqueued(trackID);
    }
}

void SourceBuffer::trySignalAllSamplesEnqueued()
{
    for (const AtomicString& trackID : m_trackBufferMap.keys())
        trySignalAllSamplesInTrackEnqueued(trackID);
}

void SourceBuffer::reenqueueMediaForTime(TrackBuffer& trackBuffer, const AtomicString& trackID, const MediaTime& time)
{
    m_private->flush(trackID);
    trackBuffer.decodeQueue.clear();

    // Find the sample which contains the current presentation time.
    auto currentSamplePTSIterator = trackBuffer.samples.presentationOrder().findSampleContainingPresentationTime(time);

    if (currentSamplePTSIterator == trackBuffer.samples.presentationOrder().end())
        currentSamplePTSIterator = trackBuffer.samples.presentationOrder().findSampleStartingOnOrAfterPresentationTime(time);

    if (currentSamplePTSIterator == trackBuffer.samples.presentationOrder().end()
        || (currentSamplePTSIterator->first - time) > MediaSource::currentTimeFudgeFactor())
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

    if (!trackBuffer.decodeQueue.empty()) {
        auto lastSampleIter = trackBuffer.decodeQueue.rbegin();
        auto lastSampleDecodeKey = lastSampleIter->first;
        auto lastSampleDuration = lastSampleIter->second->duration();
        trackBuffer.lastEnqueuedPresentationTime = lastSampleDecodeKey.second;
        trackBuffer.lastEnqueuedDecodeKey = lastSampleDecodeKey;
        trackBuffer.lastEnqueuedDecodeDuration = lastSampleDuration;
    } else {
        trackBuffer.lastEnqueuedPresentationTime = MediaTime::invalidTime();
        trackBuffer.lastEnqueuedDecodeKey = {MediaTime::invalidTime(), MediaTime::invalidTime()};
        trackBuffer.lastEnqueuedDecodeDuration = MediaTime::invalidTime();
    }

    // Fill the decode queue with the remaining samples.
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
    MonotonicTime now = MonotonicTime::now();
    Seconds interval = now - m_timeOfBufferingMonitor;
    double rateSinceLastMonitor = m_bufferedSinceLastMonitor / interval.seconds();

    m_timeOfBufferingMonitor = now;
    m_bufferedSinceLastMonitor = 0;

    m_averageBufferRate += (interval.seconds() * ExponentialMovingAverageCoefficient) * (rateSinceLastMonitor - m_averageBufferRate);

    DEBUG_LOG(LOGIDENTIFIER, m_averageBufferRate);
}

void SourceBuffer::updateBufferedFromTrackBuffers()
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
        m_buffered->ranges() = PlatformTimeRanges();
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
        if (m_source->isEnded())
            trackRanges.add(trackRanges.maximumBufferedTime(), highestEndTime);

        // 4.3 Let new intersection ranges equal the intersection between the intersection ranges and the track ranges.
        // 4.4 Replace the ranges in intersection ranges with the new intersection ranges.
        intersectionRanges.intersectWith(trackRanges);
    }

    // 5. If intersection ranges does not contain the exact same range information as the current value of this attribute,
    //    then update the current value of this attribute to intersection ranges.
    m_buffered->ranges() = intersectionRanges;
    setBufferedDirty(true);
}

bool SourceBuffer::canPlayThroughRange(PlatformTimeRanges& ranges)
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

    PlatformTimeRanges unbufferedRanges = ranges;
    unbufferedRanges.invert();
    unbufferedRanges.intersectWith(PlatformTimeRanges(currentTime, std::max(currentTime, duration)));
    MediaTime unbufferedTime = unbufferedRanges.totalDuration();
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

void SourceBuffer::reportExtraMemoryAllocated()
{
    size_t extraMemoryCost = this->extraMemoryCost();
    if (extraMemoryCost <= m_reportedExtraMemoryCost)
        return;

    size_t extraMemoryCostDelta = extraMemoryCost - m_reportedExtraMemoryCost;
    m_reportedExtraMemoryCost = extraMemoryCost;

    JSC::JSLockHolder lock(scriptExecutionContext()->vm());
    // FIXME: Adopt reportExtraMemoryVisited, and switch to reportExtraMemoryAllocated.
    // https://bugs.webkit.org/show_bug.cgi?id=142595
    scriptExecutionContext()->vm().heap.deprecatedReportExtraMemory(extraMemoryCostDelta);
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

Vector<String> SourceBuffer::enqueuedSamplesForTrackID(const AtomicString& trackID)
{
    return m_private->enqueuedSamplesForTrackID(trackID);
}

Document& SourceBuffer::document() const
{
    ASSERT(scriptExecutionContext());
    return downcast<Document>(*scriptExecutionContext());
}

ExceptionOr<void> SourceBuffer::setMode(AppendMode newMode)
{
    // 3.1 Attributes - mode
    // http://www.w3.org/TR/media-source/#widl-SourceBuffer-mode

    // On setting, run the following steps:

    // 1. Let new mode equal the new value being assigned to this attribute.
    // 2. If generate timestamps flag equals true and new mode equals "segments", then throw an InvalidAccessError exception and abort these steps.
    if (m_shouldGenerateTimestamps && newMode == AppendMode::Segments)
        return Exception { InvalidAccessError };

    // 3. If this object has been removed from the sourceBuffers attribute of the parent media source, then throw an InvalidStateError exception and abort these steps.
    // 4. If the updating attribute equals true, then throw an InvalidStateError exception and abort these steps.
    if (isRemoved() || m_updating)
        return Exception { InvalidStateError };

    // 5. If the readyState attribute of the parent media source is in the "ended" state then run the following steps:
    if (m_source->isEnded()) {
        // 5.1. Set the readyState attribute of the parent media source to "open"
        // 5.2. Queue a task to fire a simple event named sourceopen at the parent media source.
        m_source->openIfInEndedState();
    }

    // 6. If the append state equals PARSING_MEDIA_SEGMENT, then throw an InvalidStateError and abort these steps.
    if (m_appendState == ParsingMediaSegment)
        return Exception { InvalidStateError };

    // 7. If the new mode equals "sequence", then set the group start timestamp to the group end timestamp.
    if (newMode == AppendMode::Sequence)
        m_groupStartTimestamp = m_groupEndTimestamp;

    // 8. Update the attribute to new mode.
    m_mode = newMode;

    return { };
}

#if !RELEASE_LOG_DISABLED
WTFLogChannel& SourceBuffer::logChannel() const
{
    return LogMediaSource;
}
#endif

} // namespace WebCore

#endif

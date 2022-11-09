/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 * Copyright (C) 2013-2020 Apple Inc. All rights reserved.
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

#include "AudioTrack.h"
#include "AudioTrackList.h"
#include "AudioTrackPrivate.h"
#include "BufferSource.h"
#include "ContentTypeUtilities.h"
#include "Event.h"
#include "EventNames.h"
#include "HTMLMediaElement.h"
#include "InbandTextTrack.h"
#include "InbandTextTrackPrivate.h"
#include "Logging.h"
#include "MediaDescription.h"
#include "MediaSource.h"
#include "SharedBuffer.h"
#include "SourceBufferList.h"
#include "SourceBufferPrivate.h"
#include "TextTrackList.h"
#include "TimeRanges.h"
#include "VideoTrack.h"
#include "VideoTrackList.h"
#include "VideoTrackPrivate.h"
#include "WebCoreOpaqueRoot.h"
#include <JavaScriptCore/JSCInlines.h>
#include <JavaScriptCore/JSLock.h>
#include <JavaScriptCore/VM.h>
#include <limits>
#include <wtf/CheckedArithmetic.h>
#include <wtf/IsoMallocInlines.h>
#include <wtf/StringPrintStream.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SourceBuffer);

static const double ExponentialMovingAverageCoefficient = 0.2;

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
    , m_opaqueRootProvider([this] { return opaqueRoot(); })
    , m_appendBufferTimer(*this, &SourceBuffer::appendBufferTimerFired)
    , m_appendWindowStart(MediaTime::zeroTime())
    , m_appendWindowEnd(MediaTime::positiveInfiniteTime())
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
    m_private->setIsAttached(true);
}

SourceBuffer::~SourceBuffer()
{
    ASSERT(isRemoved());
    ALWAYS_LOG(LOGIDENTIFIER);

    m_private->setIsAttached(false);
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
    return m_private->buffered()->copy();
}

double SourceBuffer::timestampOffset() const
{
    return m_private->timestampOffset().toDouble();
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
        m_private->setGroupStartTimestamp(newTimestampOffset);

    // 7. Update the attribute to the new value.
    m_private->setTimestampOffset(newTimestampOffset);

    m_private->resetTimestampOffsetInTrackBuffers();

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
    m_private->setAppendWindowStart(m_appendWindowStart);

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
    m_private->setAppendWindowEnd(m_appendWindowEnd);

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
    m_private->resetTrackBuffers();

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
    m_private->setAppendWindowStart(m_appendWindowStart);

    // 7. Set appendWindowEnd to positive Infinity.
    m_appendWindowEnd = MediaTime::positiveInfiniteTime();
    m_private->setAppendWindowEnd(m_appendWindowEnd);

    return { };
}

ExceptionOr<void> SourceBuffer::remove(double start, double end)
{
    // Limit timescale to 1/1000 of microsecond so samples won't accidentally overlap with removal range by precision lost (e.g. by 0.000000000000X [sec]).
    static const uint32_t timescale = 1000000000;
    return remove(MediaTime::createWithDouble(start, timescale), MediaTime::createWithDouble(end, timescale));
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

    auto& settings = document().settings();
    if (!contentTypeMeetsContainerAndCodecTypeRequirements(contentType, settings.allowedMediaContainerTypes(), settings.allowedMediaCodecTypes()))
        return Exception { NotSupportedError };

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
    m_private->startChangingType();

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
    m_pendingAppendData = nullptr;
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
    return m_highestPresentationTimestamp;
}

void SourceBuffer::readyStateChanged()
{
    if (!isRemoved())
        m_private->updateBufferedFromTrackBuffers(m_source->isEnded());
}

void SourceBuffer::removedFromMediaSource()
{
    if (isRemoved())
        return;

    abortIfUpdating();

    m_private->clearTrackBuffers();
    m_private->removedFromMediaSource();
    m_private->setIsAttached(false);
    m_source = nullptr;
}

void SourceBuffer::seekToTime(const MediaTime& time)
{
    ALWAYS_LOG(LOGIDENTIFIER, time);
    m_private->seekToTime(time);
}

bool SourceBuffer::virtualHasPendingActivity() const
{
    return m_source;
}

void SourceBuffer::stop()
{
    m_appendBufferTimer.stop();
    m_removeTimer.stop();
}

const char* SourceBuffer::activeDOMObjectName() const
{
    return "SourceBuffer";
}

bool SourceBuffer::isRemoved() const
{
    return !m_source;
}

void SourceBuffer::scheduleEvent(const AtomString& eventName)
{
    queueTaskToDispatchEvent(*this, TaskSource::MediaElement, Event::create(eventName, Event::CanBubble::No, Event::IsCancelable::No));
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

    DEBUG_LOG(LOGIDENTIFIER, "size = ", size, ", buffered = ", m_private->buffered()->ranges());

    // 3. If the readyState attribute of the parent media source is in the "ended" state then run the following steps:
    // 3.1. Set the readyState attribute of the parent media source to "open"
    // 3.2. Queue a task to fire a simple event named sourceopen at the parent media source .
    m_source->openIfInEndedState();

    // 4. Run the coded frame eviction algorithm.
    m_private->evictCodedFrames(size, maximumBufferSize(), m_source->currentTime(), m_source->isEnded());

    // 5. If the buffer full flag equals true, then throw a QuotaExceededError exception and abort these step.
    if (m_private->isBufferFullFor(size, maximumBufferSize())) {
        ERROR_LOG(LOGIDENTIFIER, "buffer full, failing with QuotaExceededError error");
        return Exception { QuotaExceededError };
    }

    // NOTE: Return to 3.2 appendBuffer()
    // 3. Add data to the end of the input buffer.
    ASSERT(!m_pendingAppendData);
    m_pendingAppendData = SharedBuffer::create(data, size);

    // 4. Set the updating attribute to true.
    m_updating = true;

    // 5. Queue a task to fire a simple event named updatestart at this SourceBuffer object.
    scheduleEvent(eventNames().updatestartEvent);

    // 6. Asynchronously run the buffer append algorithm.
    m_appendBufferTimer.startOneShot(0_s);

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

    RefPtr<SharedBuffer> appendData = WTFMove(m_pendingAppendData);
    // 1. Loop Top: If the input buffer is empty, then jump to the need more data step below.
    if (!appendData || !appendData->size()) {
        sourceBufferPrivateAppendComplete(AppendResult::AppendSucceeded);
        return;
    }

    m_private->append(appendData.releaseNonNull());
}

void SourceBuffer::sourceBufferPrivateAppendComplete(AppendResult result)
{
    if (isRemoved())
        return;

    // Section 3.5.5 Buffer Append Algorithm, ctd.
    // https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#sourcebuffer-buffer-append

    // 2. If the input buffer contains bytes that violate the SourceBuffer byte stream format specification,
    // then run the append error algorithm with the decode error parameter set to true and abort this algorithm.
    if (result == AppendResult::ParsingFailed) {
        ERROR_LOG(LOGIDENTIFIER, "ParsingFailed");
        appendError(true);
        return;
    }

    // NOTE: Steps 3 - 6 enforced by sourceBufferPrivateDidReceiveInitializationSegment() and
    // sourceBufferPrivateDidReceiveSample below.

    // 7. Need more data: Return control to the calling algorithm.

    // NOTE: return to Section 3.5.5
    // 2.If the segment parser loop algorithm in the previous step was aborted, then abort this algorithm.
    if (result != AppendResult::AppendSucceeded)
        return;

    // 3. Set the updating attribute to false.
    m_updating = false;

    // 4. Queue a task to fire a simple event named update at this SourceBuffer object.
    scheduleEvent(eventNames().updateEvent);

    // 5. Queue a task to fire a simple event named updateend at this SourceBuffer object.
    scheduleEvent(eventNames().updateendEvent);

    m_source->monitorSourceBuffers();
    m_private->reenqueueMediaIfNeeded(m_source->currentTime());

    DEBUG_LOG(LOGIDENTIFIER, "buffered = ", m_private->buffered()->ranges());
}

void SourceBuffer::sourceBufferPrivateDidReceiveRenderingError(int64_t error)
{
#if RELEASE_LOG_DISABLED
    UNUSED_PARAM(error);
#endif

    ERROR_LOG(LOGIDENTIFIER, error);

    if (!isRemoved())
        m_source->streamEndedWithError(MediaSource::EndOfStreamError::Decode);
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

    m_private->removeCodedFrames(m_pendingRemoveStart, m_pendingRemoveEnd, m_source->currentTime(), m_source->isEnded(), [this, protectedThis = Ref { *this }] {
        // 7. Set the updating attribute to false.
        m_updating = false;
        m_pendingRemoveStart = MediaTime::invalidTime();
        m_pendingRemoveEnd = MediaTime::invalidTime();

        // 8. Queue a task to fire a simple event named update at this SourceBuffer object.
        scheduleEvent(eventNames().updateEvent);

        // 9. Queue a task to fire a simple event named updateend at this SourceBuffer object.
        scheduleEvent(eventNames().updateendEvent);
    });
}

uint64_t SourceBuffer::maximumBufferSize() const
{
    if (isRemoved())
        return 0;

    auto* element = m_source->mediaElement();
    if (!element)
        return 0;

    size_t platformMaximumBufferSize = m_private->platformMaximumBufferSize();
    if (platformMaximumBufferSize)
        return platformMaximumBufferSize;

    return element->maximumSourceBufferSize(*this);
}

VideoTrackList& SourceBuffer::videoTracks()
{
    if (!m_videoTracks) {
        m_videoTracks = VideoTrackList::create(scriptExecutionContext());
        m_videoTracks->setOpaqueRootObserver(m_opaqueRootProvider);
    }
    return *m_videoTracks;
}

AudioTrackList& SourceBuffer::audioTracks()
{
    if (!m_audioTracks) {
        m_audioTracks = AudioTrackList::create(scriptExecutionContext());
        m_audioTracks->setOpaqueRootObserver(m_opaqueRootProvider);
    }
    return *m_audioTracks;
}

TextTrackList& SourceBuffer::textTracks()
{
    if (!m_textTracks) {
        m_textTracks = TextTrackList::create(scriptExecutionContext());
        m_textTracks->setOpaqueRootObserver(m_opaqueRootProvider);
    }
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

void SourceBuffer::sourceBufferPrivateDidReceiveInitializationSegment(InitializationSegment&& segment, CompletionHandler<void(ReceiveResult)>&& completionHandler)
{
    if (isRemoved()) {
        completionHandler(ReceiveResult::BufferRemoved);
        return;
    }

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
        completionHandler(ReceiveResult::AppendError);
        return;
    }

    // 3. If the first initialization segment flag is true, then run the following steps:
    if (m_receivedFirstInitializationSegment) {
        // 3.1. Verify the following properties. If any of the checks fail then run the append error algorithm
        // with the decode error parameter set to true and abort these steps.
        if (!validateInitializationSegment(segment)) {
            appendError(true);
            completionHandler(ReceiveResult::AppendError);
            return;
        }

        Vector<std::pair<AtomString, AtomString>> trackIdPairs;

        // 3.2 Add the appropriate track descriptions from this initialization segment to each of the track buffers.
        ASSERT(segment.audioTracks.size() == audioTracks().length());
        for (auto& audioTrackInfo : segment.audioTracks) {
            if (audioTracks().length() == 1) {
                auto* track = audioTracks().item(0);
                auto oldId = track->id();
                auto newId = audioTrackInfo.track->id();
                track->setPrivate(*audioTrackInfo.track);
                if (newId != oldId)
                    trackIdPairs.append(std::make_pair(oldId, newId));
                break;
            }

            auto audioTrack = audioTracks().getTrackById(audioTrackInfo.track->id());
            ASSERT(audioTrack);
            audioTrack->setPrivate(*audioTrackInfo.track);
        }

        ASSERT(segment.videoTracks.size() == videoTracks().length());
        for (auto& videoTrackInfo : segment.videoTracks) {
            if (videoTracks().length() == 1) {
                auto* track = videoTracks().item(0);
                auto oldId = track->id();
                auto newId = videoTrackInfo.track->id();
                track->setPrivate(*videoTrackInfo.track);
                if (newId != oldId)
                    trackIdPairs.append(std::make_pair(oldId, newId));
                break;
            }

            auto videoTrack = videoTracks().getTrackById(videoTrackInfo.track->id());
            ASSERT(videoTrack);
            videoTrack->setPrivate(*videoTrackInfo.track);
        }

        ASSERT(segment.textTracks.size() == textTracks().length());
        for (auto& textTrackInfo : segment.textTracks) {
            if (textTracks().length() == 1) {
                auto* track = downcast<InbandTextTrack>(textTracks().item(0));
                auto oldId = track->id();
                auto newId = textTrackInfo.track->id();
                track->setPrivate(*textTrackInfo.track);
                if (newId != oldId)
                    trackIdPairs.append(std::make_pair(oldId, newId));
                break;
            }

            auto textTrack = textTracks().getTrackById(textTrackInfo.track->id());
            ASSERT(textTrack);
            downcast<InbandTextTrack>(*textTrack).setPrivate(*textTrackInfo.track);
        }
        
        if (!trackIdPairs.isEmpty())
            m_private->updateTrackIds(WTFMove(trackIdPairs));

        // 3.3 Set the need random access point flag on all track buffers to true.
        m_private->setAllTrackBuffersNeedRandomAccess();
    }

    // 4. Let active track flag equal false.
    bool activeTrackFlag = false;

    // 5. If the first initialization segment flag is false, then run the following steps:
    if (!m_receivedFirstInitializationSegment) {
        // 5.1 If the initialization segment contains tracks with codecs the user agent does not support,
        // then run the append error algorithm with the decode error parameter set to true and abort these steps.
        // NOTE: This check is the responsibility of the SourceBufferPrivate.
        if (auto& allowedMediaAudioCodecIDs = document().settings().allowedMediaAudioCodecIDs()) {
            for (auto& audioTrackInfo : segment.audioTracks) {
                if (audioTrackInfo.description && allowedMediaAudioCodecIDs->contains(FourCC::fromString(audioTrackInfo.description->codec())))
                    continue;
                appendError(true);
                completionHandler(ReceiveResult::AppendError);
                return;
            }
        }

        if (auto& allowedMediaVideoCodecIDs = document().settings().allowedMediaVideoCodecIDs()) {
            for (auto& videoTrackInfo : segment.videoTracks) {
                if (videoTrackInfo.description && allowedMediaVideoCodecIDs->contains(FourCC::fromString(videoTrackInfo.description->codec())))
                    continue;
                appendError(true);
                completionHandler(ReceiveResult::AppendError);
                return;
            }
        }

        // 5.2 For each audio track in the initialization segment, run following steps:
        for (auto& audioTrackInfo : segment.audioTracks) {
            // FIXME: Implement steps 5.2.1-5.2.8.1 as per Editor's Draft 09 January 2015, and reorder this
            // 5.2.1 Let new audio track be a new AudioTrack object.
            // 5.2.2 Generate a unique ID and assign it to the id property on new video track.
            auto newAudioTrack = AudioTrack::create(scriptExecutionContext(), *audioTrackInfo.track);
            newAudioTrack->addClient(*this);
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
            m_source->mediaElement()->addAudioTrack(newAudioTrack.copyRef());

            m_audioCodecs.append(audioTrackInfo.description->codec());

            // 5.2.8 Create a new track buffer to store coded frames for this track.
            m_private->addTrackBuffer(newAudioTrack->id(), WTFMove(audioTrackInfo.description));
        }

        // 5.3 For each video track in the initialization segment, run following steps:
        for (auto& videoTrackInfo : segment.videoTracks) {
            // FIXME: Implement steps 5.3.1-5.3.8.1 as per Editor's Draft 09 January 2015, and reorder this
            // 5.3.1 Let new video track be a new VideoTrack object.
            // 5.3.2 Generate a unique ID and assign it to the id property on new video track.
            auto newVideoTrack = VideoTrack::create(scriptExecutionContext(), *videoTrackInfo.track);
            newVideoTrack->addClient(*this);
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
            m_source->mediaElement()->addVideoTrack(newVideoTrack.copyRef());

            m_videoCodecs.append(videoTrackInfo.description->codec());

            // 5.3.8 Create a new track buffer to store coded frames for this track.
            m_private->addTrackBuffer(newVideoTrack->id(), WTFMove(videoTrackInfo.description));
        }

        // 5.4 For each text track in the initialization segment, run following steps:
        for (auto& textTrackInfo : segment.textTracks) {
            auto& textTrackPrivate = *textTrackInfo.track;

            // FIXME: Implement steps 5.4.1-5.4.8.1 as per Editor's Draft 09 January 2015, and reorder this
            // 5.4.1 Let new text track be a new TextTrack object with its properties populated with the
            // appropriate information from the initialization segment.
            auto newTextTrack = InbandTextTrack::create(document(), textTrackPrivate);
            newTextTrack->addClient(*this);

            // 5.4.2 If the mode property on new text track equals "showing" or "hidden", then set active
            // track flag to true.
            if (textTrackPrivate.mode() != InbandTextTrackPrivate::Mode::Disabled)
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
            m_source->mediaElement()->addTextTrack(newTextTrack.copyRef());

            m_textCodecs.append(textTrackInfo.description->codec());

            // 5.4.7 Create a new track buffer to store coded frames for this track.
            m_private->addTrackBuffer(newTextTrack->id(), WTFMove(textTrackInfo.description));
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
    completionHandler(ReceiveResult::RecieveSucceeded);

    // 6. If the HTMLMediaElement.readyState attribute is HAVE_NOTHING, then run the following steps:
    if (m_private->readyState() == MediaPlayer::ReadyState::HaveNothing) {
        // 6.1 If one or more objects in sourceBuffers have first initialization segment flag set to false, then abort these steps.
        for (auto& sourceBuffer : *m_source->sourceBuffers()) {
            if (!sourceBuffer->m_receivedFirstInitializationSegment)
                return;
        }

        // 6.2 Set the HTMLMediaElement.readyState attribute to HAVE_METADATA.
        // 6.3 Queue a task to fire a simple event named loadedmetadata at the media element.
        m_private->setReadyState(MediaPlayer::ReadyState::HaveMetadata);
    }

    // 7. If the active track flag equals true and the HTMLMediaElement.readyState
    // attribute is greater than HAVE_CURRENT_DATA, then set the HTMLMediaElement.readyState
    // attribute to HAVE_METADATA.
    if (activeTrackFlag && m_private->readyState() > MediaPlayer::ReadyState::HaveCurrentData)
        m_private->setReadyState(MediaPlayer::ReadyState::HaveMetadata);
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

    return true;
}

void SourceBuffer::sourceBufferPrivateAppendError(bool decodeError)
{
    appendError(decodeError);
}

void SourceBuffer::appendError(bool decodeError)
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
    if (decodeError && !isRemoved())
        m_source->streamEndedWithError(MediaSource::EndOfStreamError::Decode);
}

bool SourceBuffer::hasAudio() const
{
    return m_audioTracks && m_audioTracks->length();
}

bool SourceBuffer::hasVideo() const
{
    return m_videoTracks && m_videoTracks->length();
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
}

void SourceBuffer::videoTrackKindChanged(VideoTrack& track)
{
    if (m_videoTracks && m_videoTracks->contains(track))
        m_videoTracks->scheduleChangeEvent();
}

void SourceBuffer::videoTrackLabelChanged(VideoTrack& track)
{
    if (m_videoTracks && m_videoTracks->contains(track))
        m_videoTracks->scheduleChangeEvent();
}

void SourceBuffer::videoTrackLanguageChanged(VideoTrack& track)
{
    if (m_videoTracks && m_videoTracks->contains(track))
        m_videoTracks->scheduleChangeEvent();
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
}

void SourceBuffer::audioTrackKindChanged(AudioTrack& track)
{
    if (m_audioTracks && m_audioTracks->contains(track))
        m_audioTracks->scheduleChangeEvent();
}

void SourceBuffer::audioTrackLabelChanged(AudioTrack& track)
{
    if (m_audioTracks && m_audioTracks->contains(track))
        m_audioTracks->scheduleChangeEvent();
}

void SourceBuffer::audioTrackLanguageChanged(AudioTrack& track)
{
    if (m_audioTracks && m_audioTracks->contains(track))
        m_audioTracks->scheduleChangeEvent();
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
}

void SourceBuffer::textTrackKindChanged(TextTrack& track)
{
    if (m_textTracks && m_textTracks->contains(track))
        m_textTracks->scheduleChangeEvent();
}

void SourceBuffer::textTrackLanguageChanged(TextTrack& track)
{
    if (m_textTracks && m_textTracks->contains(track))
        m_textTracks->scheduleChangeEvent();
}

void SourceBuffer::sourceBufferPrivateDidParseSample(double frameDuration)
{
    m_bufferedSinceLastMonitor += frameDuration;
}

void SourceBuffer::sourceBufferPrivateDurationChanged(const MediaTime& duration)
{
    if (isRemoved())
        return;

    m_source->setDurationInternal(duration);
    if (m_textTracks)
        m_textTracks->setDuration(duration);
}

void SourceBuffer::sourceBufferPrivateHighestPresentationTimestampChanged(const MediaTime& timestamp)
{
    m_highestPresentationTimestamp = timestamp;
}

void SourceBuffer::sourceBufferPrivateDidDropSample()
{
    if (!isRemoved())
        m_source->mediaElement()->incrementDroppedFrameCount();
}

void SourceBuffer::sourceBufferPrivateStreamEndedWithDecodeError()
{
    if (!isRemoved())
        m_source->streamEndedWithError(MediaSource::EndOfStreamError::Decode);
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

void SourceBuffer::sourceBufferPrivateReportExtraMemoryCost(uint64_t extraMemory)
{
    reportExtraMemoryAllocated(extraMemory);
}

void SourceBuffer::reportExtraMemoryAllocated(uint64_t extraMemory)
{
    uint64_t extraMemoryCost = extraMemory;
    if (m_pendingAppendData)
        extraMemoryCost += m_pendingAppendData->size();

    m_extraMemoryCost = extraMemoryCost;

    if (extraMemoryCost <= m_reportedExtraMemoryCost)
        return;

    uint64_t extraMemoryCostDelta = extraMemoryCost - m_reportedExtraMemoryCost;
    m_reportedExtraMemoryCost = extraMemoryCost;

    JSC::JSLockHolder lock(scriptExecutionContext()->vm());
    // FIXME: Adopt reportExtraMemoryVisited, and switch to reportExtraMemoryAllocated.
    // https://bugs.webkit.org/show_bug.cgi?id=142595
    scriptExecutionContext()->vm().heap.deprecatedReportExtraMemory(extraMemoryCostDelta);
}

void SourceBuffer::bufferedSamplesForTrackId(const AtomString& trackID, CompletionHandler<void(Vector<String>&&)>&& completionHandler)
{
    m_private->bufferedSamplesForTrackId(trackID, WTFMove(completionHandler));
}

void SourceBuffer::enqueuedSamplesForTrackID(const AtomString& trackID, CompletionHandler<void(Vector<String>&&)>&& completionHandler)
{
    return m_private->enqueuedSamplesForTrackID(trackID, WTFMove(completionHandler));
}

MediaTime SourceBuffer::minimumUpcomingPresentationTimeForTrackID(const AtomString& trackID)
{
    return m_private->minimumUpcomingPresentationTimeForTrackID(trackID);
}

void SourceBuffer::setMaximumQueueDepthForTrackID(const AtomString& trackID, uint64_t maxQueueDepth)
{
    m_private->setMaximumQueueDepthForTrackID(trackID, maxQueueDepth);
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
        return Exception { TypeError };

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
        m_private->setGroupStartTimestampToEndTimestamp();

    // 8. Update the attribute to new mode.
    m_mode = newMode;

    if (newMode == AppendMode::Segments)
        m_private->setMode(SourceBufferAppendMode::Segments);
    else
        m_private->setMode(SourceBufferAppendMode::Sequence);

    return { };
}

void SourceBuffer::setShouldGenerateTimestamps(bool flag)
{
    m_shouldGenerateTimestamps = flag;
    m_private->setShouldGenerateTimestamps(flag);
}

void SourceBuffer::sourceBufferPrivateBufferedDirtyChanged(bool flag)
{
    m_bufferedDirty = flag;
    if (!isRemoved())
        m_source->sourceBufferDidChangeBufferedDirty(*this, flag);
}

bool SourceBuffer::isBufferedDirty() const
{
    return m_bufferedDirty;
}

void SourceBuffer::setBufferedDirty(bool flag)
{
    m_bufferedDirty = flag;
}

void SourceBuffer::setMediaSourceEnded(bool isEnded)
{
    m_private->setMediaSourceEnded(isEnded);
}

size_t SourceBuffer::memoryCost() const
{
    return sizeof(SourceBuffer) + m_extraMemoryCost;
}

WebCoreOpaqueRoot SourceBuffer::opaqueRoot()
{
    return WebCoreOpaqueRoot { this };
}

#if !RELEASE_LOG_DISABLED
WTFLogChannel& SourceBuffer::logChannel() const
{
    return LogMediaSource;
}
#endif

} // namespace WebCore

#endif

/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "Event.h"
#include "GenericEventQueue.h"
#include "Logging.h"
#include "MediaSource.h"
#include "SourceBufferPrivate.h"
#include "TimeRanges.h"

namespace WebCore {

PassRefPtr<SourceBuffer> SourceBuffer::create(PassOwnPtr<SourceBufferPrivate> sourceBufferPrivate, MediaSource* source)
{
    RefPtr<SourceBuffer> sourceBuffer(adoptRef(new SourceBuffer(sourceBufferPrivate, source)));
    sourceBuffer->suspendIfNeeded();
    return sourceBuffer.release();
}

SourceBuffer::SourceBuffer(PassOwnPtr<SourceBufferPrivate> sourceBufferPrivate, MediaSource* source)
    : ActiveDOMObject(source->scriptExecutionContext())
    , m_private(sourceBufferPrivate)
    , m_source(source)
    , m_asyncEventQueue(*this)
    , m_updating(false)
    , m_timestampOffset(0)
    , m_appendBufferTimer(this, &SourceBuffer::appendBufferTimerFired)
{
    ASSERT(m_private);
    ASSERT(m_source);
}

SourceBuffer::~SourceBuffer()
{
    ASSERT(isRemoved());
}

PassRefPtr<TimeRanges> SourceBuffer::buffered(ExceptionCode& ec) const
{
    // Section 3.1 buffered attribute steps.
    // 1. If this object has been removed from the sourceBuffers attribute of the parent media source then throw an
    //    INVALID_STATE_ERR exception and abort these steps.
    if (isRemoved()) {
        ec = INVALID_STATE_ERR;
        return 0;
    }

    // 2. Return a new static normalized TimeRanges object for the media segments buffered.
    return m_private->buffered();
}

double SourceBuffer::timestampOffset() const
{
    return m_timestampOffset;
}

void SourceBuffer::setTimestampOffset(double offset, ExceptionCode& ec)
{
    // Section 3.1 timestampOffset attribute setter steps.
    // 1. If this object has been removed from the sourceBuffers attribute of the parent media source then throw an
    //    INVALID_STATE_ERR exception and abort these steps.
    if (isRemoved()) {
        ec = INVALID_STATE_ERR;
        return;
    }

    // 3. If the updating attribute equals true, then throw an INVALID_STATE_ERR exception and abort these steps.
    if (m_updating) {
        ec = INVALID_STATE_ERR;
        return;
    }

    // 4. If the readyState attribute of the parent media source is in the "ended" state then run the following steps:
    // 4.1 Set the readyState attribute of the parent media source to "open"
    // 4.2 Queue a task to fire a simple event named sourceopen at the parent media source.
    m_source->openIfInEndedState();

    // 5. If this object is waiting for the end of a media segment to be appended, then throw an INVALID_STATE_ERR
    // and abort these steps.
    if (!m_private->setTimestampOffset(offset)) {
        ec = INVALID_STATE_ERR;
        return;
    }

    // 6. Update the attribute to the new value.
    m_timestampOffset = offset;
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


void SourceBuffer::abortIfUpdating()
{
    // Section 3.2 abort() method step 3 substeps.
    // https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#widl-SourceBuffer-abort-void

    if (!m_updating)
        return;

    // 3.1. Abort the buffer append and stream append loop algorithms if they are running.
    m_appendBufferTimer.stop();
    m_pendingAppendData.clear();

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

    m_private->removedFromMediaSource();
    m_source = 0;
    m_asyncEventQueue.close();
}

bool SourceBuffer::hasPendingActivity() const
{
    return m_source;
}

void SourceBuffer::stop()
{
    m_appendBufferTimer.stop();
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
    // 2. If this object has been removed from the sourceBuffers attribute of the parent media source then throw an INVALID_STATE_ERR exception and abort these steps.
    // 3. If the updating attribute equals true, then throw an INVALID_STATE_ERR exception and abort these steps.
    if (isRemoved() || m_updating) {
        ec = INVALID_STATE_ERR;
        return;
    }

    // 4. If the readyState attribute of the parent media source is in the "ended" state then run the following steps: ...
    m_source->openIfInEndedState();

    // Steps 5-6

    // 7. Add data to the end of the input buffer.
    m_pendingAppendData.append(data, size);

    // 8. Set the updating attribute to true.
    m_updating = true;

    // 9. Queue a task to fire a simple event named updatestart at this SourceBuffer object.
    scheduleEvent(eventNames().updatestartEvent);

    // 10. Asynchronously run the buffer append algorithm.
    m_appendBufferTimer.startOneShot(0);
}

void SourceBuffer::appendBufferTimerFired(Timer<SourceBuffer>*)
{
    ASSERT(m_updating);

    // Section 3.5.4 Buffer Append Algorithm
    // https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#sourcebuffer-buffer-append

    // 1. Run the segment parser loop algorithm.
    // Step 2 doesn't apply since we run Step 1 synchronously here.
    size_t appendSize = m_pendingAppendData.size();
    if (!appendSize) {
        // Resize buffer for 0 byte appends so we always have a valid pointer.
        // We need to convey all appends, even 0 byte ones to |m_private| so
        // that it can clear its end of stream state if necessary.
        m_pendingAppendData.resize(1);
    }
    m_private->append(m_pendingAppendData.data(), appendSize);

    // 3. Set the updating attribute to false.
    m_updating = false;
    m_pendingAppendData.clear();

    // 4. Queue a task to fire a simple event named update at this SourceBuffer object.
    scheduleEvent(eventNames().updateEvent);

    // 5. Queue a task to fire a simple event named updateend at this SourceBuffer object.
    scheduleEvent(eventNames().updateendEvent);
}

} // namespace WebCore

#endif

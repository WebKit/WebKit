/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(MEDIA_STREAM)

#include "RTCDataChannel.h"

#include "Blob.h"
#include "Event.h"
#include "ExceptionCode.h"
#include "MessageEvent.h"
#include "RTCPeerConnectionHandler.h"
#include "ScriptExecutionContext.h"
#include <wtf/ArrayBuffer.h>
#include <wtf/ArrayBufferView.h>

namespace WebCore {

PassRefPtr<RTCDataChannel> RTCDataChannel::create(ScriptExecutionContext* context, RTCPeerConnectionHandler* handler, const String& label, bool reliable, ExceptionCode& ec)
{
    ASSERT(handler);
    RefPtr<RTCDataChannel> dataChannel = create(context, handler, RTCDataChannelDescriptor::create(label, reliable));
    if (!handler->openDataChannel(dataChannel->descriptor())) {
        ec = NOT_SUPPORTED_ERR;
        return 0;
    }
    return dataChannel.release();
}

PassRefPtr<RTCDataChannel> RTCDataChannel::create(ScriptExecutionContext* context, RTCPeerConnectionHandler* handler, PassRefPtr<RTCDataChannelDescriptor> descriptor)
{
    ASSERT(handler);
    ASSERT(descriptor);
    return adoptRef(new RTCDataChannel(context, handler, descriptor));
}

RTCDataChannel::RTCDataChannel(ScriptExecutionContext* context, RTCPeerConnectionHandler* handler, PassRefPtr<RTCDataChannelDescriptor> descriptor)
    : m_scriptExecutionContext(context)
    , m_stopped(false)
    , m_descriptor(descriptor)
    , m_binaryType(BinaryTypeArrayBuffer)
    , m_handler(handler)
    , m_scheduledEventTimer(this, &RTCDataChannel::scheduledEventTimerFired)
{
    m_descriptor->setClient(this);
}

RTCDataChannel::~RTCDataChannel()
{
    m_descriptor->setClient(0);
}

String RTCDataChannel::label() const
{
    return m_descriptor->label();
}

bool RTCDataChannel::reliable() const
{
    return m_descriptor->reliable();
}

String RTCDataChannel::readyState() const
{
    switch (m_descriptor->readyState()) {
    case RTCDataChannelDescriptor::ReadyStateConnecting:
        return ASCIILiteral("connecting");
    case RTCDataChannelDescriptor::ReadyStateOpen:
        return ASCIILiteral("open");
    case RTCDataChannelDescriptor::ReadyStateClosing:
        return ASCIILiteral("closing");
    case RTCDataChannelDescriptor::ReadyStateClosed:
        return ASCIILiteral("closed");
    }

    ASSERT_NOT_REACHED();
    return String();
}

unsigned long RTCDataChannel::bufferedAmount() const
{
    return m_descriptor->bufferedAmount();
}

String RTCDataChannel::binaryType() const
{
    switch (m_binaryType) {
    case BinaryTypeBlob:
        return ASCIILiteral("blob");
    case BinaryTypeArrayBuffer:
        return ASCIILiteral("arraybuffer");
    }
    ASSERT_NOT_REACHED();
    return String();
}

void RTCDataChannel::setBinaryType(const String& binaryType, ExceptionCode& ec)
{
    if (binaryType == "blob")
        ec = NOT_SUPPORTED_ERR;
    else if (binaryType == "arraybuffer")
        m_binaryType = BinaryTypeArrayBuffer;
    else
        ec = TYPE_MISMATCH_ERR;
}

void RTCDataChannel::send(const String& data, ExceptionCode& ec)
{
    if (m_descriptor->readyState() != RTCDataChannelDescriptor::ReadyStateOpen) {
        ec = INVALID_STATE_ERR;
        return;
    }
    if (!m_handler->sendStringData(descriptor(), data)) {
        // FIXME: Decide what the right exception here is.
        ec = SYNTAX_ERR;
    }
}

void RTCDataChannel::send(PassRefPtr<ArrayBuffer> prpData, ExceptionCode& ec)
{
    if (m_descriptor->readyState() != RTCDataChannelDescriptor::ReadyStateOpen) {
        ec = INVALID_STATE_ERR;
        return;
    }

    RefPtr<ArrayBuffer> data = prpData;

    size_t dataLength = data->byteLength();
    if (!dataLength)
        return;

    const char* dataPointer = static_cast<const char*>(data->data());

    if (!m_handler->sendRawData(descriptor(), dataPointer, dataLength)) {
        // FIXME: Decide what the right exception here is.
        ec = SYNTAX_ERR;
    }
}

void RTCDataChannel::send(PassRefPtr<ArrayBufferView> data, ExceptionCode& ec)
{
    RefPtr<ArrayBuffer> arrayBuffer(data->buffer());
    send(arrayBuffer.release(), ec);
}

void RTCDataChannel::send(PassRefPtr<Blob> data, ExceptionCode& ec)
{
    // FIXME: implement
    ec = NOT_SUPPORTED_ERR;
}

void RTCDataChannel::close()
{
    if (m_stopped)
        return;

    m_handler->closeDataChannel(descriptor());
}

void RTCDataChannel::readyStateChanged()
{
    if (m_stopped)
        return;

    switch (m_descriptor->readyState()) {
    case RTCDataChannelDescriptor::ReadyStateOpen:
        scheduleDispatchEvent(Event::create(eventNames().openEvent, false, false));
        break;
    case RTCDataChannelDescriptor::ReadyStateClosed:
        scheduleDispatchEvent(Event::create(eventNames().closeEvent, false, false));
        break;
    default:
        break;
    }
}

void RTCDataChannel::dataArrived(const String& text)
{
    if (m_stopped)
        return;

    scheduleDispatchEvent(MessageEvent::create(text));
}

void RTCDataChannel::dataArrived(const char* data, size_t dataLength)
{
    if (m_stopped)
        return;

    if (m_binaryType == BinaryTypeBlob) {
        // FIXME: Implement.
        return;
    }
    if (m_binaryType == BinaryTypeArrayBuffer) {
        RefPtr<ArrayBuffer> buffer = ArrayBuffer::create(data, dataLength);
        scheduleDispatchEvent(MessageEvent::create(buffer.release()));
        return;
    }
    ASSERT_NOT_REACHED();
}

void RTCDataChannel::error()
{
    if (m_stopped)
        return;

    scheduleDispatchEvent(Event::create(eventNames().errorEvent, false, false));
}

RTCDataChannelDescriptor* RTCDataChannel::descriptor()
{
    return m_descriptor.get();
}

const AtomicString& RTCDataChannel::interfaceName() const
{
    return eventNames().interfaceForRTCDataChannel;
}

ScriptExecutionContext* RTCDataChannel::scriptExecutionContext() const
{
    return m_scriptExecutionContext;
}

void RTCDataChannel::stop()
{
    m_stopped = true;
    m_handler = 0;
    m_descriptor->setClient(0);
    m_scriptExecutionContext = 0;
}

EventTargetData* RTCDataChannel::eventTargetData()
{
    return &m_eventTargetData;
}

EventTargetData* RTCDataChannel::ensureEventTargetData()
{
    return &m_eventTargetData;
}

void RTCDataChannel::scheduleDispatchEvent(PassRefPtr<Event> event)
{
    m_scheduledEvents.append(event);

    if (!m_scheduledEventTimer.isActive())
        m_scheduledEventTimer.startOneShot(0);
}

void RTCDataChannel::scheduledEventTimerFired(Timer<RTCDataChannel>*)
{
    if (m_stopped)
        return;

    Vector<RefPtr<Event> > events;
    events.swap(m_scheduledEvents);

    Vector<RefPtr<Event> >::iterator it = events.begin();
    for (; it != events.end(); ++it)
        dispatchEvent((*it).release());

    events.clear();
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

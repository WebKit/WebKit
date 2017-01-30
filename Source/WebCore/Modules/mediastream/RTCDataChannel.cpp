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
#include "RTCDataChannel.h"

#if ENABLE(WEB_RTC)

#include "Blob.h"
#include "Event.h"
#include "EventNames.h"
#include "ExceptionCode.h"
#include "MessageEvent.h"
#include "RTCDataChannelHandler.h"
#include "ScriptExecutionContext.h"
#include <runtime/ArrayBuffer.h>
#include <runtime/ArrayBufferView.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {

static const AtomicString& blobKeyword()
{
    static NeverDestroyed<AtomicString> blob("blob", AtomicString::ConstructFromLiteral);
    return blob;
}

static const AtomicString& arraybufferKeyword()
{
    static NeverDestroyed<AtomicString> arraybuffer("arraybuffer", AtomicString::ConstructFromLiteral);
    return arraybuffer;
}

Ref<RTCDataChannel> RTCDataChannel::create(ScriptExecutionContext& context, std::unique_ptr<RTCDataChannelHandler>&& handler, String&& label, RTCDataChannelInit&& options)
{
    ASSERT(handler);
    auto channel = adoptRef(*new RTCDataChannel(context, WTFMove(handler), WTFMove(label), WTFMove(options)));
    channel->m_handler->setClient(&channel.get());
    return channel;
}

RTCDataChannel::RTCDataChannel(ScriptExecutionContext& context, std::unique_ptr<RTCDataChannelHandler>&& handler, String&& label, RTCDataChannelInit&& options)
    : m_scriptExecutionContext(&context)
    , m_handler(WTFMove(handler))
    , m_scheduledEventTimer(*this, &RTCDataChannel::scheduledEventTimerFired)
    , m_label(WTFMove(label))
    , m_options(WTFMove(options))
{
}

const AtomicString& RTCDataChannel::readyState() const
{
    static NeverDestroyed<AtomicString> connectingState("connecting", AtomicString::ConstructFromLiteral);
    static NeverDestroyed<AtomicString> openState("open", AtomicString::ConstructFromLiteral);
    static NeverDestroyed<AtomicString> closingState("closing", AtomicString::ConstructFromLiteral);
    static NeverDestroyed<AtomicString> closedState("closed", AtomicString::ConstructFromLiteral);

    switch (m_readyState) {
    case ReadyStateConnecting:
        return connectingState;
    case ReadyStateOpen:
        return openState;
    case ReadyStateClosing:
        return closingState;
    case ReadyStateClosed:
        return closedState;
    }

    ASSERT_NOT_REACHED();
    return emptyAtom;
}

size_t RTCDataChannel::bufferedAmount() const
{
    return m_handler->bufferedAmount();
}

const AtomicString& RTCDataChannel::binaryType() const
{
    switch (m_binaryType) {
    case BinaryType::Blob:
        return blobKeyword();
    case BinaryType::ArrayBuffer:
        return arraybufferKeyword();
    }

    ASSERT_NOT_REACHED();
    return emptyAtom;
}

ExceptionOr<void> RTCDataChannel::setBinaryType(const AtomicString& binaryType)
{
    if (binaryType == blobKeyword())
        return Exception { NOT_SUPPORTED_ERR };
    if (binaryType == arraybufferKeyword()) {
        m_binaryType = BinaryType::ArrayBuffer;
        return { };
    }
    return Exception { TYPE_MISMATCH_ERR };
}

ExceptionOr<void> RTCDataChannel::send(const String& data)
{
    if (m_readyState != ReadyStateOpen)
        return Exception { INVALID_STATE_ERR };

    if (!m_handler->sendStringData(data)) {
        // FIXME: Decide what the right exception here is.
        return Exception { SYNTAX_ERR };
    }

    return { };
}

ExceptionOr<void> RTCDataChannel::send(ArrayBuffer& data)
{
    if (m_readyState != ReadyStateOpen)
        return Exception { INVALID_STATE_ERR };

    size_t dataLength = data.byteLength();
    if (!dataLength)
        return { };

    const char* dataPointer = static_cast<const char*>(data.data());

    if (!m_handler->sendRawData(dataPointer, dataLength)) {
        // FIXME: Decide what the right exception here is.
        return Exception { SYNTAX_ERR };
    }

    return { };
}

ExceptionOr<void> RTCDataChannel::send(ArrayBufferView& data)
{
    return send(*data.unsharedBuffer());
}

ExceptionOr<void> RTCDataChannel::send(Blob&)
{
    // FIXME: Implement.
    return Exception { NOT_SUPPORTED_ERR };
}

void RTCDataChannel::close()
{
    if (m_stopped)
        return;

    m_handler->close();
}

void RTCDataChannel::didChangeReadyState(ReadyState newState)
{
    if (m_stopped || m_readyState == ReadyStateClosed || m_readyState == newState)
        return;

    m_readyState = newState;

    switch (m_readyState) {
    case ReadyStateOpen:
        scheduleDispatchEvent(Event::create(eventNames().openEvent, false, false));
        break;
    case ReadyStateClosed:
        scheduleDispatchEvent(Event::create(eventNames().closeEvent, false, false));
        break;
    default:
        break;
    }
}

void RTCDataChannel::didReceiveStringData(const String& text)
{
    if (m_stopped)
        return;

    scheduleDispatchEvent(MessageEvent::create(text));
}

void RTCDataChannel::didReceiveRawData(const char* data, size_t dataLength)
{
    if (m_stopped)
        return;

    if (m_binaryType == BinaryType::Blob) {
        // FIXME: Implement.
        return;
    }

    if (m_binaryType == BinaryType::ArrayBuffer) {
        scheduleDispatchEvent(MessageEvent::create(ArrayBuffer::create(data, dataLength)));
        return;
    }
    ASSERT_NOT_REACHED();
}

void RTCDataChannel::didDetectError()
{
    if (m_stopped)
        return;

    scheduleDispatchEvent(Event::create(eventNames().errorEvent, false, false));
}

void RTCDataChannel::bufferedAmountIsDecreasing()
{
    if (m_stopped)
        return;

    if (bufferedAmount() <= m_bufferedAmountLowThreshold)
        scheduleDispatchEvent(Event::create(eventNames().bufferedAmountLowThresholdEvent, false, false));
}

void RTCDataChannel::stop()
{
    m_stopped = true;
    m_readyState = ReadyStateClosed;
    m_handler->setClient(nullptr);
    m_scriptExecutionContext = nullptr;
}

void RTCDataChannel::scheduleDispatchEvent(Ref<Event>&& event)
{
    m_scheduledEvents.append(WTFMove(event));

    if (!m_scheduledEventTimer.isActive())
        m_scheduledEventTimer.startOneShot(0);
}

void RTCDataChannel::scheduledEventTimerFired()
{
    if (m_stopped)
        return;

    Vector<Ref<Event>> events;
    events.swap(m_scheduledEvents);

    for (auto& event : events)
        dispatchEvent(event);
}

} // namespace WebCore

#endif // ENABLE(WEB_RTC)

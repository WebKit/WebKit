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
#include "Dictionary.h"
#include "Event.h"
#include "ExceptionCode.h"
#include "MessageEvent.h"
#include "RTCDataChannelHandler.h"
#include "RTCPeerConnectionHandler.h"
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

PassRefPtr<RTCDataChannel> RTCDataChannel::create(ScriptExecutionContext* context, RTCPeerConnectionHandler* peerConnectionHandler, const String& label, const Dictionary& options, ExceptionCode& ec)
{
    RTCDataChannelInit initData;
    String maxRetransmitsStr;
    String maxRetransmitTimeStr;
    options.get("ordered", initData.ordered);
    options.get("negotiated", initData.negotiated);
    options.get("id", initData.id);
    options.get("maxRetransmits", maxRetransmitsStr);
    options.get("maxRetransmitTime", maxRetransmitTimeStr);
    options.get("protocol", initData.protocol);

    bool maxRetransmitsConversion;
    bool maxRetransmitTimeConversion;
    initData.maxRetransmits = maxRetransmitsStr.toUIntStrict(&maxRetransmitsConversion);
    initData.maxRetransmitTime = maxRetransmitTimeStr.toUIntStrict(&maxRetransmitTimeConversion);
    if (maxRetransmitsConversion && maxRetransmitTimeConversion) {
        ec = SYNTAX_ERR;
        return nullptr;
    }

    std::unique_ptr<RTCDataChannelHandler> handler = peerConnectionHandler->createDataChannel(label, initData);
    if (!handler) {
        ec = NOT_SUPPORTED_ERR;
        return nullptr;
    }
    return adoptRef(new RTCDataChannel(context, std::move(handler)));
}

PassRefPtr<RTCDataChannel> RTCDataChannel::create(ScriptExecutionContext* context, std::unique_ptr<RTCDataChannelHandler> handler)
{
    ASSERT(handler);
    return adoptRef(new RTCDataChannel(context, std::move(handler)));
}

RTCDataChannel::RTCDataChannel(ScriptExecutionContext* context, std::unique_ptr<RTCDataChannelHandler> handler)
    : m_scriptExecutionContext(context)
    , m_handler(std::move(handler))
    , m_stopped(false)
    , m_readyState(ReadyStateConnecting)
    , m_binaryType(BinaryTypeArrayBuffer)
    , m_scheduledEventTimer(this, &RTCDataChannel::scheduledEventTimerFired)
{
    m_handler->setClient(this);
}

RTCDataChannel::~RTCDataChannel()
{
}

String RTCDataChannel::label() const
{
    return m_handler->label();
}

bool RTCDataChannel::ordered() const
{
    return m_handler->ordered();
}

unsigned short RTCDataChannel::maxRetransmitTime() const
{
    return m_handler->maxRetransmitTime();
}

unsigned short RTCDataChannel::maxRetransmits() const
{
return m_handler->maxRetransmits();
}

String RTCDataChannel::protocol() const
{
    return m_handler->protocol();
}

bool RTCDataChannel::negotiated() const
{
    return m_handler->negotiated();
}

unsigned short RTCDataChannel::id() const
{
    return m_handler->id();
}

AtomicString RTCDataChannel::readyState() const
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

unsigned long RTCDataChannel::bufferedAmount() const
{
    return m_handler->bufferedAmount();
}

AtomicString RTCDataChannel::binaryType() const
{
    switch (m_binaryType) {
    case BinaryTypeBlob:
        return blobKeyword();
    case BinaryTypeArrayBuffer:
        return arraybufferKeyword();
    }

    ASSERT_NOT_REACHED();
    return emptyAtom;
}

void RTCDataChannel::setBinaryType(const AtomicString& binaryType, ExceptionCode& ec)
{
    if (binaryType == blobKeyword())
        ec = NOT_SUPPORTED_ERR;
    else if (binaryType == arraybufferKeyword())
        m_binaryType = BinaryTypeArrayBuffer;
    else
        ec = TYPE_MISMATCH_ERR;
}

void RTCDataChannel::send(const String& data, ExceptionCode& ec)
{
    if (m_readyState != ReadyStateOpen) {
        ec = INVALID_STATE_ERR;
        return;
    }

    if (!m_handler->sendStringData(data)) {
        // FIXME: Decide what the right exception here is.
        ec = SYNTAX_ERR;
    }
}

void RTCDataChannel::send(PassRefPtr<ArrayBuffer> prpData, ExceptionCode& ec)
{
    if (m_readyState != ReadyStateOpen) {
        ec = INVALID_STATE_ERR;
        return;
    }

    RefPtr<ArrayBuffer> data = prpData;

    size_t dataLength = data->byteLength();
    if (!dataLength)
        return;

    const char* dataPointer = static_cast<const char*>(data->data());

    if (!m_handler->sendRawData(dataPointer, dataLength)) {
        // FIXME: Decide what the right exception here is.
        ec = SYNTAX_ERR;
    }
}

void RTCDataChannel::send(PassRefPtr<ArrayBufferView> data, ExceptionCode& ec)
{
    RefPtr<ArrayBuffer> arrayBuffer(data->buffer());
    send(arrayBuffer.release(), ec);
}

void RTCDataChannel::send(PassRefPtr<Blob>, ExceptionCode& ec)
{
    // FIXME: implement
    ec = NOT_SUPPORTED_ERR;
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

void RTCDataChannel::didDetectError()
{
    if (m_stopped)
        return;

    scheduleDispatchEvent(Event::create(eventNames().errorEvent, false, false));
}

void RTCDataChannel::stop()
{
    m_stopped = true;
    m_readyState = ReadyStateClosed;
    m_handler->setClient(0);
    m_scriptExecutionContext = 0;
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

    Vector<RefPtr<Event>> events;
    events.swap(m_scheduledEvents);

    Vector<RefPtr<Event>>::iterator it = events.begin();
    for (; it != events.end(); ++it)
        dispatchEvent((*it).release());

    events.clear();
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

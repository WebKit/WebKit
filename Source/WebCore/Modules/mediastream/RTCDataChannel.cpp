/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "EventNames.h"
#include "ExceptionCode.h"
#include "Logging.h"
#include "MessageEvent.h"
#include "RTCDataChannelRemoteHandler.h"
#include "ScriptExecutionContext.h"
#include "SharedBuffer.h"
#include <JavaScriptCore/ArrayBufferView.h>
#include <wtf/IsoMallocInlines.h>
#include <wtf/Lock.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(RTCDataChannel);

static const AtomString& blobKeyword()
{
    static MainThreadNeverDestroyed<const AtomString> blob("blob", AtomString::ConstructFromLiteral);
    return blob;
}

static const AtomString& arraybufferKeyword()
{
    static MainThreadNeverDestroyed<const AtomString> arraybuffer("arraybuffer", AtomString::ConstructFromLiteral);
    return arraybuffer;
}

Ref<RTCDataChannel> RTCDataChannel::create(ScriptExecutionContext& context, std::unique_ptr<RTCDataChannelHandler>&& handler, String&& label, RTCDataChannelInit&& options)
{
    ASSERT(handler);
    auto channel = adoptRef(*new RTCDataChannel(context, WTFMove(handler), WTFMove(label), WTFMove(options)));
    channel->suspendIfNeeded();
    queueTaskKeepingObjectAlive(channel.get(), TaskSource::Networking, [channel = channel.ptr()] {
        if (!channel->m_isDetachable)
            return;
        channel->m_isDetachable = false;
        if (!channel->m_handler)
            return;
        if (auto* context = channel->scriptExecutionContext())
            channel->m_handler->setClient(*channel, context->contextIdentifier());
    });
    return channel;
}

NetworkSendQueue RTCDataChannel::createMessageQueue(ScriptExecutionContext& context, RTCDataChannel& channel)
{
    return { context, [&channel](auto& utf8) {
        if (!channel.m_handler->sendStringData(utf8))
            channel.scriptExecutionContext()->addConsoleMessage(MessageSource::JS, MessageLevel::Error, "Error sending string through RTCDataChannel."_s);
    }, [&channel](auto& span) {
        if (!channel.m_handler->sendRawData(span.data(), span.size()))
            channel.scriptExecutionContext()->addConsoleMessage(MessageSource::JS, MessageLevel::Error, "Error sending binary data through RTCDataChannel."_s);
    }, [&channel](ExceptionCode errorCode) {
        if (auto* context = channel.scriptExecutionContext()) {
            auto code = static_cast<int>(errorCode);
            context->addConsoleMessage(MessageSource::JS, MessageLevel::Error, makeString("Error ", code, " in retrieving a blob data to be sent through RTCDataChannel."));
        }
        return NetworkSendQueue::Continue::Yes;
    } };
}

RTCDataChannel::RTCDataChannel(ScriptExecutionContext& context, std::unique_ptr<RTCDataChannelHandler>&& handler, String&& label, RTCDataChannelInit&& options)
    : ActiveDOMObject(&context)
    , m_handler(WTFMove(handler))
    , m_identifier(RTCDataChannelIdentifier { Process::identifier(), ObjectIdentifier<RTCDataChannelLocalIdentifierType>::generateThreadSafe() })
    , m_contextIdentifier(context.isDocument() ? ScriptExecutionContextIdentifier { } : context.contextIdentifier())
    , m_label(WTFMove(label))
    , m_options(WTFMove(options))
    , m_messageQueue(createMessageQueue(context, *this))
{
}

const AtomString& RTCDataChannel::binaryType() const
{
    switch (m_binaryType) {
    case BinaryType::Blob:
        return blobKeyword();
    case BinaryType::ArrayBuffer:
        return arraybufferKeyword();
    }

    ASSERT_NOT_REACHED();
    return emptyAtom();
}

ExceptionOr<void> RTCDataChannel::setBinaryType(const AtomString& binaryType)
{
    if (binaryType == blobKeyword()) {
        m_binaryType = BinaryType::Blob;
        return { };
    }
    if (binaryType == arraybufferKeyword()) {
        m_binaryType = BinaryType::ArrayBuffer;
        return { };
    }
    return Exception { TypeMismatchError };
}

ExceptionOr<void> RTCDataChannel::send(const String& data)
{
    if (m_readyState != RTCDataChannelState::Open)
        return Exception { InvalidStateError };

    // FIXME: We might want to use strict conversion like WebSocket.
    auto utf8 = data.utf8();
    m_bufferedAmount += utf8.length();
    m_messageQueue.enqueue(WTFMove(utf8));
    return { };
}

ExceptionOr<void> RTCDataChannel::send(ArrayBuffer& data)
{
    if (m_readyState != RTCDataChannelState::Open)
        return Exception { InvalidStateError };

    m_bufferedAmount += data.byteLength();
    m_messageQueue.enqueue(data, 0, data.byteLength());
    return { };
}

ExceptionOr<void> RTCDataChannel::send(ArrayBufferView& data)
{
    if (m_readyState != RTCDataChannelState::Open)
        return Exception { InvalidStateError };

    m_bufferedAmount += data.byteLength();
    m_messageQueue.enqueue(*data.unsharedBuffer(), data.byteOffset(), data.byteLength());
    return { };
}

ExceptionOr<void> RTCDataChannel::send(Blob& blob)
{
    if (m_readyState != RTCDataChannelState::Open)
        return Exception { InvalidStateError };

    m_bufferedAmount += blob.size();
    m_messageQueue.enqueue(blob);
    return { };
}

void RTCDataChannel::close()
{
    if (m_stopped)
        return;

    m_stopped = true;
    m_readyState = RTCDataChannelState::Closed;

    m_messageQueue.clear();

    if (m_handler)
        m_handler->close();
    m_handler = nullptr;
}

bool RTCDataChannel::virtualHasPendingActivity() const
{
    return !m_stopped;
}

void RTCDataChannel::didChangeReadyState(RTCDataChannelState newState)
{
    if (m_stopped || m_readyState == RTCDataChannelState::Closed || m_readyState == newState)
        return;

    m_readyState = newState;

    switch (m_readyState) {
    case RTCDataChannelState::Open:
        scheduleDispatchEvent(Event::create(eventNames().openEvent, Event::CanBubble::No, Event::IsCancelable::No));
        break;
    case RTCDataChannelState::Closed:
        scheduleDispatchEvent(Event::create(eventNames().closeEvent, Event::CanBubble::No, Event::IsCancelable::No));
        break;
    default:
        break;
    }
}

void RTCDataChannel::didReceiveStringData(const String& text)
{
    scheduleDispatchEvent(MessageEvent::create(text));
}

void RTCDataChannel::didReceiveRawData(const uint8_t* data, size_t dataLength)
{
    switch (m_binaryType) {
    case BinaryType::Blob:
        scheduleDispatchEvent(MessageEvent::create(Blob::create(scriptExecutionContext(), Vector { data, dataLength }, emptyString()), { }));
        return;
    case BinaryType::ArrayBuffer:
        scheduleDispatchEvent(MessageEvent::create(ArrayBuffer::create(data, dataLength)));
        return;
    }
    ASSERT_NOT_REACHED();
}

void RTCDataChannel::didDetectError()
{
    scheduleDispatchEvent(Event::create(eventNames().errorEvent, Event::CanBubble::No, Event::IsCancelable::No));
}

void RTCDataChannel::bufferedAmountIsDecreasing(size_t amount)
{
    auto previousBufferedAmount = m_bufferedAmount;
    m_bufferedAmount -= amount;
    if (previousBufferedAmount > m_bufferedAmountLowThreshold && m_bufferedAmount <= m_bufferedAmountLowThreshold)
        scheduleDispatchEvent(Event::create(eventNames().bufferedamountlowEvent, Event::CanBubble::No, Event::IsCancelable::No));
}

void RTCDataChannel::stop()
{
    removeFromDataChannelLocalMapIfNeeded();

    close();
}

void RTCDataChannel::scheduleDispatchEvent(Ref<Event>&& event)
{
    if (m_stopped)
        return;

    // https://w3c.github.io/webrtc-pc/#operation
    queueTaskToDispatchEvent(*this, TaskSource::Networking, WTFMove(event));
}

static Lock s_rtcDataChannelLocalMapLock;
static HashMap<RTCDataChannelLocalIdentifier, std::unique_ptr<RTCDataChannelHandler>>& rtcDataChannelLocalMap() WTF_REQUIRES_LOCK(s_rtcDataChannelLocalMapLock)
{
    ASSERT(s_rtcDataChannelLocalMapLock.isHeld());
    static LazyNeverDestroyed<HashMap<RTCDataChannelLocalIdentifier, std::unique_ptr<RTCDataChannelHandler>>> map;
    static std::once_flag onceKey;
    std::call_once(onceKey, [&] {
        map.construct();
    });
    return map;
}

bool RTCDataChannel::canDetach() const
{
    return m_isDetachable && !m_isDetached && !m_bufferedAmount && m_readyState != RTCDataChannelState::Closed;
}

std::unique_ptr<DetachedRTCDataChannel> RTCDataChannel::detach()
{
    ASSERT(canDetach());

    auto state = m_readyState;

    m_isDetachable = false;
    m_isDetached = true;
    m_readyState = RTCDataChannelState::Closed;

    Locker locker { s_rtcDataChannelLocalMapLock };
    rtcDataChannelLocalMap().add(identifier().channelIdentifier, WTFMove(m_handler));

    return makeUnique<DetachedRTCDataChannel>(identifier(), label().isolatedCopy(), options(), state);
}

void RTCDataChannel::removeFromDataChannelLocalMapIfNeeded()
{
    if (!m_isDetached)
        return;

    Locker locker { s_rtcDataChannelLocalMapLock };
    rtcDataChannelLocalMap().remove(identifier().channelIdentifier);
}

std::unique_ptr<RTCDataChannelHandler> RTCDataChannel::handlerFromIdentifier(RTCDataChannelLocalIdentifier channelIdentifier)
{
    Locker locker { s_rtcDataChannelLocalMapLock };
    return rtcDataChannelLocalMap().take(channelIdentifier);
}

static Ref<RTCDataChannel> createClosedChannel(ScriptExecutionContext& context, String&& label, RTCDataChannelInit&& options)
{
    auto channel = RTCDataChannel::create(context, nullptr, WTFMove(label), WTFMove(options));
    channel->close();
    return channel;
}

Ref<RTCDataChannel> RTCDataChannel::create(ScriptExecutionContext& context, RTCDataChannelIdentifier identifier, String&& label, RTCDataChannelInit&& options, RTCDataChannelState state)
{
    RTCDataChannelRemoteHandler* remoteHandlerPtr = nullptr;
    std::unique_ptr<RTCDataChannelHandler> handler;
    if (identifier.processIdentifier == Process::identifier())
        handler = RTCDataChannel::handlerFromIdentifier(identifier.channelIdentifier);
    else {
        auto remoteHandler = RTCDataChannelRemoteHandler::create(identifier, context.createRTCDataChannelRemoteHandlerConnection());
        remoteHandlerPtr = remoteHandler.get();
        handler = WTFMove(remoteHandler);
    }

    if (!handler)
        return createClosedChannel(context, WTFMove(label), WTFMove(options));

    auto channel = RTCDataChannel::create(context, WTFMove(handler), WTFMove(label), WTFMove(options));
    channel->m_readyState = state;

    if (remoteHandlerPtr)
        remoteHandlerPtr->setLocalIdentifier(channel->identifier());

    return channel;
}

} // namespace WebCore

#endif // ENABLE(WEB_RTC)

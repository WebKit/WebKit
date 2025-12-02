/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2017-2024 Apple Inc. All rights reserved.
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
#include "ContextDestructionObserverInlines.h"
#include "EventNames.h"
#include "ExceptionCode.h"
#include "ExceptionOr.h"
#include "Logging.h"
#include "MessageEvent.h"
#include "RTCDataChannelRemoteHandler.h"
#include "RTCDataChannelRemoteHandlerConnection.h"
#include "RTCErrorEvent.h"
#include "ScriptExecutionContext.h"
#include "ScriptWrappableInlines.h"
#include "SharedBuffer.h"
#include <JavaScriptCore/ArrayBufferView.h>
#include <JavaScriptCore/ConsoleTypes.h>
#include <wtf/Lock.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/MakeString.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(DetachedRTCDataChannel);
WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(RTCDataChannel);

Ref<RTCDataChannel> RTCDataChannel::create(ScriptExecutionContext& context, std::unique_ptr<RTCDataChannelHandler>&& handler, String&& label, RTCDataChannelInit&& options, RTCDataChannelState state)
{
    ASSERT(handler);
    Ref channel = adoptRef(*new RTCDataChannel(context, WTFMove(handler), WTFMove(label), WTFMove(options), state));
    channel->suspendIfNeeded();
    queueTaskKeepingObjectAlive(channel.get(), TaskSource::Networking, [](auto& channel) {
        if (!channel.m_isDetachable)
            return;
        channel.m_isDetachable = false;
        if (!channel.m_handler)
            return;
        if (RefPtr context = channel.scriptExecutionContext())
            channel.m_handler->setClient(channel, context->identifier());
    });
    return channel;
}

Ref<NetworkSendQueue> RTCDataChannel::createMessageQueue(ScriptExecutionContext& context, RTCDataChannel& channel)
{
    return NetworkSendQueue::create(context, [&channel](auto& utf8) {
        if (!channel.m_handler->sendStringData(utf8))
            channel.protectedScriptExecutionContext()->addConsoleMessage(MessageSource::JS, MessageLevel::Error, "Error sending string through RTCDataChannel."_s);
    }, [&channel](auto span) {
        if (!channel.m_handler->sendRawData(span))
            channel.protectedScriptExecutionContext()->addConsoleMessage(MessageSource::JS, MessageLevel::Error, "Error sending binary data through RTCDataChannel."_s);
    }, [&channel](ExceptionCode errorCode) {
        if (RefPtr context = channel.scriptExecutionContext()) {
            auto code = static_cast<int>(errorCode);
            context->addConsoleMessage(MessageSource::JS, MessageLevel::Error, makeString("Error "_s, code, " in retrieving a blob data to be sent through RTCDataChannel."_s));
        }
        return NetworkSendQueue::Continue::Yes;
    });
}

RTCDataChannel::RTCDataChannel(ScriptExecutionContext& context, std::unique_ptr<RTCDataChannelHandler>&& handler, String&& label, RTCDataChannelInit&& options, RTCDataChannelState readyState)
    : ActiveDOMObject(&context)
    , m_handler(WTFMove(handler))
    , m_identifier(RTCDataChannelIdentifier::generate())
    , m_contextIdentifier(context.isDocument() ? std::nullopt : std::optional { context.identifier() })
    , m_readyState(readyState)
    , m_label(WTFMove(label))
    , m_options(WTFMove(options))
    , m_messageQueue(createMessageQueue(context, *this))
{
}

RTCDataChannel::~RTCDataChannel() = default;

std::optional<unsigned short> RTCDataChannel::id() const
{
    if (!m_options.id && m_handler)
        const_cast<RTCDataChannel*>(this)->m_options.id = m_handler->id();

    return m_options.id;
}

void RTCDataChannel::setBinaryType(BinaryType binaryType)
{
    m_binaryType = binaryType;
}

ExceptionOr<void> RTCDataChannel::send(const String& data)
{
    if (m_readyState != RTCDataChannelState::Open)
        return Exception { ExceptionCode::InvalidStateError };

    // FIXME: We might want to use strict conversion like WebSocket.
    auto utf8 = data.utf8();
    m_bufferedAmount += utf8.length();
    m_messageQueue->enqueue(WTFMove(utf8));
    return { };
}

ExceptionOr<void> RTCDataChannel::send(ArrayBuffer& data)
{
    if (m_readyState != RTCDataChannelState::Open)
        return Exception { ExceptionCode::InvalidStateError };

    m_bufferedAmount += data.byteLength();
    m_messageQueue->enqueue(data, 0, data.byteLength());
    return { };
}

ExceptionOr<void> RTCDataChannel::send(ArrayBufferView& data)
{
    if (m_readyState != RTCDataChannelState::Open)
        return Exception { ExceptionCode::InvalidStateError };

    m_bufferedAmount += data.byteLength();
    m_messageQueue->enqueue(*data.unsharedBuffer(), data.byteOffset(), data.byteLength());
    return { };
}

ExceptionOr<void> RTCDataChannel::send(Blob& blob)
{
    if (m_readyState != RTCDataChannelState::Open)
        return Exception { ExceptionCode::InvalidStateError };

    m_bufferedAmount += blob.size();
    m_messageQueue->enqueue(blob);
    return { };
}

void RTCDataChannel::close()
{
    if (m_stopped)
        return;

    if (m_readyState == RTCDataChannelState::Closing || m_readyState == RTCDataChannelState::Closed)
        return;

    m_readyState = RTCDataChannelState::Closing;

    m_messageQueue->clear();

    if (m_handler)
        m_handler->close();
}

bool RTCDataChannel::virtualHasPendingActivity() const
{
    return !m_stopped;
}

void RTCDataChannel::didChangeReadyState(RTCDataChannelState newState)
{
    queueTaskKeepingObjectAlive(*this, TaskSource::Networking, [newState](auto& channel) {
        if (channel.m_stopped || channel.m_readyState == RTCDataChannelState::Closed || channel.m_readyState == newState)
            return;

        if (channel.m_readyState == RTCDataChannelState::Closing && (newState == RTCDataChannelState::Open || newState == RTCDataChannelState::Connecting))
            return;

        if (channel.m_readyState == RTCDataChannelState::Open && newState == RTCDataChannelState::Connecting)
            return;

        channel.m_readyState = newState;

        switch (channel.m_readyState) {
        case RTCDataChannelState::Connecting:
            ASSERT_NOT_REACHED();
            break;
        case RTCDataChannelState::Open:
            channel.dispatchEvent(Event::create(eventNames().openEvent, Event::CanBubble::No, Event::IsCancelable::No));
            break;
        case RTCDataChannelState::Closing:
            channel.dispatchEvent(Event::create(eventNames().closingEvent, Event::CanBubble::No, Event::IsCancelable::No));
            break;
        case RTCDataChannelState::Closed:
            channel.dispatchEvent(Event::create(eventNames().closeEvent, Event::CanBubble::No, Event::IsCancelable::No));
            channel.m_stopped = true;
            break;
        }
    });
}

void RTCDataChannel::didReceiveStringData(const String& text)
{
    scheduleDispatchEvent(MessageEvent::create(text));
}

void RTCDataChannel::didReceiveRawData(std::span<const uint8_t> data)
{
    switch (m_binaryType) {
    case BinaryType::Blob:
        scheduleDispatchEvent(MessageEvent::create(Blob::create(protectedScriptExecutionContext().get(), Vector(data), emptyString()), { }));
        return;
    case BinaryType::Arraybuffer:
        scheduleDispatchEvent(MessageEvent::create(ArrayBuffer::create(data)));
        return;
    }
    ASSERT_NOT_REACHED();
}

void RTCDataChannel::didDetectError(Ref<RTCError>&& error)
{
    scheduleDispatchEvent(RTCErrorEvent::create(eventNames().errorEvent, WTFMove(error)));
}

void RTCDataChannel::bufferedAmountIsDecreasing(size_t amount)
{
    queueTaskKeepingObjectAlive(*this, TaskSource::Networking, [amount](auto& channel) {
        auto previousBufferedAmount = channel.m_bufferedAmount;
        channel.m_bufferedAmount -= amount;
        if (previousBufferedAmount > channel.m_bufferedAmountLowThreshold && channel.m_bufferedAmount <= channel.m_bufferedAmountLowThreshold)
            channel.dispatchEvent(Event::create(eventNames().bufferedamountlowEvent, Event::CanBubble::No, Event::IsCancelable::No));
    });
}

void RTCDataChannel::stop()
{
    removeFromDataChannelLocalMapIfNeeded();

    id();
    close();
    m_stopped = true;
    m_handler = nullptr;
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
    static NeverDestroyed<HashMap<RTCDataChannelLocalIdentifier, std::unique_ptr<RTCDataChannelHandler>>> map;
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
    rtcDataChannelLocalMap().add(identifier().object(), WTFMove(m_handler));

    return makeUnique<DetachedRTCDataChannel>(identifier(), String { label() }, RTCDataChannelInit { options() }, state);
}

void RTCDataChannel::removeFromDataChannelLocalMapIfNeeded()
{
    if (!m_isDetached)
        return;

    Locker locker { s_rtcDataChannelLocalMapLock };
    rtcDataChannelLocalMap().remove(identifier().object());
}

std::unique_ptr<RTCDataChannelHandler> RTCDataChannel::handlerFromIdentifier(RTCDataChannelLocalIdentifier channelIdentifier)
{
    Locker locker { s_rtcDataChannelLocalMapLock };
    return rtcDataChannelLocalMap().take(channelIdentifier);
}

static Ref<RTCDataChannel> createClosedChannel(ScriptExecutionContext& context, String&& label, RTCDataChannelInit&& options)
{
    auto channel = RTCDataChannel::create(context, nullptr, WTFMove(label), WTFMove(options), RTCDataChannelState::Closed);
    return channel;
}

void RTCDataChannel::fireOpenEventIfNeeded()
{
    if (readyState() != RTCDataChannelState::Closing && readyState() != RTCDataChannelState::Closed)
        dispatchEvent(Event::create(eventNames().openEvent, Event::CanBubble::No, Event::IsCancelable::No));
}

Ref<RTCDataChannel> RTCDataChannel::create(ScriptExecutionContext& context, RTCDataChannelIdentifier identifier, String&& label, RTCDataChannelInit&& options, RTCDataChannelState state)
{
    CheckedPtr<RTCDataChannelRemoteHandler> remoteHandlerPtr;
    std::unique_ptr<RTCDataChannelHandler> handler;
    if (identifier.processIdentifier() == Process::identifier())
        handler = RTCDataChannel::handlerFromIdentifier(identifier.object());
    else {
        auto remoteHandler = RTCDataChannelRemoteHandler::create(identifier, context.createRTCDataChannelRemoteHandlerConnection());
        remoteHandlerPtr = remoteHandler.get();
        handler = WTFMove(remoteHandler);
    }

    if (!handler)
        return createClosedChannel(context, WTFMove(label), WTFMove(options));

    auto channel = RTCDataChannel::create(context, WTFMove(handler), WTFMove(label), WTFMove(options), state);

    if (remoteHandlerPtr)
        remoteHandlerPtr->setLocalIdentifier(channel->identifier());

    if (state == RTCDataChannelState::Open) {
        queueTaskKeepingObjectAlive(channel.get(), TaskSource::Networking, [](auto& channel) {
            channel.fireOpenEventIfNeeded();
        });
    }

    return channel;
}

ScriptExecutionContext* RTCDataChannel::scriptExecutionContext() const
{
    return ActiveDOMObject::scriptExecutionContext();
}

} // namespace WebCore

#endif // ENABLE(WEB_RTC)

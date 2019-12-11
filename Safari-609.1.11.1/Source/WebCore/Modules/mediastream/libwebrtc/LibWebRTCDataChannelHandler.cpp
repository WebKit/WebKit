/*
 * Copyright (C) 2017 Apple Inc.
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
#include "LibWebRTCDataChannelHandler.h"

#if USE(LIBWEBRTC)

#include "EventNames.h"
#include "RTCDataChannel.h"
#include "RTCDataChannelEvent.h"
#include <wtf/MainThread.h>

namespace WebCore {

webrtc::DataChannelInit LibWebRTCDataChannelHandler::fromRTCDataChannelInit(const RTCDataChannelInit& options)
{
    webrtc::DataChannelInit init;
    if (options.ordered)
        init.ordered = *options.ordered;
    if (options.maxPacketLifeTime)
        init.maxRetransmitTime = *options.maxPacketLifeTime;
    if (options.maxRetransmits)
        init.maxRetransmits = *options.maxRetransmits;
    init.protocol = options.protocol.utf8().data();
    if (options.negotiated)
        init.negotiated = *options.negotiated;
    if (options.id)
        init.id = *options.id;
    return init;
}

static inline String fromStdString(const std::string& value)
{
    return String::fromUTF8(value.data(), value.length());
}

Ref<RTCDataChannelEvent> LibWebRTCDataChannelHandler::channelEvent(Document& document, rtc::scoped_refptr<webrtc::DataChannelInterface>&& dataChannel)
{
    auto protocol = dataChannel->protocol();
    auto label = dataChannel->label();

    RTCDataChannelInit init;
    init.ordered = dataChannel->ordered();
    init.maxPacketLifeTime = dataChannel->maxRetransmitTime();
    init.maxRetransmits = dataChannel->maxRetransmits();
    init.protocol = fromStdString(protocol);
    init.negotiated = dataChannel->negotiated();
    init.id = dataChannel->id();

    auto handler =  makeUnique<LibWebRTCDataChannelHandler>(WTFMove(dataChannel));
    auto channel = RTCDataChannel::create(document, WTFMove(handler), fromStdString(label), WTFMove(init));

    return RTCDataChannelEvent::create(eventNames().datachannelEvent, Event::CanBubble::No, Event::IsCancelable::No, WTFMove(channel));
}

LibWebRTCDataChannelHandler::~LibWebRTCDataChannelHandler()
{
    if (m_client)
        m_channel->UnregisterObserver();
}

void LibWebRTCDataChannelHandler::setClient(RTCDataChannelHandlerClient& client)
{
    ASSERT(!m_client);
    m_client = &client;
    m_channel->RegisterObserver(this);
    checkState();
}

bool LibWebRTCDataChannelHandler::sendStringData(const String& text)
{
    auto utf8Text = text.utf8();
    return m_channel->Send({ rtc::CopyOnWriteBuffer(utf8Text.data(), utf8Text.length()), false });
}

bool LibWebRTCDataChannelHandler::sendRawData(const char* data, size_t length)
{
    return m_channel->Send({rtc::CopyOnWriteBuffer(data, length), true});
}

void LibWebRTCDataChannelHandler::close()
{
    if (m_client) {
        m_channel->UnregisterObserver();
        m_client = nullptr;
    }
    m_channel->Close();
}

void LibWebRTCDataChannelHandler::OnStateChange()
{
    if (!m_client)
        return;
    checkState();
}

void LibWebRTCDataChannelHandler::checkState()
{
    RTCDataChannelState state;
    switch (m_channel->state()) {
    case webrtc::DataChannelInterface::kConnecting:
        state = RTCDataChannelState::Connecting;
        break;
    case webrtc::DataChannelInterface::kOpen:
        state = RTCDataChannelState::Open;
        break;
    case webrtc::DataChannelInterface::kClosing:
        state = RTCDataChannelState::Closing;
        break;
    case webrtc::DataChannelInterface::kClosed:
        state = RTCDataChannelState::Closed;
        break;
    }
    callOnMainThread([protectedClient = makeRef(*m_client), state] {
        protectedClient->didChangeReadyState(state);
    });
}

void LibWebRTCDataChannelHandler::OnMessage(const webrtc::DataBuffer& buffer)
{
    if (!m_client)
        return;

    std::unique_ptr<webrtc::DataBuffer> protectedBuffer(new webrtc::DataBuffer(buffer));
    callOnMainThread([protectedClient = makeRef(*m_client), buffer = WTFMove(protectedBuffer)] {
        const char* data = reinterpret_cast<const char*>(buffer->data.data<char>());
        if (buffer->binary)
            protectedClient->didReceiveRawData(data, buffer->size());
        else
            protectedClient->didReceiveStringData(String::fromUTF8(data, buffer->size()));
    });
}

void LibWebRTCDataChannelHandler::OnBufferedAmountChange(uint64_t previousAmount)
{
    if (!m_client)
        return;

    if (previousAmount <= m_channel->buffered_amount())
        return;

    callOnMainThread([protectedClient = makeRef(*m_client), amount = m_channel->buffered_amount()] {
        protectedClient->bufferedAmountIsDecreasing(static_cast<size_t>(amount));
    });
}

} // namespace WebCore

#endif // USE(LIBWEBRTC)

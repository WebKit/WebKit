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

#if ENABLE(WEB_RTC) && USE(LIBWEBRTC)

#include "EventNames.h"
#include "LibWebRTCUtils.h"
#include "RTCDataChannel.h"
#include "RTCError.h"
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
    init.priority = fromRTCPriorityType(options.priority);
    return init;
}

LibWebRTCDataChannelHandler::LibWebRTCDataChannelHandler(rtc::scoped_refptr<webrtc::DataChannelInterface>&& channel)
    : m_channel(WTFMove(channel))
{
    ASSERT(m_channel);
    checkState();
    m_channel->RegisterObserver(this);
}

LibWebRTCDataChannelHandler::~LibWebRTCDataChannelHandler()
{
    m_channel->UnregisterObserver();
}

RTCDataChannelInit LibWebRTCDataChannelHandler::dataChannelInit() const
{
    auto protocol = m_channel->protocol();
    auto label = m_channel->label();

    RTCDataChannelInit init;
    init.ordered = m_channel->ordered();
    init.maxPacketLifeTime = m_channel->maxRetransmitTime();
    init.maxRetransmits = m_channel->maxRetransmits();
    init.protocol = fromStdString(protocol);
    init.negotiated = m_channel->negotiated();
    init.id = m_channel->id();
    init.priority = toRTCPriorityType(m_channel->priority());
    return init;
}

String LibWebRTCDataChannelHandler::label() const
{
    return fromStdString(m_channel->label());
}

void LibWebRTCDataChannelHandler::setClient(RTCDataChannelHandlerClient& client, ScriptExecutionContextIdentifier contextIdentifier)
{
    Locker locker { m_clientLock };
    ASSERT(!m_client);
    ASSERT(!m_hasClient);
    m_hasClient = true;
    m_client = client;
    m_contextIdentifier = contextIdentifier;

    for (auto& message : m_bufferedMessages) {
        switchOn(message, [&](Ref<FragmentedSharedBuffer>& data) {
            client.didReceiveRawData(data->makeContiguous()->data(), data->size());
        }, [&](String& text) {
            client.didReceiveStringData(text);
        }, [&](StateChange stateChange) {
            if (stateChange.error) {
                if (auto rtcError = toRTCError(*stateChange.error))
                    client.didDetectError(rtcError.releaseNonNull());
            }
            client.didChangeReadyState(stateChange.state);
        });
    }
    m_bufferedMessages.clear();
}

bool LibWebRTCDataChannelHandler::sendStringData(const CString& utf8Text)
{
    return m_channel->Send({ rtc::CopyOnWriteBuffer(utf8Text.data(), utf8Text.length()), false });
}

bool LibWebRTCDataChannelHandler::sendRawData(const uint8_t* data, size_t length)
{
    return m_channel->Send({ rtc::CopyOnWriteBuffer(data, length), true });
}

void LibWebRTCDataChannelHandler::close()
{
    m_channel->Close();
}

std::optional<unsigned short> LibWebRTCDataChannelHandler::id() const
{
    auto id = m_channel->id();
    return id != -1 ? std::make_optional(id) : std::nullopt;
}

void LibWebRTCDataChannelHandler::OnStateChange()
{
    checkState();
}

void LibWebRTCDataChannelHandler::checkState()
{
    std::optional<webrtc::RTCError> error;
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
        error = m_channel->error();
        state = RTCDataChannelState::Closed;
        break;
    }

    Locker locker { m_clientLock };
    if (!m_hasClient) {
        m_bufferedMessages.append(StateChange { state, WTFMove(error) });
        return;
    }
    postTask([client = m_client, state, error = WTFMove(error)] {
        if (!client)
            return;
        if (error && !error->ok()) {
            auto rtcError = toRTCError(*error);
            if (!rtcError)
                rtcError = RTCError::create(RTCError::Init { RTCErrorDetailType::DataChannelFailure, { }, { }, { }, { } }, String { });
            client->didDetectError(rtcError.releaseNonNull());
        }
        client->didChangeReadyState(state);
    });
}

void LibWebRTCDataChannelHandler::OnMessage(const webrtc::DataBuffer& buffer)
{
    Locker locker { m_clientLock };
    if (!m_hasClient) {
        auto* data = buffer.data.data<uint8_t>();
        if (buffer.binary)
            m_bufferedMessages.append(SharedBuffer::create(data, buffer.size()));
        else
            m_bufferedMessages.append(String::fromUTF8(data, buffer.size()));
        return;
    }

    std::unique_ptr<webrtc::DataBuffer> protectedBuffer(new webrtc::DataBuffer(buffer));
    postTask([client = m_client, buffer = WTFMove(protectedBuffer)] {
        if (!client)
            return;

        auto* data = buffer->data.data<uint8_t>();
        if (buffer->binary)
            client->didReceiveRawData(data, buffer->size());
        else
            client->didReceiveStringData(String::fromUTF8(data, buffer->size()));
    });
}

void LibWebRTCDataChannelHandler::OnBufferedAmountChange(uint64_t amount)
{
    Locker locker { m_clientLock };
    if (!m_hasClient)
        return;

    postTask([client = m_client, amount] {
        if (client)
            client->bufferedAmountIsDecreasing(static_cast<size_t>(amount));
    });
}

void LibWebRTCDataChannelHandler::postTask(Function<void()>&& function)
{
    ASSERT(m_clientLock.isHeld());

    if (!m_contextIdentifier) {
        callOnMainThread(WTFMove(function));
        return;
    }
    ScriptExecutionContext::postTaskTo(m_contextIdentifier, WTFMove(function));
}

} // namespace WebCore

#endif // ENABLE(WEB_RTC) && USE(LIBWEBRTC)

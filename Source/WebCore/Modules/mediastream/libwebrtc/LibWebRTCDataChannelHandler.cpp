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

#include "RTCDataChannel.h"
#include <wtf/MainThread.h>

namespace WebCore {

LibWebRTCDataChannelHandler::~LibWebRTCDataChannelHandler()
{
    if (m_client)
        m_channel->UnregisterObserver();
}

void LibWebRTCDataChannelHandler::setClient(RTCDataChannelHandlerClient* client)
{
    m_client = client;
    if (m_client)
        m_channel->RegisterObserver(this);
    else
        m_channel->UnregisterObserver();
}

bool LibWebRTCDataChannelHandler::sendStringData(const String& text)
{
    return m_channel->Send({rtc::CopyOnWriteBuffer(text.utf8().data(), text.length()), false});
}

bool LibWebRTCDataChannelHandler::sendRawData(const char* data, size_t length)
{
    return m_channel->Send({rtc::CopyOnWriteBuffer(data, length), true});
}

void LibWebRTCDataChannelHandler::close()
{
    m_channel->Close();
}

void LibWebRTCDataChannelHandler::OnStateChange()
{
    RTCDataChannel::ReadyState state;
    switch (m_channel->state()) {
    case webrtc::DataChannelInterface::kConnecting:
        state = RTCDataChannel::ReadyStateConnecting;
        break;
    case webrtc::DataChannelInterface::kOpen:
        state = RTCDataChannel::ReadyStateOpen;
        break;
    case webrtc::DataChannelInterface::kClosing:
        state = RTCDataChannel::ReadyStateClosing;
        break;
    case webrtc::DataChannelInterface::kClosed:
        state = RTCDataChannel::ReadyStateClosed;
        break;
    }
    ASSERT(m_client);
    callOnMainThread([protectedClient = makeRef(*m_client), state] {
        protectedClient->didChangeReadyState(state);
    });
}

void LibWebRTCDataChannelHandler::OnMessage(const webrtc::DataBuffer& buffer)
{
    ASSERT(m_client);
    std::unique_ptr<webrtc::DataBuffer> protectedBuffer(new webrtc::DataBuffer(buffer));
    callOnMainThread([protectedClient = makeRef(*m_client), buffer = WTFMove(protectedBuffer)] {
        // FIXME: Ensure this is correct by adding some tests with non-ASCII characters.
        const char* data = reinterpret_cast<const char*>(buffer->data.data());
        if (buffer->binary)
            protectedClient->didReceiveRawData(data, buffer->size());
        else
            protectedClient->didReceiveStringData(String(data, buffer->size()));
    });
}

void LibWebRTCDataChannelHandler::OnBufferedAmountChange(uint64_t previousAmount)
{
    if (previousAmount <= m_channel->buffered_amount())
        return;
    ASSERT(m_client);
    callOnMainThread([protectedClient = makeRef(*m_client)] {
        protectedClient->bufferedAmountIsDecreasing();
    });
}

} // namespace WebCore

#endif // USE(LIBWEBRTC)

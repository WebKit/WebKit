/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebSocketChannelManager.h"

namespace WebKit {

RefPtr<WebCore::ThreadableWebSocketChannel> WebSocketChannelManager::createWebSocketChannel(WebCore::Document& document, WebCore::WebSocketChannelClient& client)
{
    auto channel = WebSocketChannel::create(document, client);
    m_channels.add(channel->identifier(), channel.copyRef());
    return channel;
}

void WebSocketChannelManager::networkProcessCrashed()
{
    auto channels = WTFMove(m_channels);
    for (auto& channel : channels.values())
        channel->networkProcessCrashed();
}

void WebSocketChannelManager::didReceiveMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
    auto iterator = m_channels.find(decoder.destinationID());
    if (iterator != m_channels.end())
        iterator->value->didReceiveMessage(connection, decoder);
}

} // namespace WebKit

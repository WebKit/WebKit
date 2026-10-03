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

#include "Decoder.h"
#include <WebCore/WebSocketIdentifier.h>

namespace WebKit {

void WebSocketChannelManager::addChannel(WebSocketChannel& channel)
{
    Locker locker { m_lock };
    ASSERT(!m_channels.contains(channel.identifier()));
    m_channels.add(channel.identifier(), ThreadSafeWeakPtr<WebSocketChannel> { channel });
}

void WebSocketChannelManager::removeChannel(WebSocketChannel& channel)
{
    Locker locker { m_lock };
    m_channels.remove(channel.identifier());
}

bool WebSocketChannelManager::hasReachedSocketLimit() const
{
    Locker locker { m_lock };
    return m_channels.size() >= maximumSocketCount;
}

void WebSocketChannelManager::networkProcessCrashed()
{
    auto channels = [&] {
        Locker locker { m_lock };
        return WTF::move(m_channels);
    }();

    for (auto& weakChannel : channels.values()) {
        if (RefPtr channel = weakChannel.get())
            channel->networkProcessCrashed();
    }
}

void WebSocketChannelManager::didReceiveMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
    RefPtr<WebSocketChannel> channel;
    {
        Locker locker { m_lock };
        auto iterator = m_channels.find(AtomicObjectIdentifier<WebCore::WebSocketIdentifierType>(decoder.destinationID()));
        if (iterator == m_channels.end())
            return;
        channel = iterator->value.get();
    }

    if (channel)
        channel->didReceiveMessage(connection, decoder);
}

} // namespace WebKit

/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#pragma once

#include "Connection.h"
#include <WebCore/BroadcastChannelIdentifier.h>
#include <WebCore/SecurityOriginData.h>
#include <wtf/HashMap.h>

namespace WebCore {
struct MessageWithMessagePorts;
}

namespace WebKit {

class NetworkBroadcastChannelRegistry {
    WTF_MAKE_FAST_ALLOCATED;
public:
    NetworkBroadcastChannelRegistry();

    void removeConnection(IPC::Connection&);

    void didReceiveMessage(IPC::Connection&, IPC::Decoder&);

    void registerChannel(IPC::Connection&, const WebCore::SecurityOriginData&, const String& name, WebCore::BroadcastChannelIdentifier);
    void unregisterChannel(IPC::Connection&, const WebCore::SecurityOriginData&, const String& name, WebCore::BroadcastChannelIdentifier);
    void postMessage(IPC::Connection&, const WebCore::SecurityOriginData&, const String& name, WebCore::BroadcastChannelIdentifier source, WebCore::MessageWithMessagePorts&&, CompletionHandler<void()>&&);

private:
    struct GlobalBroadcastChannelIdentifier {
        IPC::Connection::UniqueID connectionIdentifier;
        WebCore::BroadcastChannelIdentifier channelIndentifierInProcess;

        bool operator==(const GlobalBroadcastChannelIdentifier& other) const
        {
            return connectionIdentifier == other.connectionIdentifier && channelIndentifierInProcess == other.channelIndentifierInProcess;
        }
    };

    // FIXME: BroadcastChannel needs partitioning (https://github.com/whatwg/html/issues/5803).
    using NameToChannelIdentifiersMap = HashMap<String, Vector<GlobalBroadcastChannelIdentifier>>;
    HashMap<WebCore::SecurityOriginData, NameToChannelIdentifiersMap> m_broadcastChannels;
};

} // namespace WebKit

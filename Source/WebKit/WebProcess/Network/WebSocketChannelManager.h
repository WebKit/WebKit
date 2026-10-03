/*
 * Copyright (C) 2019-2022 Apple Inc. All rights reserved.
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

#include "WebSocketChannel.h"
#include <wtf/HashMap.h>
#include <wtf/Lock.h>
#include <wtf/ThreadSafeWeakPtr.h>

namespace IPC {
class Connection;
class Decoder;
}

namespace WebCore {
class Document;
class ThreadableWebSocketChannel;
}

namespace WebKit {

class WebSocketChannelManager {
public:
    // Choose a per-process limit that matches Firefox and Tor's global count (200),
    // and Brave's per-process limit (50). Chrome has a global limit of 256, so
    // any compatibility risk with Chrome should be very low.
    static constexpr size_t maximumSocketCount = 200;

    WebSocketChannelManager() = default;

    void networkProcessCrashed();
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&);

    void addChannel(WebSocketChannel&);
    void removeChannel(WebSocketChannel&);

    bool hasReachedSocketLimit() const;

private:
    mutable Lock m_lock;
    HashMap<WebCore::WebSocketIdentifier, ThreadSafeWeakPtr<WebSocketChannel>> m_channels WTF_GUARDED_BY_LOCK(m_lock);
};

} // namespace WebKit

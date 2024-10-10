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

#pragma once

#include "NetworkTaskCocoa.h"
#include "WebPageProxyIdentifier.h"
#include <WebCore/FrameIdentifier.h>
#include <WebCore/PageIdentifier.h>
#include <WebCore/SecurityOriginData.h>
#include <wtf/RetainPtr.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakPtr.h>

OBJC_CLASS NSURLSessionWebSocketTask;

namespace WebKit {
class WebSocketTask;
}

namespace WebCore {
class ResourceResponse;
class ResourceRequest;
struct ClientOrigin;
}

namespace WebKit {
class NetworkSession;
class NetworkSessionCocoa;
class NetworkSocketChannel;
struct SessionSet;

class WebSocketTask : public CanMakeWeakPtr<WebSocketTask>, public CanMakeCheckedPtr<WebSocketTask>, public NetworkTaskCocoa {
    WTF_MAKE_TZONE_ALLOCATED(WebSocketTask);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(WebSocketTask);
public:
    WebSocketTask(NetworkSocketChannel&, WebPageProxyIdentifier, std::optional<WebCore::FrameIdentifier>, std::optional<WebCore::PageIdentifier>, WeakPtr<SessionSet>&&, const WebCore::ResourceRequest&, const WebCore::ClientOrigin&, RetainPtr<NSURLSessionWebSocketTask>&&, WebCore::ShouldRelaxThirdPartyCookieBlocking, WebCore::StoredCredentialsPolicy);
    ~WebSocketTask();

    void sendString(std::span<const uint8_t>, CompletionHandler<void()>&&);
    void sendData(std::span<const uint8_t>, CompletionHandler<void()>&&);
    void close(int32_t code, const String& reason);

    void didConnect(const String&);
    void didClose(unsigned short code, const String& reason);

    void cancel();
    void resume();

    typedef uint64_t TaskIdentifier;
    TaskIdentifier identifier() const;

    NetworkSessionCocoa* networkSession();
    SessionSet* sessionSet() { return m_sessionSet.get(); }

    WebPageProxyIdentifier webProxyPageID() const { return m_webProxyPageID; }
    std::optional<WebCore::FrameIdentifier> frameID() const final { return m_frameID; }
    std::optional<WebCore::PageIdentifier> pageID() const final { return m_pageID; }
    String partition() const { return m_partition; }
    const WebCore::SecurityOriginData& topOrigin() const { return m_topOrigin; }

private:
    void readNextMessage();

    NSURLSessionTask* task() const final;
    WebCore::StoredCredentialsPolicy storedCredentialsPolicy() const final { return m_storedCredentialsPolicy; }

    CheckedPtr<NetworkSocketChannel> m_channel;
    RetainPtr<NSURLSessionWebSocketTask> m_task;
    bool m_receivedDidClose { false };
    bool m_receivedDidConnect { false };
    WebPageProxyIdentifier m_webProxyPageID;
    std::optional<WebCore::FrameIdentifier> m_frameID;
    std::optional<WebCore::PageIdentifier> m_pageID;
    WeakPtr<SessionSet> m_sessionSet;
    String m_partition;
    WebCore::StoredCredentialsPolicy m_storedCredentialsPolicy { WebCore::StoredCredentialsPolicy::DoNotUse };
    WebCore::SecurityOriginData m_topOrigin;
};

} // namespace WebKit

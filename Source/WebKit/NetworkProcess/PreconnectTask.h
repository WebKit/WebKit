/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#if ENABLE(SERVER_PRECONNECT)

#include "NetworkLoadClient.h"
#include <WebCore/Timer.h>
#include <wtf/CompletionHandler.h>
#include <wtf/WeakPtr.h>

namespace WebKit {

class NetworkLoad;
class NetworkProcess;
class NetworkSession;

struct NetworkLoadParameters;

class PreconnectTask final : public RefCounted<PreconnectTask>, public NetworkLoadClient {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(PreconnectTask);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(PreconnectTask);
public:
    static Ref<PreconnectTask> create(NetworkSession&, NetworkLoadParameters&&);
    ~PreconnectTask();

    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

    void setH2PingCallback(const URL&, CompletionHandler<void(Expected<WTF::Seconds, WebCore::ResourceError>&&)>&&);
    void start(CompletionHandler<void(const WebCore::ResourceError&, const WebCore::NetworkLoadMetrics&)>&& = { }, Seconds timeout = 60_s);

private:
    PreconnectTask(NetworkSession&, NetworkLoadParameters&&);

    void didTimeout();

    // NetworkLoadClient.
    bool isSynchronous() const final { return false; }
    bool isAllowedToAskUserForCredentials() const final { return false; }
    void didSendData(uint64_t bytesSent, uint64_t totalBytesToBeSent) final;
    void willSendRedirectedRequest(WebCore::ResourceRequest&&, WebCore::ResourceRequest&& redirectRequest, WebCore::ResourceResponse&& redirectResponse, CompletionHandler<void(WebCore::ResourceRequest&&)>&&) final;
    void didReceiveResponse(WebCore::ResourceResponse&&, PrivateRelayed, ResponseCompletionHandler&&) final;
    void didReceiveBuffer(const WebCore::FragmentedSharedBuffer&) final;
    void didFinishLoading(const WebCore::NetworkLoadMetrics&) final;
    void didFailLoading(const WebCore::ResourceError&) final;

    const Ref<NetworkLoad> m_networkLoad;
    CompletionHandler<void(const WebCore::ResourceError&, const WebCore::NetworkLoadMetrics&)> m_completionHandler;
    std::unique_ptr<WebCore::Timer> m_timeoutTimer;
};

} // namespace WebKit

#endif // ENABLE(SERVER_PRECONNECT)

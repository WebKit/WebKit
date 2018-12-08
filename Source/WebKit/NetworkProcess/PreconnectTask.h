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
class NetworkLoadParameters;

class PreconnectTask final : public NetworkLoadClient {
public:
    explicit PreconnectTask(NetworkLoadParameters&&, CompletionHandler<void(const WebCore::ResourceError&)>&& completionHandler = { });
    ~PreconnectTask();

private:
    // NetworkLoadClient.
    bool isSynchronous() const final { return false; }
    bool isAllowedToAskUserForCredentials() const final { return false; }
    void didSendData(unsigned long long bytesSent, unsigned long long totalBytesToBeSent) final;
    void willSendRedirectedRequest(WebCore::ResourceRequest&&, WebCore::ResourceRequest&& redirectRequest, WebCore::ResourceResponse&& redirectResponse) final;
    void didReceiveResponse(WebCore::ResourceResponse&&, ResponseCompletionHandler&&) final;
    void didReceiveBuffer(Ref<WebCore::SharedBuffer>&&, int reportedEncodedDataLength) final;
    void didFinishLoading(const WebCore::NetworkLoadMetrics&) final;
    void didFailLoading(const WebCore::ResourceError&) final;

    void didFinish(const WebCore::ResourceError&);

    std::unique_ptr<NetworkLoad> m_networkLoad;
    CompletionHandler<void(const WebCore::ResourceError&)> m_completionHandler;
    WebCore::Timer m_timeoutTimer;
};

} // namespace WebKit

#endif // ENABLE(SERVER_PRECONNECT)

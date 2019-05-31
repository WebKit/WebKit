/*
 * Copyright (C) 2016-2018 Apple Inc. All rights reserved.
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

#include "NetworkDataTask.h"
#include "NetworkResourceLoadParameters.h"
#include <WebCore/ResourceError.h>
#include <WebCore/ResourceResponse.h>
#include <wtf/CompletionHandler.h>
#include <wtf/UniqueRef.h>
#include <wtf/WeakPtr.h>

namespace WebKit {

class NetworkConnectionToWebProcess;
class NetworkLoadChecker;
class NetworkProcess;

class PingLoad final : public CanMakeWeakPtr<PingLoad>, private NetworkDataTaskClient {
public:
    PingLoad(NetworkConnectionToWebProcess&, NetworkProcess&, NetworkResourceLoadParameters&&, CompletionHandler<void(const WebCore::ResourceError&, const WebCore::ResourceResponse&)>&&);
    PingLoad(NetworkProcess&, NetworkResourceLoadParameters&&, CompletionHandler<void(const WebCore::ResourceError&, const WebCore::ResourceResponse&)>&&);

private:
    ~PingLoad();
    void initialize(NetworkProcess&);

    const URL& currentURL() const;

    void willPerformHTTPRedirection(WebCore::ResourceResponse&&, WebCore::ResourceRequest&&, RedirectCompletionHandler&&) final;
    void didReceiveChallenge(WebCore::AuthenticationChallenge&&, ChallengeCompletionHandler&&) final;
    void didReceiveResponse(WebCore::ResourceResponse&&, ResponseCompletionHandler&&) final;
    void didReceiveData(Ref<WebCore::SharedBuffer>&&) final;
    void didCompleteWithError(const WebCore::ResourceError&, const WebCore::NetworkLoadMetrics&) final;
    void didSendData(uint64_t totalBytesSent, uint64_t totalBytesExpectedToSend) final;
    void wasBlocked() final;
    void cannotShowURL() final;
    void wasBlockedByRestrictions() final;
    void timeoutTimerFired();

    void loadRequest(NetworkProcess&, WebCore::ResourceRequest&&);

    void didFinish(const WebCore::ResourceError& = { }, const WebCore::ResourceResponse& response = { });
    
    NetworkResourceLoadParameters m_parameters;
    CompletionHandler<void(const WebCore::ResourceError&, const WebCore::ResourceResponse&)> m_completionHandler;
    RefPtr<NetworkDataTask> m_task;
    WebCore::Timer m_timeoutTimer;
    UniqueRef<NetworkLoadChecker> m_networkLoadChecker;
    Vector<RefPtr<WebCore::BlobDataFileReference>> m_blobFiles;
};

}

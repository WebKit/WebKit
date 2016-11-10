/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#ifndef PingLoad_h
#define PingLoad_h

#include "AuthenticationManager.h"
#include "NetworkDataTask.h"
#include "SessionTracker.h"

namespace WebKit {

class PingLoad final : private NetworkDataTaskClient {
public:
    PingLoad(const NetworkResourceLoadParameters& parameters)
        : m_timeoutTimer(*this, &PingLoad::timeoutTimerFired)
        , m_shouldFollowRedirects(parameters.shouldFollowRedirects)
    {
        if (auto* networkSession = SessionTracker::networkSession(parameters.sessionID)) {
            m_task = NetworkDataTask::create(*networkSession, *this, parameters);
            m_task->resume();
        } else
            ASSERT_NOT_REACHED();

        // If the server never responds, this object will hang around forever.
        // Set a very generous timeout, just in case.
        m_timeoutTimer.startOneShot(60000);
    }
    
private:
    void willPerformHTTPRedirection(WebCore::ResourceResponse&&, WebCore::ResourceRequest&& request, RedirectCompletionHandler&& completionHandler) final
    {
        completionHandler(m_shouldFollowRedirects ? request : WebCore::ResourceRequest());
    }
    void didReceiveChallenge(const WebCore::AuthenticationChallenge&, ChallengeCompletionHandler&& completionHandler) final
    {
        completionHandler(AuthenticationChallengeDisposition::Cancel, { });
        delete this;
    }
    void didReceiveResponseNetworkSession(WebCore::ResourceResponse&&, ResponseCompletionHandler&& completionHandler) final
    {
        completionHandler(WebCore::PolicyAction::PolicyIgnore);
        delete this;
    }
    void didReceiveData(Ref<WebCore::SharedBuffer>&&) final { ASSERT_NOT_REACHED(); }
    void didCompleteWithError(const WebCore::ResourceError&) final { delete this; }
    void didSendData(uint64_t totalBytesSent, uint64_t totalBytesExpectedToSend) final { }
    void wasBlocked() final { delete this; }
    void cannotShowURL() final { delete this; }

    void timeoutTimerFired() { delete this; }
    
    virtual ~PingLoad()
    {
        if (m_task) {
            ASSERT(m_task->client() == this);
            m_task->clearClient();
            m_task->cancel();
        }
    }
    
    RefPtr<NetworkDataTask> m_task;
    WebCore::Timer m_timeoutTimer;
    bool m_shouldFollowRedirects;
};

}

#endif

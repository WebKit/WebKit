/*
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
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
#include "PingLoad.h"

#if USE(NETWORK_SESSION)

#include "AuthenticationManager.h"
#include "Logging.h"
#include "NetworkCORSPreflightChecker.h"
#include "SessionTracker.h"

#define RELEASE_LOG_IF_ALLOWED(fmt, ...) RELEASE_LOG_IF(m_parameters.sessionID.isAlwaysOnLoggingAllowed(), Network, "%p - PingLoad::" fmt, this, ##__VA_ARGS__)

namespace WebKit {

using namespace WebCore;

PingLoad::PingLoad(NetworkResourceLoadParameters&& parameters)
    : m_parameters(WTFMove(parameters))
    , m_timeoutTimer(*this, &PingLoad::timeoutTimerFired)
{
    // If the server never responds, this object will hang around forever.
    // Set a very generous timeout, just in case.
    m_timeoutTimer.startOneShot(60000_s);

    if (needsCORSPreflight(m_parameters.request))
        doCORSPreflight(m_parameters.request);
    else
        startNetworkLoad();
}

PingLoad::~PingLoad()
{
    if (m_task) {
        ASSERT(m_task->client() == this);
        m_task->clearClient();
        m_task->cancel();
    }
}

void PingLoad::startNetworkLoad()
{
    RELEASE_LOG_IF_ALLOWED("startNetworkLoad");
    if (auto* networkSession = SessionTracker::networkSession(m_parameters.sessionID)) {
        m_task = NetworkDataTask::create(*networkSession, *this, m_parameters);
        m_task->resume();
    } else
        ASSERT_NOT_REACHED();
}

void PingLoad::willPerformHTTPRedirection(ResourceResponse&&, ResourceRequest&& request, RedirectCompletionHandler&& completionHandler)
{
    RELEASE_LOG_IF_ALLOWED("willPerformHTTPRedirection");
    // FIXME: Do a CORS preflight if necessary.
    // FIXME: We should ensure the number of redirects does not exceed 20.
    completionHandler(m_parameters.shouldFollowRedirects ? request : ResourceRequest());
}

void PingLoad::didReceiveChallenge(const AuthenticationChallenge&, ChallengeCompletionHandler&& completionHandler)
{
    RELEASE_LOG_IF_ALLOWED("didReceiveChallenge");
    completionHandler(AuthenticationChallengeDisposition::Cancel, { });
    delete this;
}

void PingLoad::didReceiveResponseNetworkSession(ResourceResponse&&, ResponseCompletionHandler&& completionHandler)
{
    RELEASE_LOG_IF_ALLOWED("didReceiveResponseNetworkSession");
    completionHandler(PolicyAction::PolicyIgnore);
    delete this;
}

void PingLoad::didReceiveData(Ref<SharedBuffer>&&)
{
    RELEASE_LOG_IF_ALLOWED("didReceiveData");
    ASSERT_NOT_REACHED();
}

void PingLoad::didCompleteWithError(const ResourceError& error, const NetworkLoadMetrics&)
{
    if (error.isNull())
        RELEASE_LOG_IF_ALLOWED("didComplete");
    else
        RELEASE_LOG_IF_ALLOWED("didCompleteWithError, error_code: %d", error.errorCode());
    delete this;
}

void PingLoad::didSendData(uint64_t totalBytesSent, uint64_t totalBytesExpectedToSend)
{
}

void PingLoad::wasBlocked()
{
    RELEASE_LOG_IF_ALLOWED("wasBlocked");
    delete this;
}

void PingLoad::cannotShowURL()
{
    RELEASE_LOG_IF_ALLOWED("cannotShowURL");
    delete this;
}

void PingLoad::timeoutTimerFired()
{
    RELEASE_LOG_IF_ALLOWED("timeoutTimerFired");
    delete this;
}

bool PingLoad::needsCORSPreflight(const ResourceRequest& request) const
{
    if (m_parameters.mode == FetchOptions::Mode::Cors) {
        ASSERT(m_parameters.sourceOrigin);
        return !m_parameters.sourceOrigin->canRequest(request.url());
    }
    return false;
}

void PingLoad::doCORSPreflight(const ResourceRequest& request)
{
    RELEASE_LOG_IF_ALLOWED("doCORSPreflight");
    ASSERT(!m_corsPreflightChecker);
    ASSERT(m_parameters.sourceOrigin);

    NetworkCORSPreflightChecker::Parameters parameters = {
        request,
        *m_parameters.sourceOrigin,
        m_parameters.sessionID,
        m_parameters.allowStoredCredentials
    };
    m_corsPreflightChecker = std::make_unique<NetworkCORSPreflightChecker>(WTFMove(parameters), [this](NetworkCORSPreflightChecker::Result result) {
        RELEASE_LOG_IF_ALLOWED("doCORSPreflight complete, success: %d", result == NetworkCORSPreflightChecker::Result::Success);
        if (result == NetworkCORSPreflightChecker::Result::Success) {
            m_corsPreflightChecker = nullptr;
            startNetworkLoad();
        } else
            delete this;
    });
    m_corsPreflightChecker->startPreflight();
}

} // namespace WebKit

#endif // USE(NETWORK_SESSION)

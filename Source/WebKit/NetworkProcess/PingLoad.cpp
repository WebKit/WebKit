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

#include "config.h"
#include "PingLoad.h"

#include "AuthenticationChallengeDisposition.h"
#include "AuthenticationManager.h"
#include "Logging.h"
#include "NetworkLoadChecker.h"
#include "SessionTracker.h"
#include "WebErrors.h"

#define RELEASE_LOG_IF_ALLOWED(fmt, ...) RELEASE_LOG_IF(m_parameters.sessionID.isAlwaysOnLoggingAllowed(), Network, "%p - PingLoad::" fmt, this, ##__VA_ARGS__)

namespace WebKit {

using namespace WebCore;

PingLoad::PingLoad(NetworkResourceLoadParameters&& parameters, CompletionHandler<void(const ResourceError&, const ResourceResponse&)>&& completionHandler)
    : m_parameters(WTFMove(parameters))
    , m_completionHandler(WTFMove(completionHandler))
    , m_timeoutTimer(*this, &PingLoad::timeoutTimerFired)
    , m_networkLoadChecker(makeUniqueRef<NetworkLoadChecker>(FetchOptions { m_parameters.options}, m_parameters.sessionID, m_parameters.webPageID, m_parameters.webFrameID, WTFMove(m_parameters.originalRequestHeaders), URL { m_parameters.request.url() }, m_parameters.sourceOrigin.copyRef(), m_parameters.preflightPolicy, m_parameters.request.httpReferrer()))
{
    m_networkLoadChecker->enableContentExtensionsCheck();
    if (m_parameters.cspResponseHeaders)
        m_networkLoadChecker->setCSPResponseHeaders(WTFMove(m_parameters.cspResponseHeaders.value()));
#if ENABLE(CONTENT_EXTENSIONS)
    m_networkLoadChecker->setContentExtensionController(WTFMove(m_parameters.mainDocumentURL), m_parameters.userContentControllerIdentifier);
#endif

    // If the server never responds, this object will hang around forever.
    // Set a very generous timeout, just in case.
    m_timeoutTimer.startOneShot(60000_s);

    m_networkLoadChecker->check(ResourceRequest { m_parameters.request }, nullptr, [this] (auto&& result) {
        if (!result.has_value()) {
            this->didFinish(result.error());
            return;
        }
        this->loadRequest(WTFMove(result.value()));
    });
}

PingLoad::~PingLoad()
{
    if (m_task) {
        ASSERT(m_task->client() == this);
        m_task->clearClient();
        m_task->cancel();
    }
}

void PingLoad::didFinish(const ResourceError& error, const ResourceResponse& response)
{
    m_completionHandler(error, response);
    delete this;
}

void PingLoad::loadRequest(ResourceRequest&& request)
{
    RELEASE_LOG_IF_ALLOWED("startNetworkLoad");
    if (auto* networkSession = SessionTracker::networkSession(m_parameters.sessionID)) {
        auto loadParameters = m_parameters;
        loadParameters.request = WTFMove(request);
        m_task = NetworkDataTask::create(*networkSession, *this, WTFMove(loadParameters));
        m_task->resume();
    } else
        ASSERT_NOT_REACHED();
}

void PingLoad::willPerformHTTPRedirection(ResourceResponse&& redirectResponse, ResourceRequest&& request, RedirectCompletionHandler&& completionHandler)
{
    m_networkLoadChecker->checkRedirection(ResourceRequest { }, WTFMove(request), WTFMove(redirectResponse), nullptr, [this, completionHandler = WTFMove(completionHandler)] (auto&& result) mutable {
        if (!result.has_value()) {
            completionHandler({ });
            this->didFinish(result.error());
            return;
        }
        auto request = WTFMove(result->redirectRequest);
        m_networkLoadChecker->prepareRedirectedRequest(request);

        if (!request.url().protocolIsInHTTPFamily()) {
            this->didFinish(ResourceError { String { }, 0, request.url(), "Redirection to URL with a scheme that is not HTTP(S)"_s, ResourceError::Type::AccessControl });
            return;
        }

        completionHandler(WTFMove(request));
    });
}

void PingLoad::didReceiveChallenge(AuthenticationChallenge&&, ChallengeCompletionHandler&& completionHandler)
{
    RELEASE_LOG_IF_ALLOWED("didReceiveChallenge");
    auto weakThis = makeWeakPtr(*this);
    completionHandler(AuthenticationChallengeDisposition::Cancel, { });
    if (!weakThis)
        return;
    didFinish(ResourceError { String(), 0, currentURL(), "Failed HTTP authentication"_s, ResourceError::Type::AccessControl });
}

void PingLoad::didReceiveResponse(ResourceResponse&& response, ResponseCompletionHandler&& completionHandler)
{
    RELEASE_LOG_IF_ALLOWED("didReceiveResponse - httpStatusCode: %d", response.httpStatusCode());
    auto weakThis = makeWeakPtr(*this);
    completionHandler(PolicyAction::Ignore);
    if (!weakThis)
        return;
    didFinish({ }, response);
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

    didFinish(error);
}

void PingLoad::didSendData(uint64_t totalBytesSent, uint64_t totalBytesExpectedToSend)
{
}

void PingLoad::wasBlocked()
{
    RELEASE_LOG_IF_ALLOWED("wasBlocked");
    didFinish(blockedError(ResourceRequest { currentURL() }));
}

void PingLoad::cannotShowURL()
{
    RELEASE_LOG_IF_ALLOWED("cannotShowURL");
    didFinish(cannotShowURLError(ResourceRequest { currentURL() }));
}

void PingLoad::timeoutTimerFired()
{
    RELEASE_LOG_IF_ALLOWED("timeoutTimerFired");
    didFinish(ResourceError { String(), 0, currentURL(), "Load timed out"_s, ResourceError::Type::Timeout });
}

const URL& PingLoad::currentURL() const
{
    return m_networkLoadChecker->url();
}

} // namespace WebKit

#undef RELEASE_LOG_IF_ALLOWED

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

#include "config.h"
#include "PreconnectTask.h"

#if ENABLE(SERVER_PRECONNECT)

#include "AuthenticationManager.h"
#include "Logging.h"
#include "NetworkLoadParameters.h"
#include "NetworkSession.h"
#include "SessionTracker.h"
#include "WebErrors.h"
#include <WebCore/AuthenticationChallenge.h>
#include <WebCore/ResourceError.h>

#if PLATFORM(COCOA)
#include "NetworkDataTaskCocoa.h"
#endif

namespace WebKit {

using namespace WebCore;

PreconnectTask::PreconnectTask(PAL::SessionID sessionID, const URL& url, WebCore::StoredCredentialsPolicy storedCredentialsPolicy, WTF::CompletionHandler<void(const WebCore::ResourceError&)>&& completionHandler)
    : m_completionHandler(WTFMove(completionHandler))
    , m_timeoutTimer([this] { didFinish(ResourceError { String(), 0, m_task->firstRequest().url(), ASCIILiteral("Preconnection timed out"), ResourceError::Type::Timeout }); })
{
    auto* networkSession = SessionTracker::networkSession(sessionID);
    ASSERT(networkSession);

    RELEASE_LOG(Network, "%p - PreconnectTask::PreconnectTask()", this);
    NetworkLoadParameters parameters;
    parameters.request = ResourceRequest(url);
    parameters.shouldPreconnectOnly = PreconnectOnly::Yes;
    parameters.storedCredentialsPolicy = storedCredentialsPolicy;
    m_task = NetworkDataTask::create(*networkSession, *this, parameters);
    m_task->resume();

    m_timeoutTimer.startOneShot(60000_s);
}

PreconnectTask::~PreconnectTask()
{
    ASSERT(m_task->client() == this);
    m_task->clearClient();
    m_task->cancel();
}

void PreconnectTask::willPerformHTTPRedirection(WebCore::ResourceResponse&&, WebCore::ResourceRequest&&, RedirectCompletionHandler&&)
{
    ASSERT_NOT_REACHED();
}

void PreconnectTask::didReceiveChallenge(const WebCore::AuthenticationChallenge& challenge, ChallengeCompletionHandler&& completionHandler)
{
    ASSERT_NOT_REACHED();
}

void PreconnectTask::didReceiveResponseNetworkSession(WebCore::ResourceResponse&&, ResponseCompletionHandler&&)
{
    ASSERT_NOT_REACHED();
}

void PreconnectTask::didReceiveData(Ref<WebCore::SharedBuffer>&&)
{
    ASSERT_NOT_REACHED();
}

void PreconnectTask::didCompleteWithError(const WebCore::ResourceError& error, const WebCore::NetworkLoadMetrics&)
{
    if (error.isNull())
        RELEASE_LOG(Network, "%p - PreconnectTask::didComplete", this);
    else
        RELEASE_LOG(Network, "%p - PreconnectTask::didCompleteWithError, error_code: %d", this, error.errorCode());

    didFinish(error);
}

void PreconnectTask::didSendData(uint64_t totalBytesSent, uint64_t totalBytesExpectedToSend)
{
    ASSERT_NOT_REACHED();
}

void PreconnectTask::wasBlocked()
{
    RELEASE_LOG(Network, "%p - PreconnectTask::wasBlocked()", this);
    didFinish(blockedError(m_task->firstRequest()));
}

void PreconnectTask::cannotShowURL()
{
    RELEASE_LOG(Network, "%p - PreconnectTask::cannotShowURL()", this);
    didFinish(cannotShowURLError(m_task->firstRequest()));
}

void PreconnectTask::didFinish(const ResourceError& error)
{
    if (m_completionHandler)
        m_completionHandler(error);
    delete this;
}

} // namespace WebKit

#endif // ENABLE(SERVER_PRECONNECT)

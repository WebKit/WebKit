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

#include "config.h"
#include "NetworkDataTask.h"

#if USE(NETWORK_SESSION)

#include "NetworkDataTaskBlob.h"
#include "NetworkLoadParameters.h"
#include "NetworkSession.h"
#include <WebCore/ResourceError.h>
#include <WebCore/ResourceResponse.h>
#include <wtf/MainThread.h>
#include <wtf/RunLoop.h>

#if PLATFORM(COCOA)
#include "NetworkDataTaskCocoa.h"
#endif
#if USE(SOUP)
#include "NetworkDataTaskSoup.h"
#endif

using namespace WebCore;

namespace WebKit {

Ref<NetworkDataTask> NetworkDataTask::create(NetworkSession& session, NetworkDataTaskClient& client, const NetworkLoadParameters& parameters)
{
    if (parameters.request.url().protocolIsBlob())
        return NetworkDataTaskBlob::create(session, client, parameters.request, parameters.contentSniffingPolicy, parameters.blobFileReferences);

#if PLATFORM(COCOA)
    return NetworkDataTaskCocoa::create(session, client, parameters.request, parameters.allowStoredCredentials, parameters.contentSniffingPolicy, parameters.shouldClearReferrerOnHTTPSToHTTPRedirect);
#endif
#if USE(SOUP)
    return NetworkDataTaskSoup::create(session, client, parameters.request, parameters.allowStoredCredentials, parameters.contentSniffingPolicy, parameters.shouldClearReferrerOnHTTPSToHTTPRedirect);
#endif
}

NetworkDataTask::NetworkDataTask(NetworkSession& session, NetworkDataTaskClient& client, const ResourceRequest& requestWithCredentials, StoredCredentials storedCredentials, bool shouldClearReferrerOnHTTPSToHTTPRedirect)
    : m_failureTimer(*this, &NetworkDataTask::failureTimerFired)
    , m_session(session)
    , m_client(&client)
    , m_partition(requestWithCredentials.cachePartition())
    , m_storedCredentials(storedCredentials)
    , m_lastHTTPMethod(requestWithCredentials.httpMethod())
    , m_firstRequest(requestWithCredentials)
    , m_shouldClearReferrerOnHTTPSToHTTPRedirect(shouldClearReferrerOnHTTPSToHTTPRedirect)
{
    ASSERT(RunLoop::isMain());

    if (!requestWithCredentials.url().isValid()) {
        scheduleFailure(InvalidURLFailure);
        return;
    }

    if (!portAllowed(requestWithCredentials.url())) {
        scheduleFailure(BlockedFailure);
        return;
    }
}

NetworkDataTask::~NetworkDataTask()
{
    ASSERT(RunLoop::isMain());
    ASSERT(!m_client);
}

void NetworkDataTask::scheduleFailure(FailureType type)
{
    ASSERT(type != NoFailure);
    m_scheduledFailureType = type;
    m_failureTimer.startOneShot(0_s);
}

void NetworkDataTask::didReceiveResponse(ResourceResponse&& response, ResponseCompletionHandler&& completionHandler)
{
    ASSERT(m_client);
    if (response.isHTTP09()) {
        auto url = response.url();
        std::optional<uint16_t> port = url.port();
        if (port && !isDefaultPortForProtocol(port.value(), url.protocol())) {
            completionHandler(PolicyIgnore);
            cancel();
            m_client->didCompleteWithError({ String(), 0, url, "Cancelled load from '" + url.stringCenterEllipsizedToLength() + "' because it is using HTTP/0.9." });
            return;
        }
    }
    m_client->didReceiveResponseNetworkSession(WTFMove(response), WTFMove(completionHandler));
}

bool NetworkDataTask::shouldCaptureExtraNetworkLoadMetrics() const
{
    return m_client->shouldCaptureExtraNetworkLoadMetrics();
}

void NetworkDataTask::failureTimerFired()
{
    RefPtr<NetworkDataTask> protectedThis(this);

    switch (m_scheduledFailureType) {
    case BlockedFailure:
        m_scheduledFailureType = NoFailure;
        if (m_client)
            m_client->wasBlocked();
        return;
    case InvalidURLFailure:
        m_scheduledFailureType = NoFailure;
        if (m_client)
            m_client->cannotShowURL();
        return;
    case NoFailure:
        ASSERT_NOT_REACHED();
        break;
    }
    ASSERT_NOT_REACHED();
}

} // namespace WebKit

#endif // USE(NETWORK_SESSION)

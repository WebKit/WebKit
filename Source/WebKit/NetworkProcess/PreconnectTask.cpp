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

#include "Logging.h"
#include "NetworkLoad.h"
#include "NetworkLoadParameters.h"
#include "NetworkProcess.h"
#include "WebErrors.h"
#include <WebCore/ResourceError.h>

namespace WebKit {

using namespace WebCore;

PreconnectTask::PreconnectTask(NetworkSession& networkSession, NetworkLoadParameters&& parameters, CompletionHandler<void(const ResourceError&, const WebCore::NetworkLoadMetrics&)>&& completionHandler)
    : m_completionHandler(WTFMove(completionHandler))
    , m_timeout(60_s)
    , m_timeoutTimer([this] { didFinish(ResourceError { String(), 0, m_networkLoad->parameters().request.url(), "Preconnection timed out"_s, ResourceError::Type::Timeout }, { }); })
{
    RELEASE_LOG(Network, "%p - PreconnectTask::PreconnectTask()", this);

    ASSERT(parameters.shouldPreconnectOnly == PreconnectOnly::Yes);
    m_networkLoad = makeUnique<NetworkLoad>(*this, nullptr, WTFMove(parameters), networkSession);
}

void PreconnectTask::setH2PingCallback(const URL& url, CompletionHandler<void(Expected<WTF::Seconds, WebCore::ResourceError>&&)>&& completionHandler)
{
    m_networkLoad->setH2PingCallback(url, WTFMove(completionHandler));
}

void PreconnectTask::setTimeout(Seconds timeout)
{
    m_timeout = timeout;
}

void PreconnectTask::start()
{
    m_timeoutTimer.startOneShot(m_timeout);
    m_networkLoad->start();
}

PreconnectTask::~PreconnectTask() = default;

void PreconnectTask::willSendRedirectedRequest(ResourceRequest&&, ResourceRequest&& redirectRequest, ResourceResponse&& redirectResponse)
{
    // HSTS "redirection" may happen here.
#if ASSERT_ENABLED
    auto url = redirectResponse.url();
    ASSERT(url.protocol() == "http"_s);
    url.setProtocol("https"_s);
    ASSERT(redirectRequest.url() == url);
#endif
    m_networkLoad->continueWillSendRequest(WTFMove(redirectRequest));
}

void PreconnectTask::didReceiveResponse(ResourceResponse&& response, PrivateRelayed, ResponseCompletionHandler&& completionHandler)
{
    ASSERT_NOT_REACHED();
    completionHandler(PolicyAction::Ignore);
}

void PreconnectTask::didReceiveBuffer(const FragmentedSharedBuffer&, uint64_t reportedEncodedDataLength)
{
    ASSERT_NOT_REACHED();
}

void PreconnectTask::didFinishLoading(const NetworkLoadMetrics& metrics)
{
    RELEASE_LOG(Network, "%p - PreconnectTask::didFinishLoading", this);
    didFinish({ }, metrics);
}

void PreconnectTask::didFailLoading(const ResourceError& error)
{
    RELEASE_LOG(Network, "%p - PreconnectTask::didFailLoading, error_code=%d", this, error.errorCode());
    didFinish(error, NetworkLoadMetrics::emptyMetrics());
}

void PreconnectTask::didSendData(uint64_t bytesSent, uint64_t totalBytesToBeSent)
{
    ASSERT_NOT_REACHED();
}

void PreconnectTask::didFinish(const ResourceError& error, const NetworkLoadMetrics& metrics)
{
    if (m_completionHandler)
        m_completionHandler(error, metrics);
    delete this;
}

} // namespace WebKit

#endif // ENABLE(SERVER_PRECONNECT)

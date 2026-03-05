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

Ref<PreconnectTask> PreconnectTask::create(NetworkSession& networkSession, NetworkLoadParameters&& parameters)
{
    return adoptRef(*new PreconnectTask(networkSession, WTF::move(parameters)));
}

PreconnectTask::PreconnectTask(NetworkSession& networkSession, NetworkLoadParameters&& parameters)
    : m_networkLoad(NetworkLoad::create(*this, WTF::move(parameters), networkSession))
{
    RELEASE_LOG(Network, "%p - PreconnectTask::PreconnectTask()", this);

    ASSERT(m_networkLoad->parameters().shouldPreconnectOnly == PreconnectOnly::Yes);
}

void PreconnectTask::setH2PingCallback(const URL& url, CompletionHandler<void(Expected<WTF::Seconds, WebCore::ResourceError>&&)>&& completionHandler)
{
    m_networkLoad->setH2PingCallback(url, WTF::move(completionHandler));
}

void PreconnectTask::start(CompletionHandler<void(const WebCore::ResourceError&, const WebCore::NetworkLoadMetrics&)>&& completionHandler, Seconds timeout)
{
    RELEASE_LOG(Network, "%p - PreconnectTask::start() timeout=%g", this, timeout.value());
    // Keep `this` alive until completion of the network load.
    m_completionHandler = [protectedThis = Ref { *this }, completionHandler = WTF::move(completionHandler)](const WebCore::ResourceError& error, const WebCore::NetworkLoadMetrics& metrics) mutable {
        if (completionHandler)
            completionHandler(error, metrics);
    };
    m_timeoutTimer = makeUnique<WebCore::Timer>(*this, &PreconnectTask::didTimeout);
    m_timeoutTimer->startOneShot(timeout);
    m_networkLoad->start();
}

PreconnectTask::~PreconnectTask()
{
    RELEASE_LOG(Network, "%p - PreconnectTask::~PreconnectTask()", this);
}

void PreconnectTask::willSendRedirectedRequest(ResourceRequest&&, ResourceRequest&& redirectRequest, ResourceResponse&& redirectResponse, CompletionHandler<void(WebCore::ResourceRequest&&)>&& completionHandler)
{
    // HSTS "redirection" may happen here.
#if ASSERT_ENABLED
    auto url = redirectResponse.url();
    ASSERT(url.protocol() == "http"_s);
    url.setProtocol("https"_s);
    ASSERT(redirectRequest.url() == url);
#endif
    completionHandler(WTF::move(redirectRequest));
}

void PreconnectTask::didReceiveResponse(ResourceResponse&& response, PrivateRelayed, ResponseCompletionHandler&& completionHandler)
{
    RELEASE_LOG_ERROR(Network, "%p - PreconnectTask::didReceiveResponse", this);
    ASSERT_NOT_REACHED();
    completionHandler(PolicyAction::Ignore);
}

void PreconnectTask::didReceiveBuffer(const FragmentedSharedBuffer&)
{
    RELEASE_LOG_ERROR(Network, "%p - PreconnectTask::didReceiveBuffer", this);
    ASSERT_NOT_REACHED();
}

void PreconnectTask::didFinishLoading(const NetworkLoadMetrics& metrics)
{
    RELEASE_LOG(Network, "%p - PreconnectTask::didFinishLoading", this);
    m_completionHandler({ }, metrics); // Deletes this.
}

void PreconnectTask::didFailLoading(const ResourceError& error)
{
    RELEASE_LOG(Network, "%p - PreconnectTask::didFailLoading, error_code=%d", this, error.errorCode());
    m_completionHandler(error, NetworkLoadMetrics::emptyMetrics()); // Deletes this.
}

void PreconnectTask::didTimeout()
{
    RELEASE_LOG_ERROR(Network, "%p - PreconnectTask::didTimeout", this);
    m_networkLoad->cancel();
    m_completionHandler(ResourceError { String(), 0, m_networkLoad->parameters().request.url(), "Preconnection timed out"_s, ResourceError::Type::Timeout }, { }); // Deletes this.
}

void PreconnectTask::didSendData(uint64_t bytesSent, uint64_t totalBytesToBeSent)
{
    ASSERT_NOT_REACHED();
}

} // namespace WebKit

#endif // ENABLE(SERVER_PRECONNECT)

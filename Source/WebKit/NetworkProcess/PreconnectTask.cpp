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

PreconnectTask::PreconnectTask(NetworkProcess& networkProcess, NetworkLoadParameters&& parameters, CompletionHandler<void(const ResourceError&)>&& completionHandler)
    : m_completionHandler(WTFMove(completionHandler))
    , m_timeoutTimer([this] { didFinish(ResourceError { String(), 0, m_networkLoad->parameters().request.url(), "Preconnection timed out"_s, ResourceError::Type::Timeout }); })
{
    RELEASE_LOG(Network, "%p - PreconnectTask::PreconnectTask()", this);

    auto* networkSession = networkProcess.networkSession(parameters.sessionID);
    if (!networkSession) {
        ASSERT_NOT_REACHED();
        m_completionHandler(internalError(parameters.request.url()));
        return;
    }

    ASSERT(parameters.shouldPreconnectOnly == PreconnectOnly::Yes);
    m_networkLoad = makeUnique<NetworkLoad>(*this, nullptr, WTFMove(parameters), *networkSession);

    m_timeoutTimer.startOneShot(60000_s);
}

PreconnectTask::~PreconnectTask()
{
}

void PreconnectTask::willSendRedirectedRequest(ResourceRequest&&, ResourceRequest&& redirectRequest, ResourceResponse&& redirectResponse)
{
    ASSERT_NOT_REACHED();
}

void PreconnectTask::didReceiveResponse(ResourceResponse&& response, ResponseCompletionHandler&& completionHandler)
{
    ASSERT_NOT_REACHED();
    completionHandler(PolicyAction::Ignore);
}

void PreconnectTask::didReceiveBuffer(Ref<SharedBuffer>&&, int reportedEncodedDataLength)
{
    ASSERT_NOT_REACHED();
}

void PreconnectTask::didFinishLoading(const NetworkLoadMetrics&)
{
    RELEASE_LOG(Network, "%p - PreconnectTask::didFinishLoading", this);
    didFinish({ });
}

void PreconnectTask::didFailLoading(const ResourceError& error)
{
    RELEASE_LOG(Network, "%p - PreconnectTask::didFailLoading, error_code: %d", this, error.errorCode());
    didFinish(error);
}

void PreconnectTask::didSendData(unsigned long long bytesSent, unsigned long long totalBytesToBeSent)
{
    ASSERT_NOT_REACHED();
}

void PreconnectTask::didFinish(const ResourceError& error)
{
    if (m_completionHandler)
        m_completionHandler(error);
    delete this;
}

} // namespace WebKit

#endif // ENABLE(SERVER_PRECONNECT)

/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#include "NetworkLoad.h"

#include "AuthenticationChallengeDisposition.h"
#include "AuthenticationManager.h"
#include "DownloadProxyMessages.h"
#include "NetworkProcess.h"
#include "NetworkSession.h"
#include "SessionTracker.h"
#include "WebCoreArgumentCoders.h"
#include "WebErrors.h"
#include <WebCore/NotImplemented.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/SharedBuffer.h>
#include <pal/SessionID.h>
#include <wtf/MainThread.h>
#include <wtf/Seconds.h>

#if PLATFORM(COCOA)
#include "NetworkDataTaskCocoa.h"
#include "NetworkSessionCocoa.h"
#endif

namespace WebKit {

using namespace WebCore;

struct NetworkLoad::Throttle {
    Throttle(NetworkLoad& load, Seconds delay, ResourceResponse&& response, ResponseCompletionHandler&& handler)
        : timer(load, &NetworkLoad::throttleDelayCompleted)
        , response(WTFMove(response))
        , responseCompletionHandler(WTFMove(handler))
    {
        timer.startOneShot(delay);
    }
    Timer timer;
    ResourceResponse response;
    ResponseCompletionHandler responseCompletionHandler;
};

NetworkLoad::NetworkLoad(NetworkLoadClient& client, NetworkLoadParameters&& parameters, NetworkSession& networkSession)
    : m_client(client)
    , m_parameters(WTFMove(parameters))
    , m_loadThrottleLatency(networkSession.loadThrottleLatency())
    , m_currentRequest(m_parameters.request)
{
    initialize(networkSession);
}

void NetworkLoad::initialize(NetworkSession& networkSession)
{
    m_task = NetworkDataTask::create(networkSession, *this, m_parameters);
    if (!m_parameters.defersLoading)
        m_task->resume();
}

NetworkLoad::~NetworkLoad()
{
    ASSERT(RunLoop::isMain());
    if (m_redirectCompletionHandler)
        m_redirectCompletionHandler({ });
    if (m_task)
        m_task->clearClient();
}

void NetworkLoad::setDefersLoading(bool defers)
{
    if (m_task) {
        if (defers)
            m_task->suspend();
        else
            m_task->resume();
    }
}

void NetworkLoad::cancel()
{
    if (m_task)
        m_task->cancel();
}

void NetworkLoad::continueWillSendRequest(WebCore::ResourceRequest&& newRequest)
{
#if PLATFORM(COCOA)
    m_currentRequest.updateFromDelegatePreservingOldProperties(newRequest.nsURLRequest(HTTPBodyUpdatePolicy::DoNotUpdateHTTPBody));
#else
    // FIXME: Implement ResourceRequest::updateFromDelegatePreservingOldProperties. See https://bugs.webkit.org/show_bug.cgi?id=126127.
    m_currentRequest.updateFromDelegatePreservingOldProperties(newRequest);
#endif

    auto redirectCompletionHandler = std::exchange(m_redirectCompletionHandler, nullptr);
    ASSERT(redirectCompletionHandler);
    if (m_currentRequest.isNull()) {
        NetworkLoadMetrics emptyMetrics;
        didCompleteWithError(cancelledError(m_currentRequest), emptyMetrics);
        if (redirectCompletionHandler)
            redirectCompletionHandler({ });
        return;
    }

    if (redirectCompletionHandler)
        redirectCompletionHandler(ResourceRequest(m_currentRequest));
}

bool NetworkLoad::shouldCaptureExtraNetworkLoadMetrics() const
{
    return m_client.get().shouldCaptureExtraNetworkLoadMetrics();
}

bool NetworkLoad::isAllowedToAskUserForCredentials() const
{
    return m_client.get().isAllowedToAskUserForCredentials();
}

void NetworkLoad::convertTaskToDownload(PendingDownload& pendingDownload, const ResourceRequest& updatedRequest, const ResourceResponse& response, ResponseCompletionHandler&& completionHandler)
{
    if (!m_task)
        return completionHandler(PolicyAction::Ignore);

    m_client = pendingDownload;
    m_currentRequest = updatedRequest;
    m_task->setPendingDownload(pendingDownload);
    
    NetworkProcess::singleton().findPendingDownloadLocation(*m_task.get(), WTFMove(completionHandler), response);
}

void NetworkLoad::setPendingDownloadID(DownloadID downloadID)
{
    if (!m_task)
        return;

    m_task->setPendingDownloadID(downloadID);
}

void NetworkLoad::setSuggestedFilename(const String& suggestedName)
{
    if (!m_task)
        return;

    m_task->setSuggestedFilename(suggestedName);
}

void NetworkLoad::setPendingDownload(PendingDownload& pendingDownload)
{
    if (!m_task)
        return;

    m_task->setPendingDownload(pendingDownload);
}

void NetworkLoad::willPerformHTTPRedirection(ResourceResponse&& redirectResponse, ResourceRequest&& request, RedirectCompletionHandler&& completionHandler)
{
    ASSERT(!redirectResponse.isNull());
    ASSERT(RunLoop::isMain());
    ASSERT(!m_redirectCompletionHandler);

    redirectResponse.setSource(ResourceResponse::Source::Network);
    m_redirectCompletionHandler = WTFMove(completionHandler);

    auto oldRequest = WTFMove(m_currentRequest);
    request.setRequester(oldRequest.requester());

    m_currentRequest = request;
    m_client.get().willSendRedirectedRequest(WTFMove(oldRequest), WTFMove(request), WTFMove(redirectResponse));
}

void NetworkLoad::didReceiveChallenge(AuthenticationChallenge&& challenge, ChallengeCompletionHandler&& completionHandler)
{
    auto scheme = challenge.protectionSpace().authenticationScheme();
    bool isTLSHandshake = scheme == ProtectionSpaceAuthenticationSchemeServerTrustEvaluationRequested
        || scheme == ProtectionSpaceAuthenticationSchemeClientCertificateRequested;
    if (!isAllowedToAskUserForCredentials() && !isTLSHandshake) {
        m_client.get().didBlockAuthenticationChallenge();
        completionHandler(AuthenticationChallengeDisposition::UseCredential, { });
        return;
    }
    
    if (auto* pendingDownload = m_task->pendingDownload())
        NetworkProcess::singleton().authenticationManager().didReceiveAuthenticationChallenge(*pendingDownload, challenge, WTFMove(completionHandler));
    else
        NetworkProcess::singleton().authenticationManager().didReceiveAuthenticationChallenge(m_parameters.webPageID, m_parameters.webFrameID, challenge, WTFMove(completionHandler));
}

void NetworkLoad::didReceiveResponse(ResourceResponse&& response, ResponseCompletionHandler&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    ASSERT(!m_throttle);

    if (m_task && m_task->isDownload()) {
        NetworkProcess::singleton().findPendingDownloadLocation(*m_task.get(), WTFMove(completionHandler), response);
        return;
    }

    if (m_loadThrottleLatency > 0_s) {
        m_throttle = std::make_unique<Throttle>(*this, m_loadThrottleLatency, WTFMove(response), WTFMove(completionHandler));
        return;
    }

    notifyDidReceiveResponse(WTFMove(response), WTFMove(completionHandler));
}

void NetworkLoad::notifyDidReceiveResponse(ResourceResponse&& response, ResponseCompletionHandler&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    response.setSource(ResourceResponse::Source::Network);
    if (m_parameters.needsCertificateInfo)
        response.includeCertificateInfo();

    m_client.get().didReceiveResponse(WTFMove(response), WTFMove(completionHandler));
}

void NetworkLoad::didReceiveData(Ref<SharedBuffer>&& buffer)
{
    ASSERT(!m_throttle);

    // FIXME: This should be the encoded data length, not the decoded data length.
    auto size = buffer->size();
    m_client.get().didReceiveBuffer(WTFMove(buffer), size);
}

void NetworkLoad::didCompleteWithError(const ResourceError& error, const WebCore::NetworkLoadMetrics& networkLoadMetrics)
{
    ASSERT(!m_throttle);

    if (error.isNull())
        m_client.get().didFinishLoading(networkLoadMetrics);
    else
        m_client.get().didFailLoading(error);
}

void NetworkLoad::throttleDelayCompleted()
{
    ASSERT(m_throttle);

    auto throttle = WTFMove(m_throttle);

    notifyDidReceiveResponse(WTFMove(throttle->response), WTFMove(throttle->responseCompletionHandler));
}

void NetworkLoad::didSendData(uint64_t totalBytesSent, uint64_t totalBytesExpectedToSend)
{
    m_client.get().didSendData(totalBytesSent, totalBytesExpectedToSend);
}

void NetworkLoad::wasBlocked()
{
    m_client.get().didFailLoading(blockedError(m_currentRequest));
}

void NetworkLoad::cannotShowURL()
{
    m_client.get().didFailLoading(cannotShowURLError(m_currentRequest));
}


String NetworkLoad::description() const
{
    if (m_task.get())
        return m_task->description();
    return emptyString();
}

} // namespace WebKit

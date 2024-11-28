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
#include "MessageSenderInlines.h"
#include "NetworkDataTaskBlob.h"
#include "NetworkLoadClient.h"
#include "NetworkLoadScheduler.h"
#include "NetworkProcess.h"
#include "NetworkProcessProxyMessages.h"
#include "NetworkSession.h"
#include "WebCoreArgumentCoders.h"
#include "WebErrors.h"
#include <WebCore/ResourceRequest.h>
#include <WebCore/SharedBuffer.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Seconds.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(NetworkLoad);

NetworkLoad::NetworkLoad(NetworkLoadClient& client, NetworkLoadParameters&& parameters, NetworkSession& networkSession)
    : m_client(client)
    , m_networkProcess(networkSession.networkProcess())
    , m_parameters(WTFMove(parameters))
    , m_currentRequest(m_parameters.request)
{
    if (m_parameters.request.url().protocolIsBlob())
        m_task = NetworkDataTaskBlob::create(networkSession, *this, m_parameters.request, m_parameters.blobFileReferences, m_parameters.topOrigin);
    else
        m_task = NetworkDataTask::create(networkSession, *this, m_parameters);
}

NetworkLoad::NetworkLoad(NetworkLoadClient& client, NetworkSession& networkSession, const Function<RefPtr<NetworkDataTask>(NetworkDataTaskClient&)>& createTask)
    : m_client(client)
    , m_networkProcess(networkSession.networkProcess())
    , m_task(createTask(*this))
{
}

std::optional<WebCore::FrameIdentifier> NetworkLoad::webFrameID() const
{
    if (parameters().webFrameID)
        return parameters().webFrameID;
    return std::nullopt;
}

std::optional<WebCore::PageIdentifier> NetworkLoad::webPageID() const
{
    if (parameters().webPageID)
        return parameters().webPageID;
    return std::nullopt;
}

Ref<NetworkProcess> NetworkLoad::networkProcess()
{
    return m_networkProcess;
}

void NetworkLoad::start()
{
    if (!m_task)
        return;
    m_task->resume();
}

void NetworkLoad::startWithScheduling()
{
    if (!m_task || !m_task->networkSession())
        return;
    m_scheduler = m_task->networkSession()->networkLoadScheduler();
    m_scheduler->schedule(*this);
}

NetworkLoad::~NetworkLoad()
{
    ASSERT(RunLoop::isMain());
    if (m_scheduler)
        m_scheduler->unschedule(*this);
    if (m_task)
        m_task->clearClient();
}

void NetworkLoad::cancel()
{
    if (m_task)
        m_task->cancel();
}

static inline void updateRequest(ResourceRequest& currentRequest, const ResourceRequest& newRequest)
{
#if PLATFORM(COCOA)
    currentRequest.updateFromDelegatePreservingOldProperties(newRequest.nsURLRequest(HTTPBodyUpdatePolicy::DoNotUpdateHTTPBody));
#else
    currentRequest.updateFromDelegatePreservingOldProperties(newRequest);
#endif
}

void NetworkLoad::updateRequestAfterRedirection(WebCore::ResourceRequest& newRequest) const
{
    ResourceRequest updatedRequest = m_currentRequest;
    updateRequest(updatedRequest, newRequest);
    newRequest = WTFMove(updatedRequest);
}

void NetworkLoad::reprioritizeRequest(ResourceLoadPriority priority)
{
    m_currentRequest.setPriority(priority);
    if (m_task)
        m_task->setPriority(priority);
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
    
    m_networkProcess->findPendingDownloadLocation(*m_task.get(), WTFMove(completionHandler), response);
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

    if (!m_networkProcess->ftpEnabled() && request.url().protocolIsInFTPFamily()) {
        m_task->clearClient();
        m_task = nullptr;
        WebCore::NetworkLoadMetrics emptyMetrics;
        didCompleteWithError(ResourceError { errorDomainWebKitInternal, 0, url(), "FTP URLs are disabled"_s, ResourceError::Type::AccessControl }, emptyMetrics);

        if (completionHandler)
            completionHandler({ });
        return;
    }

    redirectResponse.setSource(ResourceResponse::Source::Network);

    auto oldRequest = WTFMove(m_currentRequest);
    request.setRequester(oldRequest.requester());

    m_currentRequest = request;
    m_client.get().willSendRedirectedRequest(WTFMove(oldRequest), WTFMove(request), WTFMove(redirectResponse), [this, weakThis = WeakPtr<NetworkDataTaskClient> { *this }, completionHandler = WTFMove(completionHandler)] (ResourceRequest&& newRequest) mutable {
        if (!weakThis)
            return completionHandler({ });
        updateRequest(m_currentRequest, newRequest);
        if (m_currentRequest.isNull()) {
            NetworkLoadMetrics emptyMetrics;
            didCompleteWithError(cancelledError(m_currentRequest), emptyMetrics);
            completionHandler({ });
            return;
        }
        completionHandler(ResourceRequest(m_currentRequest));
    });
}

void NetworkLoad::didReceiveChallenge(AuthenticationChallenge&& challenge, NegotiatedLegacyTLS negotiatedLegacyTLS, ChallengeCompletionHandler&& completionHandler)
{
    m_client.get().didReceiveChallenge(challenge);

    auto scheme = challenge.protectionSpace().authenticationScheme();
    bool isTLSHandshake = scheme == ProtectionSpace::AuthenticationScheme::ServerTrustEvaluationRequested
        || scheme == ProtectionSpace::AuthenticationScheme::ClientCertificateRequested;
    if (!isAllowedToAskUserForCredentials() && !isTLSHandshake && !challenge.protectionSpace().isProxy()) {
        m_client.get().didBlockAuthenticationChallenge();
        completionHandler(AuthenticationChallengeDisposition::UseCredential, { });
        return;
    }
    
    if (auto* pendingDownload = m_task->pendingDownload())
        m_networkProcess->protectedAuthenticationManager()->didReceiveAuthenticationChallenge(*pendingDownload, challenge, WTFMove(completionHandler));
    else
        m_networkProcess->protectedAuthenticationManager()->didReceiveAuthenticationChallenge(m_task->sessionID(), m_parameters.webPageProxyID, m_parameters.topOrigin ? &m_parameters.topOrigin->data() : nullptr, challenge, negotiatedLegacyTLS, WTFMove(completionHandler));
}

void NetworkLoad::didReceiveInformationalResponse(ResourceResponse&& response)
{
    m_client.get().didReceiveInformationalResponse(WTFMove(response));
}

void NetworkLoad::didReceiveResponse(ResourceResponse&& response, NegotiatedLegacyTLS negotiatedLegacyTLS, PrivateRelayed privateRelayed, ResponseCompletionHandler&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    if (m_task && m_task->isDownload()) {
        m_networkProcess->findPendingDownloadLocation(*m_task.get(), WTFMove(completionHandler), response);
        return;
    }

    if (negotiatedLegacyTLS == NegotiatedLegacyTLS::Yes)
        m_networkProcess->protectedAuthenticationManager()->negotiatedLegacyTLS(*m_parameters.webPageProxyID);
    
    notifyDidReceiveResponse(WTFMove(response), negotiatedLegacyTLS, privateRelayed, WTFMove(completionHandler));
}

void NetworkLoad::notifyDidReceiveResponse(ResourceResponse&& response, NegotiatedLegacyTLS, PrivateRelayed privateRelayed, ResponseCompletionHandler&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    if (m_parameters.needsCertificateInfo) {
        std::span<const std::byte> auditToken;

#if PLATFORM(COCOA)
        auto token = m_networkProcess->sourceApplicationAuditToken();
        if (token)
            auditToken = std::as_bytes(std::span<unsigned> { token->val });
#endif

        response.includeCertificateInfo(auditToken);
    }

    m_client.get().didReceiveResponse(WTFMove(response), privateRelayed, WTFMove(completionHandler));
}

void NetworkLoad::didReceiveData(const WebCore::SharedBuffer& buffer)
{
    // FIXME: This should be the encoded data length, not the decoded data length.
    m_client.get().didReceiveBuffer(buffer, buffer.size());
}

void NetworkLoad::didCompleteWithError(const ResourceError& error, const WebCore::NetworkLoadMetrics& networkLoadMetrics)
{
    if (m_scheduler) {
        m_scheduler->unschedule(*this, &networkLoadMetrics);
        m_scheduler = nullptr;
    }

    if (error.isNull())
        m_client.get().didFinishLoading(networkLoadMetrics);
    else
        m_client.get().didFailLoading(error);
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

void NetworkLoad::wasBlockedByRestrictions()
{
    m_client.get().didFailLoading(wasBlockedByRestrictionsError(m_currentRequest));
}

void NetworkLoad::wasBlockedByDisabledFTP()
{
    m_client.get().didFailLoading(ftpDisabledError(m_currentRequest));
}

void NetworkLoad::didNegotiateModernTLS(const URL& url)
{
    if (m_parameters.webPageProxyID)
        m_networkProcess->send(Messages::NetworkProcessProxy::DidNegotiateModernTLS(*m_parameters.webPageProxyID, url));
}

String NetworkLoad::description() const
{
    if (m_task.get())
        return m_task->description();
    return emptyString();
}

void NetworkLoad::setH2PingCallback(const URL& url, CompletionHandler<void(Expected<WTF::Seconds, WebCore::ResourceError>&&)>&& completionHandler)
{
    if (m_task)
        m_task->setH2PingCallback(url, WTFMove(completionHandler));
    else
        completionHandler(makeUnexpected(internalError(url)));
}

void NetworkLoad::setTimingAllowFailedFlag()
{
    if (m_task)
        m_task->setTimingAllowFailedFlag();
}

String NetworkLoad::attributedBundleIdentifier(WebPageProxyIdentifier pageID)
{
    if (m_task)
        return m_task->attributedBundleIdentifier(pageID);
    return { };
}

RefPtr<NetworkDataTask> NetworkLoad::protectedTask()
{
    return m_task;
}

} // namespace WebKit

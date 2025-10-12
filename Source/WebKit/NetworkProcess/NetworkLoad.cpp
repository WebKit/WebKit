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
#include "WebErrors.h"
#include <WebCore/AuthenticationChallenge.h>
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
    relaxAdoptionRequirement();
    if (m_parameters.request.url().protocolIsBlob())
        m_task = NetworkDataTaskBlob::create(networkSession, *this, m_parameters.request, m_parameters.blobFileReferences, m_parameters.topOrigin);
    else
        m_task = NetworkDataTask::create(networkSession, *this, m_parameters);
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
    if (RefPtr task = m_task)
        task->resume();
}

void NetworkLoad::startWithScheduling()
{
    RefPtr task = m_task;
    if (!task || !task->networkSession())
        return;
    Ref scheduler = task->checkedNetworkSession()->networkLoadScheduler();
    m_scheduler = scheduler.get();
    scheduler->schedule(*this);
}

NetworkLoad::~NetworkLoad()
{
    ASSERT(RunLoop::isMain());
    if (RefPtr scheduler = m_scheduler.get())
        scheduler->unschedule(*this);
    if (RefPtr task = m_task)
        task->clearClient();
}

void NetworkLoad::cancel()
{
    if (RefPtr task = m_task)
        task->cancel();
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
    if (RefPtr task = m_task)
        task->setPriority(priority);
}

bool NetworkLoad::shouldCaptureExtraNetworkLoadMetrics() const
{
    CheckedPtr client = m_client.get();
    return client && client->shouldCaptureExtraNetworkLoadMetrics();
}

bool NetworkLoad::isAllowedToAskUserForCredentials() const
{
    CheckedPtr client = m_client.get();
    return client && client->isAllowedToAskUserForCredentials();
}

void NetworkLoad::convertTaskToDownload(PendingDownload& pendingDownload, const ResourceRequest& updatedRequest, const ResourceResponse& response, ResponseCompletionHandler&& completionHandler)
{
    RefPtr task = m_task;
    if (!task)
        return completionHandler(PolicyAction::Ignore);

    m_client = &pendingDownload;
    m_currentRequest = updatedRequest;
    task->setPendingDownload(pendingDownload);
    
    m_networkProcess->findPendingDownloadLocation(*task, WTFMove(completionHandler), response);
}

void NetworkLoad::setPendingDownloadID(DownloadID downloadID)
{
    if (RefPtr task = m_task)
        task->setPendingDownloadID(downloadID);
}

void NetworkLoad::setSuggestedFilename(const String& suggestedName)
{
    if (!m_task)
        return;

    m_task->setSuggestedFilename(suggestedName);
}

void NetworkLoad::setPendingDownload(PendingDownload& pendingDownload)
{
    if (RefPtr task = m_task)
        task->setPendingDownload(pendingDownload);
}

void NetworkLoad::willPerformHTTPRedirection(ResourceResponse&& redirectResponse, ResourceRequest&& request, RedirectCompletionHandler&& completionHandler)
{
    ASSERT(!redirectResponse.isNull());
    ASSERT(RunLoop::isMain());

    if (!m_networkProcess->ftpEnabled() && request.url().protocolIsInFTPFamily()) {
        Ref { *m_task }->clearClient();
        m_task = nullptr;
        WebCore::NetworkLoadMetrics emptyMetrics;
        didCompleteWithError(ResourceError { errorDomainWebKitInternal, 0, url(), "FTP URLs are disabled"_s, ResourceError::Type::AccessControl }, emptyMetrics);

        if (completionHandler)
            completionHandler({ });
        return;
    }

    RefPtr client = m_client.get();

    if (!client)
        return completionHandler({ });

    redirectResponse.setSource(ResourceResponse::Source::Network);

    auto oldRequest = WTFMove(m_currentRequest);
    request.setRequester(oldRequest.requester());

    m_currentRequest = request;
    client->willSendRedirectedRequest(WTFMove(oldRequest), WTFMove(request), WTFMove(redirectResponse), [weakThis = WeakPtr { *this }, completionHandler = WTFMove(completionHandler)] (ResourceRequest&& newRequest) mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return completionHandler({ });
        updateRequest(protectedThis->m_currentRequest, newRequest);
        if (protectedThis->m_currentRequest.isNull()) {
            NetworkLoadMetrics emptyMetrics;
            protectedThis->didCompleteWithError(cancelledError(protectedThis->m_currentRequest), emptyMetrics);
            completionHandler({ });
            return;
        }
        completionHandler(ResourceRequest(protectedThis->m_currentRequest));
    });
}

void NetworkLoad::didReceiveChallenge(AuthenticationChallenge&& challenge, NegotiatedLegacyTLS negotiatedLegacyTLS, ChallengeCompletionHandler&& completionHandler)
{
    RefPtr client = m_client.get();

    if (!client) {
        completionHandler(AuthenticationChallengeDisposition::Cancel, { });
        return;
    }

    client->didReceiveChallenge(challenge);

    auto scheme = challenge.protectionSpace().authenticationScheme();
    bool isTLSHandshake = scheme == ProtectionSpace::AuthenticationScheme::ServerTrustEvaluationRequested
        || scheme == ProtectionSpace::AuthenticationScheme::ClientCertificateRequested;
    if (!isAllowedToAskUserForCredentials() && !isTLSHandshake && !challenge.protectionSpace().isProxy()) {
        client->didBlockAuthenticationChallenge();
        completionHandler(AuthenticationChallengeDisposition::UseCredential, { });
        return;
    }
    
    if (RefPtr pendingDownload = m_task->pendingDownload())
        m_networkProcess->protectedAuthenticationManager()->didReceiveAuthenticationChallenge(*pendingDownload, challenge, WTFMove(completionHandler));
    else
        m_networkProcess->protectedAuthenticationManager()->didReceiveAuthenticationChallenge(m_task->sessionID(), m_parameters.webPageProxyID, m_parameters.topOrigin ? &m_parameters.topOrigin->data() : nullptr, challenge, negotiatedLegacyTLS, WTFMove(completionHandler));
}

void NetworkLoad::didReceiveInformationalResponse(ResourceResponse&& response)
{
    if (RefPtr client = m_client.get())
        client->didReceiveInformationalResponse(WTFMove(response));
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

    RefPtr client = m_client.get();

    if (!client)
        return completionHandler(WebCore::PolicyAction::Ignore);

    if (m_parameters.needsCertificateInfo) {
        std::span<const std::byte> auditToken;

#if PLATFORM(COCOA)
        auto token = m_networkProcess->sourceApplicationAuditToken();
        if (token)
            auditToken = std::as_bytes(std::span<unsigned> { token->val });
#endif

        response.includeCertificateInfo(auditToken);
    }

    client->didReceiveResponse(WTFMove(response), privateRelayed, WTFMove(completionHandler));
}

void NetworkLoad::didReceiveData(const WebCore::SharedBuffer& buffer)
{
    if (RefPtr client = m_client.get())
        client->didReceiveBuffer(buffer);
}

void NetworkLoad::didCompleteWithError(const ResourceError& error, const WebCore::NetworkLoadMetrics& networkLoadMetrics)
{
    if (RefPtr scheduler = std::exchange(m_scheduler, nullptr).get())
        scheduler->unschedule(*this, &networkLoadMetrics);

    RefPtr client = m_client.get();
    if (!client)
        return;

    if (error.isNull())
        client->didFinishLoading(networkLoadMetrics);
    else
        client->didFailLoading(error);
}

void NetworkLoad::didSendData(uint64_t totalBytesSent, uint64_t totalBytesExpectedToSend)
{
    if (RefPtr client = m_client.get())
        client->didSendData(totalBytesSent, totalBytesExpectedToSend);
}

void NetworkLoad::wasBlocked()
{
    if (RefPtr client = m_client.get())
        client->didFailLoading(blockedError(m_currentRequest));
}

void NetworkLoad::cannotShowURL()
{
    if (RefPtr client = m_client.get())
        client->didFailLoading(cannotShowURLError(m_currentRequest));
}

void NetworkLoad::wasBlockedByRestrictions()
{
    if (RefPtr client = m_client.get())
        client->didFailLoading(wasBlockedByRestrictionsError(m_currentRequest));
}

void NetworkLoad::wasBlockedByDisabledFTP()
{
    if (RefPtr client = m_client.get())
        client->didFailLoading(ftpDisabledError(m_currentRequest));
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
    if (RefPtr task = m_task)
        task->setH2PingCallback(url, WTFMove(completionHandler));
    else
        completionHandler(makeUnexpected(internalError(url)));
}

void NetworkLoad::setTimingAllowFailedFlag()
{
    if (RefPtr task = m_task)
        task->setTimingAllowFailedFlag();
}

String NetworkLoad::attributedBundleIdentifier(WebPageProxyIdentifier pageID)
{
    if (RefPtr task = m_task)
        return task->attributedBundleIdentifier(pageID);
    return { };
}

RefPtr<NetworkDataTask> NetworkLoad::protectedTask()
{
    return m_task;
}

size_t NetworkLoad::bytesTransferredOverNetwork() const
{
    if (RefPtr task = m_task)
        return task->bytesTransferredOverNetwork();
    return 0;
}

} // namespace WebKit

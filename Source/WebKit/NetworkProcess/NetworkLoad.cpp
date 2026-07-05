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
#include <WebCore/HTTPStatusCodes.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/SharedBuffer.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Seconds.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

using namespace WebCore;

#if ENABLE(INSPECTOR_NETWORK_THROTTLING)

// This is the delivery cadence for bandwidth throttling. Latency delays starting the request separately.
static constexpr Seconds bandwidthDeliveryInterval = 50_ms;

#endif // ENABLE(INSPECTOR_NETWORK_THROTTLING)

WTF_MAKE_TZONE_ALLOCATED_IMPL(NetworkLoad);

NetworkLoad::NetworkLoad(NetworkLoadClient& client, NetworkLoadParameters&& parameters, NetworkSession& networkSession)
    : m_client(client)
    , m_networkProcess(networkSession.networkProcess())
    , m_parameters(WTF::move(parameters))
    , m_currentRequest(m_parameters.request)
{
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
#if ENABLE(INSPECTOR_NETWORK_THROTTLING)
    if (auto latency = emulatedLatency(); latency > 0_s) {
        scheduleEmulatedConditionTimer(latency, EmulatedConditionState::WaitingForLatency);
        return;
    }
#endif

    if (RefPtr task = m_task)
        task->resume();
}

void NetworkLoad::startWithScheduling()
{
    RefPtr task = m_task;
    if (!task || !task->networkSession())
        return;
    Ref scheduler = protect(task->networkSession())->networkLoadScheduler();
    m_scheduler = scheduler.get();
    scheduler->schedule(*this);
}

NetworkLoad::~NetworkLoad()
{
    ASSERT(RunLoop::isMain());

#if ENABLE(INSPECTOR_NETWORK_THROTTLING)
    clearEmulatedConditions();
#endif

    if (RefPtr scheduler = m_scheduler.get())
        scheduler->unschedule(*this);
    if (auto* task = m_task.get())
        task->clearClient();
}

void NetworkLoad::cancel()
{
#if ENABLE(INSPECTOR_NETWORK_THROTTLING)
    clearEmulatedConditions();
#endif

    if (RefPtr task = m_task)
        task->cancel();
}

static inline void updateRequest(ResourceRequest& currentRequest, const ResourceRequest& newRequest)
{
#if PLATFORM(COCOA)
    currentRequest.updateFromDelegatePreservingOldProperties(RetainPtr { newRequest.nsURLRequest(HTTPBodyUpdatePolicy::DoNotUpdateHTTPBody) }.get());
#else
    currentRequest.updateFromDelegatePreservingOldProperties(newRequest);
#endif
}

void NetworkLoad::updateRequestAfterRedirection(WebCore::ResourceRequest& newRequest) const
{
    ResourceRequest updatedRequest = m_currentRequest;
    updateRequest(updatedRequest, newRequest);
    newRequest = WTF::move(updatedRequest);
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
    
    m_networkProcess->findPendingDownloadLocation(*task, WTF::move(completionHandler), response);
}

void NetworkLoad::setPendingDownloadID(DownloadID downloadID)
{
    if (auto* task = m_task.get())
        task->setPendingDownloadID(downloadID);
}

void NetworkLoad::setSuggestedFilename(const String& suggestedName)
{
    if (!m_task)
        return;

    protect(m_task)->setSuggestedFilename(suggestedName);
}

void NetworkLoad::setPendingDownload(PendingDownload& pendingDownload)
{
    if (auto* task = m_task.get())
        task->setPendingDownload(pendingDownload);
}

void NetworkLoad::willPerformHTTPRedirection(ResourceResponse&& redirectResponse, ResourceRequest&& request, RedirectCompletionHandler&& completionHandler)
{
    ASSERT(!redirectResponse.isNull());
    ASSERT(RunLoop::isMain());

    auto errorCallback = [&](ResourceError&& error) {
        m_task->clearClient();
        m_task = nullptr;
        WebCore::NetworkLoadMetrics emptyMetrics;
        didCompleteWithError(WTF::move(error), emptyMetrics);

        if (completionHandler)
            completionHandler({ });
    };

    if (!m_networkProcess->ftpEnabled() && request.url().protocolIsInFTPFamily()) {
        errorCallback({ errorDomainWebKitInternal, 0, url(), "FTP URLs are disabled"_s, ResourceError::Type::AccessControl });
        return;
    }

    if (redirectResponse.httpStatusCode() != httpStatus303SeeOther && protect(m_task)->hasPendingStreamBody()) {
        errorCallback({ errorDomainWebKitInternal, 0, url(), "Fetch upload streams cannot handle redirections other than 303"_s, ResourceError::Type::Cancellation });
        return;
    }

    RefPtr client = m_client.get();

    if (!client)
        return completionHandler({ });

    redirectResponse.setSource(ResourceResponse::Source::Network);

    auto oldRequest = WTF::move(m_currentRequest);
    request.setRequester(oldRequest.requester());

    m_currentRequest = request;
    client->willSendRedirectedRequest(WTF::move(oldRequest), WTF::move(request), WTF::move(redirectResponse), [weakThis = WeakPtr { *this }, completionHandler = WTF::move(completionHandler)] (ResourceRequest&& newRequest) mutable {
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
        protect(m_networkProcess->authenticationManager())->didReceiveAuthenticationChallenge(*pendingDownload, challenge, WTF::move(completionHandler));
    else
        protect(m_networkProcess->authenticationManager())->didReceiveAuthenticationChallenge(m_task->sessionID(), m_parameters.webPageProxyID, m_parameters.topOrigin ? &m_parameters.topOrigin->data() : nullptr, challenge, negotiatedLegacyTLS, WTF::move(completionHandler));
}

void NetworkLoad::didReceiveInformationalResponse(ResourceResponse&& response)
{
    if (RefPtr client = m_client.get())
        client->didReceiveInformationalResponse(WTF::move(response));
}

void NetworkLoad::didReceiveResponse(ResourceResponse&& response, NegotiatedLegacyTLS negotiatedLegacyTLS, PrivateRelayed privateRelayed, ResponseCompletionHandler&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    if (RefPtr task = m_task; task && task->isDownload()) {
        m_networkProcess->findPendingDownloadLocation(*task, WTF::move(completionHandler), response);
        return;
    }

    if (negotiatedLegacyTLS == NegotiatedLegacyTLS::Yes)
        protect(m_networkProcess->authenticationManager())->negotiatedLegacyTLS(*m_parameters.webPageProxyID);
    
    notifyDidReceiveResponse(WTF::move(response), negotiatedLegacyTLS, privateRelayed, WTF::move(completionHandler));
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

    client->didReceiveResponse(WTF::move(response), privateRelayed, WTF::move(completionHandler));
}

void NetworkLoad::didReceiveData(const WebCore::SharedBuffer& buffer)
{
#if ENABLE(INSPECTOR_NETWORK_THROTTLING)
    if (emulatedBandwidthBytesPerSecond().value_or(0) || !m_throttledData.isEmpty()) {
        if (!buffer.size())
            return;
        m_throttledData.append(protect(buffer));

        if (m_emulatedConditionState != EmulatedConditionState::Throttling) {
            m_throttledBudgetUpdateTime = MonotonicTime::now();
            m_throttledBandwidthBytesPerSecond = emulatedBandwidthBytesPerSecond().value_or(0);
            scheduleEmulatedConditionTimer(bandwidthDeliveryInterval, EmulatedConditionState::Throttling);
        }
        return;
    }
#endif
    if (RefPtr client = m_client.get())
        client->didReceiveBuffer(buffer);
}

void NetworkLoad::didCompleteWithError(const ResourceError& error, const WebCore::NetworkLoadMetrics& networkLoadMetrics)
{
    if (RefPtr scheduler = std::exchange(m_scheduler, nullptr).get())
        scheduler->unschedule(*this, &networkLoadMetrics);

    auto completion = [weakClient = m_client, error, networkLoadMetrics] {
        RefPtr client = weakClient.get();
        if (!client)
            return;

        if (error.isNull())
            client->didFinishLoading(networkLoadMetrics);
        else
            client->didFailLoading(error);
    };

#if ENABLE(INSPECTOR_NETWORK_THROTTLING)
    if (!m_throttledData.isEmpty()) {
        ASSERT(!m_throttledCompletion);
        m_throttledCompletion = [completion = WTF::move(completion)](ShouldNotifyClient shouldNotifyClient) {
            if (shouldNotifyClient == ShouldNotifyClient::Yes)
                completion();
        };
        return;
    }
#endif

    completion();
}

void NetworkLoad::didSendData(uint64_t totalBytesSent, uint64_t totalBytesExpectedToSend)
{
    if (RefPtr client = m_client.get())
        client->didSendData(totalBytesSent, totalBytesExpectedToSend);
}

#if ENABLE(INSPECTOR_NETWORK_THROTTLING)

std::optional<uint64_t> NetworkLoad::emulatedBandwidthBytesPerSecond() const
{
    if (RefPtr task = m_task) {
        if (auto* session = task->networkSession())
            return session->emulatedBandwidthBytesPerSecond();
    }
    return std::nullopt;
}

Seconds NetworkLoad::emulatedLatency() const
{
    if (RefPtr task = m_task) {
        if (auto* session = task->networkSession())
            return session->emulatedLatency();
    }
    return 0_s;
}

void NetworkLoad::scheduleEmulatedConditionTimer(Seconds delay, EmulatedConditionState state)
{
    if (!m_emulatedConditionTimer)
        m_emulatedConditionTimer = makeUnique<RunLoop::Timer>(RunLoop::mainSingleton(), "NetworkLoad::EmulatedConditionTimer"_s, this, &NetworkLoad::emulatedConditionTimerFired);
    m_emulatedConditionState = state;
    m_emulatedConditionTimer->startOneShot(delay);
}

void NetworkLoad::updateThrottledBudget()
{
    auto now = MonotonicTime::now();
    // Retain fractional bytes, and account for elapsed time at the old rate before applying a new one.
    m_throttledBudget += (now - m_throttledBudgetUpdateTime).seconds() * m_throttledBandwidthBytesPerSecond;
    m_throttledBudgetUpdateTime = now;
    m_throttledBandwidthBytesPerSecond = emulatedBandwidthBytesPerSecond().value_or(0);
}

void NetworkLoad::emulatedConditionTimerFired()
{
    Ref protectedThis { *this };
    RefPtr client = m_client.get();
    if (!client) {
        clearEmulatedConditions();
        return;
    }

    if (m_emulatedConditionState == EmulatedConditionState::WaitingForLatency) {
        m_emulatedConditionState = EmulatedConditionState::None;
        if (RefPtr task = m_task)
            task->resume();
        return;
    }

    if (m_emulatedConditionState != EmulatedConditionState::Throttling)
        return;

    updateThrottledBudget();

    while (!m_throttledData.isEmpty() && (!m_throttledBandwidthBytesPerSecond || m_throttledBudget >= 1)) {
        auto first = m_throttledData.first();
        size_t firstRemaining = first->size() - m_throttledDataFirstOffset;
        size_t bytesToDeliver = firstRemaining;
        if (m_throttledBandwidthBytesPerSecond && m_throttledBudget < bytesToDeliver)
            bytesToDeliver = static_cast<size_t>(m_throttledBudget);

        Ref chunk = WebCore::SharedBuffer::create(first->span().subspan(m_throttledDataFirstOffset, bytesToDeliver));

        if (m_throttledBandwidthBytesPerSecond)
            m_throttledBudget -= bytesToDeliver;

        m_throttledDataFirstOffset += bytesToDeliver;
        if (m_throttledDataFirstOffset >= first->size()) {
            m_throttledData.removeFirst();
            m_throttledDataFirstOffset = 0;
        }

        client->didReceiveBuffer(chunk);

        if (m_emulatedConditionState != EmulatedConditionState::Throttling)
            return;
        if (m_client.get() != client.get()) {
            clearEmulatedConditions();
            return;
        }
    }

    if (m_throttledData.isEmpty()) {
        m_emulatedConditionTimer->stop();
        m_emulatedConditionState = EmulatedConditionState::None;
        m_throttledBudget = 0;

        if (auto completion = std::exchange(m_throttledCompletion, { }))
            completion(ShouldNotifyClient::Yes);
        return;
    }

    scheduleEmulatedConditionTimer(bandwidthDeliveryInterval, EmulatedConditionState::Throttling);
}

void NetworkLoad::emulatedConditionsDidChange()
{
    if (m_emulatedConditionState == EmulatedConditionState::WaitingForLatency && emulatedLatency() <= 0_s) {
        scheduleEmulatedConditionTimer(0_s, EmulatedConditionState::WaitingForLatency);
        return;
    }

    if (m_emulatedConditionState == EmulatedConditionState::Throttling) {
        updateThrottledBudget();
        scheduleEmulatedConditionTimer(0_s, EmulatedConditionState::Throttling);
    }
}

void NetworkLoad::clearEmulatedConditions()
{
    if (m_emulatedConditionTimer)
        m_emulatedConditionTimer->stop();
    m_emulatedConditionState = EmulatedConditionState::None;

    m_throttledData.clear();
    m_throttledDataFirstOffset = 0;
    m_throttledBudget = 0;
    m_throttledBudgetUpdateTime = { };
    m_throttledBandwidthBytesPerSecond = 0;

    if (auto completion = std::exchange(m_throttledCompletion, { }))
        completion(ShouldNotifyClient::No);
}

#endif // ENABLE(INSPECTOR_NETWORK_THROTTLING)

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
    if (RefPtr task = m_task.get())
        return task->description();
    return emptyString();
}

void NetworkLoad::setH2PingCallback(const URL& url, CompletionHandler<void(std::expected<WTF::Seconds, WebCore::ResourceError>&&)>&& completionHandler)
{
    if (RefPtr task = m_task)
        task->setH2PingCallback(url, WTF::move(completionHandler));
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

size_t NetworkLoad::bytesTransferredOverNetwork() const
{
    if (auto* task = m_task.get())
        return task->bytesTransferredOverNetwork();
    return 0;
}

} // namespace WebKit

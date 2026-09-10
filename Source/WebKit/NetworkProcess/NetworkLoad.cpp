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
#include <wtf/CompletionHandler.h>
#include <wtf/Deque.h>
#include <wtf/MonotonicTime.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RunLoop.h>
#include <wtf/Seconds.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

using namespace WebCore;

#if ENABLE(INSPECTOR_NETWORK_THROTTLING)

static constexpr Seconds bandwidthDeliveryInterval = 50_ms;

class NetworkLoad::ConditionEmulator final : public CanMakeCheckedPtr<ConditionEmulator> {
    WTF_MAKE_NONCOPYABLE(ConditionEmulator);
    WTF_MAKE_TZONE_ALLOCATED(ConditionEmulator);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(ConditionEmulator);
public:
    static std::unique_ptr<ConditionEmulator> tryCreate(NetworkLoad& load)
    {
        auto [bandwidthBytesPerSecond, latency] = emulatedConditions(load);
        if (!bandwidthBytesPerSecond && latency <= 0_s) [[likely]]
            return nullptr;

        auto emulator = makeUnique<ConditionEmulator>(load);
        emulator->m_bandwidthBytesPerSecond = bandwidthBytesPerSecond.value_or(0);
        if (latency > 0_s) {
            emulator->m_state = State::WaitingForLatency;
            emulator->m_timer.startOneShot(latency);
        }
        return emulator;
    }

    ConditionEmulator(NetworkLoad& load)
        : m_load(load)
        , m_timer(RunLoop::mainSingleton(), "NetworkLoad::ConditionEmulator"_s, this, &NetworkLoad::ConditionEmulator::timerFired)
    {
    }

    ~ConditionEmulator()
    {
        if (m_timer.isActive())
            m_timer.stop();

        if (auto completion = std::exchange(m_completion, { }))
            completion(ShouldNotifyClient::No);
    }

    bool isWaitingForLatency() const { return m_state == State::WaitingForLatency; }

    bool didReceiveData(const SharedBuffer& buffer)
    {
        if (!m_bandwidthBytesPerSecond && m_data.isEmpty())
            return false;
        if (!buffer.size())
            return true;

        m_data.append(protect(buffer));
        if (m_state != State::Throttling) {
            m_budgetUpdateTime = MonotonicTime::now();
            m_state = State::Throttling;
            m_timer.startOneShot(bandwidthDeliveryInterval);
        }
        return true;
    }

    bool deferCompletion(const ResourceError& error, const NetworkLoadMetrics& networkLoadMetrics)
    {
        if (m_data.isEmpty())
            return false;

        ASSERT(!m_completion);
        m_completion = [weakClient = m_load->m_client, error, networkLoadMetrics](ShouldNotifyClient shouldNotifyClient) {
            if (shouldNotifyClient == ShouldNotifyClient::No)
                return;

            RefPtr client = weakClient.get();
            if (!client)
                return;

            if (error.isNull())
                client->didFinishLoading(networkLoadMetrics);
            else
                client->didFailLoading(error);
        };
        return true;
    }

    void conditionsDidChange()
    {
        auto [bandwidthBytesPerSecond, latency] = emulatedConditions(protect(m_load));

        switch (m_state) {
        case State::None:
            break;
        case State::WaitingForLatency:
            if (latency <= 0_s)
                m_timer.startOneShot(0_s);
            break;
        case State::Throttling:
            updateBudget();
            m_timer.startOneShot(0_s);
            break;
        }

        m_bandwidthBytesPerSecond = bandwidthBytesPerSecond.value_or(0);
    }

private:
    static std::pair<std::optional<uint64_t> /* bandwidthBytesPerSecond */, Seconds /* latency */> emulatedConditions(const NetworkLoad& load)
    {
        if (RefPtr task = load.m_task) {
            if (CheckedPtr session = task->networkSession())
                return session->emulatedConditions();
        }
        return { };
    }

    void updateBudget()
    {
        auto now = MonotonicTime::now();
        m_budget += (now - m_budgetUpdateTime).seconds() * m_bandwidthBytesPerSecond;
        m_budgetUpdateTime = now;
    }

    void timerFired()
    {
        RefPtr client = m_load->m_client;
        if (!client)
            return;

        if (m_state == State::WaitingForLatency) {
            m_state = State::None;
            if (RefPtr task = m_load->m_task)
                task->resume();
            return;
        }

        if (m_state != State::Throttling)
            return;

        updateBudget();

        while (!m_data.isEmpty() && (!m_bandwidthBytesPerSecond || m_budget >= 1)) {
            auto first = m_data.first();
            size_t firstRemaining = first->size() - m_dataFirstOffset;
            size_t bytesToDeliver = firstRemaining;
            if (m_bandwidthBytesPerSecond && m_budget < bytesToDeliver)
                bytesToDeliver = static_cast<size_t>(m_budget);

            Ref chunk = SharedBuffer::create(first->span().subspan(m_dataFirstOffset, bytesToDeliver));
            if (m_bandwidthBytesPerSecond)
                m_budget -= bytesToDeliver;

            m_dataFirstOffset += bytesToDeliver;
            if (m_dataFirstOffset >= first->size()) {
                m_data.removeFirst();
                m_dataFirstOffset = 0;
            }

            client->didReceiveBuffer(chunk);
        }

        if (m_data.isEmpty()) {
            m_state = State::None;
            m_budget = 0;
            if (auto completion = std::exchange(m_completion, { }))
                completion(ShouldNotifyClient::Yes);
            return;
        }

        m_state = State::Throttling;
        m_timer.startOneShot(bandwidthDeliveryInterval);
    }

    WeakRef<NetworkLoad> m_load;

    RunLoop::Timer m_timer;

    enum class State : uint8_t { None, WaitingForLatency, Throttling };
    State m_state { State::None };

    uint64_t m_bandwidthBytesPerSecond { 0 };

    Deque<Ref<const WebCore::SharedBuffer>> m_data;
    size_t m_dataFirstOffset { 0 };
    double m_budget { 0 };
    MonotonicTime m_budgetUpdateTime;

    enum class ShouldNotifyClient : bool { No, Yes };
    CompletionHandler<void(ShouldNotifyClient)> m_completion;
};

WTF_MAKE_TZONE_ALLOCATED_IMPL(NetworkLoad::ConditionEmulator);

#endif // ENABLE(INSPECTOR_NETWORK_THROTTLING)

WTF_MAKE_TZONE_ALLOCATED_IMPL(NetworkLoad);

NetworkLoad::NetworkLoad(NetworkLoadClient& client, NetworkLoadParameters&& parameters, NetworkSession& networkSession)
    : m_client(client)
    , m_parameters(WTF::move(parameters))
    , m_currentRequest(m_parameters.request)
{
    if (m_parameters.request.url().protocolIsBlob())
        m_task = NetworkDataTaskBlob::create(networkSession, *this, m_parameters.request, m_parameters.blobFileReferences, m_parameters.topOrigin);
    else
        m_task = NetworkDataTask::create(networkSession, *this, m_parameters);
}

NetworkLoad::NetworkLoad(NetworkLoadClient& client, NetworkSession& networkSession, NOESCAPE const CreateTaskCallback& createTask)
    : m_client(client)
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

NetworkProcess& NetworkLoad::networkProcess()
{
    return NetworkProcess::singleton();
}

void NetworkLoad::start()
{
#if ENABLE(INSPECTOR_NETWORK_THROTTLING)
    m_conditionEmulator = ConditionEmulator::tryCreate(*this);
    if (m_conditionEmulator && m_conditionEmulator->isWaitingForLatency()) [[unlikely]]
        return;
#endif // ENABLE(INSPECTOR_NETWORK_THROTTLING)

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

    if (RefPtr scheduler = m_scheduler.get())
        scheduler->unschedule(*this);
    if (auto* task = m_task.get())
        task->clearClient();
}

void NetworkLoad::cancel()
{
#if ENABLE(INSPECTOR_NETWORK_THROTTLING)
    m_conditionEmulator = nullptr;
#endif // ENABLE(INSPECTOR_NETWORK_THROTTLING)

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
    
    NetworkProcess::singleton().findPendingDownloadLocation(*task, WTF::move(completionHandler), response);
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

    if (!NetworkProcess::singleton().ftpEnabled() && request.url().protocolIsInFTPFamily()) {
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
        protect(NetworkProcess::singleton().authenticationManager())->didReceiveAuthenticationChallenge(*pendingDownload, challenge, WTF::move(completionHandler));
    else
        protect(NetworkProcess::singleton().authenticationManager())->didReceiveAuthenticationChallenge(m_task->sessionID(), m_parameters.webPageProxyID, m_parameters.topOrigin ? &m_parameters.topOrigin->data() : nullptr, challenge, negotiatedLegacyTLS, WTF::move(completionHandler));
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
        NetworkProcess::singleton().findPendingDownloadLocation(*task, WTF::move(completionHandler), response);
        return;
    }

    if (negotiatedLegacyTLS == NegotiatedLegacyTLS::Yes)
        protect(NetworkProcess::singleton().authenticationManager())->negotiatedLegacyTLS(*m_parameters.webPageProxyID);
    
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
        auto token = NetworkProcess::singleton().sourceApplicationAuditToken();
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
    if (m_conditionEmulator && protect(m_conditionEmulator)->didReceiveData(buffer)) [[unlikely]]
        return;
#endif // ENABLE(INSPECTOR_NETWORK_THROTTLING)

    if (RefPtr client = m_client.get())
        client->didReceiveBuffer(buffer);
}

void NetworkLoad::didCompleteWithError(const ResourceError& error, const WebCore::NetworkLoadMetrics& networkLoadMetrics)
{
    if (RefPtr scheduler = std::exchange(m_scheduler, nullptr).get())
        scheduler->unschedule(*this, &networkLoadMetrics);

#if ENABLE(INSPECTOR_NETWORK_THROTTLING)
    if (m_conditionEmulator && protect(m_conditionEmulator)->deferCompletion(error, networkLoadMetrics)) [[unlikely]]
        return;
#endif // ENABLE(INSPECTOR_NETWORK_THROTTLING)

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

#if ENABLE(INSPECTOR_NETWORK_THROTTLING)

void NetworkLoad::emulatedConditionsDidChange()
{
    if (CheckedPtr conditionEmulator = m_conditionEmulator.get())
        conditionEmulator->conditionsDidChange();
    else
        m_conditionEmulator = ConditionEmulator::tryCreate(*this);
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
        NetworkProcess::singleton().send(Messages::NetworkProcessProxy::DidNegotiateModernTLS(*m_parameters.webPageProxyID, url));
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

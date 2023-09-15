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

#include "AuthenticationManager.h"
#include "NetworkDataTaskBlob.h"
#include "NetworkDataTaskDataURL.h"
#include "NetworkLoadParameters.h"
#include "NetworkProcess.h"
#include "NetworkSession.h"
#include <WebCore/RegistrableDomain.h>
#include <WebCore/ResourceError.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/ResourceResponse.h>
#include <wtf/RunLoop.h>

#if PLATFORM(COCOA)
#include "NetworkDataTaskCocoa.h"
#endif
#if USE(SOUP)
#include "NetworkDataTaskSoup.h"
#endif
#if USE(CURL)
#include "NetworkDataTaskCurl.h"
#endif

namespace WebKit {
using namespace WebCore;

Ref<NetworkDataTask> NetworkDataTask::create(NetworkSession& session, NetworkDataTaskClient& client, const NetworkLoadParameters& parameters)
{
    ASSERT(!parameters.request.url().protocolIsBlob());
    auto dataTask = [&] {
#if PLATFORM(COCOA)
        return NetworkDataTaskCocoa::create(session, client, parameters);
#else
        if (parameters.request.url().protocolIsData())
            return NetworkDataTaskDataURL::create(session, client, parameters);
#if USE(SOUP)
        return NetworkDataTaskSoup::create(session, client, parameters);
#endif
#if USE(CURL)
        return NetworkDataTaskCurl::create(session, client, parameters);
#endif
#endif
    }();

#if ENABLE(INSPECTOR_NETWORK_THROTTLING)
    dataTask->setEmulatedConditions(session.bytesPerSecondLimit());
#endif

    return dataTask;
}

NetworkDataTask::NetworkDataTask(NetworkSession& session, NetworkDataTaskClient& client, const ResourceRequest& requestWithCredentials, StoredCredentialsPolicy storedCredentialsPolicy, bool shouldClearReferrerOnHTTPSToHTTPRedirect, bool dataTaskIsForMainFrameNavigation)
    : m_session(session)
    , m_client(&client)
    , m_partition(requestWithCredentials.cachePartition())
    , m_storedCredentialsPolicy(storedCredentialsPolicy)
    , m_lastHTTPMethod(requestWithCredentials.httpMethod())
    , m_firstRequest(requestWithCredentials)
    , m_shouldClearReferrerOnHTTPSToHTTPRedirect(shouldClearReferrerOnHTTPSToHTTPRedirect)
    , m_dataTaskIsForMainFrameNavigation(dataTaskIsForMainFrameNavigation)
{
    ASSERT(RunLoop::isMain());

    if (!requestWithCredentials.url().isValid()) {
        scheduleFailure(FailureType::InvalidURL);
        return;
    }

    if (!portAllowed(requestWithCredentials.url())) {
        scheduleFailure(FailureType::Blocked);
        return;
    }

    if (!session.networkProcess().ftpEnabled()
        && requestWithCredentials.url().protocolIsInFTPFamily()) {
        scheduleFailure(FailureType::FTPDisabled);
        return;
    }

    m_session->registerNetworkDataTask(*this);
}

NetworkDataTask::~NetworkDataTask()
{
    ASSERT(RunLoop::isMain());
    ASSERT(!m_client);

    if (m_session)
        m_session->unregisterNetworkDataTask(*this);
}

void NetworkDataTask::scheduleFailure(FailureType type)
{
    m_failureScheduled = true;
    RunLoop::main().dispatch([this, weakThis = ThreadSafeWeakPtr { *this }, type] {
        auto strongThis = weakThis.get();
        if (!strongThis || !m_client)
            return;

        switch (type) {
        case FailureType::Blocked:
            m_client->wasBlocked();
            return;
        case FailureType::InvalidURL:
            m_client->cannotShowURL();
            return;
        case FailureType::RestrictedURL:
            m_client->wasBlockedByRestrictions();
            return;
        case FailureType::FTPDisabled:
            m_client->wasBlockedByDisabledFTP();
        }
    });
}

void NetworkDataTask::didReceiveInformationalResponse(ResourceResponse&& headers)
{
    if (m_client)
        m_client->didReceiveInformationalResponse(WTFMove(headers));
}

void NetworkDataTask::didReceiveResponse(ResourceResponse&& response, NegotiatedLegacyTLS negotiatedLegacyTLS, PrivateRelayed privateRelayed, ResponseCompletionHandler&& completionHandler)
{
    if (response.isHTTP09()) {
        auto url = response.url();
        std::optional<uint16_t> port = url.port();
        if (port && !WTF::isDefaultPortForProtocol(port.value(), url.protocol())) {
            completionHandler(PolicyAction::Ignore);
            cancel();
            if (m_client)
                m_client->didCompleteWithError({ String(), 0, url, "Cancelled load from '" + url.stringCenterEllipsizedToLength() + "' because it is using HTTP/0.9." });
            return;
        }
    }

    response.setSource(ResourceResponse::Source::Network);
    if (negotiatedLegacyTLS == NegotiatedLegacyTLS::Yes)
        response.setUsedLegacyTLS(UsedLegacyTLS::Yes);
    if (privateRelayed == PrivateRelayed::Yes)
        response.setWasPrivateRelayed(WasPrivateRelayed::Yes);

    if (m_client)
        m_client->didReceiveResponse(WTFMove(response), negotiatedLegacyTLS, privateRelayed, WTFMove(completionHandler));
    else
        completionHandler(PolicyAction::Ignore);
}

bool NetworkDataTask::shouldCaptureExtraNetworkLoadMetrics() const
{
    return m_client ? m_client->shouldCaptureExtraNetworkLoadMetrics() : false;
}

String NetworkDataTask::description() const
{
    return emptyString();
}

void NetworkDataTask::setH2PingCallback(const URL& url, CompletionHandler<void(Expected<WTF::Seconds, WebCore::ResourceError>&&)>&& completionHandler)
{
    ASSERT_NOT_REACHED();
    completionHandler(makeUnexpected(internalError(url)));
}

PAL::SessionID NetworkDataTask::sessionID() const
{
    return m_session->sessionID();
}

const NetworkSession* NetworkDataTask::networkSession() const
{
    return m_session.get();
}

NetworkSession* NetworkDataTask::networkSession()
{
    return m_session.get();
}

void NetworkDataTask::restrictRequestReferrerToOriginIfNeeded(WebCore::ResourceRequest& request)
{
#if ENABLE(TRACKING_PREVENTION)
    if ((m_session->sessionID().isEphemeral() || m_session->isTrackingPreventionEnabled()) && m_session->shouldDowngradeReferrer() && request.isThirdParty())
        request.setExistingHTTPReferrerToOriginString();
#endif
}

String NetworkDataTask::attributedBundleIdentifier(WebPageProxyIdentifier pageID)
{
    if (auto* session = networkSession())
        return session->attributedBundleIdentifierFromPageIdentifier(pageID);
    return { };
}

void NetworkDataTask::setPendingDownload(PendingDownload& pendingDownload)
{
    ASSERT(!m_pendingDownload);
    m_pendingDownload = { pendingDownload };
}

PendingDownload* NetworkDataTask::pendingDownload() const
{
    return m_pendingDownload.get();
}

} // namespace WebKit

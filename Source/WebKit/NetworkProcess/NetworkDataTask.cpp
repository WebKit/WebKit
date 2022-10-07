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
#if PLATFORM(COCOA)
    return NetworkDataTaskCocoa::create(session, client, parameters);
#endif
#if USE(SOUP)
    return NetworkDataTaskSoup::create(session, client, parameters);
#endif
#if USE(CURL)
    return NetworkDataTaskCurl::create(session, client, parameters.request, parameters.webFrameID, parameters.webPageID, parameters.storedCredentialsPolicy, parameters.contentSniffingPolicy, parameters.contentEncodingSniffingPolicy, parameters.shouldClearReferrerOnHTTPSToHTTPRedirect, parameters.isMainFrameNavigation, parameters.shouldRelaxThirdPartyCookieBlocking);
#endif
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
}

NetworkDataTask::~NetworkDataTask()
{
    ASSERT(RunLoop::isMain());
    ASSERT(!m_client);
}

void NetworkDataTask::scheduleFailure(FailureType type)
{
    m_failureScheduled = true;
    RunLoop::main().dispatch([this, weakThis = WeakPtr { *this }, type] {
        if (!weakThis || !m_client)
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

NetworkSession* NetworkDataTask::networkSession()
{
    return m_session.get();
}

bool NetworkDataTask::isThirdPartyRequest(const WebCore::ResourceRequest& request) const
{
    return !WebCore::areRegistrableDomainsEqual(request.url(), request.firstPartyForCookies());
}

void NetworkDataTask::restrictRequestReferrerToOriginIfNeeded(WebCore::ResourceRequest& request)
{
#if ENABLE(TRACKING_PREVENTION)
    if ((m_session->sessionID().isEphemeral() || m_session->isResourceLoadStatisticsEnabled()) && m_session->shouldDowngradeReferrer() && isThirdPartyRequest(request))
        request.setExistingHTTPReferrerToOriginString();
#endif
}

String NetworkDataTask::attributedBundleIdentifier(WebPageProxyIdentifier pageID)
{
    if (auto* session = networkSession())
        return session->attributedBundleIdentifierFromPageIdentifier(pageID);
    return { };
}

} // namespace WebKit

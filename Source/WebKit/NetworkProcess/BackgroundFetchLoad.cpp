/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#include "BackgroundFetchLoad.h"

#include "AuthenticationChallengeDisposition.h"
#include "AuthenticationManager.h"
#include "Logging.h"
#include "NetworkConnectionToWebProcess.h"
#include "NetworkLoadChecker.h"
#include "NetworkProcess.h"
#include "WebErrors.h"
#include <WebCore/BackgroundFetchRequest.h>
#include <WebCore/ClientOrigin.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/MakeString.h>

#define BGLOAD_RELEASE_LOG(fmt, ...) RELEASE_LOG(Network, "%p - BackgroundFetchLoad::" fmt, this, ##__VA_ARGS__)

namespace WebKit {

using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(BackgroundFetchLoad);

BackgroundFetchLoad::BackgroundFetchLoad(NetworkProcess& networkProcess, PAL::SessionID sessionID, BackgroundFetchRecordLoaderClient& client, const BackgroundFetchRequest& request, size_t responseDataSize, const ClientOrigin& clientOrigin)
    : m_sessionID(WTFMove(sessionID))
    , m_client(client)
    , m_request(request.internalRequest)
    , m_networkLoadChecker(NetworkLoadChecker::create(networkProcess, nullptr, nullptr, FetchOptions { request.options }, m_sessionID, std::nullopt, HTTPHeaderMap { request.httpHeaders }, URL { m_request.url() }, URL { }, clientOrigin.clientOrigin.securityOrigin(), clientOrigin.topOrigin.securityOrigin(), RefPtr<SecurityOrigin> { }, PreflightPolicy::Consider, String { request.referrer }, true, OptionSet<AdvancedPrivacyProtections> { }))
{
    if (!m_request.url().protocolIsInHTTPFamily()) {
        didFinish(ResourceError { String { }, 0, m_request.url(), "URL is not HTTP(S)"_s, ResourceError::Type::Cancellation });
        return;
    }

    if (responseDataSize)
        m_request.setHTTPHeaderField(HTTPHeaderName::Range, makeString("bytes="_s, responseDataSize, '-'));

    m_networkLoadChecker->enableContentExtensionsCheck();
    if (request.cspResponseHeaders)
        m_networkLoadChecker->setCSPResponseHeaders(ContentSecurityPolicyResponseHeaders { *request.cspResponseHeaders });

    m_networkLoadChecker->check(ResourceRequest { m_request }, nullptr, [this, weakThis = WeakPtr { *this }, networkProcess = Ref { networkProcess }] (auto&& result) {
        if (!weakThis)
            return;
        WTF::switchOn(result, [this] (ResourceError& error) {
            this->didFinish(error);
        }, [] (NetworkLoadChecker::RedirectionTriplet& triplet) {
            // We should never send a synthetic redirect for BackgroundFetchLoads.
            ASSERT_NOT_REACHED();
        }, [&] (ResourceRequest& request) {
            this->loadRequest(networkProcess, WTFMove(request));
        });
    });
}

BackgroundFetchLoad::~BackgroundFetchLoad()
{
    abort();
}

void BackgroundFetchLoad::abort()
{
    RefPtr task = m_task;
    if (!task)
        return;

    ASSERT(task->client() == this);
    task->clearClient();
    task->cancel();
    m_task = nullptr;
}

void BackgroundFetchLoad::didFinish(const ResourceError& error, const ResourceResponse& response)
{
    m_client->didFinish(error);
}

void BackgroundFetchLoad::loadRequest(NetworkProcess& networkProcess, ResourceRequest&& request)
{
    BGLOAD_RELEASE_LOG("startNetworkLoad");
    auto* networkSession = networkProcess.networkSession(m_sessionID);
    ASSERT(networkSession);
    if (!networkSession)
        return;

    NetworkLoadParameters loadParameters;
    loadParameters.request = WTFMove(request);
    loadParameters.topOrigin = m_networkLoadChecker->topOrigin();
    loadParameters.sourceOrigin = m_networkLoadChecker->origin();
    loadParameters.storedCredentialsPolicy = m_networkLoadChecker->options().credentials == FetchOptions::Credentials::Include ? StoredCredentialsPolicy::Use : StoredCredentialsPolicy::DoNotUse;
    loadParameters.clientCredentialPolicy = ClientCredentialPolicy::CannotAskClientForCredentials;

    Ref task = NetworkDataTask::create(*networkSession, *this, WTFMove(loadParameters));
    m_task = task.ptr();
    task->resume();
}

void BackgroundFetchLoad::willPerformHTTPRedirection(ResourceResponse&& redirectResponse, ResourceRequest&& request, RedirectCompletionHandler&& completionHandler)
{
    m_networkLoadChecker->checkRedirection(ResourceRequest { }, WTFMove(request), WTFMove(redirectResponse), nullptr, [weakThis = WeakPtr { *this }, completionHandler = WTFMove(completionHandler)] (auto&& result) mutable {
        if (!result.has_value()) {
            weakThis->didFinish(result.error());
            completionHandler({ });
            return;
        }
        auto request = WTFMove(result->redirectRequest);
        if (!request.url().protocolIsInHTTPFamily()) {
            weakThis->didFinish(ResourceError { String { }, 0, request.url(), "Redirection to URL with a scheme that is not HTTP(S)"_s, ResourceError::Type::AccessControl });
            completionHandler({ });
            return;
        }

        completionHandler(WTFMove(request));
    });
}

void BackgroundFetchLoad::didReceiveChallenge(AuthenticationChallenge&& challenge, NegotiatedLegacyTLS negotiatedLegacyTLS, ChallengeCompletionHandler&& completionHandler)
{
    BGLOAD_RELEASE_LOG("didReceiveChallenge");
    if (challenge.protectionSpace().authenticationScheme() == ProtectionSpace::AuthenticationScheme::ServerTrustEvaluationRequested) {
        Ref { m_networkLoadChecker }->protectedNetworkProcess()->protectedAuthenticationManager()->didReceiveAuthenticationChallenge(m_sessionID, { }, nullptr, challenge, negotiatedLegacyTLS, WTFMove(completionHandler));
        return;
    }
    WeakPtr weakThis { *this };
    completionHandler(AuthenticationChallengeDisposition::Cancel, { });
    if (!weakThis)
        return;
    didFinish(ResourceError { String(), 0, currentURL(), "Failed HTTP authentication"_s, ResourceError::Type::AccessControl });
}

void BackgroundFetchLoad::didReceiveResponse(ResourceResponse&& response, NegotiatedLegacyTLS, PrivateRelayed, ResponseCompletionHandler&& completionHandler)
{
    BGLOAD_RELEASE_LOG("didReceiveResponse - httpStatusCode=%d", response.httpStatusCode());

    auto error = m_networkLoadChecker->validateResponse(m_request, response);
    if (!error.isNull()) {
        BGLOAD_RELEASE_LOG("didReceiveResponse: NetworkLoadChecker::validateResponse returned an error (error.domain=%" PUBLIC_LOG_STRING ", error.code=%d)", error.domain().utf8().data(), error.errorCode());

        WeakPtr weakThis { *this };
        completionHandler(PolicyAction::Ignore);
        if (!weakThis)
            return;

        didFinish(error);
        return;
    }

    WeakPtr weakThis { *this };
    completionHandler(PolicyAction::Use);
    if (!weakThis)
        return;

    m_client->didReceiveResponse(WTFMove(response));
}

void BackgroundFetchLoad::didReceiveData(const SharedBuffer& data)
{
    BGLOAD_RELEASE_LOG("didReceiveData");
    m_client->didReceiveResponseBodyChunk(data);
}

void BackgroundFetchLoad::didCompleteWithError(const ResourceError& error, const NetworkLoadMetrics&)
{
    if (error.isNull())
        BGLOAD_RELEASE_LOG("didComplete");
    else
        BGLOAD_RELEASE_LOG("didCompleteWithError, error_code=%d", error.errorCode());

    didFinish(error);
}

void BackgroundFetchLoad::didSendData(uint64_t totalBytesSent, uint64_t totalBytesExpectedToSend)
{
    m_client->didSendData(totalBytesSent);
}

void BackgroundFetchLoad::wasBlocked()
{
    BGLOAD_RELEASE_LOG("wasBlocked");
    didFinish(blockedError(ResourceRequest { currentURL() }));
}

void BackgroundFetchLoad::cannotShowURL()
{
    BGLOAD_RELEASE_LOG("cannotShowURL");
    didFinish(cannotShowURLError(ResourceRequest { currentURL() }));
}

void BackgroundFetchLoad::wasBlockedByRestrictions()
{
    BGLOAD_RELEASE_LOG("wasBlockedByRestrictions");
    didFinish(wasBlockedByRestrictionsError(ResourceRequest { currentURL() }));
}

void BackgroundFetchLoad::wasBlockedByDisabledFTP()
{
    BGLOAD_RELEASE_LOG("wasBlockedByDisabledFTP");
    didFinish(ftpDisabledError(ResourceRequest(currentURL())));
}

const URL& BackgroundFetchLoad::currentURL() const
{
    return m_networkLoadChecker->url();
}

} // namespace WebKit

#undef BGLOAD_RELEASE_LOG

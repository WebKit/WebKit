/*
 * Copyright (C) 2016-2025 Apple Inc. All rights reserved.
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
#include "PingLoad.h"

#include "AuthenticationChallengeDisposition.h"
#include "AuthenticationManager.h"
#include "Logging.h"
#include "NetworkConnectionToWebProcess.h"
#include "NetworkLoadChecker.h"
#include "NetworkProcess.h"
#include "NetworkSchemeRegistry.h"
#include "WebErrors.h"

#define PING_RELEASE_LOG(fmt, ...) RELEASE_LOG(Network, "%p - PingLoad::" fmt, this, ##__VA_ARGS__)

namespace WebKit {

using namespace WebCore;

PingLoad::PingLoad(NetworkProcess& networkProcess, PAL::SessionID sessionID, NetworkResourceLoadParameters&& parameters, CompletionHandler<void(const ResourceError&, const ResourceResponse&)>&& completionHandler)
    : m_sessionID(sessionID)
    , m_parameters(WTF::move(parameters))
    , m_completionHandler(WTF::move(completionHandler))
    , m_timeoutTimer(*this, &PingLoad::timeoutTimerFired)
    , m_networkLoadChecker(NetworkLoadChecker::create(networkProcess, nullptr, nullptr, FetchOptions { m_parameters.options }, m_sessionID, m_parameters.webPageProxyID, WTF::move(m_parameters.originalRequestHeaders), URL { m_parameters.request.url() }, URL { m_parameters.documentURL }, m_parameters.sourceOrigin.copyRef(), m_parameters.topOrigin.copyRef(), m_parameters.parentOrigin(), m_parameters.preflightPolicy, m_parameters.request.httpReferrer(), m_parameters.allowPrivacyProxy, m_parameters.advancedPrivacyProtections))
{
    initialize(networkProcess);
}

PingLoad::PingLoad(NetworkConnectionToWebProcess& connection, NetworkResourceLoadParameters&& parameters, CompletionHandler<void(const ResourceError&, const ResourceResponse&)>&& completionHandler)
    : m_sessionID(connection.sessionID())
    , m_parameters(WTF::move(parameters))
    , m_completionHandler(WTF::move(completionHandler))
    , m_timeoutTimer(*this, &PingLoad::timeoutTimerFired)
    , m_networkLoadChecker(NetworkLoadChecker::create(connection.networkProcess(), nullptr,  &connection.schemeRegistry(), FetchOptions { m_parameters.options }, m_sessionID, m_parameters.webPageProxyID, WTF::move(m_parameters.originalRequestHeaders), URL { m_parameters.request.url() }, URL { m_parameters.documentURL }, m_parameters.sourceOrigin.copyRef(), m_parameters.topOrigin.copyRef(), m_parameters.parentOrigin(), m_parameters.preflightPolicy, m_parameters.request.httpReferrer(), m_parameters.allowPrivacyProxy, m_parameters.advancedPrivacyProtections))
    , m_blobFiles(connection.resolveBlobReferences(m_parameters))
{
    for (auto& file : m_blobFiles)
        file->prepareForFileAccess();

    initialize(Ref { connection.networkProcess() });
}

void PingLoad::initialize(NetworkProcess& networkProcess)
{
    m_networkLoadChecker->enableContentExtensionsCheck();
    if (m_parameters.cspResponseHeaders)
        m_networkLoadChecker->setCSPResponseHeaders(WTF::move(m_parameters.cspResponseHeaders.value()));
    m_networkLoadChecker->setParentCrossOriginEmbedderPolicy(m_parameters.parentCrossOriginEmbedderPolicy);
    m_networkLoadChecker->setCrossOriginEmbedderPolicy(m_parameters.crossOriginEmbedderPolicy);
#if ENABLE(CONTENT_EXTENSIONS)
    m_networkLoadChecker->setContentExtensionController(WTF::move(m_parameters.mainDocumentURL), WTF::move(m_parameters.frameURL), m_parameters.userContentControllerIdentifier);
#endif

    // If the server never responds, this object will hang around forever.
    // Set a very generous timeout, just in case.
    m_timeoutTimer.startOneShot(60000_s);

    m_networkLoadChecker->check(ResourceRequest { m_parameters.request }, nullptr, [weakThis = WeakPtr { *this }, networkProcess = Ref { networkProcess }] (auto&& result) {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;
        WTF::switchOn(result,
            [&protectedThis] (ResourceError& error) {
                protectedThis->didFinish(error);
            },
            [] (NetworkLoadChecker::RedirectionTriplet& triplet) {
                // We should never send a synthetic redirect for PingLoads.
                ASSERT_NOT_REACHED();
            },
            [&protectedThis, &networkProcess] (ResourceRequest& request) {
                protectedThis->loadRequest(networkProcess, WTF::move(request));
            }
        );
    });
}

PingLoad::~PingLoad()
{
    if (RefPtr task = m_task) {
        ASSERT(task->client() == this);
        task->clearClient();
        task->cancel();
    }
    for (auto& file : m_blobFiles)
        file->revokeFileAccess();
}

void PingLoad::didFinish(const ResourceError& error, const ResourceResponse& response)
{
    m_completionHandler(error, response);

    m_selfReference = nullptr;
}

void PingLoad::loadRequest(NetworkProcess& networkProcess, ResourceRequest&& request)
{
    PING_RELEASE_LOG("startNetworkLoad");
    if (CheckedPtr networkSession = networkProcess.networkSession(m_sessionID)) {
        auto loadParameters = m_parameters.networkLoadParameters();
        loadParameters.request = WTF::move(request);
        Ref task = NetworkDataTask::create(*networkSession, *this, WTF::move(loadParameters));
        m_task = task.copyRef();
        task->resume();
    } else
        ASSERT_NOT_REACHED();
}

void PingLoad::willPerformHTTPRedirection(ResourceResponse&& redirectResponse, ResourceRequest&& request, RedirectCompletionHandler&& completionHandler)
{
    m_networkLoadChecker->checkRedirection(ResourceRequest { }, WTF::move(request), WTF::move(redirectResponse), nullptr, [protectedThis = Ref { *this }, completionHandler = WTF::move(completionHandler)] (auto&& result) mutable {
        if (!result.has_value()) {
            protectedThis->didFinish(result.error());
            completionHandler({ });
            return;
        }
        auto request = WTF::move(result->redirectRequest);
        if (!request.url().protocolIsInHTTPFamily()) {
            protectedThis->didFinish(ResourceError { String { }, 0, request.url(), "Redirection to URL with a scheme that is not HTTP(S)"_s, ResourceError::Type::AccessControl });
            completionHandler({ });
            return;
        }

        completionHandler(WTF::move(request));
    });
}

void PingLoad::didReceiveChallenge(AuthenticationChallenge&& challenge, NegotiatedLegacyTLS negotiatedLegacyTLS, ChallengeCompletionHandler&& completionHandler)
{
    PING_RELEASE_LOG("didReceiveChallenge");
    if (challenge.protectionSpace().authenticationScheme() == ProtectionSpace::AuthenticationScheme::ServerTrustEvaluationRequested) {
        protect(protect(m_networkLoadChecker->networkProcess())->authenticationManager())->didReceiveAuthenticationChallenge(m_sessionID, m_parameters.webPageProxyID,  m_parameters.topOrigin ? &m_parameters.topOrigin->data() : nullptr, challenge, negotiatedLegacyTLS, WTF::move(completionHandler));
        return;
    }
    WeakPtr weakThis { *this };
    completionHandler(AuthenticationChallengeDisposition::Cancel, { });
    if (!weakThis)
        return;
    didFinish(ResourceError { String(), 0, currentURL(), "Failed HTTP authentication"_s, ResourceError::Type::AccessControl });
}

void PingLoad::didReceiveResponse(ResourceResponse&& response, NegotiatedLegacyTLS, PrivateRelayed, ResponseCompletionHandler&& completionHandler)
{
    PING_RELEASE_LOG("didReceiveResponse - httpStatusCode=%d", response.httpStatusCode());
    WeakPtr weakThis { *this };
    completionHandler(PolicyAction::Ignore);
    if (!weakThis)
        return;
    didFinish({ }, response);
}

void PingLoad::didReceiveData(const SharedBuffer&)
{
    PING_RELEASE_LOG("didReceiveData");
    ASSERT_NOT_REACHED();
}

void PingLoad::didCompleteWithError(const ResourceError& error, const NetworkLoadMetrics&)
{
    if (error.isNull())
        PING_RELEASE_LOG("didComplete");
    else
        PING_RELEASE_LOG("didCompleteWithError, error_code=%d", error.errorCode());

    didFinish(error);
}

void PingLoad::didSendData(uint64_t totalBytesSent, uint64_t totalBytesExpectedToSend)
{
}

void PingLoad::wasBlocked()
{
    PING_RELEASE_LOG("wasBlocked");
    didFinish(blockedError(ResourceRequest { URL { currentURL() } }));
}

void PingLoad::cannotShowURL()
{
    PING_RELEASE_LOG("cannotShowURL");
    didFinish(cannotShowURLError(ResourceRequest { URL { currentURL() } }));
}

void PingLoad::wasBlockedByRestrictions()
{
    PING_RELEASE_LOG("wasBlockedByRestrictions");
    didFinish(wasBlockedByRestrictionsError(ResourceRequest { URL { currentURL() } }));
}

void PingLoad::wasBlockedByDisabledFTP()
{
    PING_RELEASE_LOG("wasBlockedByDisabledFTP");
    didFinish(ftpDisabledError(ResourceRequest(URL { currentURL() })));
}

void PingLoad::timeoutTimerFired()
{
    PING_RELEASE_LOG("timeoutTimerFired");
    didFinish(ResourceError { String(), 0, currentURL(), "Load timed out"_s, ResourceError::Type::Timeout });
}

const URL& PingLoad::currentURL() const
{
    return m_networkLoadChecker->url();
}

} // namespace WebKit

#undef PING_RELEASE_LOG

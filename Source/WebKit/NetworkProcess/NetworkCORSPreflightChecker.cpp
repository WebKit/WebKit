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
#include "NetworkCORSPreflightChecker.h"

#include "AuthenticationChallengeDisposition.h"
#include "AuthenticationManager.h"
#include "Logging.h"
#include "NetworkLoadParameters.h"
#include "NetworkProcess.h"
#include "NetworkResourceLoader.h"
#include "WebErrors.h"
#include <WebCore/CrossOriginAccessControl.h>
#include <WebCore/SecurityOrigin.h>

#define CORS_CHECKER_RELEASE_LOG(fmt, ...) RELEASE_LOG(Network, "%p - NetworkCORSPreflightChecker::" fmt, this, ##__VA_ARGS__)

namespace WebKit {

using namespace WebCore;

NetworkCORSPreflightChecker::NetworkCORSPreflightChecker(NetworkProcess& networkProcess, NetworkResourceLoader* networkResourceLoader, Parameters&& parameters, bool shouldCaptureExtraNetworkLoadMetrics, CompletionCallback&& completionCallback)
    : m_parameters(WTFMove(parameters))
    , m_networkProcess(networkProcess)
    , m_completionCallback(WTFMove(completionCallback))
    , m_shouldCaptureExtraNetworkLoadMetrics(shouldCaptureExtraNetworkLoadMetrics)
    , m_networkResourceLoader(networkResourceLoader)
{
}

NetworkCORSPreflightChecker::~NetworkCORSPreflightChecker()
{
    if (m_task) {
        ASSERT(m_task->client() == this);
        m_task->clearClient();
        m_task->cancel();
    }
    if (m_completionCallback)
        m_completionCallback(ResourceError { ResourceError::Type::Cancellation });
}

void NetworkCORSPreflightChecker::startPreflight()
{
    CORS_CHECKER_RELEASE_LOG("startPreflight");

    NetworkLoadParameters loadParameters;
    loadParameters.request = createAccessControlPreflightRequest(m_parameters.originalRequest, m_parameters.sourceOrigin, m_parameters.referrer);
    loadParameters.networkConnectionIntegrityPolicy = m_parameters.networkConnectionIntegrityPolicy;
    if (!m_parameters.userAgent.isNull())
        loadParameters.request.setHTTPHeaderField(HTTPHeaderName::UserAgent, m_parameters.userAgent);

    if (m_shouldCaptureExtraNetworkLoadMetrics)
        m_loadInformation = NetworkTransactionInformation { NetworkTransactionInformation::Type::Preflight, loadParameters.request, { }, { } };

    loadParameters.webPageProxyID = m_parameters.webPageProxyID;
    loadParameters.allowPrivacyProxy = m_parameters.allowPrivacyProxy;

    if (auto* networkSession = m_networkProcess->networkSession(m_parameters.sessionID)) {
        m_task = NetworkDataTask::create(*networkSession, *this, WTFMove(loadParameters));
        m_task->resume();
    } else
        ASSERT_NOT_REACHED();
}

void NetworkCORSPreflightChecker::willPerformHTTPRedirection(WebCore::ResourceResponse&& response, WebCore::ResourceRequest&&, RedirectCompletionHandler&& completionHandler)
{
    if (m_shouldCaptureExtraNetworkLoadMetrics)
        m_loadInformation.response = WTFMove(response);

    CORS_CHECKER_RELEASE_LOG("willPerformHTTPRedirection");
    completionHandler({ });
    m_completionCallback(ResourceError { errorDomainWebKitInternal, 0, m_parameters.originalRequest.url(), makeString("Preflight response is not successful. Status code: ", response.httpStatusCode()), ResourceError::Type::AccessControl });
}

void NetworkCORSPreflightChecker::didReceiveChallenge(WebCore::AuthenticationChallenge&& challenge, NegotiatedLegacyTLS negotiatedLegacyTLS, ChallengeCompletionHandler&& completionHandler)
{
    CORS_CHECKER_RELEASE_LOG("didReceiveChallenge, authentication scheme: %u", static_cast<unsigned>(challenge.protectionSpace().authenticationScheme()));

    auto scheme = challenge.protectionSpace().authenticationScheme();
    bool isTLSHandshake = scheme == ProtectionSpace::AuthenticationScheme::ServerTrustEvaluationRequested
        || scheme == ProtectionSpace::AuthenticationScheme::ClientCertificateRequested;

    if (!isTLSHandshake) {
        completionHandler(AuthenticationChallengeDisposition::UseCredential, { });
        return;
    }

    m_networkProcess->authenticationManager().didReceiveAuthenticationChallenge(m_parameters.sessionID, m_parameters.webPageProxyID, m_parameters.topOrigin ? &m_parameters.topOrigin->data() : nullptr, challenge, negotiatedLegacyTLS, WTFMove(completionHandler));
}

void NetworkCORSPreflightChecker::didReceiveResponse(WebCore::ResourceResponse&& response, NegotiatedLegacyTLS, PrivateRelayed, ResponseCompletionHandler&& completionHandler)
{
    CORS_CHECKER_RELEASE_LOG("didReceiveResponse");

    if (m_shouldCaptureExtraNetworkLoadMetrics)
        m_loadInformation.response = response;

    m_response = WTFMove(response);
    completionHandler(PolicyAction::Use);
}

void NetworkCORSPreflightChecker::didReceiveData(const WebCore::SharedBuffer&)
{
    CORS_CHECKER_RELEASE_LOG("didReceiveData");
}

void NetworkCORSPreflightChecker::didCompleteWithError(const WebCore::ResourceError& preflightError, const WebCore::NetworkLoadMetrics& metrics)
{
    if (m_shouldCaptureExtraNetworkLoadMetrics)
        m_loadInformation.metrics = metrics;

    if (!preflightError.isNull()) {
        CORS_CHECKER_RELEASE_LOG("didCompleteWithError");
        auto error = preflightError;
        if (error.isNull() || error.isGeneral())
            error.setType(ResourceError::Type::AccessControl);

        m_completionCallback(WTFMove(error));
        return;
    }

    CORS_CHECKER_RELEASE_LOG("didComplete http_status_code=%d", m_response.httpStatusCode());

    auto result = validatePreflightResponse(m_parameters.sessionID, m_parameters.originalRequest, m_response, m_parameters.storedCredentialsPolicy, m_parameters.sourceOrigin, m_networkResourceLoader.get());
    if (!result) {
        CORS_CHECKER_RELEASE_LOG("didComplete, AccessControl error: %s", result.error().utf8().data());
        m_completionCallback(ResourceError { errorDomainWebKitInternal, 0, m_parameters.originalRequest.url(), result.error(), ResourceError::Type::AccessControl });
        return;
    }
    m_completionCallback(ResourceError { });
}

void NetworkCORSPreflightChecker::didSendData(uint64_t totalBytesSent, uint64_t totalBytesExpectedToSend)
{
}

void NetworkCORSPreflightChecker::wasBlocked()
{
    CORS_CHECKER_RELEASE_LOG("wasBlocked");
    m_completionCallback(ResourceError { errorDomainWebKitInternal, 0, m_parameters.originalRequest.url(), "CORS-preflight request was blocked"_s, ResourceError::Type::AccessControl });
}

void NetworkCORSPreflightChecker::cannotShowURL()
{
    CORS_CHECKER_RELEASE_LOG("cannotShowURL");
    m_completionCallback(ResourceError { errorDomainWebKitInternal, 0, m_parameters.originalRequest.url(), "Preflight response was blocked"_s, ResourceError::Type::AccessControl });
}

void NetworkCORSPreflightChecker::wasBlockedByRestrictions()
{
    CORS_CHECKER_RELEASE_LOG("wasBlockedByRestrictions");
    m_completionCallback(ResourceError { errorDomainWebKitInternal, 0, m_parameters.originalRequest.url(), "Preflight response was blocked"_s, ResourceError::Type::AccessControl });
}

void NetworkCORSPreflightChecker::wasBlockedByDisabledFTP()
{
    m_completionCallback(ftpDisabledError(m_parameters.originalRequest));
}

NetworkTransactionInformation NetworkCORSPreflightChecker::takeInformation()
{
    ASSERT(m_shouldCaptureExtraNetworkLoadMetrics);
    return WTFMove(m_loadInformation);
}

} // Namespace WebKit

#undef CORS_CHECKER_RELEASE_LOG

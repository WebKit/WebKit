/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#include "NetworkLoadChecker.h"

#include "Logging.h"
#include "NetworkCORSPreflightChecker.h"
#include "NetworkProcess.h"
#include "WebCompiledContentRuleList.h"
#include "WebUserContentController.h"
#include <WebCore/ContentSecurityPolicy.h>
#include <WebCore/CrossOriginAccessControl.h>
#include <WebCore/CrossOriginPreflightResultCache.h>
#include <WebCore/HTTPParsers.h>

#define RELEASE_LOG_IF_ALLOWED(fmt, ...) RELEASE_LOG_IF(m_sessionID.isAlwaysOnLoggingAllowed(), Network, "%p - NetworkLoadChecker::" fmt, this, ##__VA_ARGS__)

namespace WebKit {

using namespace WebCore;

NetworkLoadChecker::NetworkLoadChecker(WebCore::FetchOptions&& options, PAL::SessionID sessionID, WebCore::HTTPHeaderMap&& originalRequestHeaders, URL&& url, RefPtr<SecurityOrigin>&& sourceOrigin, PreflightPolicy preflightPolicy)
    : m_options(WTFMove(options))
    , m_sessionID(sessionID)
    , m_originalRequestHeaders(WTFMove(originalRequestHeaders))
    , m_url(WTFMove(url))
    , m_origin(WTFMove(sourceOrigin))
    , m_preflightPolicy(preflightPolicy)
{
    if (m_options.mode == FetchOptions::Mode::Cors || m_options.mode == FetchOptions::Mode::SameOrigin)
        m_isSameOriginRequest = m_url.protocolIsData() || m_url.protocolIsBlob() || m_origin->canRequest(m_url);
    switch (options.credentials) {
    case FetchOptions::Credentials::Include:
        m_storedCredentialsPolicy = StoredCredentialsPolicy::Use;
        break;
    case FetchOptions::Credentials::SameOrigin:
        m_storedCredentialsPolicy = m_isSameOriginRequest ? StoredCredentialsPolicy::Use : StoredCredentialsPolicy::DoNotUse;
        break;
    case FetchOptions::Credentials::Omit:
        m_storedCredentialsPolicy = StoredCredentialsPolicy::DoNotUse;
        break;
    }
}

NetworkLoadChecker::~NetworkLoadChecker() = default;

void NetworkLoadChecker::check(ResourceRequest&& request, ValidationHandler&& handler)
{
    ASSERT(!isChecking());

    m_firstRequestHeaders = request.httpHeaderFields();
    // FIXME: We should not get this information from the request but directly from some NetworkProcess setting.
    m_dntHeaderValue = m_firstRequestHeaders.get(HTTPHeaderName::DNT);
    if (m_dntHeaderValue.isNull() && m_sessionID.isEphemeral()) {
        m_dntHeaderValue = "1";
        request.setHTTPHeaderField(HTTPHeaderName::DNT, m_dntHeaderValue);
    }
    checkRequest(WTFMove(request), WTFMove(handler));
}

void NetworkLoadChecker::prepareRedirectedRequest(ResourceRequest& request)
{
    if (!m_dntHeaderValue.isNull())
        request.setHTTPHeaderField(HTTPHeaderName::DNT, m_dntHeaderValue);
}

void NetworkLoadChecker::checkRedirection(WebCore::ResourceResponse& redirectResponse, ResourceRequest&& request, ValidationHandler&& handler)
{
    ASSERT(!isChecking());

    auto error = validateResponse(redirectResponse);
    if (!error.isNull()) {
        handler(makeUnexpected(WTFMove(error)));
        return;
    }

    m_previousURL = WTFMove(m_url);
    m_url = request.url();

    if (m_options.redirect != FetchOptions::Redirect::Follow) {
        handler(returnError(ASCIILiteral("Load parameters do not allow following redirections")));
        return;
    }

    if (++m_redirectCount > 20) {
        handler(returnError(ASCIILiteral("Load cannot follow more than 20 redirections")));
        return;
    }

    if (!m_url.protocolIsInHTTPFamily()) {
        handler(returnError(ASCIILiteral("Redirection to URL with a scheme that is not HTTP(S)")));
        return;
    }

    checkRequest(WTFMove(request), WTFMove(handler));
}

ResourceError NetworkLoadChecker::validateResponse(ResourceResponse& response)
{
    if (m_redirectCount)
        response.setRedirected(true);

    if (m_isSameOriginRequest) {
        response.setTainting(ResourceResponse::Tainting::Basic);
        return { };
    }

    if (m_options.mode == FetchOptions::Mode::NoCors) {
        response.setTainting(ResourceResponse::Tainting::Opaque);
        return { };
    }

    ASSERT(m_options.mode == FetchOptions::Mode::Cors);

    String errorMessage;
    if (!WebCore::passesAccessControlCheck(response, m_storedCredentialsPolicy, *m_origin, errorMessage))
        return ResourceError { errorDomainWebKitInternal, 0, m_url, WTFMove(errorMessage), ResourceError::Type::AccessControl };

    response.setTainting(ResourceResponse::Tainting::Cors);
    return { };
}

NetworkLoadChecker::RequestOrError NetworkLoadChecker::returnError(String&& error)
{
    return makeUnexpected(ResourceError { String { }, 0, m_url, WTFMove(error), ResourceError::Type::AccessControl });
}

void NetworkLoadChecker::checkRequest(ResourceRequest&& request, ValidationHandler&& handler)
{
#if ENABLE(CONTENT_EXTENSIONS)
    processContentExtensionRulesForLoad(WTFMove(request), [this, handler = WTFMove(handler)](auto&& request, auto status) mutable {
        if (status.blockedLoad) {
            handler(this->returnError(ASCIILiteral("Blocked by content extension")));
            return;
        }
        this->continueCheckingRequest(WTFMove(request), WTFMove(handler));
    });
#else
    continueCheckingRequest(WTFMove(request), WTFMove(handler));
#endif
}

void NetworkLoadChecker::continueCheckingRequest(ResourceRequest&& request, ValidationHandler&& handler)
{
    if (auto* contentSecurityPolicy = this->contentSecurityPolicy()) {
        if (isRedirected()) {
            URL url = request.url();
            auto type = m_options.mode == FetchOptions::Mode::Navigate ? ContentSecurityPolicy::InsecureRequestType::Navigation : ContentSecurityPolicy::InsecureRequestType::Load;
            contentSecurityPolicy->upgradeInsecureRequestIfNeeded(url, type);
            if (url != request.url())
                request.setURL(url);
        }
        if (!contentSecurityPolicy->allowConnectToSource(request.url(), isRedirected() ? ContentSecurityPolicy::RedirectResponseReceived::Yes : ContentSecurityPolicy::RedirectResponseReceived::No)) {
            handler(returnError(ASCIILiteral("Blocked by Content Security Policy")));
            return;
        }
    }

    if (m_options.credentials == FetchOptions::Credentials::SameOrigin)
        m_storedCredentialsPolicy =  (m_isSameOriginRequest && m_origin->canRequest(request.url())) ? StoredCredentialsPolicy::Use : StoredCredentialsPolicy::DoNotUse;

    if (doesNotNeedCORSCheck(request.url())) {
        handler(WTFMove(request));
        return;
    }

    if (m_options.mode == FetchOptions::Mode::SameOrigin) {
        handler(returnError(ASCIILiteral("SameOrigin mode does not allow cross origin requests")));
        return;
    }

    if (isRedirected()) {
        RELEASE_LOG_IF_ALLOWED("checkRequest - Redirect requires CORS checks");
        checkCORSRedirectedRequest(WTFMove(request), WTFMove(handler));
        return;
    }

    checkCORSRequest(WTFMove(request), WTFMove(handler));
}

void NetworkLoadChecker::checkCORSRequest(ResourceRequest&& request, ValidationHandler&& handler)
{
    ASSERT(m_options.mode == FetchOptions::Mode::Cors);

    // Except in case where preflight is needed, loading should be able to continue on its own.
    switch (m_preflightPolicy) {
    case PreflightPolicy::Force:
        checkCORSRequestWithPreflight(WTFMove(request), WTFMove(handler));
        break;
    case PreflightPolicy::Consider:
        if (!m_isSimpleRequest || !isSimpleCrossOriginAccessRequest(request.httpMethod(), m_originalRequestHeaders)) {
            checkCORSRequestWithPreflight(WTFMove(request), WTFMove(handler));
            return;
        }
        FALLTHROUGH;
    case PreflightPolicy::Prevent:
        updateRequestForAccessControl(request, *m_origin, m_storedCredentialsPolicy);
        handler(WTFMove(request));
        break;
    }
}

void NetworkLoadChecker::checkCORSRedirectedRequest(ResourceRequest&& request, ValidationHandler&& handler)
{
    ASSERT(m_options.mode == FetchOptions::Mode::Cors);
    ASSERT(isRedirected());

    // Force any subsequent request to use these checks.
    m_isSameOriginRequest = false;

    if (!m_origin->canRequest(m_previousURL) && !protocolHostAndPortAreEqual(m_previousURL, request.url())) {
        // Use a unique origin for subsequent loads if needed.
        // https://fetch.spec.whatwg.org/#concept-http-redirect-fetch (Step 10).
        if (!m_origin || !m_origin->isUnique())
            m_origin = SecurityOrigin::createUnique();

        // FIXME: Add support for SameOrigin credentials.
    }

    // FIXME: We should set the request referrer according the referrer policy.

    // Let's fetch the request with the original headers (equivalent to request cloning specified by fetch algorithm).
    request.setHTTPHeaderFields(m_firstRequestHeaders);

    checkCORSRequest(WTFMove(request), WTFMove(handler));
}

void NetworkLoadChecker::checkCORSRequestWithPreflight(ResourceRequest&& request, ValidationHandler&& handler)
{
    ASSERT(m_options.mode == FetchOptions::Mode::Cors);

    m_isSimpleRequest = false;
    // FIXME: We should probably partition preflight result cache by session ID.
    if (CrossOriginPreflightResultCache::singleton().canSkipPreflight(m_origin->toString(), request.url(), m_storedCredentialsPolicy, request.httpMethod(), m_originalRequestHeaders)) {
        RELEASE_LOG_IF_ALLOWED("checkCORSRequestWithPreflight - preflight can be skipped thanks to cached result");
        updateRequestForAccessControl(request, *m_origin, m_storedCredentialsPolicy);
        handler(WTFMove(request));
        return;
    }

    auto requestForPreflight = request;
    // We need to set header fields to m_originalRequestHeaders to correctly compute Access-Control-Request-Headers header value.
    requestForPreflight.setHTTPHeaderFields(m_originalRequestHeaders);
    NetworkCORSPreflightChecker::Parameters parameters = {
        WTFMove(requestForPreflight),
        *m_origin,
        request.httpReferrer(),
        m_sessionID,
        m_storedCredentialsPolicy
    };
    m_corsPreflightChecker = std::make_unique<NetworkCORSPreflightChecker>(WTFMove(parameters), [this, request = WTFMove(request), handler = WTFMove(handler)](auto&& error) mutable {
        if (error.isCancellation())
            return;

        RELEASE_LOG_IF_ALLOWED("checkCORSRequestWithPreflight - makeCrossOriginAccessRequestWithPreflight preflight complete, success: %d forRedirect? %d", error.isNull(), isRedirected());

        if (!error.isNull()) {
            handler(makeUnexpected(WTFMove(error)));
            return;
        }

        auto corsPreflightChecker = WTFMove(m_corsPreflightChecker);
        updateRequestForAccessControl(request, *m_origin, m_storedCredentialsPolicy);
        handler(WTFMove(request));
    });
    m_corsPreflightChecker->startPreflight();
}

bool NetworkLoadChecker::doesNotNeedCORSCheck(const URL& url) const
{
    if (m_options.mode == FetchOptions::Mode::NoCors || m_options.mode == FetchOptions::Mode::Navigate)
        return true;

    return m_isSameOriginRequest && m_origin->canRequest(url);
}

ContentSecurityPolicy* NetworkLoadChecker::contentSecurityPolicy() const
{
    if (!m_contentSecurityPolicy && m_cspResponseHeaders) {
        m_contentSecurityPolicy = std::make_unique<ContentSecurityPolicy>(*m_origin);
        m_contentSecurityPolicy->didReceiveHeaders(*m_cspResponseHeaders, ContentSecurityPolicy::ReportParsingErrors::No);
    }
    return m_contentSecurityPolicy.get();
}

#if ENABLE(CONTENT_EXTENSIONS)
void NetworkLoadChecker::processContentExtensionRulesForLoad(ResourceRequest&& request, CompletionHandler<void(WebCore::ResourceRequest&&, const ContentExtensions::BlockedStatus&)>&& callback)
{
    if (!m_userContentControllerIdentifier) {
        ContentExtensions::BlockedStatus status;
        callback(WTFMove(request), status);
        return;
    }
    NetworkProcess::singleton().networkContentRuleListManager().contentExtensionsBackend(*m_userContentControllerIdentifier, [protectedThis = makeRef(*this), this, request = WTFMove(request), callback = WTFMove(callback)](auto& backend) mutable {
        auto status = backend.processContentExtensionRulesForPingLoad(request.url(), m_mainDocumentURL);
        applyBlockedStatusToRequest(status, nullptr, request);
        callback(WTFMove(request), status);
    });
}
#endif // ENABLE(CONTENT_EXTENSIONS)

} // namespace WebKit

/*
 * Copyright (C) 2018-2019 Apple Inc. All rights reserved.
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

#include "Download.h"
#include "Logging.h"
#include "NetworkCORSPreflightChecker.h"
#include "NetworkProcess.h"
#include "NetworkResourceLoader.h"
#include "NetworkSchemeRegistry.h"
#include "WebPageMessages.h"
#include <WebCore/ContentRuleListResults.h>
#include <WebCore/ContentSecurityPolicy.h>
#include <WebCore/CrossOriginAccessControl.h>
#include <WebCore/CrossOriginEmbedderPolicy.h>
#include <WebCore/CrossOriginPreflightResultCache.h>
#include <WebCore/LegacySchemeRegistry.h>
#include <wtf/Scope.h>

#define LOAD_CHECKER_RELEASE_LOG(fmt, ...) RELEASE_LOG(Network, "%p - NetworkLoadChecker::" fmt, this, ##__VA_ARGS__)

namespace WebKit {

using namespace WebCore;

static inline bool isSameOrigin(const URL& url, const SecurityOrigin* origin)
{
    return url.protocolIsData() || url.protocolIsBlob() || !origin || origin->canRequest(url);
}

NetworkLoadChecker::NetworkLoadChecker(NetworkProcess& networkProcess, NetworkResourceLoader* networkResourceLoader, NetworkSchemeRegistry* schemeRegistry, FetchOptions&& options, PAL::SessionID sessionID, WebPageProxyIdentifier webPageProxyID, HTTPHeaderMap&& originalRequestHeaders, URL&& url, DocumentURL&& documentURL, RefPtr<SecurityOrigin>&& sourceOrigin, RefPtr<SecurityOrigin>&& topOrigin, RefPtr<SecurityOrigin>&& parentOrigin, PreflightPolicy preflightPolicy, String&& referrer, bool allowPrivacyProxy, bool networkConnectionIntegrityEnabled, bool shouldCaptureExtraNetworkLoadMetrics, LoadType requestLoadType)
    : m_options(WTFMove(options))
    , m_allowPrivacyProxy(allowPrivacyProxy)
    , m_networkConnectionIntegrityEnabled(networkConnectionIntegrityEnabled)
    , m_sessionID(sessionID)
    , m_networkProcess(networkProcess)
    , m_webPageProxyID(webPageProxyID)
    , m_originalRequestHeaders(WTFMove(originalRequestHeaders))
    , m_url(WTFMove(url))
    , m_documentURL(WTFMove(documentURL))
    , m_origin(WTFMove(sourceOrigin))
    , m_topOrigin(WTFMove(topOrigin))
    , m_parentOrigin(WTFMove(parentOrigin))
    , m_preflightPolicy(preflightPolicy)
    , m_referrer(WTFMove(referrer))
    , m_shouldCaptureExtraNetworkLoadMetrics(shouldCaptureExtraNetworkLoadMetrics)
    , m_requestLoadType(requestLoadType)
    , m_schemeRegistry(schemeRegistry)
    , m_networkResourceLoader(networkResourceLoader)
{
    m_isSameOriginRequest = isSameOrigin(m_url, m_origin.get());
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

void NetworkLoadChecker::check(ResourceRequest&& request, ContentSecurityPolicyClient* client, ValidationHandler&& handler)
{
    ASSERT(!isChecking());

    if (m_shouldCaptureExtraNetworkLoadMetrics)
        m_loadInformation.request = request;

    m_firstRequestHeaders = request.httpHeaderFields();
    checkRequest(WTFMove(request), client, WTFMove(handler));
}

static inline NetworkLoadChecker::RedirectionRequestOrError redirectionError(const ResourceResponse& redirectResponse, String&& errorMessage)
{
    return makeUnexpected(ResourceError { String { }, 0, redirectResponse.url(), WTFMove(errorMessage), ResourceError::Type::AccessControl });
}

void NetworkLoadChecker::checkRedirection(ResourceRequest&& request, ResourceRequest&& redirectRequest, ResourceResponse&& redirectResponse, ContentSecurityPolicyClient* client, RedirectionValidationHandler&& handler)
{
    ASSERT(!isChecking());

    auto error = validateResponse(request, redirectResponse);
    if (!error.isNull()) {
        handler(redirectionError(redirectResponse, makeString("Cross-origin redirection to ", redirectRequest.url().string(), " denied by Cross-Origin Resource Sharing policy: ", error.localizedDescription())));
        return;
    }

    if (m_options.redirect == FetchOptions::Redirect::Error) {
        handler(redirectionError(redirectResponse, makeString("Not allowed to follow a redirection while loading ", redirectResponse.url().string())));
        return;
    }
    if (m_options.redirect == FetchOptions::Redirect::Manual) {
        handler(RedirectionTriplet { WTFMove(request), WTFMove(redirectRequest), WTFMove(redirectResponse) });
        return;
    }

    // FIXME: We should check that redirections are only HTTP(s) as per fetch spec.
    // See https://github.com/whatwg/fetch/issues/393

    if (m_options.mode == FetchOptions::Mode::Cors && (!m_isSameOriginRequest || !isSameOrigin(request.url(), m_origin.get()))) {
        auto location = URL(redirectResponse.url(), redirectResponse.httpHeaderField(HTTPHeaderName::Location));
        if (m_schemeRegistry && !m_schemeRegistry->shouldTreatURLSchemeAsCORSEnabled(location.protocol())) {
            handler(redirectionError(redirectResponse, makeString("Cross-origin redirection to ", redirectRequest.url().string(), " denied by Cross-Origin Resource Sharing policy: not allowed to follow a cross-origin CORS redirection with non CORS scheme")));
            return;
        }
        if (location.hasCredentials()) {
            handler(redirectionError(redirectResponse, makeString("Cross-origin redirection to ", redirectRequest.url().string(), " denied by Cross-Origin Resource Sharing policy: redirection URL ", location.string(), " has credentials")));
            return;
        }
    }

    if (++m_redirectCount > 20) {
        handler(redirectionError(redirectResponse, "Load cannot follow more than 20 redirections"_s));
        return;
    }

    m_previousURL = WTFMove(m_url);
    m_url = redirectRequest.url();

    checkRequest(WTFMove(redirectRequest), client, [handler = WTFMove(handler), request = WTFMove(request), redirectResponse](auto&& result) mutable {
        WTF::switchOn(result,
            [&handler] (ResourceError& error) mutable {
                handler(makeUnexpected(WTFMove(error)));
            },
            [&handler, &request, &redirectResponse] (RedirectionTriplet& triplet) mutable {
                // FIXME: if checkRequest returns a RedirectionTriplet, it means the requested URL has changed and we should update the redirectResponse to match.
                handler(RedirectionTriplet { WTFMove(request), WTFMove(triplet.redirectRequest), WTFMove(redirectResponse) });
            },
            [&handler, &request, &redirectResponse] (ResourceRequest& redirectRequest) mutable {
                handler(RedirectionTriplet { WTFMove(request), WTFMove(redirectRequest), WTFMove(redirectResponse) });
            }
        );
    });
}

static URL contextURLforCORPViolation(NetworkResourceLoader& loader)
{
    auto& url = loader.isMainResource() ? loader.parameters().parentFrameURL : loader.parameters().frameURL;
    return url.isValid() ? url : aboutBlankURL();
}

// https://fetch.spec.whatwg.org/#cross-origin-resource-policy-check
static std::optional<ResourceError> performCORPCheck(const CrossOriginEmbedderPolicy& embedderCOEP, const SecurityOrigin& embedderOrigin, const URL& url, ResourceResponse& response, ForNavigation forNavigation, NetworkResourceLoader* loader)
{
    if (auto error = validateCrossOriginResourcePolicy(CrossOriginEmbedderPolicyValue::UnsafeNone, embedderOrigin, url, response, forNavigation))
        return error;

    if (embedderCOEP.reportOnlyValue == CrossOriginEmbedderPolicyValue::RequireCORP && loader) {
        if (auto error = validateCrossOriginResourcePolicy(embedderCOEP.reportOnlyValue, embedderOrigin, url, response, forNavigation))
            sendCOEPCORPViolation(*loader, contextURLforCORPViolation(*loader), embedderCOEP.reportOnlyReportingEndpoint, COEPDisposition::Reporting, loader->parameters().options.destination, loader->firstResponseURL());
    }

    if (embedderCOEP.value == CrossOriginEmbedderPolicyValue::RequireCORP) {
        if (auto error = validateCrossOriginResourcePolicy(embedderCOEP.value, embedderOrigin, url, response, forNavigation)) {
            if (loader)
                sendCOEPCORPViolation(*loader, contextURLforCORPViolation(*loader), embedderCOEP.reportingEndpoint, COEPDisposition::Enforce, loader->parameters().options.destination, loader->firstResponseURL());
            return error;
        }
    }
    return std::nullopt;
}

ResourceError NetworkLoadChecker::validateResponse(const ResourceRequest& request, ResourceResponse& response)
{
    if (m_redirectCount)
        response.setRedirected(true);

    if (response.type() == ResourceResponse::Type::Opaqueredirect) {
        response.setTainting(ResourceResponse::Tainting::Opaqueredirect);
        return { };
    }

    if (m_options.mode == FetchOptions::Mode::Navigate || m_isSameOriginRequest) {
        if (m_options.mode == FetchOptions::Mode::Navigate && m_parentOrigin) {
            if (auto error = performCORPCheck(m_parentCrossOriginEmbedderPolicy, *m_parentOrigin, m_url, response, ForNavigation::Yes, m_networkResourceLoader.get()))
                return WTFMove(*error);
        }
        response.setTainting(ResourceResponse::Tainting::Basic);
        return { };
    }

    if (request.hasHTTPHeaderField(HTTPHeaderName::Range))
        response.setAsRangeRequested();

    if (m_options.mode == FetchOptions::Mode::NoCors) {
        if (auto error = performCORPCheck(m_crossOriginEmbedderPolicy, *m_origin, m_url, response, ForNavigation::No, m_networkResourceLoader.get()))
            return WTFMove(*error);

        response.setTainting(ResourceResponse::Tainting::Opaque);
        return { };
    }

    ASSERT(m_options.mode == FetchOptions::Mode::Cors);

    // If we have a 304, the cached response is in WebProcess so we let WebProcess do the CORS check on the cached response.
    if (response.httpStatusCode() == 304)
        return { };

    auto result = passesAccessControlCheck(response, m_storedCredentialsPolicy, *m_origin, m_networkResourceLoader.get());
    if (!result)
        return ResourceError { String { }, 0, m_url, WTFMove(result.error()), ResourceError::Type::AccessControl };

    response.setTainting(ResourceResponse::Tainting::Cors);
    return { };
}

auto NetworkLoadChecker::accessControlErrorForValidationHandler(String&& message) -> RequestOrRedirectionTripletOrError
{
    return ResourceError { String { }, 0, m_url, WTFMove(message), ResourceError::Type::AccessControl };
}

void NetworkLoadChecker::checkRequest(ResourceRequest&& request, ContentSecurityPolicyClient* client, ValidationHandler&& handler)
{
    ResourceRequest originalRequest = request;

    if (auto* contentSecurityPolicy = this->contentSecurityPolicy()) {
        if (this->isRedirected()) {
            auto type = m_options.mode == FetchOptions::Mode::Navigate ? ContentSecurityPolicy::InsecureRequestType::Navigation : ContentSecurityPolicy::InsecureRequestType::Load;
            contentSecurityPolicy->upgradeInsecureRequestIfNeeded(request, type);
        }
        if (!this->isAllowedByContentSecurityPolicy(request, client)) {
            handler(this->accessControlErrorForValidationHandler("Blocked by Content Security Policy."_s));
            return;
        }
    }

#if ENABLE(CONTENT_EXTENSIONS)
    this->processContentRuleListsForLoad(WTFMove(request), [weakThis = WeakPtr { *this }, handler = WTFMove(handler), originalRequest = WTFMove(originalRequest)](auto&& result) mutable {
        if (!result.has_value()) {
            ASSERT(result.error().isCancellation());
            handler(WTFMove(result.error()));
            return;
        }
        if (!weakThis)
            return handler({ ResourceError { ResourceError::Type::Cancellation }});
        if (result.value().results.summary.blockedLoad) {
            handler(weakThis->accessControlErrorForValidationHandler("Blocked by content extension"_s));
            return;
        }
        weakThis->continueCheckingRequestOrDoSyntheticRedirect(WTFMove(originalRequest), WTFMove(result.value().request), WTFMove(handler));
    });
#else
    this->continueCheckingRequestOrDoSyntheticRedirect(WTFMove(originalRequest), WTFMove(request), WTFMove(handler));
#endif
}

void NetworkLoadChecker::continueCheckingRequestOrDoSyntheticRedirect(ResourceRequest&& originalRequest, ResourceRequest&& currentRequest, ValidationHandler&& handler)
{
    // If main frame load and request has been modified, trigger a synthetic redirect.
    if (m_requestLoadType == LoadType::MainFrame && currentRequest.url() != originalRequest.url()) {
        ResourceResponse redirectResponse = ResourceResponse::syntheticRedirectResponse(originalRequest.url(), currentRequest.url());
        handler(RedirectionTriplet { WTFMove(originalRequest), WTFMove(currentRequest), WTFMove(redirectResponse) });
        return;
    }
    this->continueCheckingRequest(WTFMove(currentRequest), WTFMove(handler));
}

bool NetworkLoadChecker::isAllowedByContentSecurityPolicy(const ResourceRequest& request, WebCore::ContentSecurityPolicyClient* client)
{
    auto* contentSecurityPolicy = this->contentSecurityPolicy();
    contentSecurityPolicy->setClient(client);
    auto clearContentSecurityPolicyClient = makeScopeExit([&] {
        contentSecurityPolicy->setClient(nullptr);
    });

    auto preRedirectURL = m_networkResourceLoader ? m_networkResourceLoader.get()->originalRequest().url() : URL();
    auto redirectResponseReceived = isRedirected() ? ContentSecurityPolicy::RedirectResponseReceived::Yes : ContentSecurityPolicy::RedirectResponseReceived::No;
    switch (m_options.destination) {
    case FetchOptions::Destination::Audioworklet:
    case FetchOptions::Destination::Paintworklet:
    case FetchOptions::Destination::Worker:
    case FetchOptions::Destination::Serviceworker:
    case FetchOptions::Destination::Sharedworker:
        return contentSecurityPolicy->allowWorkerFromSource(request.url(), redirectResponseReceived, preRedirectURL);
    case FetchOptions::Destination::Script:
        if (request.requester() == ResourceRequest::Requester::ImportScripts && !contentSecurityPolicy->allowScriptFromSource(request.url(), redirectResponseReceived, preRedirectURL))
            return false;
        // FIXME: Check CSP for non-importScripts() initiated loads.
        return true;
    case FetchOptions::Destination::EmptyString:
        return contentSecurityPolicy->allowConnectToSource(request.url(), redirectResponseReceived, preRedirectURL);
    case FetchOptions::Destination::Audio:
    case FetchOptions::Destination::Document:
    case FetchOptions::Destination::Embed:
    case FetchOptions::Destination::Font:
    case FetchOptions::Destination::Image:
    case FetchOptions::Destination::Iframe:
    case FetchOptions::Destination::Manifest:
    case FetchOptions::Destination::Model:
    case FetchOptions::Destination::Object:
    case FetchOptions::Destination::Report:
    case FetchOptions::Destination::Style:
    case FetchOptions::Destination::Track:
    case FetchOptions::Destination::Video:
    case FetchOptions::Destination::Xslt:
        // FIXME: Check CSP for these destinations.
        return true;
    }
    ASSERT_NOT_REACHED();
    return true;
}

void NetworkLoadChecker::continueCheckingRequest(ResourceRequest&& request, ValidationHandler&& handler)
{
    if (m_options.credentials == FetchOptions::Credentials::SameOrigin)
        m_storedCredentialsPolicy = m_isSameOriginRequest && m_origin->canRequest(request.url()) ? StoredCredentialsPolicy::Use : StoredCredentialsPolicy::DoNotUse;

    m_isSameOriginRequest = m_isSameOriginRequest && isSameOrigin(request.url(), m_origin.get());

    if (doesNotNeedCORSCheck(request.url())) {
        handler(WTFMove(request));
        return;
    }

    if (m_options.mode == FetchOptions::Mode::SameOrigin) {
        handler(accessControlErrorForValidationHandler(makeString("Unsafe attempt to load URL ", request.url().stringCenterEllipsizedToLength(), " from origin ", m_origin->toString(), ". Domains, protocols and ports must match.\n")));
        return;
    }

    if (isRedirected()) {
        LOAD_CHECKER_RELEASE_LOG("checkRequest - Redirect requires CORS checks");
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
        // Use an opaque origin for subsequent loads if needed.
        // https://fetch.spec.whatwg.org/#concept-http-redirect-fetch (Step 10).
        if (!m_origin || !m_origin->isOpaque())
            m_origin = SecurityOrigin::createOpaque();
    }

    // FIXME: We should set the request referrer according the referrer policy.

    // Let's fetch the request with the original headers (equivalent to request cloning specified by fetch algorithm).
    if (!request.httpHeaderFields().contains(HTTPHeaderName::Authorization))
        m_firstRequestHeaders.remove(HTTPHeaderName::Authorization);
    request.setHTTPHeaderFields(m_firstRequestHeaders);

    checkCORSRequest(WTFMove(request), WTFMove(handler));
}

void NetworkLoadChecker::checkCORSRequestWithPreflight(ResourceRequest&& request, ValidationHandler&& handler)
{
    ASSERT(m_options.mode == FetchOptions::Mode::Cors);

    m_isSimpleRequest = false;
    if (CrossOriginPreflightResultCache::singleton().canSkipPreflight(m_sessionID, m_origin->toString(), request.url(), m_storedCredentialsPolicy, request.httpMethod(), m_originalRequestHeaders)) {
        LOAD_CHECKER_RELEASE_LOG("checkCORSRequestWithPreflight - preflight can be skipped thanks to cached result");
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
        m_topOrigin,
        request.httpReferrer(),
        request.httpUserAgent(),
        m_sessionID,
        m_webPageProxyID,
        m_storedCredentialsPolicy,
        m_allowPrivacyProxy,
        m_networkConnectionIntegrityEnabled
    };
    m_corsPreflightChecker = makeUnique<NetworkCORSPreflightChecker>(m_networkProcess.get(), m_networkResourceLoader.get(), WTFMove(parameters), m_shouldCaptureExtraNetworkLoadMetrics, [this, request = WTFMove(request), handler = WTFMove(handler), isRedirected = isRedirected()](auto&& error) mutable {
        LOAD_CHECKER_RELEASE_LOG("checkCORSRequestWithPreflight - makeCrossOriginAccessRequestWithPreflight preflight complete, success=%d forRedirect=%d", error.isNull(), isRedirected);

        if (!error.isNull()) {
            handler(WTFMove(error));
            return;
        }

        if (m_shouldCaptureExtraNetworkLoadMetrics)
            m_loadInformation.transactions.append(m_corsPreflightChecker->takeInformation());

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

    if (m_schemeRegistry && !m_schemeRegistry->shouldTreatURLSchemeAsCORSEnabled(url.protocol()))
        return true;

    return m_isSameOriginRequest;
}

ContentSecurityPolicy* NetworkLoadChecker::contentSecurityPolicy()
{
    if (!m_contentSecurityPolicy && m_cspResponseHeaders) {
        // FIXME: Pass the URL of the protected resource instead of its origin.
        m_contentSecurityPolicy = makeUnique<ContentSecurityPolicy>(URL { m_origin->toRawString() }, nullptr, m_networkResourceLoader.get());
        m_contentSecurityPolicy->didReceiveHeaders(*m_cspResponseHeaders, String { m_referrer }, ContentSecurityPolicy::ReportParsingErrors::No);
        if (!m_documentURL.isEmpty())
            m_contentSecurityPolicy->setDocumentURL(m_documentURL);
    }
    return m_contentSecurityPolicy.get();
}

#if ENABLE(CONTENT_EXTENSIONS)
void NetworkLoadChecker::processContentRuleListsForLoad(ResourceRequest&& request, ContentExtensionCallback&& callback)
{
    // FIXME: Enable content blockers for navigation loads.
    if (!m_checkContentExtensions || !m_userContentControllerIdentifier || m_options.mode == FetchOptions::Mode::Navigate) {
        ContentRuleListResults results;
        callback(ContentExtensionResult { WTFMove(request), results });
        return;
    }

    m_networkProcess->networkContentRuleListManager().contentExtensionsBackend(*m_userContentControllerIdentifier, [this, weakThis = WeakPtr { *this }, request = WTFMove(request), callback = WTFMove(callback)](auto& backend) mutable {
        if (!weakThis) {
            callback(makeUnexpected(ResourceError { ResourceError::Type::Cancellation }));
            return;
        }

        auto results = backend.processContentRuleListsForPingLoad(request.url(), m_mainDocumentURL, m_frameURL);
        WebCore::ContentExtensions::applyResultsToRequest(ContentRuleListResults { results }, nullptr, request);
        callback(ContentExtensionResult { WTFMove(request), results });
    });
}
#endif // ENABLE(CONTENT_EXTENSIONS)

void NetworkLoadChecker::storeRedirectionIfNeeded(const ResourceRequest& request, const ResourceResponse& response)
{
    if (!m_shouldCaptureExtraNetworkLoadMetrics)
        return;
    m_loadInformation.transactions.append(NetworkTransactionInformation { NetworkTransactionInformation::Type::Redirection, ResourceRequest { request }, ResourceResponse { response }, { } });
}

} // namespace WebKit

#undef LOAD_CHECKER_RELEASE_LOG

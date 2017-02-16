/*
 * Copyright (C) 2011, 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "DocumentThreadableLoader.h"

#include "CachedRawResource.h"
#include "CachedResourceLoader.h"
#include "CachedResourceRequest.h"
#include "CachedResourceRequestInitiators.h"
#include "CrossOriginAccessControl.h"
#include "CrossOriginPreflightChecker.h"
#include "CrossOriginPreflightResultCache.h"
#include "DOMWindow.h"
#include "Document.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "InspectorInstrumentation.h"
#include "LoadTiming.h"
#include "Performance.h"
#include "ProgressTracker.h"
#include "ResourceError.h"
#include "ResourceRequest.h"
#include "ResourceTiming.h"
#include "RuntimeEnabledFeatures.h"
#include "SchemeRegistry.h"
#include "SecurityOrigin.h"
#include "SubresourceLoader.h"
#include "ThreadableLoaderClient.h"
#include <wtf/Assertions.h>
#include <wtf/Ref.h>

namespace WebCore {

void DocumentThreadableLoader::loadResourceSynchronously(Document& document, ResourceRequest&& request, ThreadableLoaderClient& client, const ThreadableLoaderOptions& options, RefPtr<SecurityOrigin>&& origin, std::unique_ptr<ContentSecurityPolicy>&& contentSecurityPolicy)
{
    // The loader will be deleted as soon as this function exits.
    Ref<DocumentThreadableLoader> loader = adoptRef(*new DocumentThreadableLoader(document, client, LoadSynchronously, WTFMove(request), options, WTFMove(origin), WTFMove(contentSecurityPolicy), String(), ShouldLogError::Yes));
    ASSERT(loader->hasOneRef());
}

void DocumentThreadableLoader::loadResourceSynchronously(Document& document, ResourceRequest&& request, ThreadableLoaderClient& client, const ThreadableLoaderOptions& options)
{
    loadResourceSynchronously(document, WTFMove(request), client, options, nullptr, nullptr);
}

RefPtr<DocumentThreadableLoader> DocumentThreadableLoader::create(Document& document, ThreadableLoaderClient& client,
ResourceRequest&& request, const ThreadableLoaderOptions& options, RefPtr<SecurityOrigin>&& origin,
std::unique_ptr<ContentSecurityPolicy>&& contentSecurityPolicy, String&& referrer, ShouldLogError shouldLogError)
{
    RefPtr<DocumentThreadableLoader> loader = adoptRef(new DocumentThreadableLoader(document, client, LoadAsynchronously, WTFMove(request), options, WTFMove(origin), WTFMove(contentSecurityPolicy), WTFMove(referrer), shouldLogError));
    if (!loader->isLoading())
        loader = nullptr;
    return loader;
}

RefPtr<DocumentThreadableLoader> DocumentThreadableLoader::create(Document& document, ThreadableLoaderClient& client, ResourceRequest&& request, const ThreadableLoaderOptions& options, String&& referrer)
{
    return create(document, client, WTFMove(request), options, nullptr, nullptr, WTFMove(referrer), ShouldLogError::Yes);
}

DocumentThreadableLoader::DocumentThreadableLoader(Document& document, ThreadableLoaderClient& client, BlockingBehavior blockingBehavior, ResourceRequest&& request, const ThreadableLoaderOptions& options, RefPtr<SecurityOrigin>&& origin, std::unique_ptr<ContentSecurityPolicy>&& contentSecurityPolicy, String&& referrer, ShouldLogError shouldLogError)
    : m_client(&client)
    , m_document(document)
    , m_options(options)
    , m_origin(WTFMove(origin))
    , m_referrer(WTFMove(referrer))
    , m_sameOriginRequest(securityOrigin().canRequest(request.url()))
    , m_simpleRequest(true)
    , m_async(blockingBehavior == LoadAsynchronously)
    , m_contentSecurityPolicy(WTFMove(contentSecurityPolicy))
    , m_shouldLogError(shouldLogError)
{
    relaxAdoptionRequirement();

    // Setting a referrer header is only supported in the async code path.
    ASSERT(m_async || m_referrer.isEmpty());

    // Referrer and Origin headers should be set after the preflight if any.
    ASSERT(!request.hasHTTPReferrer() && !request.hasHTTPOrigin());

    ASSERT_WITH_SECURITY_IMPLICATION(isAllowedByContentSecurityPolicy(request.url(), ContentSecurityPolicy::RedirectResponseReceived::No));

    m_options.allowCredentials = (m_options.credentials == FetchOptions::Credentials::Include || (m_options.credentials == FetchOptions::Credentials::SameOrigin && m_sameOriginRequest)) ? AllowStoredCredentials : DoNotAllowStoredCredentials;

    ASSERT(!request.httpHeaderFields().contains(HTTPHeaderName::Origin));

    // Copy headers if we need to replay the request after a redirection.
    if (m_async && m_options.mode == FetchOptions::Mode::Cors)
        m_originalHeaders = request.httpHeaderFields();

    if (document.page() && document.page()->isRunningUserScripts() && SchemeRegistry::isUserExtensionScheme(request.url().protocol().toStringWithoutCopying())) {
        m_options.mode = FetchOptions::Mode::NoCors;
        m_options.filteringPolicy = ResponseFilteringPolicy::Disable;
    }

    // As per step 11 of https://fetch.spec.whatwg.org/#main-fetch, data scheme (if same-origin data-URL flag is set) and about scheme are considered same-origin.
    if (request.url().protocolIsData())
        m_sameOriginRequest = options.sameOriginDataURLFlag == SameOriginDataURLFlag::Set;

    if (m_sameOriginRequest || m_options.mode == FetchOptions::Mode::NoCors) {
        loadRequest(WTFMove(request), DoSecurityCheck);
        return;
    }

    if (m_options.mode == FetchOptions::Mode::SameOrigin) {
        logErrorAndFail(ResourceError(errorDomainWebKitInternal, 0, request.url(), "Cross origin requests are not allowed when using same-origin fetch mode."));
        return;
    }

    makeCrossOriginAccessRequest(WTFMove(request));
}

void DocumentThreadableLoader::makeCrossOriginAccessRequest(ResourceRequest&& request)
{
    ASSERT(m_options.mode == FetchOptions::Mode::Cors);

    if ((m_options.preflightPolicy == ConsiderPreflight && isSimpleCrossOriginAccessRequest(request.httpMethod(), request.httpHeaderFields())) || m_options.preflightPolicy == PreventPreflight)
        makeSimpleCrossOriginAccessRequest(WTFMove(request));
    else {
        m_simpleRequest = false;
        if (CrossOriginPreflightResultCache::singleton().canSkipPreflight(securityOrigin().toString(), request.url(), m_options.allowCredentials, request.httpMethod(), request.httpHeaderFields()))
            preflightSuccess(WTFMove(request));
        else
            makeCrossOriginAccessRequestWithPreflight(WTFMove(request));
    }
}

void DocumentThreadableLoader::makeSimpleCrossOriginAccessRequest(ResourceRequest&& request)
{
    ASSERT(m_options.preflightPolicy != ForcePreflight);
    ASSERT(m_options.preflightPolicy == PreventPreflight || isSimpleCrossOriginAccessRequest(request.httpMethod(), request.httpHeaderFields()));

    // Cross-origin requests are only allowed for HTTP and registered schemes. We would catch this when checking response headers later, but there is no reason to send a request that's guaranteed to be denied.
    if (!SchemeRegistry::shouldTreatURLSchemeAsCORSEnabled(request.url().protocol().toStringWithoutCopying())) {
        logErrorAndFail(ResourceError(errorDomainWebKitInternal, 0, request.url(), "Cross origin requests are only supported for HTTP.", ResourceError::Type::AccessControl));
        return;
    }

    updateRequestForAccessControl(request, securityOrigin(), m_options.allowCredentials);
    loadRequest(WTFMove(request), DoSecurityCheck);
}

void DocumentThreadableLoader::makeCrossOriginAccessRequestWithPreflight(ResourceRequest&& request)
{
    if (m_async) {
        m_preflightChecker.emplace(*this, WTFMove(request));
        m_preflightChecker->startPreflight();
        return;
    }
    CrossOriginPreflightChecker::doPreflight(*this, WTFMove(request));
}

DocumentThreadableLoader::~DocumentThreadableLoader()
{
    if (m_resource)
        m_resource->removeClient(*this);
}

void DocumentThreadableLoader::cancel()
{
    Ref<DocumentThreadableLoader> protectedThis(*this);

    // Cancel can re-enter and m_resource might be null here as a result.
    if (m_client && m_resource) {
        // FIXME: This error is sent to the client in didFail(), so it should not be an internal one. Use FrameLoaderClient::cancelledError() instead.
        ResourceError error(errorDomainWebKitInternal, 0, m_resource->url(), "Load cancelled", ResourceError::Type::Cancellation);
        m_client->didFail(error);
    }
    clearResource();
    m_client = nullptr;
}

void DocumentThreadableLoader::setDefersLoading(bool value)
{
    if (m_resource)
        m_resource->setDefersLoading(value);
    if (m_preflightChecker)
        m_preflightChecker->setDefersLoading(value);
}

void DocumentThreadableLoader::clearResource()
{
    // Script can cancel and restart a request reentrantly within removeClient(),
    // which could lead to calling CachedResource::removeClient() multiple times for
    // this DocumentThreadableLoader. Save off a copy of m_resource and clear it to
    // prevent the reentrancy.
    if (CachedResourceHandle<CachedRawResource> resource = m_resource) {
        m_resource = nullptr;
        resource->removeClient(*this);
    }
    if (m_preflightChecker)
        m_preflightChecker = std::nullopt;
}

void DocumentThreadableLoader::redirectReceived(CachedResource& resource, ResourceRequest& request, const ResourceResponse& redirectResponse)
{
    ASSERT(m_client);
    ASSERT_UNUSED(resource, &resource == m_resource);

    Ref<DocumentThreadableLoader> protectedThis(*this);

    // FIXME: We restrict this check to Fetch API for the moment, as this might disrupt WorkerScriptLoader.
    // Reassess this check based on https://github.com/whatwg/fetch/issues/393 discussions.
    // We should also disable that check in navigation mode.
    if (!request.url().protocolIsInHTTPFamily() && m_options.initiator == cachedResourceRequestInitiators().fetch) {
        reportRedirectionWithBadScheme(request.url());
        clearResource();
        return;
    }

    if (!isAllowedByContentSecurityPolicy(request.url(), redirectResponse.isNull() ? ContentSecurityPolicy::RedirectResponseReceived::No : ContentSecurityPolicy::RedirectResponseReceived::Yes)) {
        reportContentSecurityPolicyError(redirectResponse.url());
        clearResource();
        return;
    }

    // Allow same origin requests to continue after allowing clients to audit the redirect.
    if (isAllowedRedirect(request.url()))
        return;

    // Force any subsequent request to use these checks.
    m_sameOriginRequest = false;

    ASSERT(m_resource);
    ASSERT(m_resource->loader());
    ASSERT(m_options.mode == FetchOptions::Mode::Cors);
    ASSERT(m_originalHeaders);

    // Loader might have modified the origin to a unique one, let's reuse it for subsequent loads.
    m_origin = m_resource->loader()->origin();

    // Except in case where preflight is needed, loading should be able to continue on its own.
    // But we also handle credentials here if it is restricted to SameOrigin.
    if (m_options.credentials != FetchOptions::Credentials::SameOrigin && m_simpleRequest && isSimpleCrossOriginAccessRequest(request.httpMethod(), *m_originalHeaders))
        return;

    m_options.allowCredentials = DoNotAllowStoredCredentials;
    m_options.maxRedirectCount -= m_resource->loader()->redirectCount();

    clearResource();

    // Let's fetch the request with the original headers (equivalent to request cloning specified by fetch algorithm).
    // Do not copy the Authorization header if removed by the network layer.
    if (!request.httpHeaderFields().contains(HTTPHeaderName::Authorization))
        m_originalHeaders->remove(HTTPHeaderName::Authorization);
    request.setHTTPHeaderFields(*m_originalHeaders);

    makeCrossOriginAccessRequest(ResourceRequest(request));
}

void DocumentThreadableLoader::dataSent(CachedResource& resource, unsigned long long bytesSent, unsigned long long totalBytesToBeSent)
{
    ASSERT(m_client);
    ASSERT_UNUSED(resource, &resource == m_resource);
    m_client->didSendData(bytesSent, totalBytesToBeSent);
}

void DocumentThreadableLoader::responseReceived(CachedResource& resource, const ResourceResponse& response)
{
    ASSERT_UNUSED(resource, &resource == m_resource);
    didReceiveResponse(m_resource->identifier(), response, m_resource->responseTainting());
}

void DocumentThreadableLoader::didReceiveResponse(unsigned long identifier, const ResourceResponse& response, ResourceResponse::Tainting tainting)
{
    ASSERT(m_client);
    ASSERT(response.type() != ResourceResponse::Type::Error);

    InspectorInstrumentation::didReceiveThreadableLoaderResponse(*this, identifier);

    if (options().filteringPolicy == ResponseFilteringPolicy::Disable) {
        m_client->didReceiveResponse(identifier, response);
        return;
    }

    if (response.type() == ResourceResponse::Type::Default) {
        m_client->didReceiveResponse(identifier, ResourceResponse::filterResponse(response, tainting));
        if (tainting == ResourceResponse::Tainting::Opaque) {
            clearResource();
            if (m_client)
                m_client->didFinishLoading(identifier, 0.0);
        }
    } else {
        ASSERT(response.type() == ResourceResponse::Type::Opaqueredirect);
        m_client->didReceiveResponse(identifier, response);
    }
}

void DocumentThreadableLoader::dataReceived(CachedResource& resource, const char* data, int dataLength)
{
    ASSERT_UNUSED(resource, &resource == m_resource);
    didReceiveData(m_resource->identifier(), data, dataLength);
}

void DocumentThreadableLoader::didReceiveData(unsigned long, const char* data, int dataLength)
{
    ASSERT(m_client);

    m_client->didReceiveData(data, dataLength);
}

void DocumentThreadableLoader::finishedTimingForWorkerLoad(CachedResource& resource, const ResourceTiming& resourceTiming)
{
    ASSERT(m_client);
    ASSERT_UNUSED(resource, &resource == m_resource);
    UNUSED_PARAM(resourceTiming);

#if ENABLE(WEB_TIMING)
    finishedTimingForWorkerLoad(resourceTiming);
#endif
}

#if ENABLE(WEB_TIMING)
void DocumentThreadableLoader::finishedTimingForWorkerLoad(const ResourceTiming& resourceTiming)
{
    ASSERT(m_options.initiatorContext == InitiatorContext::Worker);

    m_client->didFinishTiming(resourceTiming);
}
#endif

void DocumentThreadableLoader::notifyFinished(CachedResource& resource)
{
    ASSERT(m_client);
    ASSERT_UNUSED(resource, &resource == m_resource);

    if (m_resource->errorOccurred())
        didFail(m_resource->identifier(), m_resource->resourceError());
    else
        didFinishLoading(m_resource->identifier(), m_resource->loadFinishTime());
}

void DocumentThreadableLoader::didFinishLoading(unsigned long identifier, double finishTime)
{
    ASSERT(m_client);
    m_client->didFinishLoading(identifier, finishTime);
}

void DocumentThreadableLoader::didFail(unsigned long, const ResourceError& error)
{
    ASSERT(m_client);
    logErrorAndFail(error);
}

void DocumentThreadableLoader::preflightSuccess(ResourceRequest&& request)
{
    ResourceRequest actualRequest(WTFMove(request));
    updateRequestForAccessControl(actualRequest, securityOrigin(), m_options.allowCredentials);

    m_preflightChecker = std::nullopt;

    // It should be ok to skip the security check since we already asked about the preflight request.
    loadRequest(WTFMove(actualRequest), SkipSecurityCheck);
}

void DocumentThreadableLoader::preflightFailure(unsigned long identifier, const ResourceError& error)
{
    m_preflightChecker = std::nullopt;

    InspectorInstrumentation::didFailLoading(m_document.frame(), m_document.frame()->loader().documentLoader(), identifier, error);
    ASSERT(m_client);
    logErrorAndFail(error);
}

void DocumentThreadableLoader::loadRequest(ResourceRequest&& request, SecurityCheckPolicy securityCheck)
{
    Ref<DocumentThreadableLoader> protectedThis(*this);

    // Any credential should have been removed from the cross-site requests.
    const URL& requestURL = request.url();
    m_options.securityCheck = securityCheck;
    ASSERT(m_sameOriginRequest || requestURL.user().isEmpty());
    ASSERT(m_sameOriginRequest || requestURL.pass().isEmpty());

    if (!m_referrer.isNull())
        request.setHTTPReferrer(m_referrer);

    if (m_async) {
        ResourceLoaderOptions options = m_options;
        options.clientCredentialPolicy = m_sameOriginRequest ? ClientCredentialPolicy::MayAskClientForCredentials : ClientCredentialPolicy::CannotAskClientForCredentials;
        options.contentSecurityPolicyImposition = ContentSecurityPolicyImposition::SkipPolicyCheck;

        request.setAllowCookies(m_options.allowCredentials == AllowStoredCredentials);
        CachedResourceRequest newRequest(WTFMove(request), options);
        if (RuntimeEnabledFeatures::sharedFeatures().resourceTimingEnabled())
            newRequest.setInitiator(m_options.initiator);
        newRequest.setOrigin(securityOrigin());

        ASSERT(!m_resource);
        if (m_resource) {
            CachedResourceHandle<CachedRawResource> resource = std::exchange(m_resource, nullptr);
            resource->removeClient(*this);
        }

        // We create an URL here as the request will be moved in requestRawResource
        URL requestUrl = newRequest.resourceRequest().url();
        m_resource = m_document.cachedResourceLoader().requestRawResource(WTFMove(newRequest));
        if (m_resource)
            m_resource->addClient(*this);
        else {
            // FIXME: Since we receive a synchronous error, this is probably due to some AccessControl checks. We should try to retrieve the actual error.
            logErrorAndFail(ResourceError(String(), 0, requestUrl, String(), ResourceError::Type::AccessControl));
        }
        return;
    }

    // If credentials mode is 'Omit', we should disable cookie sending.
    ASSERT(m_options.credentials != FetchOptions::Credentials::Omit);

#if ENABLE(WEB_TIMING)
    LoadTiming loadTiming;
    loadTiming.markStartTimeAndFetchStart();
#endif

    // FIXME: ThreadableLoaderOptions.sniffContent is not supported for synchronous requests.
    RefPtr<SharedBuffer> data;
    ResourceError error;
    ResourceResponse response;
    unsigned long identifier = std::numeric_limits<unsigned long>::max();
    if (m_document.frame()) {
        auto& frameLoader = m_document.frame()->loader();
        if (!frameLoader.mixedContentChecker().canRunInsecureContent(m_document.securityOrigin(), requestURL))
            return;
        identifier = frameLoader.loadResourceSynchronously(request, m_options.allowCredentials, m_options.clientCredentialPolicy, error, response, data);
    }

#if ENABLE(WEB_TIMING)
    loadTiming.setResponseEnd(MonotonicTime::now());
#endif

    if (!error.isNull() && response.httpStatusCode() <= 0) {
        if (requestURL.isLocalFile()) {
            // We don't want XMLHttpRequest to raise an exception for file:// resources, see <rdar://problem/4962298>.
            // FIXME: XMLHttpRequest quirks should be in XMLHttpRequest code, not in DocumentThreadableLoader.cpp.
            didReceiveResponse(identifier, response, ResourceResponse::Tainting::Basic);
            didFinishLoading(identifier, 0.0);
            return;
        }
        logErrorAndFail(error);
        return;
    }

    // FIXME: FrameLoader::loadSynchronously() does not tell us whether a redirect happened or not, so we guess by comparing the
    // request and response URLs. This isn't a perfect test though, since a server can serve a redirect to the same URL that was
    // requested. Also comparing the request and response URLs as strings will fail if the requestURL still has its credentials.
    bool didRedirect = requestURL != response.url();
    if (didRedirect) {
        if (!isAllowedByContentSecurityPolicy(response.url(), ContentSecurityPolicy::RedirectResponseReceived::Yes)) {
            reportContentSecurityPolicyError(requestURL);
            return;
        }
        if (!isAllowedRedirect(response.url())) {
            reportCrossOriginResourceSharingError(requestURL);
            return;
        }
    }

    ResourceResponse::Tainting tainting = ResourceResponse::Tainting::Basic;
    if (!m_sameOriginRequest) {
        if (m_options.mode == FetchOptions::Mode::NoCors)
            tainting = ResourceResponse::Tainting::Opaque;
        else {
            ASSERT(m_options.mode == FetchOptions::Mode::Cors);
            tainting = ResourceResponse::Tainting::Cors;
            String accessControlErrorDescription;
            if (!passesAccessControlCheck(response, m_options.allowCredentials, securityOrigin(), accessControlErrorDescription)) {
                logErrorAndFail(ResourceError(errorDomainWebKitInternal, 0, response.url(), accessControlErrorDescription, ResourceError::Type::AccessControl));
                return;
            }
        }
    }
    didReceiveResponse(identifier, response, tainting);

    if (data)
        didReceiveData(identifier, data->data(), data->size());

#if ENABLE(WEB_TIMING)
    if (RuntimeEnabledFeatures::sharedFeatures().resourceTimingEnabled()) {
        ResourceTiming resourceTiming = ResourceTiming::fromSynchronousLoad(requestURL, m_options.initiator, loadTiming, response.networkLoadTiming(), response, securityOrigin());
        if (options().initiatorContext == InitiatorContext::Worker)
            finishedTimingForWorkerLoad(resourceTiming);
        else {
            if (document().domWindow() && document().domWindow()->performance())
                document().domWindow()->performance()->addResourceTiming(WTFMove(resourceTiming));
        }        
    }
#endif

    didFinishLoading(identifier, 0.0);
}

bool DocumentThreadableLoader::isAllowedByContentSecurityPolicy(const URL& url, ContentSecurityPolicy::RedirectResponseReceived redirectResponseReceived)
{
    switch (m_options.contentSecurityPolicyEnforcement) {
    case ContentSecurityPolicyEnforcement::DoNotEnforce:
        return true;
    case ContentSecurityPolicyEnforcement::EnforceChildSrcDirective:
        return contentSecurityPolicy().allowChildContextFromSource(url, redirectResponseReceived);
    case ContentSecurityPolicyEnforcement::EnforceConnectSrcDirective:
        return contentSecurityPolicy().allowConnectToSource(url, redirectResponseReceived);
    case ContentSecurityPolicyEnforcement::EnforceScriptSrcDirective:
        return contentSecurityPolicy().allowScriptFromSource(url, redirectResponseReceived);
    }
    ASSERT_NOT_REACHED();
    return false;
}

bool DocumentThreadableLoader::isAllowedRedirect(const URL& url)
{
    if (m_options.mode == FetchOptions::Mode::NoCors)
        return true;

    return m_sameOriginRequest && securityOrigin().canRequest(url);
}

bool DocumentThreadableLoader::isXMLHttpRequest() const
{
    return m_options.initiator == cachedResourceRequestInitiators().xmlhttprequest;
}

SecurityOrigin& DocumentThreadableLoader::securityOrigin() const
{
    return m_origin ? *m_origin : m_document.securityOrigin();
}

const ContentSecurityPolicy& DocumentThreadableLoader::contentSecurityPolicy() const
{
    if (m_contentSecurityPolicy)
        return *m_contentSecurityPolicy.get();
    ASSERT(m_document.contentSecurityPolicy());
    return *m_document.contentSecurityPolicy();
}

void DocumentThreadableLoader::reportRedirectionWithBadScheme(const URL& url)
{
    logErrorAndFail(ResourceError(errorDomainWebKitInternal, 0, url, "Redirection to URL with a scheme that is not HTTP(S).", ResourceError::Type::AccessControl));
}

void DocumentThreadableLoader::reportContentSecurityPolicyError(const URL& url)
{
    logErrorAndFail(ResourceError(errorDomainWebKitInternal, 0, url, "Cross-origin redirection denied by Content Security Policy.", ResourceError::Type::AccessControl));
}

void DocumentThreadableLoader::reportCrossOriginResourceSharingError(const URL& url)
{
    logErrorAndFail(ResourceError(errorDomainWebKitInternal, 0, url, "Cross-origin redirection denied by Cross-Origin Resource Sharing policy.", ResourceError::Type::AccessControl));
}

void DocumentThreadableLoader::logErrorAndFail(const ResourceError& error)
{
    if (m_shouldLogError == ShouldLogError::Yes)
        logError(m_document, error, m_options.initiator);
    ASSERT(m_client);
    m_client->didFail(error);
}

} // namespace WebCore

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
#include "DocumentLoader.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "InspectorInstrumentation.h"
#include "LegacySchemeRegistry.h"
#include "LoaderStrategy.h"
#include "MixedContentChecker.h"
#include "Performance.h"
#include "PlatformStrategies.h"
#include "ProgressTracker.h"
#include "ResourceError.h"
#include "ResourceRequest.h"
#include "ResourceTiming.h"
#include "RuntimeApplicationChecks.h"
#include "RuntimeEnabledFeatures.h"
#include "SecurityOrigin.h"
#include "Settings.h"
#include "SharedBuffer.h"
#include "SubresourceIntegrity.h"
#include "SubresourceLoader.h"
#include "ThreadableLoaderClient.h"
#include <wtf/Assertions.h>
#include <wtf/Ref.h>

#if PLATFORM(IOS_FAMILY)
#include <wtf/cocoa/RuntimeApplicationChecksCocoa.h>
#endif

namespace WebCore {

void DocumentThreadableLoader::loadResourceSynchronously(Document& document, ResourceRequest&& request, ThreadableLoaderClient& client, const ThreadableLoaderOptions& options, RefPtr<SecurityOrigin>&& origin, std::unique_ptr<ContentSecurityPolicy>&& contentSecurityPolicy, std::optional<CrossOriginEmbedderPolicy>&& crossOriginEmbedderPolicy)
{
    // The loader will be deleted as soon as this function exits.
    Ref<DocumentThreadableLoader> loader = adoptRef(*new DocumentThreadableLoader(document, client, LoadSynchronously, WTFMove(request), options, WTFMove(origin), WTFMove(contentSecurityPolicy), WTFMove(crossOriginEmbedderPolicy), String(), ShouldLogError::Yes));
    ASSERT(loader->hasOneRef());
}

void DocumentThreadableLoader::loadResourceSynchronously(Document& document, ResourceRequest&& request, ThreadableLoaderClient& client, const ThreadableLoaderOptions& options)
{
    loadResourceSynchronously(document, WTFMove(request), client, options, nullptr, nullptr, std::nullopt);
}

RefPtr<DocumentThreadableLoader> DocumentThreadableLoader::create(Document& document, ThreadableLoaderClient& client,
ResourceRequest&& request, const ThreadableLoaderOptions& options, RefPtr<SecurityOrigin>&& origin,
std::unique_ptr<ContentSecurityPolicy>&& contentSecurityPolicy, std::optional<CrossOriginEmbedderPolicy>&& crossOriginEmbedderPolicy, String&& referrer, ShouldLogError shouldLogError)
{
    RefPtr<DocumentThreadableLoader> loader = adoptRef(new DocumentThreadableLoader(document, client, LoadAsynchronously, WTFMove(request), options, WTFMove(origin), WTFMove(contentSecurityPolicy), WTFMove(crossOriginEmbedderPolicy), WTFMove(referrer), shouldLogError));
    if (!loader->isLoading())
        loader = nullptr;
    return loader;
}

RefPtr<DocumentThreadableLoader> DocumentThreadableLoader::create(Document& document, ThreadableLoaderClient& client, ResourceRequest&& request, const ThreadableLoaderOptions& options, String&& referrer)
{
    return create(document, client, WTFMove(request), options, nullptr, nullptr, std::nullopt, WTFMove(referrer), ShouldLogError::Yes);
}

static inline bool shouldPerformSecurityChecks()
{
    return platformStrategies()->loaderStrategy()->shouldPerformSecurityChecks();
}

bool DocumentThreadableLoader::shouldSetHTTPHeadersToKeep() const
{
    if (m_options.mode == FetchOptions::Mode::Cors && shouldPerformSecurityChecks())
        return true;

#if ENABLE(SERVICE_WORKER)
    if (m_options.serviceWorkersMode == ServiceWorkersMode::All && m_async)
        return m_options.serviceWorkerRegistrationIdentifier || m_document.activeServiceWorker();
#endif

    return false;
}

DocumentThreadableLoader::DocumentThreadableLoader(Document& document, ThreadableLoaderClient& client, BlockingBehavior blockingBehavior, ResourceRequest&& request, const ThreadableLoaderOptions& options, RefPtr<SecurityOrigin>&& origin, std::unique_ptr<ContentSecurityPolicy>&& contentSecurityPolicy, std::optional<CrossOriginEmbedderPolicy>&& crossOriginEmbedderPolicy, String&& referrer, ShouldLogError shouldLogError)
    : m_client(&client)
    , m_document(document)
    , m_options(options)
    , m_origin(WTFMove(origin))
    , m_referrer(WTFMove(referrer))
    , m_sameOriginRequest(securityOrigin().canRequest(request.url()))
    , m_simpleRequest(true)
    , m_async(blockingBehavior == LoadAsynchronously)
    , m_delayCallbacksForIntegrityCheck(!m_options.integrity.isEmpty())
    , m_contentSecurityPolicy(WTFMove(contentSecurityPolicy))
    , m_crossOriginEmbedderPolicy(WTFMove(crossOriginEmbedderPolicy))
    , m_shouldLogError(shouldLogError)
{
    relaxAdoptionRequirement();

    // Setting a referrer header is only supported in the async code path.
    ASSERT(m_async || m_referrer.isEmpty());

    if (document.settings().disallowSyncXHRDuringPageDismissalEnabled() && !m_async && (!document.page() || !document.page()->areSynchronousLoadsAllowed())) {
        document.didRejectSyncXHRDuringPageDismissal();
        logErrorAndFail(ResourceError(errorDomainWebKitInternal, 0, request.url(), "Synchronous loads are not allowed at this time"));
        return;
    }

    // Referrer and Origin headers should be set after the preflight if any.
    ASSERT(!request.hasHTTPReferrer() && !request.hasHTTPOrigin());

    ASSERT_WITH_SECURITY_IMPLICATION(isAllowedByContentSecurityPolicy(request.url(), ContentSecurityPolicy::RedirectResponseReceived::No));

    m_options.storedCredentialsPolicy = (m_options.credentials == FetchOptions::Credentials::Include || (m_options.credentials == FetchOptions::Credentials::SameOrigin && m_sameOriginRequest)) ? StoredCredentialsPolicy::Use : StoredCredentialsPolicy::DoNotUse;

    ASSERT(!request.httpHeaderFields().contains(HTTPHeaderName::Origin));

    // Copy headers if we need to replay the request after a redirection.
    if (m_options.mode == FetchOptions::Mode::Cors)
        m_originalHeaders = request.httpHeaderFields();

    if (shouldSetHTTPHeadersToKeep())
        m_options.httpHeadersToKeep = httpHeadersToKeepFromCleaning(request.httpHeaderFields());

    bool shouldDisableCORS = document.isRunningUserScripts() && LegacySchemeRegistry::isUserExtensionScheme(request.url().protocol().toStringWithoutCopying());
    if (auto* page = document.page())
        shouldDisableCORS |= page->shouldDisableCorsForRequestTo(request.url());

    if (shouldDisableCORS) {
        m_options.mode = FetchOptions::Mode::NoCors;
        m_options.filteringPolicy = ResponseFilteringPolicy::Disable;
        m_responsesCanBeOpaque = false;
    }

    m_options.cspResponseHeaders = m_options.contentSecurityPolicyEnforcement != ContentSecurityPolicyEnforcement::DoNotEnforce ? this->contentSecurityPolicy().responseHeaders() : ContentSecurityPolicyResponseHeaders { };
    m_options.crossOriginEmbedderPolicy = this->crossOriginEmbedderPolicy();

    // As per step 11 of https://fetch.spec.whatwg.org/#main-fetch, data scheme (if same-origin data-URL flag is set) and about scheme are considered same-origin.
    if (request.url().protocolIsData())
        m_sameOriginRequest = options.sameOriginDataURLFlag == SameOriginDataURLFlag::Set;

    if (m_sameOriginRequest || m_options.mode == FetchOptions::Mode::NoCors || m_options.mode == FetchOptions::Mode::Navigate) {
        loadRequest(WTFMove(request), SecurityCheckPolicy::DoSecurityCheck);
        return;
    }

    if (m_options.mode == FetchOptions::Mode::SameOrigin) {
        logErrorAndFail(ResourceError(errorDomainWebKitInternal, 0, request.url(), "Cross origin requests are not allowed when using same-origin fetch mode."));
        return;
    }

    makeCrossOriginAccessRequest(WTFMove(request));
}

bool DocumentThreadableLoader::checkURLSchemeAsCORSEnabled(const URL& url)
{
    // Cross-origin requests are only allowed for HTTP and registered schemes. We would catch this when checking response headers later, but there is no reason to send a request that's guaranteed to be denied.
    if (!LegacySchemeRegistry::shouldTreatURLSchemeAsCORSEnabled(url.protocol().toStringWithoutCopying())) {
        logErrorAndFail(ResourceError(errorDomainWebKitInternal, 0, url, "Cross origin requests are only supported for HTTP.", ResourceError::Type::AccessControl));
        return false;
    }
    return true;
}

void DocumentThreadableLoader::makeCrossOriginAccessRequest(ResourceRequest&& request)
{
    ASSERT(m_options.mode == FetchOptions::Mode::Cors);

#if PLATFORM(IOS_FAMILY)
    bool needsPreflightQuirk = IOSApplication::isMoviStarPlus() && applicationSDKVersion() < DYLD_IOS_VERSION_12_0 && (m_options.preflightPolicy == PreflightPolicy::Consider || m_options.preflightPolicy == PreflightPolicy::Force);
#else
    bool needsPreflightQuirk = false;
#endif

    if ((m_options.preflightPolicy == PreflightPolicy::Consider && isSimpleCrossOriginAccessRequest(request.httpMethod(), request.httpHeaderFields())) || m_options.preflightPolicy == PreflightPolicy::Prevent || (shouldPerformSecurityChecks() && !needsPreflightQuirk)) {
        if (checkURLSchemeAsCORSEnabled(request.url()))
            makeSimpleCrossOriginAccessRequest(WTFMove(request));
    } else {
#if ENABLE(SERVICE_WORKER)
        if (m_options.serviceWorkersMode == ServiceWorkersMode::All && m_async) {
            if (m_options.serviceWorkerRegistrationIdentifier || document().activeServiceWorker()) {
                ASSERT(!m_bypassingPreflightForServiceWorkerRequest);
                m_bypassingPreflightForServiceWorkerRequest = WTFMove(request);
                m_options.serviceWorkersMode = ServiceWorkersMode::Only;
                loadRequest(ResourceRequest { m_bypassingPreflightForServiceWorkerRequest.value() }, SecurityCheckPolicy::SkipSecurityCheck);
                return;
            }
        }
#endif
        if (!needsPreflightQuirk && !checkURLSchemeAsCORSEnabled(request.url()))
            return;

        m_simpleRequest = false;
        if (auto* page = document().page(); page && CrossOriginPreflightResultCache::singleton().canSkipPreflight(page->sessionID(), securityOrigin().toString(), request.url(), m_options.storedCredentialsPolicy, request.httpMethod(), request.httpHeaderFields()))
            preflightSuccess(WTFMove(request));
        else
            makeCrossOriginAccessRequestWithPreflight(WTFMove(request));
    }
}

void DocumentThreadableLoader::makeSimpleCrossOriginAccessRequest(ResourceRequest&& request)
{
    ASSERT(m_options.preflightPolicy != PreflightPolicy::Force || shouldPerformSecurityChecks());
    ASSERT(m_options.preflightPolicy == PreflightPolicy::Prevent || isSimpleCrossOriginAccessRequest(request.httpMethod(), request.httpHeaderFields()) || shouldPerformSecurityChecks());

    updateRequestForAccessControl(request, securityOrigin(), m_options.storedCredentialsPolicy);
    loadRequest(WTFMove(request), SecurityCheckPolicy::DoSecurityCheck);
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

void DocumentThreadableLoader::computeIsDone()
{
    if (!m_async || m_preflightChecker || !m_resource) {
        if (m_client)
            m_client->notifyIsDone(m_async && !m_preflightChecker && !m_resource);
        return;
    }
    platformStrategies()->loaderStrategy()->isResourceLoadFinished(*m_resource, [this, weakThis = makeWeakPtr(*this)](bool isDone) {
        if (weakThis && m_client)
            m_client->notifyIsDone(isDone);
    });
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

void DocumentThreadableLoader::redirectReceived(CachedResource& resource, ResourceRequest&& request, const ResourceResponse& redirectResponse, CompletionHandler<void(ResourceRequest&&)>&& completionHandler)
{
    ASSERT(m_client);
    ASSERT_UNUSED(resource, &resource == m_resource);

    Ref<DocumentThreadableLoader> protectedThis(*this);
    --m_options.maxRedirectCount;

    // FIXME: We restrict this check to Fetch API for the moment, as this might disrupt WorkerScriptLoader.
    // Reassess this check based on https://github.com/whatwg/fetch/issues/393 discussions.
    // We should also disable that check in navigation mode.
    if (!request.url().protocolIsInHTTPFamily() && m_options.initiator == cachedResourceRequestInitiators().fetch) {
        reportRedirectionWithBadScheme(request.url());
        clearResource();
        return completionHandler(WTFMove(request));
    }

    if (platformStrategies()->loaderStrategy()->havePerformedSecurityChecks(redirectResponse)) {
        completionHandler(WTFMove(request));
        return;
    }

    if (!isAllowedByContentSecurityPolicy(request.url(), redirectResponse.isNull() ? ContentSecurityPolicy::RedirectResponseReceived::No : ContentSecurityPolicy::RedirectResponseReceived::Yes, redirectResponse.url())) {
        reportContentSecurityPolicyError(redirectResponse.url());
        clearResource();
        return completionHandler(WTFMove(request));
    }

    // Allow same origin requests to continue after allowing clients to audit the redirect.
    if (isAllowedRedirect(request.url()))
        return completionHandler(WTFMove(request));

    // Force any subsequent request to use these checks.
    m_sameOriginRequest = false;

    ASSERT(m_resource);
    ASSERT(m_originalHeaders);

    // Use a unique for subsequent loads if needed.
    // https://fetch.spec.whatwg.org/#concept-http-redirect-fetch (Step 10).
    ASSERT(m_options.mode == FetchOptions::Mode::Cors);
    if (!securityOrigin().canRequest(redirectResponse.url()) && !protocolHostAndPortAreEqual(redirectResponse.url(), request.url()))
        m_origin = SecurityOrigin::createUnique();

    // Except in case where preflight is needed, loading should be able to continue on its own.
    // But we also handle credentials here if it is restricted to SameOrigin.
    if (m_options.credentials != FetchOptions::Credentials::SameOrigin && m_simpleRequest && isSimpleCrossOriginAccessRequest(request.httpMethod(), *m_originalHeaders))
        return completionHandler(WTFMove(request));

    if (m_options.credentials == FetchOptions::Credentials::SameOrigin)
        m_options.storedCredentialsPolicy = StoredCredentialsPolicy::DoNotUse;

    clearResource();

    m_referrer = request.httpReferrer();
    if (m_referrer.isNull())
        m_options.referrerPolicy = ReferrerPolicy::NoReferrer;

    // Let's fetch the request with the original headers (equivalent to request cloning specified by fetch algorithm).
    // Do not copy the Authorization header if removed by the network layer.
    if (!request.httpHeaderFields().contains(HTTPHeaderName::Authorization))
        m_originalHeaders->remove(HTTPHeaderName::Authorization);
    request.setHTTPHeaderFields(*m_originalHeaders);

#if ENABLE(SERVICE_WORKER)
    if (redirectResponse.source() != ResourceResponse::Source::ServiceWorker && redirectResponse.source() != ResourceResponse::Source::MemoryCache)
        m_options.serviceWorkersMode = ServiceWorkersMode::None;
#endif
    makeCrossOriginAccessRequest(ResourceRequest(request));
    completionHandler(WTFMove(request));
}

void DocumentThreadableLoader::dataSent(CachedResource& resource, unsigned long long bytesSent, unsigned long long totalBytesToBeSent)
{
    ASSERT(m_client);
    ASSERT_UNUSED(resource, &resource == m_resource);
    m_client->didSendData(bytesSent, totalBytesToBeSent);
}

void DocumentThreadableLoader::responseReceived(CachedResource& resource, const ResourceResponse& response, CompletionHandler<void()>&& completionHandler)
{
    ASSERT_UNUSED(resource, &resource == m_resource);
    if (!m_responsesCanBeOpaque) {
        ResourceResponse responseWithoutTainting = response;
        responseWithoutTainting.setTainting(ResourceResponse::Tainting::Basic);
        didReceiveResponse(m_resource->identifier(), responseWithoutTainting);
    } else
        didReceiveResponse(m_resource->identifier(), response);

    if (completionHandler)
        completionHandler();
}

void DocumentThreadableLoader::didReceiveResponse(unsigned long identifier, const ResourceResponse& response)
{
    ASSERT(m_client);
    ASSERT(response.type() != ResourceResponse::Type::Error);

    InspectorInstrumentation::didReceiveThreadableLoaderResponse(*this, identifier);

    if (m_delayCallbacksForIntegrityCheck)
        return;

    if (options().filteringPolicy == ResponseFilteringPolicy::Disable) {
        m_client->didReceiveResponse(identifier, response);
        return;
    }

    if (response.type() == ResourceResponse::Type::Default) {
        m_client->didReceiveResponse(identifier, ResourceResponse::filter(response, m_options.credentials == FetchOptions::Credentials::Include ? ResourceResponse::PerformExposeAllHeadersCheck::No : ResourceResponse::PerformExposeAllHeadersCheck::Yes));
        if (response.tainting() == ResourceResponse::Tainting::Opaque) {
            clearResource();
            if (m_client)
                m_client->didFinishLoading(identifier);
        }
        return;
    }
    ASSERT(response.type() == ResourceResponse::Type::Opaqueredirect || response.source() == ResourceResponse::Source::ServiceWorker || response.source() == ResourceResponse::Source::MemoryCache);
    m_client->didReceiveResponse(identifier, response);
}

void DocumentThreadableLoader::dataReceived(CachedResource& resource, const uint8_t* data, int dataLength)
{
    ASSERT_UNUSED(resource, &resource == m_resource);
    didReceiveData(m_resource->identifier(), data, dataLength);
}

void DocumentThreadableLoader::didReceiveData(unsigned long, const uint8_t* data, int dataLength)
{
    ASSERT(m_client);

    if (m_delayCallbacksForIntegrityCheck)
        return;

    m_client->didReceiveData(data, dataLength);
}

void DocumentThreadableLoader::finishedTimingForWorkerLoad(CachedResource& resource, const ResourceTiming& resourceTiming)
{
    ASSERT(m_client);
    ASSERT_UNUSED(resource, &resource == m_resource);

    finishedTimingForWorkerLoad(resourceTiming);
}

void DocumentThreadableLoader::finishedTimingForWorkerLoad(const ResourceTiming& resourceTiming)
{
    ASSERT(m_options.initiatorContext == InitiatorContext::Worker);

    m_client->didFinishTiming(resourceTiming);
}

void DocumentThreadableLoader::notifyFinished(CachedResource& resource, const NetworkLoadMetrics&)
{
    ASSERT(m_client);
    ASSERT_UNUSED(resource, &resource == m_resource);

    if (m_resource->errorOccurred())
        didFail(m_resource->identifier(), m_resource->resourceError());
    else
        didFinishLoading(m_resource->identifier());
}

void DocumentThreadableLoader::didFinishLoading(unsigned long identifier)
{
    ASSERT(m_client);

    if (m_delayCallbacksForIntegrityCheck) {
        if (!matchIntegrityMetadata(*m_resource, m_options.integrity)) {
            reportIntegrityMetadataError(*m_resource, m_options.integrity);
            return;
        }

        auto response = m_resource->response();

        if (options().filteringPolicy == ResponseFilteringPolicy::Disable) {
            m_client->didReceiveResponse(identifier, response);
            if (auto* buffer = m_resource->resourceBuffer()) {
                buffer->forEachSegment([&](auto& segment) {
                    m_client->didReceiveData(segment.data(), segment.size());
                });
            }
        } else {
            ASSERT(response.type() == ResourceResponse::Type::Default);

            m_client->didReceiveResponse(identifier, ResourceResponse::filter(response, m_options.credentials == FetchOptions::Credentials::Include ? ResourceResponse::PerformExposeAllHeadersCheck::No : ResourceResponse::PerformExposeAllHeadersCheck::Yes));
            if (auto* buffer = m_resource->resourceBuffer()) {
                buffer->forEachSegment([&](auto& segment) {
                    m_client->didReceiveData(segment.data(), segment.size());
                });
            }
        }
    }

    m_client->didFinishLoading(identifier);
}

void DocumentThreadableLoader::didFail(unsigned long, const ResourceError& error)
{
    ASSERT(m_client);
#if ENABLE(SERVICE_WORKER)
    if (m_bypassingPreflightForServiceWorkerRequest && error.isCancellation()) {
        clearResource();

        m_options.serviceWorkersMode = ServiceWorkersMode::None;
        makeCrossOriginAccessRequestWithPreflight(WTFMove(m_bypassingPreflightForServiceWorkerRequest.value()));
        ASSERT(m_bypassingPreflightForServiceWorkerRequest->isNull());
        m_bypassingPreflightForServiceWorkerRequest = std::nullopt;
        return;
    }
#endif

    if (m_shouldLogError == ShouldLogError::Yes)
        logError(m_document, error, m_options.initiator);

    m_client->didFail(error);
}

void DocumentThreadableLoader::preflightSuccess(ResourceRequest&& request)
{
    ResourceRequest actualRequest(WTFMove(request));
    updateRequestForAccessControl(actualRequest, securityOrigin(), m_options.storedCredentialsPolicy);

    m_preflightChecker = std::nullopt;

    // It should be ok to skip the security check since we already asked about the preflight request.
    loadRequest(WTFMove(actualRequest), SecurityCheckPolicy::SkipSecurityCheck);
}

void DocumentThreadableLoader::preflightFailure(unsigned long identifier, const ResourceError& error)
{
    m_preflightChecker = std::nullopt;

    InspectorInstrumentation::didFailLoading(m_document.frame(), m_document.frame()->loader().documentLoader(), identifier, error);

    if (m_shouldLogError == ShouldLogError::Yes)
        logError(m_document, error, m_options.initiator);

    m_client->didFail(error);
}

void DocumentThreadableLoader::loadRequest(ResourceRequest&& request, SecurityCheckPolicy securityCheck)
{
    Ref<DocumentThreadableLoader> protectedThis(*this);

    // Any credential should have been removed from the cross-site requests.
    const URL& requestURL = request.url();
    m_options.securityCheck = securityCheck;
    ASSERT(m_sameOriginRequest || !requestURL.hasCredentials());

    if (!m_referrer.isNull())
        request.setHTTPReferrer(m_referrer);

    if (m_async) {
        ResourceLoaderOptions options = m_options;
        options.clientCredentialPolicy = m_sameOriginRequest ? ClientCredentialPolicy::MayAskClientForCredentials : ClientCredentialPolicy::CannotAskClientForCredentials;
        options.contentSecurityPolicyImposition = ContentSecurityPolicyImposition::SkipPolicyCheck;
        
        // If there is integrity metadata to validate, we must buffer.
        if (!m_options.integrity.isEmpty())
            options.dataBufferingPolicy = DataBufferingPolicy::BufferData;

        request.setAllowCookies(m_options.storedCredentialsPolicy == StoredCredentialsPolicy::Use);
        CachedResourceRequest newRequest(WTFMove(request), options);
        newRequest.setInitiator(m_options.initiator);
        newRequest.setOrigin(securityOrigin());

        ASSERT(!m_resource);
        if (m_resource) {
            CachedResourceHandle<CachedRawResource> resource = std::exchange(m_resource, nullptr);
            resource->removeClient(*this);
        }

        auto cachedResource = m_document.cachedResourceLoader().requestRawResource(WTFMove(newRequest));
        m_resource = cachedResource.value_or(nullptr);
        if (m_resource)
            m_resource->addClient(*this);
        else
            logErrorAndFail(cachedResource.error());
        return;
    }

    // If credentials mode is 'Omit', we should disable cookie sending.
    ASSERT(m_options.credentials != FetchOptions::Credentials::Omit);

    ResourceLoadTiming loadTiming;
    loadTiming.markStartTime();

    // FIXME: ThreadableLoaderOptions.sniffContent is not supported for synchronous requests.
    RefPtr<SharedBuffer> data;
    ResourceError error;
    ResourceResponse response;
    unsigned long identifier = std::numeric_limits<unsigned long>::max();
    if (auto* frame = m_document.frame()) {
        if (!MixedContentChecker::canRunInsecureContent(*frame, m_document.securityOrigin(), requestURL))
            return;
        auto& frameLoader = frame->loader();
        identifier = frameLoader.loadResourceSynchronously(request, m_options.clientCredentialPolicy, m_options, *m_originalHeaders, error, response, data);
    }

    loadTiming.markEndTime();

    if (!error.isNull() && response.httpStatusCode() <= 0) {
        if (requestURL.isLocalFile()) {
            // We don't want XMLHttpRequest to raise an exception for file:// resources, see <rdar://problem/4962298>.
            // FIXME: XMLHttpRequest quirks should be in XMLHttpRequest code, not in DocumentThreadableLoader.cpp.
            didReceiveResponse(identifier, response);
            didFinishLoading(identifier);
            return;
        }
        logErrorAndFail(error);
        return;
    }

    if (response.containsInvalidHTTPHeaders()) {
        didFail(identifier, ResourceError(errorDomainWebKitInternal, 0, request.url(), "Response contained invalid HTTP headers", ResourceError::Type::General));
        return;
    }

    if (!shouldPerformSecurityChecks()) {
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

        if (!m_sameOriginRequest) {
            if (m_options.mode == FetchOptions::Mode::NoCors)
                response.setTainting(ResourceResponse::Tainting::Opaque);
            else {
                ASSERT(m_options.mode == FetchOptions::Mode::Cors);
                response.setTainting(ResourceResponse::Tainting::Cors);
                auto accessControlCheckResult = passesAccessControlCheck(response, m_options.storedCredentialsPolicy, securityOrigin(), &CrossOriginAccessControlCheckDisabler::singleton());
                if (!accessControlCheckResult) {
                    logErrorAndFail(ResourceError(errorDomainWebKitInternal, 0, response.url(), accessControlCheckResult.error(), ResourceError::Type::AccessControl));
                    return;
                }
            }
        }
    }
    didReceiveResponse(identifier, response);

    if (data) {
        data->forEachSegment([&](auto& segment) {
            didReceiveData(identifier, segment.data(), segment.size());
        });
    }

    const auto* timing = response.deprecatedNetworkLoadMetricsOrNull();
    auto resourceTiming = ResourceTiming::fromSynchronousLoad(requestURL, m_options.initiator, loadTiming, timing ? *timing : NetworkLoadMetrics { }, response, securityOrigin());
    if (options().initiatorContext == InitiatorContext::Worker)
        finishedTimingForWorkerLoad(resourceTiming);
    else {
        if (auto* window = document().domWindow())
            window->performance().addResourceTiming(WTFMove(resourceTiming));
    }

    didFinishLoading(identifier);
}

bool DocumentThreadableLoader::isAllowedByContentSecurityPolicy(const URL& url, ContentSecurityPolicy::RedirectResponseReceived redirectResponseReceived, const URL& preRedirectURL)
{
    switch (m_options.contentSecurityPolicyEnforcement) {
    case ContentSecurityPolicyEnforcement::DoNotEnforce:
        return true;
    case ContentSecurityPolicyEnforcement::EnforceChildSrcDirective:
        return contentSecurityPolicy().allowChildContextFromSource(url, redirectResponseReceived);
    case ContentSecurityPolicyEnforcement::EnforceConnectSrcDirective:
        return contentSecurityPolicy().allowConnectToSource(url, redirectResponseReceived, preRedirectURL);
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

const CrossOriginEmbedderPolicy& DocumentThreadableLoader::crossOriginEmbedderPolicy() const
{
    if (m_crossOriginEmbedderPolicy)
        return *m_crossOriginEmbedderPolicy;
    return m_document.crossOriginEmbedderPolicy();
}

void DocumentThreadableLoader::reportRedirectionWithBadScheme(const URL& url)
{
    logErrorAndFail(ResourceError(errorDomainWebKitInternal, 0, url, "Redirection to URL with a scheme that is not HTTP(S)."_s, ResourceError::Type::AccessControl));
}

void DocumentThreadableLoader::reportContentSecurityPolicyError(const URL& url)
{
    logErrorAndFail(ResourceError(errorDomainWebKitInternal, 0, url, "Blocked by Content Security Policy."_s, ResourceError::Type::AccessControl));
}

void DocumentThreadableLoader::reportCrossOriginResourceSharingError(const URL& url)
{
    logErrorAndFail(ResourceError(errorDomainWebKitInternal, 0, url, "Cross-origin redirection denied by Cross-Origin Resource Sharing policy."_s, ResourceError::Type::AccessControl));
}

void DocumentThreadableLoader::reportIntegrityMetadataError(const CachedResource& resource, const String& expectedMetadata)
{
    logErrorAndFail(ResourceError(errorDomainWebKitInternal, 0, resource.url(), makeString("Failed integrity metadata check. "_s, integrityMismatchDescription(resource, expectedMetadata)), ResourceError::Type::AccessControl));
}

void DocumentThreadableLoader::logErrorAndFail(const ResourceError& error)
{
    if (m_shouldLogError == ShouldLogError::Yes) {
        if (error.isAccessControl() && !error.localizedDescription().isEmpty())
            m_document.addConsoleMessage(MessageSource::Security, MessageLevel::Error, error.localizedDescription());
        logError(m_document, error, m_options.initiator);
    }
    ASSERT(m_client);
    m_client->didFail(error);
}

} // namespace WebCore

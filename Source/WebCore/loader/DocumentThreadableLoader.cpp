/*
 * Copyright (C) 2011, 2012 Google Inc. All rights reserved.
 * Copyright (C) 2011-2025 Apple Inc. All rights reserved.
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
#include "CachedResourceRequest.h"
#include "CachedResourceRequestInitiatorTypes.h"
#include "CrossOriginAccessControl.h"
#include "CrossOriginPreflightChecker.h"
#include "CrossOriginPreflightResultCache.h"
#include "DocumentInlines.h"
#include "DocumentLoader.h"
#include "DocumentResourceLoader.h"
#include "DocumentView.h"
#include "FrameDestructionObserverInlines.h"
#include "FrameLoader.h"
#include "InspectorInstrumentation.h"
#include "InspectorNetworkAgent.h"
#include "LegacySchemeRegistry.h"
#include "LoaderStrategy.h"
#include "LocalDOMWindow.h"
#include "LocalFrame.h"
#include "MixedContentChecker.h"
#include "OriginAccessPatterns.h"
#include "Performance.h"
#include "PlatformStrategies.h"
#include "ProgressTracker.h"
#include "ResourceError.h"
#include "ResourceRequest.h"
#include "ResourceTiming.h"
#include "SecurityOrigin.h"
#include "Settings.h"
#include "SharedBuffer.h"
#include "SubresourceIntegrity.h"
#include "SubresourceLoader.h"
#include "ThreadableLoaderClient.h"
#include <wtf/Assertions.h>
#include <wtf/Ref.h>
#include <wtf/text/MakeString.h>

namespace WebCore {

void DocumentThreadableLoader::loadResourceSynchronously(Document& document, ResourceRequest&& request, ThreadableLoaderClient& client, const ThreadableLoaderOptions& options, RefPtr<SecurityOrigin>&& origin, std::unique_ptr<ContentSecurityPolicy>&& contentSecurityPolicy, std::optional<CrossOriginEmbedderPolicy>&& crossOriginEmbedderPolicy)
{
    // The loader will be deleted as soon as this function exits.
    Ref loader = adoptRef(*new DocumentThreadableLoader(document, client, LoadSynchronously, WTFMove(request), options, WTFMove(origin), WTFMove(contentSecurityPolicy), WTFMove(crossOriginEmbedderPolicy), String(), ShouldLogError::Yes));
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
    RefPtr loader = adoptRef(new DocumentThreadableLoader(document, client, LoadAsynchronously, WTFMove(request), options, WTFMove(origin), WTFMove(contentSecurityPolicy), WTFMove(crossOriginEmbedderPolicy), WTFMove(referrer), shouldLogError));
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

    if (m_options.serviceWorkersMode == ServiceWorkersMode::All && m_async)
        return m_options.serviceWorkerRegistrationIdentifier || m_document->activeServiceWorker();

    return false;
}

DocumentThreadableLoader::DocumentThreadableLoader(Document& document, ThreadableLoaderClient& client, BlockingBehavior blockingBehavior, ResourceRequest&& request, const ThreadableLoaderOptions& options, RefPtr<SecurityOrigin>&& origin, std::unique_ptr<ContentSecurityPolicy>&& contentSecurityPolicy, std::optional<CrossOriginEmbedderPolicy>&& crossOriginEmbedderPolicy, String&& referrer, ShouldLogError shouldLogError)
    : m_client(&client)
    , m_document(document)
    , m_options(options)
    , m_origin(WTFMove(origin))
    , m_referrer(WTFMove(referrer))
    , m_sameOriginRequest(protectedSecurityOrigin()->canRequest(request.url(), OriginAccessPatternsForWebProcess::singleton()))
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

    if (!m_async && (!document.page() || !document.page()->areSynchronousLoadsAllowed())) {
        document.didRejectSyncXHRDuringPageDismissal();
        logErrorAndFail(ResourceError(errorDomainWebKitInternal, 0, request.url(), "Synchronous loads are not allowed at this time"_s));
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

    bool shouldDisableCORS = false;
    if (RefPtr page = document.page()) {
        shouldDisableCORS = page->hasInjectedUserScript() && LegacySchemeRegistry::isUserExtensionScheme(request.url().protocol());
        shouldDisableCORS |= page->shouldDisableCorsForRequestTo(request.url());
    }

    if (shouldDisableCORS) {
        m_options.mode = FetchOptions::Mode::NoCors;
        m_options.filteringPolicy = ResponseFilteringPolicy::Disable;
        m_responsesCanBeOpaque = false;
    }

    m_options.cspResponseHeaders = m_options.contentSecurityPolicyEnforcement != ContentSecurityPolicyEnforcement::DoNotEnforce ? checkedContentSecurityPolicy()->responseHeaders() : ContentSecurityPolicyResponseHeaders { };
    m_options.crossOriginEmbedderPolicy = this->crossOriginEmbedderPolicy();

    // As per step 11 of https://fetch.spec.whatwg.org/#main-fetch, data scheme (if same-origin data-URL flag is set) and about scheme are considered same-origin.
    if (request.url().protocolIsData())
        m_sameOriginRequest = options.sameOriginDataURLFlag == SameOriginDataURLFlag::Set;

    if (m_sameOriginRequest || m_options.mode == FetchOptions::Mode::NoCors || m_options.mode == FetchOptions::Mode::Navigate) {
        loadRequest(WTFMove(request), SecurityCheckPolicy::DoSecurityCheck);
        return;
    }

    if (m_options.mode == FetchOptions::Mode::SameOrigin) {
        logErrorAndFail(ResourceError(errorDomainWebKitInternal, 0, request.url(), "Cross origin requests are not allowed when using same-origin fetch mode."_s));
        return;
    }

    makeCrossOriginAccessRequest(WTFMove(request));
}

bool DocumentThreadableLoader::checkURLSchemeAsCORSEnabled(const URL& url)
{
    // Cross-origin requests are only allowed for HTTP and registered schemes. We would catch this when checking response headers later, but there is no reason to send a request that's guaranteed to be denied.
    if (!LegacySchemeRegistry::shouldTreatURLSchemeAsCORSEnabled(url.protocol())) {
        logErrorAndFail(ResourceError(errorDomainWebKitInternal, 0, url, "Cross origin requests are only supported for HTTP."_s, ResourceError::Type::AccessControl));
        return false;
    }
    return true;
}

void DocumentThreadableLoader::makeCrossOriginAccessRequest(ResourceRequest&& request)
{
    ASSERT(m_options.mode == FetchOptions::Mode::Cors);

    if ((m_options.preflightPolicy == PreflightPolicy::Consider && isSimpleCrossOriginAccessRequest(request.httpMethod(), request.httpHeaderFields())) || m_options.preflightPolicy == PreflightPolicy::Prevent || shouldPerformSecurityChecks()) {
        if (checkURLSchemeAsCORSEnabled(request.url()))
            makeSimpleCrossOriginAccessRequest(WTFMove(request));
    } else {
        if (m_options.serviceWorkersMode == ServiceWorkersMode::All && m_async) {
            if (m_options.serviceWorkerRegistrationIdentifier || document().activeServiceWorker()) {
                ASSERT(!m_bypassingPreflightForServiceWorkerRequest);
                m_bypassingPreflightForServiceWorkerRequest = WTFMove(request);
                m_options.serviceWorkersMode = ServiceWorkersMode::Only;
                loadRequest(ResourceRequest { m_bypassingPreflightForServiceWorkerRequest.value() }, SecurityCheckPolicy::SkipSecurityCheck);
                return;
            }
        }
        if (!checkURLSchemeAsCORSEnabled(request.url()))
            return;

        m_simpleRequest = false;
        if (RefPtr page = document().page(); page && CrossOriginPreflightResultCache::singleton().canSkipPreflight(page->sessionID(), document().clientOrigin(), request.url(), m_options.storedCredentialsPolicy, request.httpMethod(), request.httpHeaderFields()))
            preflightSuccess(WTFMove(request));
        else
            makeCrossOriginAccessRequestWithPreflight(WTFMove(request));
    }
}

void DocumentThreadableLoader::makeSimpleCrossOriginAccessRequest(ResourceRequest&& request)
{
    ASSERT(m_options.preflightPolicy != PreflightPolicy::Force || shouldPerformSecurityChecks());
    ASSERT(m_options.preflightPolicy == PreflightPolicy::Prevent || isSimpleCrossOriginAccessRequest(request.httpMethod(), request.httpHeaderFields()) || shouldPerformSecurityChecks());

    updateRequestForAccessControl(request, protectedSecurityOrigin(), m_options.storedCredentialsPolicy);
    loadRequest(WTFMove(request), SecurityCheckPolicy::DoSecurityCheck);
}

void DocumentThreadableLoader::makeCrossOriginAccessRequestWithPreflight(ResourceRequest&& request)
{
    if (m_async) {
        Ref preflightChecker = CrossOriginPreflightChecker::create(*this, WTFMove(request));
        m_preflightChecker = preflightChecker.copyRef();
        preflightChecker->startPreflight();
        return;
    }
    CrossOriginPreflightChecker::doPreflight(*this, WTFMove(request));
}

DocumentThreadableLoader::~DocumentThreadableLoader()
{
    if (CachedResourceHandle resource = m_resource)
        resource->removeClient(*this);
}

void DocumentThreadableLoader::cancel()
{
    Ref protectedThis { *this };

    // Cancel can re-enter and m_resource might be null here as a result.
    if (m_client && m_resource) {
        // FIXME: This error is sent to the client in didFail(), so it should not be an internal one. Use LocalFrameLoaderClient::cancelledError() instead.
        ResourceError error(errorDomainWebKitInternal, 0, m_resource->url(), "Load cancelled"_s, ResourceError::Type::Cancellation);
        m_client->didFail(m_document->identifier(), error); // May destroy the client.
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
    platformStrategies()->loaderStrategy()->isResourceLoadFinished(*protectedResource(), [weakThis = WeakPtr { *this }](bool isDone) {
        RefPtr protectedThis = weakThis.get();
        if (protectedThis && protectedThis->m_client)
            protectedThis->m_client->notifyIsDone(isDone);
    });
}

CachedResourceHandle<CachedRawResource> DocumentThreadableLoader::protectedResource() const
{
    return m_resource;
}

void DocumentThreadableLoader::setDefersLoading(bool value)
{
    if (CachedResourceHandle resource = m_resource)
        resource->setDefersLoading(value);
    if (m_preflightChecker)
        m_preflightChecker->setDefersLoading(value);
}

void DocumentThreadableLoader::clearResource()
{
    // Script can cancel and restart a request reentrantly within removeClient(),
    // which could lead to calling CachedResource::removeClient() multiple times for
    // this DocumentThreadableLoader. Save off a copy of m_resource and clear it to
    // prevent the reentrancy.
    if (CachedResourceHandle resource = m_resource) {
        m_resource = nullptr;
        resource->removeClient(*this);
    }
    m_preflightChecker = nullptr;
}

void DocumentThreadableLoader::redirectReceived(CachedResource& resource, ResourceRequest&& request, const ResourceResponse& redirectResponse, CompletionHandler<void(ResourceRequest&&)>&& completionHandler)
{
    ASSERT(m_client);
    ASSERT_UNUSED(resource, &resource == m_resource);

    Ref protectedThis { *this };
    --m_options.maxRedirectCount;

    m_responseURL = request.url();

    // FIXME: We restrict this check to Fetch API for the moment, as this might disrupt WorkerScriptLoader.
    // Reassess this check based on https://github.com/whatwg/fetch/issues/393 discussions.
    // We should also disable that check in navigation mode.
    if (!request.url().protocolIsInHTTPFamily() && m_options.initiatorType == cachedResourceRequestInitiatorTypes().fetch) {
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
    if (!protectedSecurityOrigin()->canRequest(redirectResponse.url(), OriginAccessPatternsForWebProcess::singleton()) && !protocolHostAndPortAreEqual(redirectResponse.url(), request.url()))
        m_origin = SecurityOrigin::createOpaque();

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

    if (redirectResponse.source() != ResourceResponse::Source::ServiceWorker && redirectResponse.source() != ResourceResponse::Source::MemoryCache)
        m_options.serviceWorkersMode = ServiceWorkersMode::None;
    makeCrossOriginAccessRequest(ResourceRequest(request));
    completionHandler(WTFMove(request));
}

void DocumentThreadableLoader::dataSent(CachedResource& resource, unsigned long long bytesSent, unsigned long long totalBytesToBeSent)
{
    ASSERT(m_client);
    ASSERT_UNUSED(resource, &resource == m_resource);
    m_client->didSendData(bytesSent, totalBytesToBeSent);
}

void DocumentThreadableLoader::responseReceived(const CachedResource& resource, const ResourceResponse& response, CompletionHandler<void()>&& completionHandler)
{
    ASSERT_UNUSED(resource, &resource == m_resource);
    ResourceResponse responseWithCorrectFragmentIdentifier;
    if (response.source() != ResourceResponse::Source::ServiceWorker && response.url().fragmentIdentifier() != m_responseURL.fragmentIdentifier()) {
        responseWithCorrectFragmentIdentifier = response;
        responseWithCorrectFragmentIdentifier.setURL(URL { m_responseURL });
    }

    if (!m_responsesCanBeOpaque) {
        ResourceResponse responseWithoutTainting = responseWithCorrectFragmentIdentifier.isNull() ? response : WTFMove(responseWithCorrectFragmentIdentifier);
        responseWithoutTainting.setTainting(ResourceResponse::Tainting::Basic);
        didReceiveResponse(*m_resource->resourceLoaderIdentifier(), WTFMove(responseWithoutTainting));
    } else
        didReceiveResponse(*m_resource->resourceLoaderIdentifier(), responseWithCorrectFragmentIdentifier.isNull() ? ResourceResponse { response } : WTFMove(responseWithCorrectFragmentIdentifier));

    if (completionHandler)
        completionHandler();
}

void DocumentThreadableLoader::didReceiveResponse(ResourceLoaderIdentifier identifier, ResourceResponse&& response)
{
    ASSERT(m_client);
    ASSERT(response.type() != ResourceResponse::Type::Error);

    // https://fetch.spec.whatwg.org/commit-snapshots/6257e220d70f560a037e46f1b4206325400db8dc/#main-fetch step 17.
    if (response.source() == ResourceResponse::Source::ServiceWorker && response.url() != m_resource->url()) {
        if (!isResponseAllowedByContentSecurityPolicy(response)) {
            reportContentSecurityPolicyError(response.url());
            return;
        }
    }

    InspectorInstrumentation::didReceiveThreadableLoaderResponse(document(), *this, identifier);

    if (m_delayCallbacksForIntegrityCheck)
        return;

    if (options().filteringPolicy == ResponseFilteringPolicy::Disable) {
        m_client->didReceiveResponse(m_document->identifier(), identifier, WTFMove(response));
        return;
    }

    if (response.type() == ResourceResponse::Type::Default) {
        m_client->didReceiveResponse(m_document->identifier(), identifier, ResourceResponse::filter(response, m_options.credentials == FetchOptions::Credentials::Include ? ResourceResponse::PerformExposeAllHeadersCheck::No : ResourceResponse::PerformExposeAllHeadersCheck::Yes));
        if (response.tainting() == ResourceResponse::Tainting::Opaque) {
            clearResource();
            if (m_client)
                m_client->didFinishLoading(m_document->identifier(), identifier, { });
        }
        return;
    }
    ASSERT(response.type() == ResourceResponse::Type::Opaqueredirect || response.source() == ResourceResponse::Source::ServiceWorker || response.source() == ResourceResponse::Source::MemoryCache);
    m_client->didReceiveResponse(m_document->identifier(), identifier, WTFMove(response));
}

void DocumentThreadableLoader::dataReceived(CachedResource& resource, const SharedBuffer& buffer)
{
    ASSERT_UNUSED(resource, &resource == m_resource);
    didReceiveData(buffer);
}

void DocumentThreadableLoader::didReceiveData(const SharedBuffer& buffer)
{
    ASSERT(m_client);

    if (m_delayCallbacksForIntegrityCheck)
        return;

    m_client->didReceiveData(buffer);
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

void DocumentThreadableLoader::notifyFinished(CachedResource& resource, const NetworkLoadMetrics& metrics, LoadWillContinueInAnotherProcess)
{
    ASSERT(m_client);
    ASSERT_UNUSED(resource, &resource == m_resource);

    if (m_resource->errorOccurred())
        didFail(m_resource->resourceLoaderIdentifier(), m_resource->resourceError());
    else
        didFinishLoading(m_resource->resourceLoaderIdentifier(), metrics);
}

void DocumentThreadableLoader::didFinishLoading(std::optional<ResourceLoaderIdentifier> identifier, const NetworkLoadMetrics& metrics)
{
    ASSERT(m_client);
    RefPtr document = m_document.get();
    if (!document)
        return;

    if (m_delayCallbacksForIntegrityCheck) {
        CachedResourceHandle resource = m_resource;
        if (!matchIntegrityMetadata(*resource, m_options.integrity)) {
            reportIntegrityMetadataError(*resource, m_options.integrity);
            return;
        }

        auto response = resource->response();

        RefPtr<SharedBuffer> buffer;
        if (resource->resourceBuffer())
            buffer = resource->resourceBuffer()->makeContiguous();
        if (options().filteringPolicy == ResponseFilteringPolicy::Disable) {
            m_client->didReceiveResponse(document->identifier(), identifier, WTFMove(response));
            if (buffer)
                m_client->didReceiveData(*buffer);
        } else {
            ASSERT(response.type() == ResourceResponse::Type::Default);

            m_client->didReceiveResponse(document->identifier(), identifier, ResourceResponse::filter(response, m_options.credentials == FetchOptions::Credentials::Include ? ResourceResponse::PerformExposeAllHeadersCheck::No : ResourceResponse::PerformExposeAllHeadersCheck::Yes));
            if (buffer)
                m_client->didReceiveData(*buffer);
        }
    }

    m_client->didFinishLoading(document->identifier(), identifier, metrics);
}

void DocumentThreadableLoader::didFail(std::optional<ResourceLoaderIdentifier>, const ResourceError& error)
{
    ASSERT(m_client);
    if (m_bypassingPreflightForServiceWorkerRequest && error.isCancellation()) {
        clearResource();

        m_options.serviceWorkersMode = ServiceWorkersMode::None;
        makeCrossOriginAccessRequestWithPreflight(*std::exchange(m_bypassingPreflightForServiceWorkerRequest, std::nullopt));
        return;
    }

    if (m_shouldLogError == ShouldLogError::Yes)
        logError(protectedDocument(), error, m_options.initiatorType);

    RefPtr document = m_document.get();
    if (!document)
        return;
    if (m_client)
        m_client->didFail(document->identifier(), error); // May cause the client to get destroyed.
}

Ref<Document> DocumentThreadableLoader::protectedDocument()
{
    return *m_document;
}

void DocumentThreadableLoader::preflightSuccess(ResourceRequest&& request)
{
    ResourceRequest actualRequest(WTFMove(request));
    updateRequestForAccessControl(actualRequest, protectedSecurityOrigin(), m_options.storedCredentialsPolicy);

    m_preflightChecker = nullptr;

    // It should be ok to skip the security check since we already asked about the preflight request.
    loadRequest(WTFMove(actualRequest), SecurityCheckPolicy::SkipSecurityCheck);
}

void DocumentThreadableLoader::preflightFailure(std::optional<ResourceLoaderIdentifier> identifier, const ResourceError& error)
{
    m_preflightChecker = nullptr;

    RefPtr frame = m_document->frame();
    if (identifier)
        InspectorInstrumentation::didFailLoading(frame.get(), frame->loader().protectedDocumentLoader().get(), *identifier, error);

    if (m_shouldLogError == ShouldLogError::Yes)
        logError(protectedDocument(), error, m_options.initiatorType);

    m_client->didFail(m_document->identifier(), error);
}

void DocumentThreadableLoader::loadRequest(ResourceRequest&& request, SecurityCheckPolicy securityCheck)
{
    Ref<DocumentThreadableLoader> protectedThis(*this);

    // Any credential should have been removed from the cross-site requests.
    m_responseURL = request.url();

    const URL& requestURL = request.url();
    m_options.securityCheck = securityCheck;
    ASSERT(m_sameOriginRequest || !requestURL.hasCredentials());

    if (!m_referrer.isNull())
        request.setHTTPReferrer(m_referrer);

    if (m_async) {
        ResourceLoaderOptions options = m_options;
        options.loadedFromFetch = m_options.initiatorType == cachedResourceRequestInitiatorTypes().fetch ? LoadedFromFetch::Yes : LoadedFromFetch::No;
        options.clientCredentialPolicy = m_sameOriginRequest ? ClientCredentialPolicy::MayAskClientForCredentials : ClientCredentialPolicy::CannotAskClientForCredentials;
        options.contentSecurityPolicyImposition = ContentSecurityPolicyImposition::SkipPolicyCheck;
        
        // If there is integrity metadata to validate, we must buffer.
        if (!m_options.integrity.isEmpty())
            options.dataBufferingPolicy = DataBufferingPolicy::BufferData;

        request.setAllowCookies(m_options.storedCredentialsPolicy == StoredCredentialsPolicy::Use);
        CachedResourceRequest newRequest(WTFMove(request), options);
        newRequest.setInitiatorType(AtomString { m_options.initiatorType });
        newRequest.setOrigin(protectedSecurityOrigin());

        ASSERT(!m_resource);
        if (CachedResourceHandle resource = std::exchange(m_resource, nullptr))
            resource->removeClient(*this);

        auto cachedResource = m_document->protectedCachedResourceLoader()->requestRawResource(WTFMove(newRequest));
        m_resource = cachedResource.value_or(nullptr);
        if (CachedResourceHandle resource = m_resource)
            resource->addClient(*this);
        else
            logErrorAndFail(cachedResource.error());
        return;
    }

    // If credentials mode is 'Omit', we should disable cookie sending.
    ASSERT(m_options.credentials != FetchOptions::Credentials::Omit);

    ResourceLoadTiming loadTiming;
    loadTiming.markStartTime();

    // FIXME: ThreadableLoaderOptions.sniffContent is not supported for synchronous requests.
    RefPtr frame = m_document->frame();
    if (!frame)
        return;

    if (MixedContentChecker::shouldBlockRequest(*frame, requestURL))
        return;

    RefPtr<SharedBuffer> data;
    ResourceError error;
    ResourceResponse response;
    Ref frameLoader = frame->loader();
    auto identifier = frameLoader->loadResourceSynchronously(request, m_options.clientCredentialPolicy, m_options, *m_originalHeaders, error, response, data);

    loadTiming.markEndTime();

    if (!error.isNull() && response.httpStatusCode() <= 0) {
        if (requestURL.protocolIsFile()) {
            // We don't want XMLHttpRequest to raise an exception for file:// resources, see <rdar://problem/4962298>.
            // FIXME: XMLHttpRequest quirks should be in XMLHttpRequest code, not in DocumentThreadableLoader.cpp.
            didReceiveResponse(identifier, WTFMove(response));
            didFinishLoading(identifier, { });
            return;
        }
        logErrorAndFail(error);
        return;
    }

    if (response.containsInvalidHTTPHeaders()) {
        didFail(identifier, badResponseHeadersError(request.url()));
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
                auto accessControlCheckResult = passesAccessControlCheck(response, m_options.storedCredentialsPolicy, protectedSecurityOrigin(), &CrossOriginAccessControlCheckDisabler::singleton());
                if (!accessControlCheckResult) {
                    logErrorAndFail(ResourceError(errorDomainWebKitInternal, 0, response.url(), accessControlCheckResult.error(), ResourceError::Type::AccessControl));
                    return;
                }
            }
        }
    }

    const auto* timing = response.deprecatedNetworkLoadMetricsOrNull();
    auto resourceTiming = ResourceTiming::fromSynchronousLoad(requestURL, m_options.initiatorType, loadTiming, timing ? *timing : NetworkLoadMetrics::emptyMetrics(), response, securityOrigin());

    didReceiveResponse(identifier, WTFMove(response));

    if (data)
        didReceiveData(*data);

    if (options().initiatorContext == InitiatorContext::Worker)
        finishedTimingForWorkerLoad(resourceTiming);
    else {
        if (RefPtr window = document().window())
            window->protectedPerformance()->addResourceTiming(WTFMove(resourceTiming));
    }

    didFinishLoading(identifier, { });
}

bool DocumentThreadableLoader::isAllowedByContentSecurityPolicy(const URL& url, ContentSecurityPolicy::RedirectResponseReceived redirectResponseReceived, const URL& preRedirectURL)
{
    switch (m_options.contentSecurityPolicyEnforcement) {
    case ContentSecurityPolicyEnforcement::DoNotEnforce:
        return true;
    case ContentSecurityPolicyEnforcement::EnforceWorkerSrcDirective:
        return checkedContentSecurityPolicy()->allowWorkerFromSource(url, redirectResponseReceived, preRedirectURL);
    case ContentSecurityPolicyEnforcement::EnforceConnectSrcDirective:
        return checkedContentSecurityPolicy()->allowConnectToSource(url, redirectResponseReceived, preRedirectURL);
    case ContentSecurityPolicyEnforcement::EnforceScriptSrcDirective:
        return checkedContentSecurityPolicy()->allowScriptFromSource(url, redirectResponseReceived, preRedirectURL, m_options.integrity, m_options.nonce);
    }
    ASSERT_NOT_REACHED();
    return false;
}

bool DocumentThreadableLoader::isResponseAllowedByContentSecurityPolicy(const ResourceResponse& response)
{
    return isAllowedByContentSecurityPolicy(response.url(), ContentSecurityPolicy::RedirectResponseReceived::Yes, { });
}

bool DocumentThreadableLoader::isAllowedRedirect(const URL& url)
{
    if (m_options.mode == FetchOptions::Mode::NoCors)
        return true;

    return m_sameOriginRequest && protectedSecurityOrigin()->canRequest(url, OriginAccessPatternsForWebProcess::singleton());
}

SecurityOrigin& DocumentThreadableLoader::securityOrigin() const
{
    return m_origin ? *m_origin : m_document->securityOrigin();
}

Ref<SecurityOrigin> DocumentThreadableLoader::topOrigin() const
{
    return m_document->topOrigin();
}

Ref<SecurityOrigin> DocumentThreadableLoader::protectedSecurityOrigin() const
{
    return securityOrigin();
}

const ContentSecurityPolicy& DocumentThreadableLoader::contentSecurityPolicy() const
{
    if (m_contentSecurityPolicy)
        return *m_contentSecurityPolicy.get();
    ASSERT(m_document->contentSecurityPolicy());
    return *m_document->contentSecurityPolicy();
}

CheckedRef<const ContentSecurityPolicy> DocumentThreadableLoader::checkedContentSecurityPolicy() const
{
    return contentSecurityPolicy();
}

const CrossOriginEmbedderPolicy& DocumentThreadableLoader::crossOriginEmbedderPolicy() const
{
    if (m_crossOriginEmbedderPolicy)
        return *m_crossOriginEmbedderPolicy;
    return m_document->crossOriginEmbedderPolicy();
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
        Ref document = *m_document;
        if (error.isAccessControl() && error.domain() != InspectorNetworkAgent::errorDomain() && !error.localizedDescription().isEmpty())
            document->addConsoleMessage(MessageSource::Security, MessageLevel::Error, error.localizedDescription());
        logError(document, error, m_options.initiatorType);
    }
    ASSERT(m_client);
    if (m_client)
        m_client->didFail(m_document->identifier(), error); // May cause the client to get destroyed.
}

} // namespace WebCore

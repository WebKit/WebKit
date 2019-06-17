/*
 * Copyright (C) 2012 Google, Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CachedResourceRequest.h"

#include "CachedResourceLoader.h"
#include "ContentExtensionActions.h"
#include "CrossOriginAccessControl.h"
#include "Document.h"
#include "Element.h"
#include "FrameLoader.h"
#include "HTTPHeaderValues.h"
#include "ImageDecoder.h"
#include "MemoryCache.h"
#include "ServiceWorkerRegistrationData.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

CachedResourceRequest::CachedResourceRequest(ResourceRequest&& resourceRequest, const ResourceLoaderOptions& options, Optional<ResourceLoadPriority> priority, String&& charset)
    : m_resourceRequest(WTFMove(resourceRequest))
    , m_charset(WTFMove(charset))
    , m_options(options)
    , m_priority(priority)
    , m_fragmentIdentifier(splitFragmentIdentifierFromRequestURL(m_resourceRequest))
{
}

String CachedResourceRequest::splitFragmentIdentifierFromRequestURL(ResourceRequest& request)
{
    if (!MemoryCache::shouldRemoveFragmentIdentifier(request.url()))
        return { };
    URL url = request.url();
    String fragmentIdentifier = url.fragmentIdentifier();
    url.removeFragmentIdentifier();
    request.setURL(url);
    return fragmentIdentifier;
}

void CachedResourceRequest::setInitiator(Element& element)
{
    ASSERT(!m_initiatorElement);
    ASSERT(m_initiatorName.isEmpty());
    m_initiatorElement = &element;
}

void CachedResourceRequest::setInitiator(const AtomString& name)
{
    ASSERT(!m_initiatorElement);
    ASSERT(m_initiatorName.isEmpty());
    m_initiatorName = name;
}

const AtomString& CachedResourceRequest::initiatorName() const
{
    if (m_initiatorElement)
        return m_initiatorElement->localName();
    if (!m_initiatorName.isEmpty())
        return m_initiatorName;

    static NeverDestroyed<AtomString> defaultName("other", AtomString::ConstructFromLiteral);
    return defaultName;
}

void CachedResourceRequest::deprecatedSetAsPotentiallyCrossOrigin(const String& mode, Document& document)
{
    ASSERT(m_options.mode == FetchOptions::Mode::NoCors);

    m_origin = &document.securityOrigin();

    if (mode.isNull())
        return;

    m_options.mode = FetchOptions::Mode::Cors;

    FetchOptions::Credentials credentials = equalLettersIgnoringASCIICase(mode, "omit")
        ? FetchOptions::Credentials::Omit : equalLettersIgnoringASCIICase(mode, "use-credentials")
        ? FetchOptions::Credentials::Include : FetchOptions::Credentials::SameOrigin;
    m_options.credentials = credentials;
    m_options.storedCredentialsPolicy = credentials == FetchOptions::Credentials::Include ? StoredCredentialsPolicy::Use : StoredCredentialsPolicy::DoNotUse;
    updateRequestForAccessControl(m_resourceRequest, document.securityOrigin(), m_options.storedCredentialsPolicy);
}

void CachedResourceRequest::updateForAccessControl(Document& document)
{
    ASSERT(m_options.mode == FetchOptions::Mode::Cors);

    m_origin = &document.securityOrigin();
    updateRequestForAccessControl(m_resourceRequest, *m_origin, m_options.storedCredentialsPolicy);
}

void upgradeInsecureResourceRequestIfNeeded(ResourceRequest& request, Document& document)
{
    URL url = request.url();

    ASSERT(document.contentSecurityPolicy());
    document.contentSecurityPolicy()->upgradeInsecureRequestIfNeeded(url, ContentSecurityPolicy::InsecureRequestType::Load);

    if (url == request.url())
        return;

    request.setURL(url);
}

void CachedResourceRequest::upgradeInsecureRequestIfNeeded(Document& document)
{
    upgradeInsecureResourceRequestIfNeeded(m_resourceRequest, document);
}

void CachedResourceRequest::setDomainForCachePartition(Document& document)
{
    m_resourceRequest.setDomainForCachePartition(document.domainForCachePartition());
}

void CachedResourceRequest::setDomainForCachePartition(const String& domain)
{
    m_resourceRequest.setDomainForCachePartition(domain);
}

static inline String acceptHeaderValueFromType(CachedResource::Type type)
{
    switch (type) {
    case CachedResource::Type::MainResource:
        return "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8"_s;
    case CachedResource::Type::ImageResource:
        if (ImageDecoder::supportsMediaType(ImageDecoder::MediaType::Video))
            return "image/png,image/svg+xml,image/*;q=0.8,video/*;q=0.8,*/*;q=0.5"_s;
        return "image/png,image/svg+xml,image/*;q=0.8,*/*;q=0.5"_s;
    case CachedResource::Type::CSSStyleSheet:
        return "text/css,*/*;q=0.1"_s;
    case CachedResource::Type::SVGDocumentResource:
        return "image/svg+xml"_s;
#if ENABLE(XSLT)
    case CachedResource::Type::XSLStyleSheet:
        // FIXME: This should accept more general xml formats */*+xml, image/svg+xml for example.
        return "text/xml,application/xml,application/xhtml+xml,text/xsl,application/rss+xml,application/atom+xml"_s;
#endif
    default:
        return "*/*"_s;
    }
}

void CachedResourceRequest::setAcceptHeaderIfNone(CachedResource::Type type)
{
    if (!m_resourceRequest.hasHTTPHeader(HTTPHeaderName::Accept))
        m_resourceRequest.setHTTPHeaderField(HTTPHeaderName::Accept, acceptHeaderValueFromType(type));
}

void CachedResourceRequest::updateAccordingCacheMode()
{
    if (m_options.cache == FetchOptions::Cache::Default
        && (m_resourceRequest.hasHTTPHeaderField(HTTPHeaderName::IfModifiedSince)
            || m_resourceRequest.hasHTTPHeaderField(HTTPHeaderName::IfNoneMatch)
            || m_resourceRequest.hasHTTPHeaderField(HTTPHeaderName::IfUnmodifiedSince)
            || m_resourceRequest.hasHTTPHeaderField(HTTPHeaderName::IfMatch)
            || m_resourceRequest.hasHTTPHeaderField(HTTPHeaderName::IfRange)))
        m_options.cache = FetchOptions::Cache::NoStore;

    switch (m_options.cache) {
    case FetchOptions::Cache::NoCache:
        m_resourceRequest.setCachePolicy(ResourceRequestCachePolicy::RefreshAnyCacheData);
        m_resourceRequest.addHTTPHeaderFieldIfNotPresent(HTTPHeaderName::CacheControl, HTTPHeaderValues::maxAge0());
        break;
    case FetchOptions::Cache::NoStore:
        m_options.cachingPolicy = CachingPolicy::DisallowCaching;
        m_resourceRequest.setCachePolicy(ResourceRequestCachePolicy::DoNotUseAnyCache);
        m_resourceRequest.addHTTPHeaderFieldIfNotPresent(HTTPHeaderName::Pragma, HTTPHeaderValues::noCache());
        m_resourceRequest.addHTTPHeaderFieldIfNotPresent(HTTPHeaderName::CacheControl, HTTPHeaderValues::noCache());
        break;
    case FetchOptions::Cache::Reload:
        m_resourceRequest.setCachePolicy(ResourceRequestCachePolicy::ReloadIgnoringCacheData);
        m_resourceRequest.addHTTPHeaderFieldIfNotPresent(HTTPHeaderName::Pragma, HTTPHeaderValues::noCache());
        m_resourceRequest.addHTTPHeaderFieldIfNotPresent(HTTPHeaderName::CacheControl, HTTPHeaderValues::noCache());
        break;
    case FetchOptions::Cache::Default:
        break;
    case FetchOptions::Cache::ForceCache:
        m_resourceRequest.setCachePolicy(ResourceRequestCachePolicy::ReturnCacheDataElseLoad);
        break;
    case FetchOptions::Cache::OnlyIfCached:
        m_resourceRequest.setCachePolicy(ResourceRequestCachePolicy::ReturnCacheDataDontLoad);
        break;
    }
}

void CachedResourceRequest::updateAcceptEncodingHeader()
{
    if (!m_resourceRequest.hasHTTPHeaderField(HTTPHeaderName::Range))
        return;

    // FIXME: rdar://problem/40879225. Media engines triggering the load should not set this Accept-Encoding header.
    ASSERT(!m_resourceRequest.hasHTTPHeaderField(HTTPHeaderName::AcceptEncoding) || m_options.destination == FetchOptions::Destination::Audio || m_options.destination == FetchOptions::Destination::Video);

    m_resourceRequest.addHTTPHeaderFieldIfNotPresent(HTTPHeaderName::AcceptEncoding, "identity"_s);
}

void CachedResourceRequest::removeFragmentIdentifierIfNeeded()
{
    URL url = MemoryCache::removeFragmentIdentifierIfNeeded(m_resourceRequest.url());
    if (url.string() != m_resourceRequest.url())
        m_resourceRequest.setURL(url);
}

#if ENABLE(CONTENT_EXTENSIONS)

void CachedResourceRequest::applyResults(ContentRuleListResults&& results, Page* page)
{
    ContentExtensions::applyResultsToRequest(WTFMove(results), page, m_resourceRequest);
}

#endif

void CachedResourceRequest::updateReferrerPolicy(ReferrerPolicy defaultPolicy)
{
    if (m_options.referrerPolicy == ReferrerPolicy::EmptyString)
        m_options.referrerPolicy = defaultPolicy;
}

void CachedResourceRequest::updateReferrerOriginAndUserAgentHeaders(FrameLoader& frameLoader)
{
    // Implementing step 9 to 11 of https://fetch.spec.whatwg.org/#http-network-or-cache-fetch as of 16 March 2018
    String outgoingReferrer = frameLoader.outgoingReferrer();
    String outgoingOrigin = frameLoader.outgoingOrigin();
    if (m_resourceRequest.hasHTTPReferrer()) {
        outgoingReferrer = m_resourceRequest.httpReferrer();
        outgoingOrigin = SecurityOrigin::createFromString(outgoingReferrer)->toString();
    }
    updateRequestReferrer(m_resourceRequest, m_options.referrerPolicy, outgoingReferrer);

    FrameLoader::addHTTPOriginIfNeeded(m_resourceRequest, outgoingOrigin);

    frameLoader.applyUserAgentIfNeeded(m_resourceRequest);
}

bool isRequestCrossOrigin(SecurityOrigin* origin, const URL& requestURL, const ResourceLoaderOptions& options)
{
    if (!origin)
        return false;

    // Using same origin mode guarantees the loader will not do a cross-origin load, so we let it take care of it and just return false.
    if (options.mode == FetchOptions::Mode::SameOrigin)
        return false;

    // FIXME: We should remove options.sameOriginDataURLFlag once https://github.com/whatwg/fetch/issues/393 is fixed.
    if (requestURL.protocolIsData() && options.sameOriginDataURLFlag == SameOriginDataURLFlag::Set)
        return false;

    return !origin->canRequest(requestURL);
}

void CachedResourceRequest::setDestinationIfNotSet(FetchOptions::Destination destination)
{
    if (m_options.destination != FetchOptions::Destination::EmptyString)
        return;
    m_options.destination = destination;
}

#if ENABLE(SERVICE_WORKER)
void CachedResourceRequest::setClientIdentifierIfNeeded(DocumentIdentifier clientIdentifier)
{
    if (!m_options.clientIdentifier)
        m_options.clientIdentifier = clientIdentifier;
}

void CachedResourceRequest::setSelectedServiceWorkerRegistrationIdentifierIfNeeded(ServiceWorkerRegistrationIdentifier identifier)
{
    if (isNonSubresourceRequest(m_options.destination))
        return;
    if (isPotentialNavigationOrSubresourceRequest(m_options.destination))
        return;

    if (m_options.serviceWorkersMode == ServiceWorkersMode::None)
        return;
    if (m_options.serviceWorkerRegistrationIdentifier)
        return;

    m_options.serviceWorkerRegistrationIdentifier = identifier;
}

void CachedResourceRequest::setNavigationServiceWorkerRegistrationData(const Optional<ServiceWorkerRegistrationData>& data)
{
    if (!data || !data->activeWorker) {
        m_options.serviceWorkersMode = ServiceWorkersMode::None;
        return;
    }
    m_options.serviceWorkerRegistrationIdentifier = data->identifier;
}
#endif

} // namespace WebCore

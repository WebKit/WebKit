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
#include "MemoryCache.h"
#include "SecurityPolicy.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

CachedResourceRequest::CachedResourceRequest(ResourceRequest&& resourceRequest, const ResourceLoaderOptions& options, std::optional<ResourceLoadPriority> priority, String&& charset)
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

void CachedResourceRequest::setInitiator(const AtomicString& name)
{
    ASSERT(!m_initiatorElement);
    ASSERT(m_initiatorName.isEmpty());
    m_initiatorName = name;
}

const AtomicString& CachedResourceRequest::initiatorName() const
{
    if (m_initiatorElement)
        return m_initiatorElement->localName();
    if (!m_initiatorName.isEmpty())
        return m_initiatorName;

    static NeverDestroyed<AtomicString> defaultName("other", AtomicString::ConstructFromLiteral);
    return defaultName;
}

void CachedResourceRequest::setAsPotentiallyCrossOrigin(const String& mode, Document& document)
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
    m_options.allowCredentials = credentials == FetchOptions::Credentials::Include ? AllowStoredCredentials : DoNotAllowStoredCredentials;
    WebCore::updateRequestForAccessControl(m_resourceRequest, document.securityOrigin(), m_options.allowCredentials);
}

void CachedResourceRequest::updateForAccessControl(Document& document)
{
    ASSERT(m_options.mode == FetchOptions::Mode::Cors);

    m_origin = &document.securityOrigin();
    WebCore::updateRequestForAccessControl(m_resourceRequest, *m_origin, m_options.allowCredentials);
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
    m_resourceRequest.setDomainForCachePartition(document.topOrigin().domainForCachePartition());
}

static inline String acceptHeaderValueFromType(CachedResource::Type type)
{
    switch (type) {
    case CachedResource::Type::MainResource:
        return ASCIILiteral("text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8");
    case CachedResource::Type::ImageResource:
        return ASCIILiteral("image/png,image/svg+xml,image/*;q=0.8,*/*;q=0.5");
    case CachedResource::Type::CSSStyleSheet:
        return ASCIILiteral("text/css,*/*;q=0.1");
    case CachedResource::Type::SVGDocumentResource:
        return ASCIILiteral("image/svg+xml");
#if ENABLE(XSLT)
    case CachedResource::Type::XSLStyleSheet:
        // FIXME: This should accept more general xml formats */*+xml, image/svg+xml for example.
        return ASCIILiteral("text/xml,application/xml,application/xhtml+xml,text/xsl,application/rss+xml,application/atom+xml");
#endif
    default:
        return ASCIILiteral("*/*");
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
        m_resourceRequest.setCachePolicy(RefreshAnyCacheData);
        m_resourceRequest.addHTTPHeaderFieldIfNotPresent(HTTPHeaderName::CacheControl, HTTPHeaderValues::maxAge0());
        break;
    case FetchOptions::Cache::NoStore:
        m_options.cachingPolicy = CachingPolicy::DisallowCaching;
        m_resourceRequest.setCachePolicy(DoNotUseAnyCache);
        m_resourceRequest.addHTTPHeaderFieldIfNotPresent(HTTPHeaderName::Pragma, HTTPHeaderValues::noCache());
        m_resourceRequest.addHTTPHeaderFieldIfNotPresent(HTTPHeaderName::CacheControl, HTTPHeaderValues::noCache());
        break;
    case FetchOptions::Cache::Reload:
        m_resourceRequest.setCachePolicy(ReloadIgnoringCacheData);
        m_resourceRequest.addHTTPHeaderFieldIfNotPresent(HTTPHeaderName::Pragma, HTTPHeaderValues::noCache());
        m_resourceRequest.addHTTPHeaderFieldIfNotPresent(HTTPHeaderName::CacheControl, HTTPHeaderValues::noCache());
        break;
    case FetchOptions::Cache::Default:
        break;
    case FetchOptions::Cache::ForceCache:
        m_resourceRequest.setCachePolicy(ReturnCacheDataElseLoad);
        break;
    case FetchOptions::Cache::OnlyIfCached:
        m_resourceRequest.setCachePolicy(ReturnCacheDataDontLoad);
        break;
    }
}

void CachedResourceRequest::removeFragmentIdentifierIfNeeded()
{
    URL url = MemoryCache::removeFragmentIdentifierIfNeeded(m_resourceRequest.url());
    if (url.string() != m_resourceRequest.url())
        m_resourceRequest.setURL(url);
}

#if ENABLE(CONTENT_EXTENSIONS)

void CachedResourceRequest::applyBlockedStatus(const ContentExtensions::BlockedStatus& blockedStatus)
{
    ContentExtensions::applyBlockedStatusToRequest(blockedStatus, m_resourceRequest);
}

#endif

void CachedResourceRequest::updateReferrerOriginAndUserAgentHeaders(FrameLoader& frameLoader, ReferrerPolicy defaultPolicy)
{
    // Implementing step 7 to 9 of https://fetch.spec.whatwg.org/#http-network-or-cache-fetch

    String outgoingOrigin;
    String outgoingReferrer = m_resourceRequest.httpReferrer();
    if (!outgoingReferrer.isNull())
        outgoingOrigin = SecurityOrigin::createFromString(outgoingReferrer)->toString();
    else {
        outgoingReferrer = frameLoader.outgoingReferrer();
        outgoingOrigin = frameLoader.outgoingOrigin();
    }

    // FIXME: Refactor SecurityPolicy::generateReferrerHeader to align with new terminology used in https://w3c.github.io/webappsec-referrer-policy.
    switch (m_options.referrerPolicy) {
    case FetchOptions::ReferrerPolicy::EmptyString: {
        outgoingReferrer = SecurityPolicy::generateReferrerHeader(defaultPolicy, m_resourceRequest.url(), outgoingReferrer);
        break; }
    case FetchOptions::ReferrerPolicy::NoReferrerWhenDowngrade:
        outgoingReferrer = SecurityPolicy::generateReferrerHeader(ReferrerPolicy::Default, m_resourceRequest.url(), outgoingReferrer);
        break;
    case FetchOptions::ReferrerPolicy::NoReferrer:
        outgoingReferrer = String();
        break;
    case FetchOptions::ReferrerPolicy::Origin:
        outgoingReferrer = SecurityPolicy::generateReferrerHeader(ReferrerPolicy::Origin, m_resourceRequest.url(), outgoingReferrer);
        break;
    case FetchOptions::ReferrerPolicy::OriginWhenCrossOrigin:
        if (isRequestCrossOrigin(m_origin.get(), m_resourceRequest.url(), m_options))
            outgoingReferrer = SecurityPolicy::generateReferrerHeader(ReferrerPolicy::Origin, m_resourceRequest.url(), outgoingReferrer);
        break;
    case FetchOptions::ReferrerPolicy::UnsafeUrl:
        break;
    };

    if (outgoingReferrer.isEmpty())
        m_resourceRequest.clearHTTPReferrer();
    else
        m_resourceRequest.setHTTPReferrer(outgoingReferrer);
    FrameLoader::addHTTPOriginIfNeeded(m_resourceRequest, outgoingOrigin);

    frameLoader.applyUserAgent(m_resourceRequest);
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

} // namespace WebCore

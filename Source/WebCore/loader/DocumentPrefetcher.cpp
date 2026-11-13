/*
 * Copyright (C) 2025 Shopify Inc. All rights reserved.
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
#include "DocumentPrefetcher.h"

#include "CachedRawResource.h"
#include "CachedResourceClient.h"
#include "CachedResourceLoader.h"
#include "CachedResourceRequest.h"
#include "CrossOriginEmbedderPolicy.h"
#include "Document.h"
#include "DocumentIsolationPolicy.h"
#include "DocumentLoader.h"
#include "DocumentResourceLoader.h"
#include "DocumentSettingsValues.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "LocalFrame.h"
#include "LocalFrameInlines.h"
#include "MemoryCache.h"
#include "NoVarySearch.h"
#include "OriginAccessPatterns.h"
#include "ReferrerPolicy.h"
#include "ResourceRequest.h"
#include "SecurityOrigin.h"
#include "SecurityPolicy.h"
#include "Settings.h"

namespace WebCore {

DocumentPrefetcher::DocumentPrefetcher(LocalFrame& frame)
    : m_frame(frame)
{
}

DocumentPrefetcher::~DocumentPrefetcher()
{
    clear();
}

void DocumentPrefetcher::clear()
{
    for (auto& [url, data] : m_prefetchedData) {
        RefPtr resource = data.resource;
        if (!resource)
            continue;
        if (resource->hasClient(*this))
            resource->removeClient(*this);
        MemoryCache::singleton().remove(*resource);
    }
    m_prefetchedData.clear();
}

static bool isPassingSecurityChecks(const URL& url, Document& document)
{
    Ref documentOrigin = document.securityOrigin();
    Ref urlOrigin = SecurityOrigin::create(url);
    if (!documentOrigin->isSameOriginAs(urlOrigin)) {
        document.addConsoleMessage(MessageSource::Security, MessageLevel::Error,
            "Prefetch request denied: not same origin as document"_s);
        return false;
    }

    if (!SecurityOrigin::isSecure(url)) {
        document.addConsoleMessage(MessageSource::Security, MessageLevel::Error,
            "Prefetch request denied: URL must be secure (HTTPS)"_s);
        return false;
    }

    return true;
}

static ResourceRequest makePrefetchRequest(URL&& url, const Vector<String>& tags, std::optional<ReferrerPolicy> referrerPolicy, const URL& referrerURL, const Document& document)
{
    if (!referrerPolicy)
        referrerPolicy = document.referrerPolicy();

    String referrer = SecurityPolicy::generateReferrerHeader(*referrerPolicy, url, referrerURL, OriginAccessPatternsForWebProcess::singleton());

    ResourceRequest request { WTF::move(url) };
    request.setPriority(ResourceLoadPriority::VeryLow);

    // https://html.spec.whatwg.org/multipage/speculative-loading.html#the-sec-speculation-tags-header
    if (!tags.isEmpty()) {
        StringBuilder builder;
        for (size_t i = 0; i < tags.size(); ++i) {
            if (i > 0)
                builder.append(", "_s);
            if (tags[i] == nullAtom())
                builder.append("null"_s);
            else
                builder.append(tags[i]);
        }
        request.setHTTPHeaderField(HTTPHeaderName::SecSpeculationTags, builder.toString());
    }
    request.setHTTPHeaderField(HTTPHeaderName::SecPurpose, "prefetch"_s);

    if (!referrer.isEmpty())
        request.setHTTPReferrer(WTF::move(referrer));

    return request;
}

void DocumentPrefetcher::prefetch(const URL& url, const Vector<String>& tags, std::optional<ReferrerPolicy> referrerPolicy, bool lowPriority, const String& noVarySearchHint)
{
    RefPtr document = m_frame ? m_frame->document() : nullptr;
    if (!document)
        return;

    if (!url.isValid())
        return;

    if (m_prefetchedData.contains(url))
        return;

    if (!isPassingSecurityChecks(url, *document))
        return;

    // TODO: This needs to be specified.
    if (url.hasFragmentIdentifier() && equalIgnoringFragmentIdentifier(url, document->url()))
        return;

    ResourceRequest request = makePrefetchRequest(URL { url }, tags, referrerPolicy, protect(m_frame->loader())->outgoingReferrerURL(), *document);

    ResourceLoaderOptions prefetchOptions(
        SendCallbackPolicy::SendCallbacks,
        ContentSniffingPolicy::DoNotSniffContent,
        DataBufferingPolicy::BufferData,
        StoredCredentialsPolicy::Use,
        ClientCredentialPolicy::MayAskClientForCredentials,
        FetchOptions::Credentials::Include,
        SecurityCheckPolicy::DoSecurityCheck,
        FetchOptions::Mode::Navigate,
        CertificateInfoPolicy::IncludeCertificateInfo,
        ContentSecurityPolicyImposition::DoPolicyCheck,
        DefersLoadingPolicy::AllowDefersLoading,
        CachingPolicy::AllowCachingMainResourcePrefetch
    );
    prefetchOptions.destination = FetchOptions::Destination::Document;
    CachedResourceRequest prefetchRequest(WTF::move(request), prefetchOptions);
    if (lowPriority)
        prefetchRequest.setPriority(ResourceLoadPriority::Low);

    auto resourceErrorOr = protect(document->cachedResourceLoader())->requestRawResource(WTF::move(prefetchRequest));

    if (!resourceErrorOr)
        return;
    auto& prefetchedResource = resourceErrorOr.value();
    m_prefetchedData.set(url, PrefetchedResourceData { CachedResourceHandle { prefetchedResource.get() }, { }, noVarySearchEnabled() ? NoVarySearch::parse(noVarySearchHint) : NoVarySearch { }, m_nextPrefetchOrder++ });
    prefetchedResource->addClient(*this);
}

void DocumentPrefetcher::redirectReceived(CachedResource& resource, ResourceRequest&& request, const ResourceResponse&, CompletionHandler<void(ResourceRequest&&)>&& completionHandler)
{
    RefPtr document = m_frame ? m_frame->document() : nullptr;
    if (document && isPassingSecurityChecks(request.url(), *document))
        return completionHandler(WTF::move(request));

    // If a navigation has joined this in-flight prefetch, abandon the prefetch but let the navigation decide whether to follow the redirect.
    if (resource.numberOfClients() > 1) {
        m_prefetchedData.removeIf([&](auto& entry) {
            return entry.value.resource.get() == &resource;
        });
        if (resource.hasClient(*this))
            resource.removeClient(*this);
        MemoryCache::singleton().remove(resource);
        return completionHandler(WTF::move(request));
    }

    completionHandler({ });
}

bool DocumentPrefetcher::canServeNavigations(const ResourceResponse& response) const
{
    if (!response.isSuccessful())
        return false;
    // A navigation served from a prefetch stays in this process, so its Document-Isolation-Policy has to match this document's.
    RefPtr document = m_frame ? m_frame->document() : nullptr;
    if (!document || !document->settings().documentIsolationPolicyEnabled() || !document->settings().siteIsolationEnabled())
        return true;
    auto documentIsolationPolicy = obtainDocumentIsolationPolicy(response, document->isSecureContext() ? IsSecureContext::Yes : IsSecureContext::No, credentiallessSupport(document->settingsValues()));
    return documentIsolationPolicy.value == document->documentIsolationPolicy().value;
}

void DocumentPrefetcher::responseReceived(const CachedResource& resource, const ResourceResponse& response, CompletionHandler<void()>&& completionHandler)
{
    // Remove unsuccessful prefetches from the memory cache as soon as the
    // response headers arrive, rather than waiting for notifyFinished.
    // This prevents navigations from finding and reusing a 503 (or other
    // error) response that is still in the cache while the body loads.
    // We only remove from the memory cache here (not the client registration),
    // since the load is still in progress. Full cleanup happens in notifyFinished.
    if (!canServeNavigations(response))
        MemoryCache::singleton().remove(const_cast<CachedResource&>(resource));
    if (completionHandler)
        completionHandler();
}

void DocumentPrefetcher::notifyFinished(CachedResource& resource, const NetworkLoadMetrics& metrics, LoadWillContinueInAnotherProcess)
{
    URL resourceURL = resource.url();
    auto it = m_prefetchedData.find(resourceURL);
    if (it != m_prefetchedData.end())
        it->value.metrics = Box<NetworkLoadMetrics>::create(metrics);

    if (!canServeNavigations(resource.response())) {
        if (resource.hasClient(*this))
            resource.removeClient(*this);
        m_prefetchedData.remove(resourceURL);
        MemoryCache::singleton().remove(resource);
    }
    // For successful responses, keep the client registration so the resource
    // stays "live" in the memory cache and is not prematurely evicted. The
    // client is removed later when the prefetch is consumed or cancelled.
}

void DocumentPrefetcher::removePrefetch(const URL& url)
{
    if (url.isNull())
        return;

    auto it = m_prefetchedData.find(url);
    if (it == m_prefetchedData.end())
        return;

    if (CachedResourceHandle<CachedRawResource>& resourceHandle = it->value.resource) {
        RefPtr resource = resourceHandle;
        if (resource->hasClient(*this))
            resource->removeClient(*this);
        MemoryCache::singleton().remove(*resource);
    }
    m_prefetchedData.remove(it);
}

// A prefetch can serve a navigation to a different URL if its No-Vary-Search value says the URLs are equivalent.
// Until the response headers arrive, the value the speculation rule expects stands in for the header.
// https://httpwg.org/http-extensions/draft-ietf-httpbis-no-vary-search.html#name-caching
static bool isUsableThroughNoVarySearch(const DocumentPrefetcher::PrefetchedResourceData& data, const URL& prefetchedURL, const URL& url)
{
    RefPtr resource = data.resource;
    if (!resource || resource->errorOccurred())
        return false;
    auto noVarySearch = resource->response().isNull() ? data.expectedNoVarySearch : NoVarySearch::obtain(resource->response());
    return !noVarySearch.isDefault() && noVarySearch.equivalent(prefetchedURL, url);
}

bool DocumentPrefetcher::noVarySearchEnabled() const
{
    RefPtr document = m_frame ? m_frame->document() : nullptr;
    return document && document->settings().noVarySearchEnabled();
}

std::optional<URL> DocumentPrefetcher::keyMatching(const URL& url, MatchExactURL matchExactURL) const
{
    if (url.isNull())
        return std::nullopt;
    if (matchExactURL == MatchExactURL::Yes && m_prefetchedData.contains(url))
        return url;
    if (!noVarySearchEnabled())
        return std::nullopt;
    // The map is unordered, so pick the earliest started prefetch like the spec's list order.
    std::optional<URL> result;
    uint64_t earliestOrder = 0;
    for (auto& [prefetchedURL, data] : m_prefetchedData) {
        if (prefetchedURL == url || !isUsableThroughNoVarySearch(data, prefetchedURL, url))
            continue;
        if (result && data.order > earliestOrder)
            continue;
        RefPtr resource = data.resource;
        if (!resource->response().isNull() && !canServeNavigations(resource->response()))
            continue;
        result = prefetchedURL;
        earliestOrder = data.order;
    }
    return result;
}

// Exact URL matches are left to the memory cache, which owns the lifetime of those entries.
RefPtr<CachedResource> DocumentPrefetcher::resourceMatchingNoVarySearch(const URL& url) const
{
    auto key = keyMatching(url, MatchExactURL::No);
    if (!key)
        return nullptr;
    auto it = m_prefetchedData.find(*key);
    if (it == m_prefetchedData.end())
        return nullptr;
    RefPtr resource = it->value.resource;
    return resource && resource->inCache() ? WTF::move(resource) : nullptr;
}

bool DocumentPrefetcher::wasPrefetched(const URL& url) const
{
    return keyMatching(url, MatchExactURL::Yes).has_value();
}

Box<NetworkLoadMetrics> DocumentPrefetcher::takePrefetchedResourceMetrics(const URL& url)
{
    auto key = keyMatching(url, MatchExactURL::Yes);
    if (!key)
        return { };

    auto it = m_prefetchedData.find(*key);
    if (it != m_prefetchedData.end() && it->value.metrics) {
        auto metrics = WTF::move(it->value.metrics);
        if (CachedResourceHandle<CachedRawResource>& resourceHandle = it->value.resource) {
            RefPtr resource = resourceHandle;
            if (resource->hasClient(*this))
                resource->removeClient(*this);
            MemoryCache::singleton().remove(*resource);
        }
        m_prefetchedData.remove(it);
        return metrics;
    }
    return { };
}

void DocumentPrefetcher::clearPrefetchedResourcesExcept(const URL& url)
{
    // Only the entry that serves this navigation survives, which is the exact match or else the first No-Vary-Search match.
    auto keyToKeep = keyMatching(url, MatchExactURL::Yes);
    m_prefetchedData.removeIf([&](auto& entry) {
        if (!keyToKeep || entry.key != *keyToKeep) {
            if (CachedResourceHandle<CachedRawResource>& resourceHandle = entry.value.resource) {
                RefPtr resource = resourceHandle;
                if (resource->hasClient(*this))
                    resource->removeClient(*this);
                MemoryCache::singleton().remove(*resource);
            }
            return true;
        }
        return false;
    });
}

// https://wicg.github.io/nav-speculation/prefetch.html#clear-prefetch-cache
void DocumentPrefetcher::clearPrefetchedResourcesForOrigin(const SecurityOrigin& origin)
{
    m_prefetchedData.removeIf([&](auto& entry) {
        Ref urlOrigin = SecurityOrigin::create(entry.key);
        if (origin.isSameOriginAs(urlOrigin)) {
            if (CachedResourceHandle<CachedRawResource>& resourceHandle = entry.value.resource) {
                RefPtr resource = resourceHandle;
                if (resource->hasClient(*this))
                    resource->removeClient(*this);
                MemoryCache::singleton().remove(*resource);
            }
            return true;
        }
        return false;
    });
}

} // namespace WebCore

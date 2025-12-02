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
#include "Document.h"
#include "DocumentLoader.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "MemoryCache.h"
#include "ReferrerPolicy.h"
#include "ResourceRequest.h"
#include "SecurityOrigin.h"
#include "SecurityPolicy.h"

namespace WebCore {

DocumentPrefetcher::DocumentPrefetcher(FrameLoader& frameLoader)
    : m_frameLoader(frameLoader)
{
}

DocumentPrefetcher::~DocumentPrefetcher() = default;

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

    ResourceRequest request { WTFMove(url) };
    request.setPriority(ResourceLoadPriority::VeryLow);

    // https://html.spec.whatwg.org/multipage/speculative-loading.html#the-sec-speculation-tags-header
    if (!tags.isEmpty()) {
        StringBuilder builder;
        for (size_t i = 0; i < tags.size(); ++i) {
            if (i > 0)
                builder.append(", "_s);
            builder.append(tags[i]);
        }
        request.setHTTPHeaderField(HTTPHeaderName::SecSpeculationTags, builder.toString());
    }
    request.setHTTPHeaderField(HTTPHeaderName::SecPurpose, "prefetch"_s);

    if (!referrer.isEmpty())
        request.setHTTPReferrer(WTFMove(referrer));

    return request;
}

void DocumentPrefetcher::prefetch(const URL& url, const Vector<String>& tags, std::optional<ReferrerPolicy> referrerPolicy, bool lowPriority)
{
    WeakRef<FrameLoader> frameLoader = m_frameLoader;
    if (!frameLoader.ptr())
        return;
    RefPtr<Document> document = frameLoader->frame().document();
    if (!document)
        return;

    if (m_prefetchedData.contains(url))
        return;

    if (!url.isValid())
        return;

    if (!isPassingSecurityChecks(url, *document.get()))
        return;

    // TODO: This needs to be specified.
    if (url.hasFragmentIdentifier() && equalIgnoringFragmentIdentifier(url, document->url()))
        return;

    ResourceRequest request = makePrefetchRequest(URL { url }, tags, referrerPolicy, frameLoader->outgoingReferrerURL(), *document);

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
    CachedResourceRequest prefetchRequest(WTFMove(request), prefetchOptions);
    if (lowPriority)
        prefetchRequest.setPriority(ResourceLoadPriority::Low);

    auto resourceErrorOr = document->protectedCachedResourceLoader()->requestRawResource(WTFMove(prefetchRequest));

    if (!resourceErrorOr)
        return;
    auto prefetchedResource = resourceErrorOr.value();
    if (prefetchedResource) {
        m_prefetchedData.set(url, PrefetchedResourceData { prefetchedResource, { } });
        prefetchedResource->addClient(*this);
    }
}

void DocumentPrefetcher::responseReceived(const CachedResource&, const ResourceResponse&, CompletionHandler<void()>&& completionHandler)
{
    if (completionHandler)
        completionHandler();
}

void DocumentPrefetcher::notifyFinished(CachedResource& resource, const NetworkLoadMetrics& metrics, LoadWillContinueInAnotherProcess)
{
    URL resourceURL = resource.url();
    auto it = m_prefetchedData.find(resourceURL);
    if (it != m_prefetchedData.end())
        it->value.metrics = Box<NetworkLoadMetrics>::create(metrics);

    if (!resource.response().isSuccessful()) {
        m_prefetchedData.remove(resourceURL);
        MemoryCache::singleton().remove(resource);
    }

    if (resource.hasClient(*this))
        resource.removeClient(*this);
}

bool DocumentPrefetcher::wasPrefetched(const URL& url) const
{
    return m_prefetchedData.contains(url);
}

Box<NetworkLoadMetrics> DocumentPrefetcher::takePrefetchedNetworkLoadMetrics(const URL& url)
{
    auto it = m_prefetchedData.find(url);
    if (it != m_prefetchedData.end() && it->value.metrics) {
        auto metrics = WTFMove(it->value.metrics);
        m_prefetchedData.remove(it);
        return metrics;
    }
    return { };
}


} // namespace WebCore

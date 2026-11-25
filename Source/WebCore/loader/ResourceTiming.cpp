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
#include "ResourceTiming.h"

#include "CachedResource.h"
#include "DocumentLoadTiming.h"
#include "OriginAccessPatterns.h"
#include "PerformanceServerTiming.h"
#include "SecurityOrigin.h"
#include "ServerTimingParser.h"
#include <wtf/CrossThreadCopier.h>

namespace WebCore {

ResourceTiming ResourceTiming::fromMemoryCache(const URL& url, StringImpl* initiator, const ResourceLoadTiming& loadTiming, const ResourceResponse& response, const NetworkLoadMetrics& networkLoadMetrics, const SecurityOrigin& securityOrigin)
{
    return ResourceTiming(url, initiator, loadTiming, networkLoadMetrics, response, securityOrigin);
}

ResourceTiming ResourceTiming::fromLoad(CachedResource& resource, const URL& url, StringImpl* initiator, const ResourceLoadTiming& loadTiming, const NetworkLoadMetrics& networkLoadMetrics, const SecurityOrigin& securityOrigin)
{
    return ResourceTiming(url, initiator, loadTiming, networkLoadMetrics, resource.response(), securityOrigin);
}

ResourceTiming ResourceTiming::fromSynchronousLoad(const URL& url, StringImpl* initiator, const ResourceLoadTiming& loadTiming, const NetworkLoadMetrics& networkLoadMetrics, const ResourceResponse& response, const SecurityOrigin& securityOrigin)
{
    return ResourceTiming(url, initiator, loadTiming, networkLoadMetrics, response, securityOrigin);
}

ResourceTiming::ResourceTiming(const URL& url, StringImpl* initiatorType, const ResourceLoadTiming& timing, const NetworkLoadMetrics& networkLoadMetrics, const ResourceResponse& response, const SecurityOrigin& origin)
    : m_url(url)
    , m_initiatorType(initiatorType)
    , m_resourceLoadTiming(timing)
    , m_networkLoadMetrics(networkLoadMetrics)
    , m_serverTiming(ServerTimingParser::parseServerTiming(response.httpHeaderField(HTTPHeaderName::ServerTiming)))
    , m_isLoadedFromServiceWorker(response.source() == ResourceResponse::Source::ServiceWorker)
    , m_isSameOriginRequest(!m_networkLoadMetrics.hasCrossOriginRedirect
        && origin.protocol() == url.protocol()
        && origin.host() == url.host()
        && origin.port() == url.port())
{
}

ResourceTiming::ResourceTiming(URL&& url, RefPtr<StringImpl>&& initiatorType, ResourceLoadTiming&& resourceLoadTiming, NetworkLoadMetrics&& networkLoadMetrics, Vector<ServerTiming>&& serverTiming, bool isLoadedFromServiceWorker, bool isSameOriginRequest)
    : m_url(WTF::move(url))
    , m_initiatorType(WTF::move(initiatorType))
    , m_resourceLoadTiming(WTF::move(resourceLoadTiming))
    , m_networkLoadMetrics(WTF::move(networkLoadMetrics))
    , m_serverTiming(WTF::move(serverTiming))
    , m_isLoadedFromServiceWorker(isLoadedFromServiceWorker)
    , m_isSameOriginRequest(isSameOriginRequest)
{
}

void ResourceTiming::updateExposure(const SecurityOrigin& origin)
{
    m_isSameOriginRequest = m_isSameOriginRequest && origin.canRequest(m_url, OriginAccessPatternsForWebProcess::singleton());
}

RefPtr<StringImpl> ResourceTiming::deliveryType() const
{
    if (m_networkLoadMetrics.fromPrefetch)
        return "navigational-prefetch"_si;
    if (m_networkLoadMetrics.fromCache)
        return "cache"_si;
    return emptyString();
}

Vector<Ref<PerformanceServerTiming>> ResourceTiming::populateServerTiming() const
{
    // To increase privacy, this additional check was proposed at https://github.com/w3c/resource-timing/issues/342 .
    if (!m_isSameOriginRequest)
        return { };

    return WTF::map(m_serverTiming, [] (auto& entry) {
        return PerformanceServerTiming::create(RefPtr { entry.name }, entry.duration, RefPtr { entry.description });
    });
}

ResourceTiming ResourceTiming::isolatedCopy() const &
{
    return ResourceTiming {
        m_url.isolatedCopy(),
        WTF::isolatedCopy(m_initiatorType),
        m_resourceLoadTiming.isolatedCopy(),
        m_networkLoadMetrics.isolatedCopy(),
        crossThreadCopy(m_serverTiming),
        m_isLoadedFromServiceWorker,
        m_isSameOriginRequest
    };
}

ResourceTiming ResourceTiming::isolatedCopy() &&
{
    return ResourceTiming {
        WTF::move(m_url).isolatedCopy(),
        WTF::isolatedCopy(WTF::move(m_initiatorType)),
        WTF::move(m_resourceLoadTiming).isolatedCopy(),
        WTF::move(m_networkLoadMetrics).isolatedCopy(),
        crossThreadCopy(WTF::move(m_serverTiming)),
        m_isLoadedFromServiceWorker,
        m_isSameOriginRequest
    };
}

} // namespace WebCore

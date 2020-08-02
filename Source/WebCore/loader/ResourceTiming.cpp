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
#include "PerformanceServerTiming.h"
#include "RuntimeEnabledFeatures.h"
#include "SecurityOrigin.h"
#include "ServerTimingParser.h"
#include <wtf/CrossThreadCopier.h>

namespace WebCore {

static bool passesTimingAllowCheck(const ResourceResponse& response, const SecurityOrigin& initiatorSecurityOrigin)
{
    Ref<SecurityOrigin> resourceOrigin = SecurityOrigin::create(response.url());
    if (resourceOrigin->isSameSchemeHostPort(initiatorSecurityOrigin))
        return true;

    const String& timingAllowOriginString = response.httpHeaderField(HTTPHeaderName::TimingAllowOrigin);
    if (timingAllowOriginString.isEmpty() || equalLettersIgnoringASCIICase(timingAllowOriginString, "null"))
        return false;

    if (timingAllowOriginString == "*")
        return true;

    const String& securityOrigin = initiatorSecurityOrigin.toString();
    for (auto& origin : timingAllowOriginString.split(',')) {
        if (origin.stripWhiteSpace() == securityOrigin)
            return true;
    }

    return false;
}

ResourceTiming ResourceTiming::fromCache(const URL& url, const String& initiator, const LoadTiming& loadTiming, const ResourceResponse& response, const SecurityOrigin& securityOrigin)
{
    return ResourceTiming(url, initiator, loadTiming, response, securityOrigin);
}

ResourceTiming ResourceTiming::fromLoad(CachedResource& resource, const String& initiator, const LoadTiming& loadTiming, const NetworkLoadMetrics& networkLoadMetrics, const SecurityOrigin& securityOrigin)
{
    return ResourceTiming(resource.resourceRequest().url(), initiator, loadTiming, networkLoadMetrics, resource.response(), securityOrigin);
}

ResourceTiming ResourceTiming::fromSynchronousLoad(const URL& url, const String& initiator, const LoadTiming& loadTiming, const NetworkLoadMetrics& networkLoadMetrics, const ResourceResponse& response, const SecurityOrigin& securityOrigin)
{
    return ResourceTiming(url, initiator, loadTiming, networkLoadMetrics, response, securityOrigin);
}

ResourceTiming::ResourceTiming(const URL& url, const String& initiator, const LoadTiming& loadTiming, const ResourceResponse& response, const SecurityOrigin& securityOrigin)
    : m_url(url)
    , m_initiator(initiator)
    , m_loadTiming(loadTiming)
    , m_allowTimingDetails(passesTimingAllowCheck(response, securityOrigin))
{
    initServerTiming(response);
}

ResourceTiming::ResourceTiming(const URL& url, const String& initiator, const LoadTiming& loadTiming, const NetworkLoadMetrics& networkLoadMetrics, const ResourceResponse& response, const SecurityOrigin& securityOrigin)
    : m_url(url)
    , m_initiator(initiator)
    , m_loadTiming(loadTiming)
    , m_networkLoadMetrics(networkLoadMetrics)
    , m_allowTimingDetails(passesTimingAllowCheck(response, securityOrigin))
{
    initServerTiming(response);
}

void ResourceTiming::initServerTiming(const ResourceResponse& response)
{
    if (RuntimeEnabledFeatures::sharedFeatures().serverTimingEnabled() && m_allowTimingDetails)
        m_serverTiming = ServerTimingParser::parseServerTiming(response.httpHeaderField(HTTPHeaderName::ServerTiming));
}

Vector<Ref<PerformanceServerTiming>> ResourceTiming::populateServerTiming()
{
    return WTF::map(m_serverTiming, [] (auto& entry) {
        return PerformanceServerTiming::create(WTFMove(entry.name), entry.duration, WTFMove(entry.description));
    });
}

ResourceTiming ResourceTiming::isolatedCopy() const
{
    return ResourceTiming(m_url.isolatedCopy(), m_initiator.isolatedCopy(), m_loadTiming.isolatedCopy(), m_networkLoadMetrics.isolatedCopy(), m_allowTimingDetails, crossThreadCopy(m_serverTiming));
}

} // namespace WebCore

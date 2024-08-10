/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "NetworkLoadScheduler.h"

#include "Logging.h"
#include "NetworkLoad.h"
#include <WebCore/ResourceError.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/WeakHashSet.h>
#include <wtf/WeakListHashSet.h>

namespace WebKit {

static constexpr size_t maximumActiveCountForLowPriority = 2;
static constexpr size_t maximumTrackedHTTP1XOrigins = 128;

class NetworkLoadScheduler::HostContext {
    WTF_MAKE_TZONE_ALLOCATED(NetworkLoadScheduler::HostContext);
public:
    HostContext() = default;
    ~HostContext();

    void schedule(NetworkLoad&);
    void unschedule(NetworkLoad&);
    void prioritize(NetworkLoad&);

private:
    void start(NetworkLoad&);
    bool shouldDelayLowPriority() const { return m_activeLoads.computeSize() >= maximumActiveCountForLowPriority; }

    WeakHashSet<NetworkLoad> m_activeLoads;
    WeakListHashSet<NetworkLoad> m_pendingLoads;
};

WTF_MAKE_TZONE_ALLOCATED_IMPL_NESTED(NetworkLoadSchedulerHostContext, NetworkLoadScheduler::HostContext);

void NetworkLoadScheduler::HostContext::schedule(NetworkLoad& load)
{
    auto startImmediately = [&] {
        auto& request = load.currentRequest();
        if (request.priority() > WebCore::ResourceLoadPriority::Low)
            return true;
        
        if (request.isConditional())
            return true;

        if (!shouldDelayLowPriority())
            return true;

        return false;
    }();

    if (startImmediately) {
        start(load);
        return;
    }

    m_pendingLoads.add(load);
}

void NetworkLoadScheduler::HostContext::unschedule(NetworkLoad& load)
{
    m_activeLoads.remove(load);
    m_pendingLoads.remove(load);

    if (shouldDelayLowPriority())
        return;

    if (auto* firstPendingLoad = m_pendingLoads.tryTakeFirst())
        start(*firstPendingLoad);
}

void NetworkLoadScheduler::HostContext::prioritize(NetworkLoad& load)
{
    auto priority = load.parameters().request.priority();
    load.reprioritizeRequest(++priority);

    if (!m_pendingLoads.remove(load))
        return;

    start(load);
}

void NetworkLoadScheduler::HostContext::start(NetworkLoad& load)
{
    m_activeLoads.add(load);

    load.start();
}

NetworkLoadScheduler::HostContext::~HostContext()
{
    for (auto& load : m_pendingLoads)
        start(load);
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(NetworkLoadScheduler);

NetworkLoadScheduler::NetworkLoadScheduler() = default;
NetworkLoadScheduler::~NetworkLoadScheduler() = default;

void NetworkLoadScheduler::schedule(NetworkLoad& load)
{
    bool isMainFrameMainResource = load.currentRequest().isTopSite();
    if (isMainFrameMainResource)
        scheduleMainResourceLoad(load);
    else
        scheduleLoad(load);
}

void NetworkLoadScheduler::unschedule(NetworkLoad& load, const WebCore::NetworkLoadMetrics* metrics)
{
    bool isMainFrameMainResource = load.currentRequest().isTopSite();
    if (isMainFrameMainResource)
        unscheduleMainResourceLoad(load, metrics);
    else
        unscheduleLoad(load);
}

void NetworkLoadScheduler::scheduleLoad(NetworkLoad& load)
{
    auto* context = contextForLoad(load);

    if (!context) {
        load.start();
        return;
    }

    context->schedule(load);
}

void NetworkLoadScheduler::unscheduleLoad(NetworkLoad& load)
{
    if (auto* context = contextForLoad(load))
        context->unschedule(load);
}

// We add User-Agent to the preconnect key since it part of the HTTP connection cache key used for
// coalescing sockets in CFNetwork when using an HTTPS proxy (<rdar://problem/59434166>).
static std::tuple<String, String> mainResourceLoadKey(const String& protocolHostAndPort, const String& userAgent)
{
    return std::make_tuple(protocolHostAndPort.isNull() ? emptyString() : protocolHostAndPort, userAgent.isNull() ? emptyString() : userAgent);
}

void NetworkLoadScheduler::scheduleMainResourceLoad(NetworkLoad& load)
{
    String protocolHostAndPort = load.url().protocolHostAndPort();
    if (!isOriginHTTP1X(protocolHostAndPort)) {
        load.start();
        return;
    }

    auto iter = m_pendingMainResourcePreconnects.find(mainResourceLoadKey(protocolHostAndPort, load.parameters().request.httpUserAgent()));
    if (iter == m_pendingMainResourcePreconnects.end()) {
        load.start();
        return;
    }

    auto& info = iter->value;
    if (!info.pendingPreconnects) {
        load.start();
        return;
    }

    --info.pendingPreconnects;
    info.pendingLoads.add(load);
    RELEASE_LOG(Network, "%p - NetworkLoadScheduler::scheduleMainResourceLoad deferring load %p; %u pending preconnects; %u pending loads", this, &load, info.pendingPreconnects, info.pendingLoads.computeSize());
}

void NetworkLoadScheduler::unscheduleMainResourceLoad(NetworkLoad& load, const WebCore::NetworkLoadMetrics* metrics)
{
    String protocolHostAndPort = load.url().protocolHostAndPort();

    if (metrics)
        updateOriginProtocolInfo(protocolHostAndPort, metrics->protocol);

    auto iter = m_pendingMainResourcePreconnects.find(mainResourceLoadKey(protocolHostAndPort, load.parameters().request.httpUserAgent()));
    if (iter == m_pendingMainResourcePreconnects.end())
        return;

    PendingMainResourcePreconnectInfo& info = iter->value;
    if (info.pendingLoads.remove(load))
        maybePrunePreconnectInfo(iter);
}

void NetworkLoadScheduler::startedPreconnectForMainResource(const URL& url, const String& userAgent)
{
    auto key = mainResourceLoadKey(url.protocolHostAndPort(), userAgent);
    auto iter = m_pendingMainResourcePreconnects.find(key);
    if (iter != m_pendingMainResourcePreconnects.end()) {
        PendingMainResourcePreconnectInfo& info = iter->value;
        info.pendingPreconnects++;
        return;
    }

    PendingMainResourcePreconnectInfo info;
    m_pendingMainResourcePreconnects.add(key, WTFMove(info));
}

void NetworkLoadScheduler::finishedPreconnectForMainResource(const URL& url, const String& userAgent, const WebCore::ResourceError& error)
{
    auto iter = m_pendingMainResourcePreconnects.find(mainResourceLoadKey(url.protocolHostAndPort(), userAgent));
    if (iter == m_pendingMainResourcePreconnects.end())
        return;

    PendingMainResourcePreconnectInfo& info = iter->value;
    if (!info.pendingLoads.isEmptyIgnoringNullReferences()) {
        auto& load = info.pendingLoads.takeFirst();
        RELEASE_LOG(Network, "%p - NetworkLoadScheduler::finishedPreconnectForMainResource (error: %d) starting delayed main resource load %p; %u pending preconnects; %u total pending loads", this, static_cast<int>(error.type()), &load, info.pendingPreconnects, info.pendingLoads.computeSize());
        load.start();
    } else
        --info.pendingPreconnects;

    maybePrunePreconnectInfo(iter);
}

void NetworkLoadScheduler::maybePrunePreconnectInfo(PendingPreconnectMap::iterator& iter)
{
    PendingMainResourcePreconnectInfo& info = iter->value;
    if (!info.pendingPreconnects && info.pendingLoads.isEmptyIgnoringNullReferences())
        m_pendingMainResourcePreconnects.remove(iter);
}


bool NetworkLoadScheduler::isOriginHTTP1X(const String& protocolHostAndPort)
{
    return m_http1XOrigins.contains(protocolHostAndPort);
}

void NetworkLoadScheduler::updateOriginProtocolInfo(const String& protocolHostAndPort, const String& alpnProtocolID)
{
    if (alpnProtocolID != "http/1.1"_s) {
        m_http1XOrigins.remove(protocolHostAndPort);
        return;
    }

    if (m_http1XOrigins.size() >= maximumTrackedHTTP1XOrigins)
        m_http1XOrigins.remove(m_http1XOrigins.random());

    m_http1XOrigins.add(protocolHostAndPort);
}

void NetworkLoadScheduler::setResourceLoadSchedulingMode(WebCore::PageIdentifier pageIdentifier, WebCore::LoadSchedulingMode mode)
{
    switch (mode) {
    case WebCore::LoadSchedulingMode::Prioritized:
        m_pageContexts.ensure(pageIdentifier, [&] {
            return makeUnique<PageContext>();
        });
        break;
    case WebCore::LoadSchedulingMode::Direct:
        m_pageContexts.remove(pageIdentifier);
        break;
    }
}

void NetworkLoadScheduler::prioritizeLoads(const Vector<NetworkLoad*>& loads)
{
    for (auto* load : loads) {
        if (auto* context = contextForLoad(*load))
            context->prioritize(*load);
    }
}

void NetworkLoadScheduler::clearPageData(WebCore::PageIdentifier pageIdentifier)
{
    m_pageContexts.remove(pageIdentifier);
}

auto NetworkLoadScheduler::contextForLoad(const NetworkLoad& load) -> HostContext*
{
    if (!load.url().protocolIsInHTTPFamily())
        return nullptr;

    auto* pageContext = m_pageContexts.get(load.parameters().webPageID);
    if (!pageContext)
        return nullptr;

    auto host = load.url().host().toString();
    return pageContext->ensure(host, [&] {
        return makeUnique<HostContext>();
    }).iterator->value.get();
}

}

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

#include "NetworkLoad.h"
#include <wtf/ListHashSet.h>

namespace WebKit {

static constexpr size_t maximumActiveCountForLowPriority = 6;

class NetworkLoadScheduler::HostContext {
    WTF_MAKE_FAST_ALLOCATED;
public:
    HostContext() = default;
    ~HostContext();

    void schedule(NetworkLoad&);
    void unschedule(NetworkLoad&);
    void prioritize(NetworkLoad&);

    void finishPage(WebCore::PageIdentifier);

private:
    void start(NetworkLoad&);
    bool shouldDelayLowPriority() const { return m_activeLoads.size() >= maximumActiveCountForLowPriority; }

    HashSet<NetworkLoad*> m_activeLoads;
    ListHashSet<NetworkLoad*> m_pendingLoads;
};

void NetworkLoadScheduler::HostContext::schedule(NetworkLoad& load)
{
    auto startImmediately = [&] {
        auto priority = load.parameters().request.priority();
        if (priority > WebCore::ResourceLoadPriority::Low)
            return true;

        if (!shouldDelayLowPriority())
            return true;

        return false;
    }();

    if (startImmediately) {
        start(load);
        return;
    }

    m_pendingLoads.add(&load);
}

void NetworkLoadScheduler::HostContext::unschedule(NetworkLoad& load)
{
    m_activeLoads.remove(&load);
    m_pendingLoads.remove(&load);

    if (m_pendingLoads.isEmpty())
        return;
    if (shouldDelayLowPriority())
        return;

    start(*m_pendingLoads.takeFirst());
}

void NetworkLoadScheduler::HostContext::prioritize(NetworkLoad& load)
{
    auto priority = load.parameters().request.priority();
    load.reprioritizeRequest(++priority);

    if (!m_pendingLoads.remove(&load))
        return;

    start(load);
}

void NetworkLoadScheduler::HostContext::start(NetworkLoad& load)
{
    m_activeLoads.add(&load);

    load.start();
}

NetworkLoadScheduler::HostContext::~HostContext()
{
    for (auto* load : m_pendingLoads)
        start(*load);
}

NetworkLoadScheduler::NetworkLoadScheduler() = default;
NetworkLoadScheduler::~NetworkLoadScheduler() = default;

void NetworkLoadScheduler::schedule(NetworkLoad& load)
{
    auto* context = contextForLoad(load);

    if (!context) {
        load.start();
        return;
    }

    context->schedule(load);
}

void NetworkLoadScheduler::unschedule(NetworkLoad& load)
{
    if (auto* context = contextForLoad(load))
        context->unschedule(load);
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

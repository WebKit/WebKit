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

#pragma once

#include <WebCore/LoadSchedulingMode.h>
#include <WebCore/NetworkLoadMetrics.h>
#include <WebCore/PageIdentifier.h>
#include <tuple>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakListHashSet.h>
#include <wtf/WeakPtr.h>

namespace WebCore {
class ResourceError;
}

namespace WebKit {

class NetworkLoad;

class NetworkLoadScheduler : public RefCountedAndCanMakeWeakPtr<NetworkLoadScheduler> {
    WTF_MAKE_TZONE_ALLOCATED(NetworkLoadScheduler);
public:
    static Ref<NetworkLoadScheduler> create()
    {
        return adoptRef(*new NetworkLoadScheduler);
    }

    ~NetworkLoadScheduler();

    void schedule(NetworkLoad&);
    void unschedule(NetworkLoad&, const WebCore::NetworkLoadMetrics* = nullptr);

    void startedPreconnectForMainResource(const URL&, const String& userAgent);
    void finishedPreconnectForMainResource(const URL&, const String& userAgent, const WebCore::ResourceError&);

    void setResourceLoadSchedulingMode(WebCore::PageIdentifier, WebCore::LoadSchedulingMode);
    void prioritizeLoads(const Vector<NetworkLoad*>&);
    void clearPageData(WebCore::PageIdentifier);

private:
    NetworkLoadScheduler();

    void scheduleLoad(NetworkLoad&);
    void unscheduleLoad(NetworkLoad&);

    void scheduleMainResourceLoad(NetworkLoad&);
    void unscheduleMainResourceLoad(NetworkLoad&, const WebCore::NetworkLoadMetrics*);

    bool isOriginHTTP1X(const String&);
    void updateOriginProtocolInfo(const String&, const String&);

    class HostContext;
    HostContext* contextForLoad(const NetworkLoad&);

    using PageContext = HashMap<String, std::unique_ptr<HostContext>>;
    HashMap<WebCore::PageIdentifier, std::unique_ptr<PageContext>> m_pageContexts;

    struct PendingMainResourcePreconnectInfo {
        unsigned pendingPreconnects {1};
        WeakListHashSet<NetworkLoad> pendingLoads;
    };
    // Maps (protocolHostAndPort, userAgent) => PendingMainResourcePreconnectInfo.
    using PendingPreconnectMap = HashMap<std::tuple<String, String>, PendingMainResourcePreconnectInfo>;
    PendingPreconnectMap m_pendingMainResourcePreconnects;

    void maybePrunePreconnectInfo(PendingPreconnectMap::iterator&);

    HashSet<String> m_http1XOrigins;
};

}

/*
 * Copyright (C) 2012, 2015 Apple Inc. All rights reserved.
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

#include "WebResourceLoader.h"
#include <WebCore/LoaderStrategy.h>
#include <WebCore/ResourceError.h>
#include <WebCore/ResourceLoader.h>
#include <WebCore/ResourceResponse.h>
#include <wtf/HashSet.h>
#include <wtf/RunLoop.h>

namespace WebCore {
struct FetchOptions;
}

namespace WebKit {

class NetworkProcessConnection;
class WebFrame;
class WebPage;
class WebURLSchemeTaskProxy;
typedef uint64_t ResourceLoadIdentifier;

class WebLoaderStrategy final : public WebCore::LoaderStrategy {
    WTF_MAKE_NONCOPYABLE(WebLoaderStrategy); WTF_MAKE_FAST_ALLOCATED;
public:
    WebLoaderStrategy();
    ~WebLoaderStrategy() final;
    
    void loadResource(WebCore::Frame&, WebCore::CachedResource&, WebCore::ResourceRequest&&, const WebCore::ResourceLoaderOptions&, CompletionHandler<void(RefPtr<WebCore::SubresourceLoader>&&)>&&) final;
    void loadResourceSynchronously(WebCore::FrameLoader&, unsigned long resourceLoadIdentifier, const WebCore::ResourceRequest&, WebCore::ClientCredentialPolicy, const WebCore::FetchOptions&, const WebCore::HTTPHeaderMap&, WebCore::ResourceError&, WebCore::ResourceResponse&, Vector<char>& data) final;
    void pageLoadCompleted(WebCore::Page&) final;

    void remove(WebCore::ResourceLoader*) final;
    void setDefersLoading(WebCore::ResourceLoader&, bool) final;
    void crossOriginRedirectReceived(WebCore::ResourceLoader*, const URL& redirectURL) final;
    
    void servePendingRequests(WebCore::ResourceLoadPriority minimumPriority) final;

    void suspendPendingRequests() final;
    void resumePendingRequests() final;

    bool usePingLoad() const final;
    void startPingLoad(WebCore::Frame&, WebCore::ResourceRequest&, const WebCore::HTTPHeaderMap& originalRequestHeaders, const WebCore::FetchOptions&, WebCore::ContentSecurityPolicyImposition, PingLoadCompletionHandler&&) final;
    void didFinishPingLoad(uint64_t pingLoadIdentifier, WebCore::ResourceError&&, WebCore::ResourceResponse&&);

    void preconnectTo(WebCore::ResourceRequest&&, WebPage&, WebFrame&, WebCore::StoredCredentialsPolicy, PreconnectCompletionHandler&& = nullptr);
    void preconnectTo(WebCore::FrameLoader&, const URL&, WebCore::StoredCredentialsPolicy, PreconnectCompletionHandler&&) final;
    void didFinishPreconnection(uint64_t preconnectionIdentifier, WebCore::ResourceError&&);

    void setCaptureExtraNetworkLoadMetricsEnabled(bool) final;

    WebResourceLoader* webResourceLoaderForIdentifier(ResourceLoadIdentifier identifier) const { return m_webResourceLoaders.get(identifier); }
    void schedulePluginStreamLoad(WebCore::Frame&, WebCore::NetscapePlugInStreamLoaderClient&, WebCore::ResourceRequest&&, CompletionHandler<void(RefPtr<WebCore::NetscapePlugInStreamLoader>&&)>&&);

    void networkProcessCrashed();

    void addURLSchemeTaskProxy(WebURLSchemeTaskProxy&);
    void removeURLSchemeTaskProxy(WebURLSchemeTaskProxy&);

    void scheduleLoadFromNetworkProcess(WebCore::ResourceLoader&, const WebCore::ResourceRequest&, const WebResourceLoader::TrackingParameters&, bool shouldClearReferrerOnHTTPSToHTTPRedirect, Seconds maximumBufferingTime);

    bool isOnLine() const final;
    void addOnlineStateChangeListener(Function<void(bool)>&&) final;
    void setOnLineState(bool);

private:
    void scheduleLoad(WebCore::ResourceLoader&, WebCore::CachedResource*, bool shouldClearReferrerOnHTTPSToHTTPRedirect);
    void scheduleInternallyFailedLoad(WebCore::ResourceLoader&);
    void internallyFailedLoadTimerFired();
    void startLocalLoad(WebCore::ResourceLoader&);
    bool tryLoadingUsingURLSchemeHandler(WebCore::ResourceLoader&, const WebResourceLoader::TrackingParameters&);
    
    struct SyncLoadResult {
        WebCore::ResourceResponse response;
        WebCore::ResourceError error;
        Vector<char> data;
    };
    Optional<SyncLoadResult> tryLoadingSynchronouslyUsingURLSchemeHandler(WebCore::FrameLoader&, ResourceLoadIdentifier, const WebCore::ResourceRequest&);

    WebCore::ResourceResponse responseFromResourceLoadIdentifier(uint64_t resourceLoadIdentifier) final;
    Vector<WebCore::NetworkTransactionInformation> intermediateLoadInformationFromResourceLoadIdentifier(uint64_t resourceLoadIdentifier) final;
    WebCore::NetworkLoadMetrics networkMetricsFromResourceLoadIdentifier(uint64_t resourceLoadIdentifier) final;

    bool shouldPerformSecurityChecks() const final;
    bool havePerformedSecurityChecks(const WebCore::ResourceResponse&) const final;

    Vector<uint64_t> ongoingLoads() const final
    {
        return WTF::map(m_webResourceLoaders, [](auto&& keyValue) -> uint64_t {
            return keyValue.key;
        });
    }

    HashSet<RefPtr<WebCore::ResourceLoader>> m_internallyFailedResourceLoaders;
    RunLoop::Timer<WebLoaderStrategy> m_internallyFailedLoadTimer;
    
    HashMap<unsigned long, RefPtr<WebResourceLoader>> m_webResourceLoaders;
    HashMap<unsigned long, WebURLSchemeTaskProxy*> m_urlSchemeTasks;
    HashMap<unsigned long, PingLoadCompletionHandler> m_pingLoadCompletionHandlers;
    HashMap<unsigned long, PreconnectCompletionHandler> m_preconnectCompletionHandlers;
    Vector<Function<void(bool)>> m_onlineStateChangeListeners;
    bool m_isOnLine { true };
    HashSet<WebResourceLoader*> m_loadersWithUploads;
};

} // namespace WebKit

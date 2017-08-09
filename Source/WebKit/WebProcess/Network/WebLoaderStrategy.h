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
#include <WebCore/ResourceLoader.h>
#include <wtf/HashSet.h>
#include <wtf/RunLoop.h>

namespace WebCore {
struct FetchOptions;
}

namespace WebKit {

class NetworkProcessConnection;
class WebURLSchemeTaskProxy;
typedef uint64_t ResourceLoadIdentifier;

class WebLoaderStrategy : public WebCore::LoaderStrategy {
    WTF_MAKE_NONCOPYABLE(WebLoaderStrategy); WTF_MAKE_FAST_ALLOCATED;
public:
    WebLoaderStrategy();
    ~WebLoaderStrategy() override;
    
    RefPtr<WebCore::SubresourceLoader> loadResource(WebCore::Frame&, WebCore::CachedResource&, const WebCore::ResourceRequest&, const WebCore::ResourceLoaderOptions&) override;
    void loadResourceSynchronously(WebCore::NetworkingContext*, unsigned long resourceLoadIdentifier, const WebCore::ResourceRequest&, WebCore::StoredCredentials, WebCore::ClientCredentialPolicy, WebCore::ResourceError&, WebCore::ResourceResponse&, Vector<char>& data) override;

    void remove(WebCore::ResourceLoader*) override;
    void setDefersLoading(WebCore::ResourceLoader*, bool) override;
    void crossOriginRedirectReceived(WebCore::ResourceLoader*, const WebCore::URL& redirectURL) override;
    
    void servePendingRequests(WebCore::ResourceLoadPriority minimumPriority) override;

    void suspendPendingRequests() override;
    void resumePendingRequests() override;

    void createPingHandle(WebCore::NetworkingContext*, WebCore::ResourceRequest&, Ref<WebCore::SecurityOrigin>&& sourceOrigin, const WebCore::FetchOptions&) override;

    void storeDerivedDataToCache(const SHA1::Digest& bodyHash, const String& type, const String& partition, WebCore::SharedBuffer&) override;

    void setCaptureExtraNetworkLoadMetricsEnabled(bool) override;

    WebResourceLoader* webResourceLoaderForIdentifier(ResourceLoadIdentifier identifier) const { return m_webResourceLoaders.get(identifier); }
    RefPtr<WebCore::NetscapePlugInStreamLoader> schedulePluginStreamLoad(WebCore::Frame&, WebCore::NetscapePlugInStreamLoaderClient&, const WebCore::ResourceRequest&);

    void networkProcessCrashed();

    void addURLSchemeTaskProxy(WebURLSchemeTaskProxy&);
    void removeURLSchemeTaskProxy(WebURLSchemeTaskProxy&);

private:
    void scheduleLoad(WebCore::ResourceLoader&, WebCore::CachedResource*, bool shouldClearReferrerOnHTTPSToHTTPRedirect);
    void scheduleInternallyFailedLoad(WebCore::ResourceLoader&);
    void internallyFailedLoadTimerFired();
    void startLocalLoad(WebCore::ResourceLoader&);

    HashSet<RefPtr<WebCore::ResourceLoader>> m_internallyFailedResourceLoaders;
    RunLoop::Timer<WebLoaderStrategy> m_internallyFailedLoadTimer;
    
    HashMap<unsigned long, RefPtr<WebResourceLoader>> m_webResourceLoaders;
    HashMap<unsigned long, WebURLSchemeTaskProxy*> m_urlSchemeTasks;
};

} // namespace WebKit

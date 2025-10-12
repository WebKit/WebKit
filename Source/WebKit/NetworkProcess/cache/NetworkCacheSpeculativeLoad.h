/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#include "NetworkCache.h"
#include "NetworkCacheEntry.h"
#include "NetworkLoadClient.h"
#include "PolicyDecision.h"
#include "PrivateRelayed.h"
#include <WebCore/ResourceRequest.h>
#include <WebCore/ResourceResponse.h>
#include <WebCore/SharedBuffer.h>
#include <wtf/CompletionHandler.h>
#include <wtf/TZoneMalloc.h>

namespace WebCore {
enum class AdvancedPrivacyProtections : uint16_t;
}

namespace WebKit {

class NetworkLoad;

namespace NetworkCache {

class SpeculativeLoad final : public RefCounted<SpeculativeLoad>, public NetworkLoadClient {
    WTF_MAKE_TZONE_ALLOCATED(SpeculativeLoad);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(SpeculativeLoad);
public:
    using RevalidationCompletionHandler = CompletionHandler<void(std::unique_ptr<NetworkCache::Entry>)>;
    static Ref<SpeculativeLoad> create(Cache&, const GlobalFrameID&, const WebCore::ResourceRequest&, std::unique_ptr<NetworkCache::Entry>, std::optional<NavigatingToAppBoundDomain>, bool allowPrivacyProxy, OptionSet<WebCore::AdvancedPrivacyProtections>, RevalidationCompletionHandler&&);

    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

    virtual ~SpeculativeLoad();

    const WebCore::ResourceRequest& originalRequest() const { return m_originalRequest; }

    void cancel();

private:
    SpeculativeLoad(Cache&, const GlobalFrameID&, const WebCore::ResourceRequest&, std::unique_ptr<NetworkCache::Entry>, std::optional<NavigatingToAppBoundDomain>, bool allowPrivacyProxy, OptionSet<WebCore::AdvancedPrivacyProtections>, RevalidationCompletionHandler&&);

    // NetworkLoadClient.
    void didSendData(uint64_t bytesSent, uint64_t totalBytesToBeSent) override { }
    bool isSynchronous() const override { return false; }
    bool isAllowedToAskUserForCredentials() const final { return false; }
    void willSendRedirectedRequest(WebCore::ResourceRequest&&, WebCore::ResourceRequest&& redirectRequest, WebCore::ResourceResponse&& redirectResponse, CompletionHandler<void(WebCore::ResourceRequest&&)>&&) override;
    void didReceiveResponse(WebCore::ResourceResponse&&, PrivateRelayed, ResponseCompletionHandler&&) override;
    void didReceiveBuffer(const WebCore::FragmentedSharedBuffer&) override;
    void didFinishLoading(const WebCore::NetworkLoadMetrics&) override;
    void didFailLoading(const WebCore::ResourceError&) override;

    void didComplete();

    const Ref<Cache> m_cache;
    RevalidationCompletionHandler m_completionHandler;
    WebCore::ResourceRequest m_originalRequest;

    RefPtr<NetworkLoad> m_networkLoad;

    WebCore::ResourceResponse m_response;

    WebCore::SharedBufferBuilder m_bufferedDataForCache;
    std::unique_ptr<NetworkCache::Entry> m_cacheEntry;
    bool m_didComplete { false };
    PrivateRelayed m_privateRelayed { PrivateRelayed::No };
};

bool requestsHeadersMatch(const WebCore::ResourceRequest& speculativeValidationRequest, const WebCore::ResourceRequest& actualRequest);

} // namespace NetworkCache
} // namespace WebKit

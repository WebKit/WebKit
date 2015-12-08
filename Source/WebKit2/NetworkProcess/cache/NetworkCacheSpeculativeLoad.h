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

#ifndef NetworkCacheSpeculativeLoad_h
#define NetworkCacheSpeculativeLoad_h

#if ENABLE(NETWORK_CACHE_SPECULATIVE_REVALIDATION)

#include "NetworkCache.h"
#include "NetworkCacheEntry.h"
#include "NetworkLoadClient.h"
#include <WebCore/ResourceRequest.h>
#include <WebCore/ResourceResponse.h>
#include <WebCore/SharedBuffer.h>

namespace WebKit {

class NetworkLoad;

namespace NetworkCache {

class SpeculativeLoad final : public NetworkLoadClient {
    WTF_MAKE_FAST_ALLOCATED;
public:
    typedef std::function<void (std::unique_ptr<NetworkCache::Entry>)> RevalidationCompletionHandler;
    SpeculativeLoad(const GlobalFrameID&, const WebCore::ResourceRequest&, std::unique_ptr<NetworkCache::Entry>, RevalidationCompletionHandler&&);

    virtual ~SpeculativeLoad();

private:
    // NetworkLoadClient.
    virtual void didSendData(unsigned long long bytesSent, unsigned long long totalBytesToBeSent) override { }
    virtual void canAuthenticateAgainstProtectionSpaceAsync(const WebCore::ProtectionSpace&) override { }
    virtual bool isSynchronous() const override { return false; }
    virtual void willSendRedirectedRequest(const WebCore::ResourceRequest&, const WebCore::ResourceResponse& redirectResponse) override;
    virtual ShouldContinueDidReceiveResponse didReceiveResponse(const WebCore::ResourceResponse&) override;
    virtual void didReceiveBuffer(RefPtr<WebCore::SharedBuffer>&&, int reportedEncodedDataLength) override;
    virtual void didFinishLoading(double finishTime) override;
    virtual void didFailLoading(const WebCore::ResourceError&) override;
    virtual void didConvertToDownload() override { ASSERT_NOT_REACHED(); }
#if PLATFORM(COCOA)
    virtual void willCacheResponseAsync(CFCachedURLResponseRef) override { }
#endif

    void didComplete();

    GlobalFrameID m_frameID;
    RevalidationCompletionHandler m_completionHandler;
    WebCore::ResourceRequest m_originalRequest;

    std::unique_ptr<NetworkLoad> m_networkLoad;

    WebCore::ResourceResponse m_response;

    RefPtr<WebCore::SharedBuffer> m_bufferedDataForCache;
    std::unique_ptr<NetworkCache::Entry> m_cacheEntryForValidation;

    WebCore::RedirectChainCacheStatus m_redirectChainCacheStatus;
};

} // namespace NetworkCache
} // namespace WebKit

#endif // ENABLE(NETWORK_CACHE_SPECULATIVE_REVALIDATION)

#endif // NetworkCacheSpeculativeLoad_h

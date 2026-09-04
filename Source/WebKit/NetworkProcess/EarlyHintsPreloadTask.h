/*
 * Copyright (C) 2026 Shopify Inc. All rights reserved.
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
#include "NetworkLoadClient.h"
#include "PrivateRelayed.h"
#include <WebCore/FetchOptions.h>
#include <WebCore/ResourceResponse.h>
#include <WebCore/SecurityOriginData.h>
#include <WebCore/SharedBuffer.h>
#include <WebCore/StoredCredentialsPolicy.h>
#include <wtf/CheckedPtr.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

class NetworkLoad;
class NetworkSession;

struct NetworkLoadParameters;

// A speculative resource load for an HTTP 103 `Link: rel=preload` hint, parked for later reuse.
class EarlyHintsPreloadTask final : public RefCounted<EarlyHintsPreloadTask>, public NetworkLoadClient {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(EarlyHintsPreloadTask);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(EarlyHintsPreloadTask);
public:
    static Ref<EarlyHintsPreloadTask> create(NetworkSession&, NetworkLoadParameters&&, NetworkCache::GlobalFrameID, WebCore::SecurityOriginData&& hintingOrigin, URL&&, String&& destination, WebCore::FetchOptions::Mode, WebCore::StoredCredentialsPolicy, String&& cachePartition);
    ~EarlyHintsPreloadTask();

    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

    void start();

private:
    EarlyHintsPreloadTask(NetworkSession&, NetworkLoadParameters&&, NetworkCache::GlobalFrameID, WebCore::SecurityOriginData&& hintingOrigin, URL&&, String&& destination, WebCore::FetchOptions::Mode, WebCore::StoredCredentialsPolicy, String&& cachePartition);

    // NetworkLoadClient.
    bool isSynchronous() const final { return false; }
    bool isAllowedToAskUserForCredentials() const final { return false; }
    void didSendData(uint64_t, uint64_t) final { }
    void willSendRedirectedRequest(WebCore::ResourceRequest&&, WebCore::ResourceRequest&& redirectRequest, WebCore::ResourceResponse&&, CompletionHandler<void(WebCore::ResourceRequest&&)>&&) final;
    void didReceiveResponse(WebCore::ResourceResponse&&, PrivateRelayed, ResponseCompletionHandler&&) final;
    void didReceiveBuffer(const WebCore::FragmentedSharedBuffer&) final;
    void didFinishLoading(const WebCore::NetworkLoadMetrics&) final;
    void didFailLoading(const WebCore::ResourceError&) final;

    void complete();

    const Ref<NetworkLoad> m_networkLoad;
    const CheckedPtr<NetworkSession> m_session;
    const NetworkCache::GlobalFrameID m_frameID;
    const WebCore::SecurityOriginData m_hintingOrigin;
    const URL m_url;
    String m_destination;
    const WebCore::FetchOptions::Mode m_mode;
    const WebCore::StoredCredentialsPolicy m_storedCredentialsPolicy;
    const String m_cachePartition;
    WebCore::ResourceResponse m_response;
    WebCore::SharedBufferBuilder m_buffer;
    PrivateRelayed m_privateRelayed { PrivateRelayed::No };
    RefPtr<EarlyHintsPreloadTask> m_selfKeepAlive;
};

} // namespace WebKit

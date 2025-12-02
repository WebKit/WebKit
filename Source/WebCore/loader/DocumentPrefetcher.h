/*
 * Copyright (C) 2025 Shopify Inc. All rights reserved.
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

#include <WebCore/CachedResourceClient.h>
#include <WebCore/CachedResourceHandle.h>
#include <optional>
#include <wtf/CompletionHandler.h>
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/URLHash.h>


namespace WebCore {

class CachedRawResource;
class DocumentLoader;
class FrameLoader;
class ResourceRequest;

enum class ReferrerPolicy : uint8_t;

class DocumentPrefetcher : public RefCounted<DocumentPrefetcher>, public CachedRawResourceClient {
public:
    struct PrefetchedResourceData {
        // The resource needs to be here in order to be kept alive.
        CachedResourceHandle<CachedRawResource> resource;
        Box<NetworkLoadMetrics> metrics;
    };
    static Ref<DocumentPrefetcher> create(FrameLoader& frameLoader) { return adoptRef(*new DocumentPrefetcher(frameLoader)); }
    ~DocumentPrefetcher();

    // CachedResourceClient.
    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

    void prefetch(const URL&, const Vector<String>& tags, std::optional<ReferrerPolicy>, bool lowPriority = false);
    bool wasPrefetched(const URL&) const;
    Box<NetworkLoadMetrics> takePrefetchedNetworkLoadMetrics(const URL&);

    // CachedRawResourceClient
    void responseReceived(const CachedResource&, const ResourceResponse&, CompletionHandler<void()>&&) override;
    void notifyFinished(CachedResource&, const NetworkLoadMetrics&, LoadWillContinueInAnotherProcess = LoadWillContinueInAnotherProcess::No) override;
    CachedResourceClientType resourceClientType() const override { return RawResourceType; }


private:
    explicit DocumentPrefetcher(FrameLoader&);

    WeakRef<FrameLoader> m_frameLoader;
    HashMap<URL, PrefetchedResourceData> m_prefetchedData;
};

} // namespace WebCore

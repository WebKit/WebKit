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
#include "PrivateRelayed.h"
#include <WebCore/FetchOptions.h>
#include <WebCore/ResourceResponse.h>
#include <WebCore/SecurityOriginData.h>
#include <WebCore/StoredCredentialsPolicy.h>
#include <WebCore/Timer.h>
#include <wtf/CheckedPtr.h>
#include <wtf/HashMap.h>
#include <wtf/RefPtr.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/URLHash.h>
#include <wtf/UniqueRef.h>
#include <wtf/WallTime.h>
#include <wtf/text/WTFString.h>

namespace WebCore {
class ContentSecurityPolicy;
class FragmentedSharedBuffer;
}

namespace WebKit {

// Per-navigation store of HTTP 103 `Link: rel=preload` resources, consumed by later subresource loads.
class EarlyHintsPreloadCache : public CanMakeCheckedPtr<EarlyHintsPreloadCache> {
    WTF_MAKE_TZONE_ALLOCATED(EarlyHintsPreloadCache);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(EarlyHintsPreloadCache);
    WTF_MAKE_NONCOPYABLE(EarlyHintsPreloadCache);
public:
    EarlyHintsPreloadCache();
    ~EarlyHintsPreloadCache();

    struct Entry {
        WTF_DEPRECATED_MAKE_STRUCT_FAST_ALLOCATED(Entry);
        Entry(WebCore::ResourceResponse&&, PrivateRelayed, RefPtr<WebCore::FragmentedSharedBuffer>&&, String&& destination, WebCore::FetchOptions::Mode, WebCore::StoredCredentialsPolicy, String&& cachePartition);

        RefPtr<WebCore::FragmentedSharedBuffer> releaseBuffer() { return WTF::move(buffer); }

        WebCore::ResourceResponse response;
        PrivateRelayed privateRelayed { PrivateRelayed::No };
        RefPtr<WebCore::FragmentedSharedBuffer> buffer;
        String destination;
        // A consumer is only served an entry whose CORS mode, credentials policy and cache partition match its request.
        WebCore::FetchOptions::Mode mode { WebCore::FetchOptions::Mode::NoCors };
        WebCore::StoredCredentialsPolicy storedCredentialsPolicy { WebCore::StoredCredentialsPolicy::DoNotUse };
        String cachePartition;
    };

    // Records the 103's origin so late preloads from a superseded cross-origin navigation are rejected.
    void registerNavigation(const NetworkCache::GlobalFrameID&, const WebCore::SecurityOriginData& hintingOrigin);

    void store(const NetworkCache::GlobalFrameID&, const WebCore::SecurityOriginData& hintingOrigin, const URL&, String&& destination, WebCore::FetchOptions::Mode, WebCore::StoredCredentialsPolicy, String&& cachePartition, WebCore::ResourceResponse&&, PrivateRelayed, RefPtr<WebCore::FragmentedSharedBuffer>&&);
    std::unique_ptr<Entry> take(const NetworkCache::GlobalFrameID&, const URL&, WebCore::FetchOptions::Mode, WebCore::StoredCredentialsPolicy, const String& cachePartition);

    // The final response's CSP may differ from the 103's.
    void pruneForFinalResponse(const NetworkCache::GlobalFrameID&, const WebCore::ContentSecurityPolicy&, const URL& baseURL);

    void clear(const NetworkCache::GlobalFrameID&);
    void clear();

private:
    void clearExpiredEntries();

    // Expiry is per-navigation since all preloads arrive in one 103; origin gates late stores.
    struct Navigation {
        WTF_DEPRECATED_MAKE_STRUCT_FAST_ALLOCATED(Navigation);
        HashMap<URL, UniqueRef<Entry>> entries;
        WebCore::SecurityOriginData origin;
        WallTime expiry;
    };
    HashMap<NetworkCache::GlobalFrameID, UniqueRef<Navigation>> m_navigations;
    WebCore::Timer m_expirationTimer;
};

} // namespace WebKit

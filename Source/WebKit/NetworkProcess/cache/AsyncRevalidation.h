/*
 * Copyright (C) 2019 Igalia S.L.
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

#if ENABLE(NETWORK_CACHE_STALE_WHILE_REVALIDATE)

#include "NetworkCache.h"
#include "NetworkCacheEntry.h"
#include "NetworkCacheSpeculativeLoad.h"
#include <wtf/CompletionHandler.h>
#include <wtf/WeakPtr.h>

namespace WebCore {
enum class NetworkConnectionIntegrity : uint8_t;
class ResourceRequest;
};

namespace WebKit {

class SpeculativeLoad;

namespace NetworkCache {

class AsyncRevalidation : public CanMakeWeakPtr<AsyncRevalidation> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    enum class Result {
        Failure,
        Timeout,
        Success,
    };
    AsyncRevalidation(Cache&, const GlobalFrameID&, const WebCore::ResourceRequest&, std::unique_ptr<NetworkCache::Entry>&&, std::optional<NavigatingToAppBoundDomain>, bool allowPrivacyProxy, OptionSet<WebCore::NetworkConnectionIntegrity> networkConnectionIntegrityPolicy, CompletionHandler<void(Result)>&&);
    void cancel();

    const SpeculativeLoad& load() const { return *m_load; }

private:
    void staleWhileRevalidateEnding();

    std::unique_ptr<SpeculativeLoad> m_load;
    WebCore::Timer m_timer;
    CompletionHandler<void(Result)> m_completionHandler;
};

} // namespace NetworkCache
} // namespace WebKit

#endif // ENABLE(NETWORK_CACHE_STALE_WHILE_REVALIDATE)

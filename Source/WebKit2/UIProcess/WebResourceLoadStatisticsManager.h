/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include <chrono>
#include <wtf/Seconds.h>
#include <wtf/text/WTFString.h>

namespace WebCore {
class URL;
}

namespace WebKit {

class WebResourceLoadStatisticsStore;

// FIXME: This should probably become an APIObject for the WebResourceLoadStatisticsStore.
class WebResourceLoadStatisticsManager {
    friend class NeverDestroyed<WebResourceLoadStatisticsManager>;
public:
    // FIXME: This should not be a singleton.
    static WebResourceLoadStatisticsManager& shared();
    void setStatisticsStore(Ref<WebResourceLoadStatisticsStore>&&);

    ~WebResourceLoadStatisticsManager();

    void logUserInteraction(const WebCore::URL&);
    bool hasHadUserInteraction(const WebCore::URL&);
    void clearUserInteraction(const WebCore::URL&);

    void setPrevalentResource(const WebCore::URL&);
    bool isPrevalentResource(const WebCore::URL&);
    void clearPrevalentResource(const WebCore::URL&);
    void setGrandfathered(const WebCore::URL&, bool value);
    bool isGrandfathered(const WebCore::URL&);

    void setSubframeUnderTopFrameOrigin(const WebCore::URL& subframe, const WebCore::URL& topFrame);
    void setSubresourceUnderTopFrameOrigin(const WebCore::URL& subresource, const WebCore::URL& topFrame);
    void setSubresourceUniqueRedirectTo(const WebCore::URL& subresource, const WebCore::URL& hostNameRedirectedTo);

    void setTimeToLiveUserInteraction(Seconds);
    void setTimeToLiveCookiePartitionFree(Seconds);
    void setMinimumTimeBetweenDataRecordsRemoval(Seconds);
    void setGrandfatheringTime(Seconds);

    void fireDataModificationHandler();
    void fireShouldPartitionCookiesHandler();
    void fireShouldPartitionCookiesHandler(const Vector<String>& domainsToRemove, const Vector<String>& domainsToAdd, bool clearFirst);
    void fireTelemetryHandler();

    void clearInMemoryStore();
    void clearInMemoryAndPersistentStore();
    void clearInMemoryAndPersistentStore(std::chrono::system_clock::time_point modifiedSince);

private:
    WebResourceLoadStatisticsManager();

    RefPtr<WebResourceLoadStatisticsStore> m_store;
};

} // namespace WebKit

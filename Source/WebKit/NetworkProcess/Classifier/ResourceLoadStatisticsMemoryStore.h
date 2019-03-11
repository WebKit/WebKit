/*
 * Copyright (C) 2017-2019 Apple Inc. All rights reserved.
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

#if ENABLE(RESOURCE_LOAD_STATISTICS)

#include "ResourceLoadStatisticsStore.h"
#include "WebResourceLoadStatisticsStore.h"
#include <wtf/CompletionHandler.h>
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>
#include <wtf/WorkQueue.h>

namespace WebCore {
class KeyedDecoder;
class KeyedEncoder;
struct ResourceLoadStatistics;
}

namespace WebKit {

class ResourceLoadStatisticsPersistentStorage;

// This is always constructed / used / destroyed on the WebResourceLoadStatisticsStore's statistics queue.
class ResourceLoadStatisticsMemoryStore final : public ResourceLoadStatisticsStore {
public:
    ResourceLoadStatisticsMemoryStore(WebResourceLoadStatisticsStore&, WorkQueue&, ShouldIncludeLocalhost);

    void setPersistentStorage(ResourceLoadStatisticsPersistentStorage&);

    void clear(CompletionHandler<void()>&&) override;
    bool isEmpty() const override;

    const HashMap<RegistrableDomain, WebCore::ResourceLoadStatistics>& data() const { return m_resourceStatisticsMap; }

    std::unique_ptr<WebCore::KeyedEncoder> createEncoderFromData() const;
    void mergeWithDataFromDecoder(WebCore::KeyedDecoder&);

    void mergeStatistics(Vector<ResourceLoadStatistics>&&);
    void processStatistics(const Function<void(const ResourceLoadStatistics&)>&) const;

    void updateCookieBlocking(CompletionHandler<void()>&&) override;

    void classifyPrevalentResources() override;
    void syncStorageIfNeeded() override;
    void syncStorageImmediately() override;

    void requestStorageAccessUnderOpener(DomainInNeedOfStorageAccess&&, OpenerPageID, OpenerDomain&&) override;

    void grandfatherDataForDomains(const HashSet<RegistrableDomain>&) override;

    bool isRegisteredAsSubresourceUnder(const SubResourceDomain&, const TopFrameDomain&) const override;
    bool isRegisteredAsSubFrameUnder(const SubFrameDomain&, const TopFrameDomain&) const override;
    bool isRegisteredAsRedirectingTo(const RedirectedFromDomain&, const RedirectedToDomain&) const override;

    void clearPrevalentResource(const RegistrableDomain&) override;
    String dumpResourceLoadStatistics() const override;
    bool isPrevalentResource(const RegistrableDomain&) const override;
    bool isVeryPrevalentResource(const RegistrableDomain&) const override;
    void setPrevalentResource(const RegistrableDomain&) override;
    void setVeryPrevalentResource(const RegistrableDomain&) override;

    void setGrandfathered(const RegistrableDomain&, bool value) override;
    bool isGrandfathered(const RegistrableDomain&) const override;

    void setSubframeUnderTopFrameDomain(const SubFrameDomain&, const TopFrameDomain&) override;
    void setSubresourceUnderTopFrameDomain(const SubResourceDomain&, const TopFrameDomain&) override;
    void setSubresourceUniqueRedirectTo(const SubResourceDomain&, const RedirectDomain&) override;
    void setSubresourceUniqueRedirectFrom(const SubResourceDomain&, const RedirectDomain&) override;
    void setTopFrameUniqueRedirectTo(const TopFrameDomain&, const RedirectDomain&) override;
    void setTopFrameUniqueRedirectFrom(const TopFrameDomain&, const RedirectDomain&) override;

    void calculateAndSubmitTelemetry() const override;

    void hasStorageAccess(const SubFrameDomain&, const TopFrameDomain&, Optional<FrameID>, PageID, CompletionHandler<void(bool)>&&) override;
    void requestStorageAccess(SubFrameDomain&&, TopFrameDomain&&, FrameID, PageID, CompletionHandler<void(StorageAccessStatus)>&&) override;
    void grantStorageAccess(SubFrameDomain&&, TopFrameDomain&&, FrameID, PageID, bool userWasPromptedNow, CompletionHandler<void(bool)>&&) override;

    void logFrameNavigation(const NavigatedToDomain&, const TopFrameDomain&, const NavigatedFromDomain&, bool isRedirect, bool isMainFrame) override;
    void logUserInteraction(const TopFrameDomain&) override;
    void logSubresourceLoading(const SubResourceDomain&, const TopFrameDomain&, WallTime lastSeen) override;
    void logSubresourceRedirect(const RedirectedFromDomain&, const RedirectedToDomain&) override;
    void logCrossSiteLoadWithLinkDecoration(const NavigatedFromDomain&, const NavigatedToDomain&) override;

    void clearUserInteraction(const RegistrableDomain&) override;
    bool hasHadUserInteraction(const RegistrableDomain&) override;

    void setLastSeen(const RegistrableDomain&, Seconds) override;

private:
    static bool shouldBlockAndKeepCookies(const ResourceLoadStatistics&);
    static bool shouldBlockAndPurgeCookies(const ResourceLoadStatistics&);
    static bool hasUserGrantedStorageAccessThroughPrompt(const ResourceLoadStatistics&, const RegistrableDomain&);
    bool hasHadUnexpiredRecentUserInteraction(ResourceLoadStatistics&) const;
    bool wasAccessedAsFirstPartyDueToUserInteraction(const ResourceLoadStatistics& current, const ResourceLoadStatistics& updated) const;
    void incrementRecordsDeletedCountForDomains(HashSet<RegistrableDomain>&&) override;
    void setPrevalentResource(ResourceLoadStatistics&, ResourceLoadPrevalence);
    unsigned recursivelyGetAllDomainsThatHaveRedirectedToThisDomain(const ResourceLoadStatistics&, HashSet<RedirectedToDomain>&, unsigned numberOfRecursiveCalls) const;
    void grantStorageAccessInternal(SubFrameDomain&&, TopFrameDomain&&, Optional<FrameID>, PageID, bool userWasPromptedNowOrEarlier, CompletionHandler<void(bool)>&&);
    void markAsPrevalentIfHasRedirectedToPrevalent(ResourceLoadStatistics&);
    bool isPrevalentDueToDebugMode(ResourceLoadStatistics&);
    Vector<RegistrableDomain> ensurePrevalentResourcesForDebugMode() override;
    void removeDataRecords(CompletionHandler<void()>&&);
    void pruneStatisticsIfNeeded() override;
    ResourceLoadStatistics& ensureResourceStatisticsForRegistrableDomain(const RegistrableDomain&);
    Vector<RegistrableDomain> registrableDomainsToRemoveWebsiteDataFor() override;
    bool isMemoryStore() const final { return true; }


    WeakPtr<ResourceLoadStatisticsPersistentStorage> m_persistentStorage;
    HashMap<RegistrableDomain, ResourceLoadStatistics> m_resourceStatisticsMap;
};

} // namespace WebKit

SPECIALIZE_TYPE_TRAITS_BEGIN(WebKit::ResourceLoadStatisticsMemoryStore)
    static bool isType(const WebKit::ResourceLoadStatisticsStore& store) { return store.isMemoryStore(); }
SPECIALIZE_TYPE_TRAITS_END()

#endif

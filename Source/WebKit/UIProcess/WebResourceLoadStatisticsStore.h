/*
 * Copyright (C) 2016-2018 Apple Inc. All rights reserved.
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

#include "Connection.h"
#include "WebsiteDataType.h"
#include <wtf/CompletionHandler.h>
#include <wtf/RunLoop.h>
#include <wtf/Vector.h>
#include <wtf/WallTime.h>
#include <wtf/text/WTFString.h>

namespace WTF {
class WorkQueue;
}

namespace WebCore {
class ResourceRequest;
class URL;
struct ResourceLoadStatistics;
}

namespace WebKit {

class ResourceLoadStatisticsMemoryStore;
class ResourceLoadStatisticsPersistentStorage;
class WebFrameProxy;
class WebProcessProxy;

enum class ShouldClearFirst;
enum class StorageAccessStatus {
    CannotRequestAccess,
    RequiresUserPrompt,
    HasAccess
};

class WebResourceLoadStatisticsStore final : public IPC::Connection::WorkQueueMessageReceiver {
public:
    using UpdatePrevalentDomainsToPartitionOrBlockCookiesHandler = Function<void(const Vector<String>& domainsToPartition, const Vector<String>& domainsToBlock, const Vector<String>& domainsToNeitherPartitionNorBlock, ShouldClearFirst, CompletionHandler<void()>&&)>;
    using HasStorageAccessForFrameHandler = Function<void(const String& resourceDomain, const String& firstPartyDomain, uint64_t frameID, uint64_t pageID, Function<void(bool hasAccess)>&& callback)>;
    using GrantStorageAccessHandler = Function<void(const String& resourceDomain, const String& firstPartyDomain, std::optional<uint64_t> frameID, uint64_t pageID, Function<void(bool wasGranted)>&& callback)>;
    using RemoveAllStorageAccessHandler = Function<void()>;
    using RemovePrevalentDomainsHandler = Function<void(const Vector<String>&)>;
    static Ref<WebResourceLoadStatisticsStore> create(const String& resourceLoadStatisticsDirectory, Function<void (const String&)>&& testingCallback, bool isEphemeral, UpdatePrevalentDomainsToPartitionOrBlockCookiesHandler&& updatePrevalentDomainsToPartitionOrBlockCookiesHandler = [](const Vector<String>&, const Vector<String>&, const Vector<String>&, ShouldClearFirst, CompletionHandler<void()>&& callback) { callback(); }, HasStorageAccessForFrameHandler&& hasStorageAccessForFrameHandler = [](const String&, const String&, uint64_t, uint64_t, Function<void(bool)>&&) { }, GrantStorageAccessHandler&& grantStorageAccessHandler = [](const String&, const String&, std::optional<uint64_t>, uint64_t, Function<void(bool)>&&) { }, RemoveAllStorageAccessHandler&& removeAllStorageAccessHandler = []() { }, RemovePrevalentDomainsHandler&& removeDomainsHandler = [] (const Vector<String>&) { })
    {
        return adoptRef(*new WebResourceLoadStatisticsStore(resourceLoadStatisticsDirectory, WTFMove(testingCallback), isEphemeral, WTFMove(updatePrevalentDomainsToPartitionOrBlockCookiesHandler), WTFMove(hasStorageAccessForFrameHandler), WTFMove(grantStorageAccessHandler), WTFMove(removeAllStorageAccessHandler), WTFMove(removeDomainsHandler)));
    }

    ~WebResourceLoadStatisticsStore();

    static const OptionSet<WebsiteDataType>& monitoredDataTypes();

    WorkQueue& statisticsQueue() { return m_statisticsQueue.get(); }

    void setNotifyPagesWhenDataRecordsWereScanned(bool);
    void setShouldClassifyResourcesBeforeDataRecordsRemoval(bool);
    void setShouldSubmitTelemetry(bool);

    void resourceLoadStatisticsUpdated(Vector<WebCore::ResourceLoadStatistics>&& origins);

    void hasStorageAccess(String&& subFrameHost, String&& topFrameHost, uint64_t frameID, uint64_t pageID, CompletionHandler<void(bool)>&& callback);
    void requestStorageAccess(String&& subFrameHost, String&& topFrameHost, uint64_t frameID, uint64_t pageID, bool promptEnabled, CompletionHandler<void(StorageAccessStatus)>&&);
    void requestStorageAccessUnderOpener(String&& primaryDomainInNeedOfStorageAccess, uint64_t openerPageID, String&& openerPrimaryDomain, bool isTriggeredByUserGesture);
    void grantStorageAccess(String&& subFrameHost, String&& topFrameHost, uint64_t frameID, uint64_t pageID, bool userWasPromptedNow, CompletionHandler<void(bool)>&&);
    void requestStorageAccessCallback(bool wasGranted, uint64_t contextId);

    void processWillOpenConnection(WebProcessProxy&, IPC::Connection&);
    void processDidCloseConnection(WebProcessProxy&, IPC::Connection&);
    void applicationWillTerminate();

    void logFrameNavigation(const WebFrameProxy&, const WebCore::URL& pageURL, const WebCore::ResourceRequest&, const WebCore::URL& redirectURL);
    void logUserInteraction(const WebCore::URL&);
    void logNonRecentUserInteraction(const WebCore::URL&);
    void clearUserInteraction(const WebCore::URL&);
    void hasHadUserInteraction(const WebCore::URL&, CompletionHandler<void (bool)>&&);
    void setLastSeen(const WebCore::URL&, Seconds);
    void setPrevalentResource(const WebCore::URL&);
    void setVeryPrevalentResource(const WebCore::URL&);
    void isPrevalentResource(const WebCore::URL&, CompletionHandler<void (bool)>&&);
    void isVeryPrevalentResource(const WebCore::URL&, CompletionHandler<void(bool)>&&);
    void isRegisteredAsSubFrameUnder(const WebCore::URL& subFrame, const WebCore::URL& topFrame, CompletionHandler<void (bool)>&&);
    void isRegisteredAsRedirectingTo(const WebCore::URL& hostRedirectedFrom, const WebCore::URL& hostRedirectedTo, CompletionHandler<void (bool)>&&);
    void clearPrevalentResource(const WebCore::URL&);
    void setGrandfathered(const WebCore::URL&, bool);
    void isGrandfathered(const WebCore::URL&, CompletionHandler<void (bool)>&&);
    void setSubframeUnderTopFrameOrigin(const WebCore::URL& subframe, const WebCore::URL& topFrame);
    void setSubresourceUnderTopFrameOrigin(const WebCore::URL& subresource, const WebCore::URL& topFrame);
    void setSubresourceUniqueRedirectTo(const WebCore::URL& subresource, const WebCore::URL& hostNameRedirectedTo);
    void setSubresourceUniqueRedirectFrom(const WebCore::URL& subresource, const WebCore::URL& hostNameRedirectedFrom);
    void setTopFrameUniqueRedirectTo(const WebCore::URL& topFrameHostName, const WebCore::URL& hostNameRedirectedTo);
    void setTopFrameUniqueRedirectFrom(const WebCore::URL& topFrameHostName, const WebCore::URL& hostNameRedirectedFrom);
    void scheduleCookiePartitioningUpdate(CompletionHandler<void()>&&);
    void scheduleCookiePartitioningUpdateForDomains(const Vector<String>& domainsToPartition, const Vector<String>& domainsToBlock, const Vector<String>& domainsToNeitherPartitionNorBlock, ShouldClearFirst, CompletionHandler<void()>&&);
    void scheduleClearPartitioningStateForDomains(const Vector<String>& domains, CompletionHandler<void()>&&);
    void scheduleStatisticsAndDataRecordsProcessing();
    void submitTelemetry();
    void scheduleCookiePartitioningStateReset();

    void scheduleClearInMemory();
    
    enum class ShouldGrandfather {
        No,
        Yes,
    };
    void scheduleClearInMemoryAndPersistent(ShouldGrandfather, CompletionHandler<void()>&&);
    void scheduleClearInMemoryAndPersistent(WallTime modifiedSince, ShouldGrandfather, CompletionHandler<void()>&&);

    void setTimeToLiveUserInteraction(Seconds);
    void setTimeToLiveCookiePartitionFree(Seconds);
    void setMinimumTimeBetweenDataRecordsRemoval(Seconds);
    void setGrandfatheringTime(Seconds);
    void setMaxStatisticsEntries(size_t);
    void setPruneEntriesDownTo(size_t);

    void resetParametersToDefaultValues();

    void setResourceLoadStatisticsDebugMode(bool);

    void setStatisticsTestingCallback(Function<void (const String&)>&& callback) { m_statisticsTestingCallback = WTFMove(callback); }
    void logTestingEvent(const String&);
    void callGrantStorageAccessHandler(const String& subFramePrimaryDomain, const String& topFramePrimaryDomain, std::optional<uint64_t> frameID, uint64_t pageID, CompletionHandler<void(bool)>&&);
    void removeAllStorageAccess();
    void callUpdatePrevalentDomainsToPartitionOrBlockCookiesHandler(const Vector<String>& domainsToPartition, const Vector<String>& domainsToBlock, const Vector<String>& domainsToNeitherPartitionNorBlock, ShouldClearFirst, CompletionHandler<void()>&&);
    void callRemoveDomainsHandler(const Vector<String>& domains);
    void callHasStorageAccessForFrameHandler(const String& resourceDomain, const String& firstPartyDomain, uint64_t frameID, uint64_t pageID, Function<void(bool)>&&);

private:
    WebResourceLoadStatisticsStore(const String&, Function<void(const String&)>&& testingCallback, bool isEphemeral, UpdatePrevalentDomainsToPartitionOrBlockCookiesHandler&&, HasStorageAccessForFrameHandler&&, GrantStorageAccessHandler&&, RemoveAllStorageAccessHandler&&, RemovePrevalentDomainsHandler&&);

    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;

    void performDailyTasks();

    StorageAccessStatus storageAccessStatus(const String& subFramePrimaryDomain, const String& topFramePrimaryDomain);

    void flushAndDestroyPersistentStore();

    Ref<WorkQueue> m_statisticsQueue;
    std::unique_ptr<ResourceLoadStatisticsMemoryStore> m_memoryStore;
    std::unique_ptr<ResourceLoadStatisticsPersistentStorage> m_persistentStorage;

    UpdatePrevalentDomainsToPartitionOrBlockCookiesHandler m_updatePrevalentDomainsToPartitionOrBlockCookiesHandler;
    HasStorageAccessForFrameHandler m_hasStorageAccessForFrameHandler;
    GrantStorageAccessHandler m_grantStorageAccessHandler;
    RemoveAllStorageAccessHandler m_removeAllStorageAccessHandler;
    RemovePrevalentDomainsHandler m_removeDomainsHandler;

    RunLoop::Timer<WebResourceLoadStatisticsStore> m_dailyTasksTimer;

    bool m_hasScheduledProcessStats { false };

    Function<void (const String&)> m_statisticsTestingCallback;
};

} // namespace WebKit

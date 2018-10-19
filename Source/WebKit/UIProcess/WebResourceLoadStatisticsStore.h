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
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/Vector.h>
#include <wtf/WallTime.h>
#include <wtf/WeakPtr.h>
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
class WebsiteDataStore;

enum class StorageAccessStatus {
    CannotRequestAccess,
    RequiresUserPrompt,
    HasAccess
};

class WebResourceLoadStatisticsStore final : public ThreadSafeRefCounted<WebResourceLoadStatisticsStore, WTF::DestructionThread::Main>, public IPC::MessageReceiver {
public:
    static Ref<WebResourceLoadStatisticsStore> create(WebsiteDataStore& websiteDataStore)
    {
        return adoptRef(*new WebResourceLoadStatisticsStore(websiteDataStore));
    }

    ~WebResourceLoadStatisticsStore();

    static const OptionSet<WebsiteDataType>& monitoredDataTypes();

    WorkQueue& statisticsQueue() { return m_statisticsQueue.get(); }

    void setNotifyPagesWhenDataRecordsWereScanned(bool);
    void setShouldClassifyResourcesBeforeDataRecordsRemoval(bool);
    void setShouldSubmitTelemetry(bool);

    void hasStorageAccess(String&& subFrameHost, String&& topFrameHost, uint64_t frameID, uint64_t pageID, CompletionHandler<void(bool)>&& callback);
    void requestStorageAccess(String&& subFrameHost, String&& topFrameHost, uint64_t frameID, uint64_t pageID, bool promptEnabled, CompletionHandler<void(StorageAccessStatus)>&&);
    void grantStorageAccess(String&& subFrameHost, String&& topFrameHost, uint64_t frameID, uint64_t pageID, bool userWasPromptedNow, CompletionHandler<void(bool)>&&);
    void requestStorageAccessCallback(bool wasGranted, uint64_t contextId);

    void applicationWillTerminate();

    void logFrameNavigation(const WebFrameProxy&, const WebCore::URL& pageURL, const WebCore::ResourceRequest&, const WebCore::URL& redirectURL);
    void logUserInteraction(const WebCore::URL&, CompletionHandler<void()>&&);
    void clearUserInteraction(const WebCore::URL&, CompletionHandler<void()>&&);
    void hasHadUserInteraction(const WebCore::URL&, CompletionHandler<void(bool)>&&);
    void setLastSeen(const WebCore::URL&, Seconds, CompletionHandler<void()>&&);
    void setPrevalentResource(const WebCore::URL&, CompletionHandler<void()>&&);
    void setVeryPrevalentResource(const WebCore::URL&, CompletionHandler<void()>&&);
    void dumpResourceLoadStatistics(CompletionHandler<void(const String&)>&&);
    void isPrevalentResource(const WebCore::URL&, CompletionHandler<void(bool)>&&);
    void isVeryPrevalentResource(const WebCore::URL&, CompletionHandler<void(bool)>&&);
    void isRegisteredAsSubresourceUnder(const WebCore::URL& subresource, const WebCore::URL& topFrame, CompletionHandler<void(bool)>&&);
    void isRegisteredAsSubFrameUnder(const WebCore::URL& subFrame, const WebCore::URL& topFrame, CompletionHandler<void(bool)>&&);
    void isRegisteredAsRedirectingTo(const WebCore::URL& hostRedirectedFrom, const WebCore::URL& hostRedirectedTo, CompletionHandler<void(bool)>&&);
    void clearPrevalentResource(const WebCore::URL&, CompletionHandler<void()>&&);
    void setGrandfathered(const WebCore::URL&, bool);
    void isGrandfathered(const WebCore::URL&, CompletionHandler<void(bool)>&&);
    void setSubframeUnderTopFrameOrigin(const WebCore::URL& subframe, const WebCore::URL& topFrame);
    void setSubresourceUnderTopFrameOrigin(const WebCore::URL& subresource, const WebCore::URL& topFrame);
    void setSubresourceUniqueRedirectTo(const WebCore::URL& subresource, const WebCore::URL& hostNameRedirectedTo);
    void setSubresourceUniqueRedirectFrom(const WebCore::URL& subresource, const WebCore::URL& hostNameRedirectedFrom);
    void setTopFrameUniqueRedirectTo(const WebCore::URL& topFrameHostName, const WebCore::URL& hostNameRedirectedTo);
    void setTopFrameUniqueRedirectFrom(const WebCore::URL& topFrameHostName, const WebCore::URL& hostNameRedirectedFrom);
    void scheduleCookieBlockingUpdate(CompletionHandler<void()>&&);
    void scheduleCookieBlockingUpdateForDomains(const Vector<String>& domainsToBlock, CompletionHandler<void()>&&);
    void scheduleClearBlockingStateForDomains(const Vector<String>& domains, CompletionHandler<void()>&&);
    void scheduleStatisticsAndDataRecordsProcessing();
    void submitTelemetry();

    enum class ShouldGrandfather {
        No,
        Yes,
    };
    void scheduleClearInMemoryAndPersistent(ShouldGrandfather, CompletionHandler<void()>&&);
    void scheduleClearInMemoryAndPersistent(WallTime modifiedSince, ShouldGrandfather, CompletionHandler<void()>&&);

    void setTimeToLiveUserInteraction(Seconds);
    void setMinimumTimeBetweenDataRecordsRemoval(Seconds);
    void setGrandfatheringTime(Seconds);
    void setCacheMaxAgeCap(Seconds, CompletionHandler<void()>&&);
    void setMaxStatisticsEntries(size_t);
    void setPruneEntriesDownTo(size_t);

    void resetParametersToDefaultValues(CompletionHandler<void()>&&);

    void setResourceLoadStatisticsDebugMode(bool, CompletionHandler<void()>&&);
    void setPrevalentResourceForDebugMode(const WebCore::URL&, CompletionHandler<void()>&&);
    
    void setStatisticsTestingCallback(WTF::Function<void(const String&)>&& callback) { m_statisticsTestingCallback = WTFMove(callback); }
    void logTestingEvent(const String&);
    void callGrantStorageAccessHandler(const String& subFramePrimaryDomain, const String& topFramePrimaryDomain, std::optional<uint64_t> frameID, uint64_t pageID, CompletionHandler<void(bool)>&&);
    void removeAllStorageAccess(CompletionHandler<void()>&&);
    void callUpdatePrevalentDomainsToBlockCookiesForHandler(const Vector<String>& domainsToBlock, CompletionHandler<void()>&&);
    void callRemoveDomainsHandler(const Vector<String>& domains);
    void callHasStorageAccessForFrameHandler(const String& resourceDomain, const String& firstPartyDomain, uint64_t frameID, uint64_t pageID, CompletionHandler<void(bool)>&&);

    void didCreateNetworkProcess();

private:
    explicit WebResourceLoadStatisticsStore(WebsiteDataStore&);

    void postTask(WTF::Function<void()>&&);
    static void postTaskReply(WTF::Function<void()>&&);

    // IPC::MessageReceiver.
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;

    // IPC message handlers.
    void resourceLoadStatisticsUpdated(Vector<WebCore::ResourceLoadStatistics>&& origins);
    void requestStorageAccessUnderOpener(String&& primaryDomainInNeedOfStorageAccess, uint64_t openerPageID, String&& openerPrimaryDomain);

    void performDailyTasks();

    StorageAccessStatus storageAccessStatus(const String& subFramePrimaryDomain, const String& topFramePrimaryDomain);

    void flushAndDestroyPersistentStore();

    WeakPtr<WebsiteDataStore> m_websiteDataStore;
    Ref<WorkQueue> m_statisticsQueue;
    std::unique_ptr<ResourceLoadStatisticsMemoryStore> m_memoryStore;
    std::unique_ptr<ResourceLoadStatisticsPersistentStorage> m_persistentStorage;

    RunLoop::Timer<WebResourceLoadStatisticsStore> m_dailyTasksTimer;

    bool m_hasScheduledProcessStats { false };

    WTF::Function<void(const String&)> m_statisticsTestingCallback;

    bool m_firstNetworkProcessCreated { false };
};

} // namespace WebKit

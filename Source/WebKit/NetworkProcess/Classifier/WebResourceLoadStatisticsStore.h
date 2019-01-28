/*
 * Copyright (C) 2016-2019 Apple Inc. All rights reserved.
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

#include "Connection.h"
#include "StorageAccessStatus.h"
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
struct ResourceLoadStatistics;
enum class ShouldSample : bool;
}

namespace WebKit {

class NetworkSession;
class ResourceLoadStatisticsMemoryStore;
class ResourceLoadStatisticsPersistentStorage;
class WebFrameProxy;
class WebProcessProxy;
enum class ShouldGrandfatherStatistics : bool;

class WebResourceLoadStatisticsStore final : public ThreadSafeRefCounted<WebResourceLoadStatisticsStore, WTF::DestructionThread::Main>, public IPC::MessageReceiver {
public:
    static Ref<WebResourceLoadStatisticsStore> create(NetworkSession& networkSession, const String& resourceLoadStatisticsDirectory)
    {
        return adoptRef(*new WebResourceLoadStatisticsStore(networkSession, resourceLoadStatisticsDirectory));
    }

    ~WebResourceLoadStatisticsStore();

    static const OptionSet<WebsiteDataType>& monitoredDataTypes();

    WorkQueue& statisticsQueue() { return m_statisticsQueue.get(); }

    void setNotifyPagesWhenDataRecordsWereScanned(bool);
    void setNotifyPagesWhenTelemetryWasCaptured(bool, CompletionHandler<void()>&&);
    void setShouldClassifyResourcesBeforeDataRecordsRemoval(bool, CompletionHandler<void()>&&);
    void setShouldSubmitTelemetry(bool);

    void requestStorageAccess(String&& subFrameHost, String&& topFrameHost, uint64_t frameID, uint64_t pageID, bool promptEnabled, CompletionHandler<void(StorageAccessStatus)>&&);
    void grantStorageAccess(const String& subFrameHost, const String& topFrameHost, uint64_t frameID, uint64_t pageID, bool userWasPromptedNow, CompletionHandler<void(bool)>&&);
    void requestStorageAccessCallback(bool wasGranted, uint64_t contextId);

    void applicationWillTerminate();

    void logFrameNavigation(const WebFrameProxy&, const URL& pageURL, const WebCore::ResourceRequest&, const URL& redirectURL);
    void logFrameNavigation(const String& targetPrimaryDomain, const String& mainFramePrimaryDomain, const String& sourcePrimaryDomain, const String& targetHost, const String& mainFrameHost, bool isRedirect, bool isMainFrame);
    void logUserInteraction(const String& targetPrimaryDomain, CompletionHandler<void()>&&);
    void logWebSocketLoading(const String& targetPrimaryDomain, const String& mainFramePrimaryDomain, WallTime lastSeen, CompletionHandler<void()>&&);
    void logSubresourceLoading(const String& targetPrimaryDomain, const String& mainFramePrimaryDomain, WallTime lastSeen, CompletionHandler<void()>&&);
    void logSubresourceRedirect(const String& sourcePrimaryDomain, const String& targetPrimaryDomain, CompletionHandler<void()>&&);
    void clearUserInteraction(const String& targetPrimaryDomain, CompletionHandler<void()>&&);
    void deleteWebsiteDataForTopPrivatelyControlledDomainsInAllPersistentDataStores(OptionSet<WebsiteDataType>, Vector<String>&& topPrivatelyControlledDomains, bool shouldNotifyPage, CompletionHandler<void(const HashSet<String>&)>&&);
    void topPrivatelyControlledDomainsWithWebsiteData(OptionSet<WebsiteDataType>, bool shouldNotifyPage, CompletionHandler<void(HashSet<String>&&)>&&);
    bool grantStorageAccess(const String& resourceDomain, const String& firstPartyDomain, Optional<uint64_t> frameID, uint64_t pageID);
    void hasHadUserInteraction(const String& resourceDomain, CompletionHandler<void(bool)>&&);
    void hasStorageAccess(const String& subFrameHost, const String& topFrameHost, Optional<uint64_t> frameID, uint64_t pageID, CompletionHandler<void(bool)>&& callback);
    bool hasStorageAccessForFrame(const String& resourceDomain, const String& firstPartyDomain, uint64_t frameID, uint64_t pageID);
    void requestStorageAccess(const String& resourceDomain, const String& firstPartyDomain, Optional<uint64_t> frameID, uint64_t pageID, bool promptEnabled, CompletionHandler<void(StorageAccessStatus)>&&);
    void requestUpdate();
    void setLastSeen(const String& resourceDomain, Seconds, CompletionHandler<void()>&&);
    void setPrevalentResource(const String& resourceDomain, CompletionHandler<void()>&&);
    void setVeryPrevalentResource(const String& resourceDomain, CompletionHandler<void()>&&);
    void dumpResourceLoadStatistics(CompletionHandler<void(String)>&&);
    void isPrevalentResource(const String&, CompletionHandler<void(bool)>&&);
    void isVeryPrevalentResource(const String&, CompletionHandler<void(bool)>&&);
    void isRegisteredAsSubresourceUnder(const String& subresource, const String& topFrame, CompletionHandler<void(bool)>&&);
    void isRegisteredAsSubFrameUnder(const String& subFrame, const String& topFrame, CompletionHandler<void(bool)>&&);
    void isRegisteredAsRedirectingTo(const String& hostRedirectedFrom, const String& hostRedirectedTo, CompletionHandler<void(bool)>&&);
    void clearPrevalentResource(const String&, CompletionHandler<void()>&&);
    void setGrandfathered(const String&, bool, CompletionHandler<void()>&&);
    void isGrandfathered(const String&, CompletionHandler<void(bool)>&&);
    void removePrevalentDomains(const Vector<String>& domainsToBlock);
    void setNotifyPagesWhenDataRecordsWereScanned(bool, CompletionHandler<void()>&&);
    void setSubframeUnderTopFrameOrigin(const String& subframe, const String& topFrame, CompletionHandler<void()>&&);
    void setSubresourceUnderTopFrameOrigin(const String& subresource, const String& topFrame, CompletionHandler<void()>&&);
    void setSubresourceUniqueRedirectTo(const String& subresource, const String& hostNameRedirectedTo, CompletionHandler<void()>&&);
    void setSubresourceUniqueRedirectFrom(const String& subresource, const String& hostNameRedirectedFrom, CompletionHandler<void()>&&);
    void setTopFrameUniqueRedirectTo(const String& topFrameHostName, const String& hostNameRedirectedTo, CompletionHandler<void()>&&);
    void setTopFrameUniqueRedirectFrom(const String& topFrameHostName, const String& hostNameRedirectedFrom, CompletionHandler<void()>&&);
    void scheduleCookieBlockingUpdate(CompletionHandler<void()>&&);
    void scheduleCookieBlockingUpdateForDomains(const Vector<String>& domainsToBlock, CompletionHandler<void()>&&);
    void scheduleClearBlockingStateForDomains(const Vector<String>& domains, CompletionHandler<void()>&&);
    void scheduleStatisticsAndDataRecordsProcessing(CompletionHandler<void()>&&);
    void submitTelemetry(CompletionHandler<void()>&&);
    void scheduleClearInMemoryAndPersistent(ShouldGrandfatherStatistics, CompletionHandler<void()>&&);
    void scheduleClearInMemoryAndPersistent(WallTime modifiedSince, ShouldGrandfatherStatistics, CompletionHandler<void()>&&);

    void setTimeToLiveUserInteraction(Seconds, CompletionHandler<void()>&&);
    void setMinimumTimeBetweenDataRecordsRemoval(Seconds, CompletionHandler<void()>&&);
    void setGrandfatheringTime(Seconds, CompletionHandler<void()>&&);
    void setCacheMaxAgeCap(Seconds, CompletionHandler<void()>&&);
    void setMaxStatisticsEntries(size_t, CompletionHandler<void()>&&);
    void setPruneEntriesDownTo(size_t, CompletionHandler<void()>&&);

    void resetParametersToDefaultValues(CompletionHandler<void()>&&);

    void setResourceLoadStatisticsDebugMode(bool, CompletionHandler<void()>&&);
    void setPrevalentResourceForDebugMode(const String& resourceDomain, CompletionHandler<void()>&&);

    void logTestingEvent(const String&);
    void callGrantStorageAccessHandler(const String& subFramePrimaryDomain, const String& topFramePrimaryDomain, Optional<uint64_t> frameID, uint64_t pageID, CompletionHandler<void(bool)>&&);
    void removeAllStorageAccess(CompletionHandler<void()>&&);
    void callUpdatePrevalentDomainsToBlockCookiesForHandler(const Vector<String>& domainsToBlock, CompletionHandler<void()>&&);
    void callRemoveDomainsHandler(const Vector<String>& domains);
    void callHasStorageAccessForFrameHandler(const String& resourceDomain, const String& firstPartyDomain, uint64_t frameID, uint64_t pageID, CompletionHandler<void(bool)>&&);

    void didCreateNetworkProcess();

    void notifyResourceLoadStatisticsProcessed();

    NetworkSession* networkSession() { return m_networkSession.get(); }

    void sendDiagnosticMessageWithValue(const String& message, const String& description, unsigned value, unsigned sigDigits, WebCore::ShouldSample) const;
    void notifyPageStatisticsTelemetryFinished(unsigned totalPrevalentResources, unsigned totalPrevalentResourcesWithUserInteraction, unsigned top3SubframeUnderTopFrameOrigins) const;

private:
    explicit WebResourceLoadStatisticsStore(NetworkSession&, const String&);

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

    WeakPtr<NetworkSession> m_networkSession;
    Ref<WorkQueue> m_statisticsQueue;
    std::unique_ptr<ResourceLoadStatisticsMemoryStore> m_memoryStore;
    std::unique_ptr<ResourceLoadStatisticsPersistentStorage> m_persistentStorage;

    RunLoop::Timer<WebResourceLoadStatisticsStore> m_dailyTasksTimer;

    bool m_hasScheduledProcessStats { false };

    bool m_firstNetworkProcessCreated { false };
};

} // namespace WebKit

#endif

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
#include <WebCore/RegistrableDomain.h>
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
enum class IncludeHttpOnlyCookies : bool;
}

namespace WebKit {

class NetworkSession;
class ResourceLoadStatisticsStore;
class ResourceLoadStatisticsPersistentStorage;
class WebFrameProxy;
class WebProcessProxy;
enum class ShouldGrandfatherStatistics : bool;
enum class ShouldIncludeLocalhost : bool { No, Yes };

class WebResourceLoadStatisticsStore final : public ThreadSafeRefCounted<WebResourceLoadStatisticsStore, WTF::DestructionThread::Main>, public IPC::MessageReceiver {
public:
    using ResourceLoadStatistics = WebCore::ResourceLoadStatistics;
    using RegistrableDomain = WebCore::RegistrableDomain;
    using TopFrameDomain = WebCore::RegistrableDomain;
    using SubFrameDomain = WebCore::RegistrableDomain;
    using SubResourceDomain = WebCore::RegistrableDomain;
    using RedirectDomain = WebCore::RegistrableDomain;
    using RedirectedFromDomain = WebCore::RegistrableDomain;
    using RedirectedToDomain = WebCore::RegistrableDomain;
    using NavigatedFromDomain = WebCore::RegistrableDomain;
    using NavigatedToDomain = WebCore::RegistrableDomain;
    using DomainInNeedOfStorageAccess = WebCore::RegistrableDomain;
    using OpenerDomain = WebCore::RegistrableDomain;
    using OpenerPageID = uint64_t;
    using PageID = uint64_t;
    using FrameID = uint64_t;

    static Ref<WebResourceLoadStatisticsStore> create(NetworkSession& networkSession, const String& resourceLoadStatisticsDirectory, ShouldIncludeLocalhost shouldIncludeLocalhost)
    {
        return adoptRef(*new WebResourceLoadStatisticsStore(networkSession, resourceLoadStatisticsDirectory, shouldIncludeLocalhost));
    }

    ~WebResourceLoadStatisticsStore();

    static const OptionSet<WebsiteDataType>& monitoredDataTypes();

    WorkQueue& statisticsQueue() { return m_statisticsQueue.get(); }

    void setNotifyPagesWhenDataRecordsWereScanned(bool);
    void setNotifyPagesWhenTelemetryWasCaptured(bool, CompletionHandler<void()>&&);
    void setShouldClassifyResourcesBeforeDataRecordsRemoval(bool, CompletionHandler<void()>&&);
    void setShouldSubmitTelemetry(bool);

    void grantStorageAccess(const SubFrameDomain&, const TopFrameDomain&, FrameID, PageID, bool userWasPromptedNow, CompletionHandler<void(bool)>&&);

    void applicationWillTerminate();

    void logFrameNavigation(const WebFrameProxy&, const URL& pageURL, const WebCore::ResourceRequest&, const URL& redirectURL);
    void logFrameNavigation(const NavigatedToDomain&, const TopFrameDomain&, const NavigatedFromDomain&, bool isRedirect, bool isMainFrame);
    void logUserInteraction(const TopFrameDomain&, CompletionHandler<void()>&&);
    void logWebSocketLoading(const SubResourceDomain&, const TopFrameDomain&, WallTime lastSeen, CompletionHandler<void()>&&);
    void logSubresourceLoading(const SubResourceDomain&, const TopFrameDomain&, WallTime lastSeen, CompletionHandler<void()>&&);
    void logSubresourceRedirect(const RedirectedFromDomain&, const RedirectedToDomain&, CompletionHandler<void()>&&);
    void logCrossSiteLoadWithLinkDecoration(const NavigatedFromDomain&, const NavigatedToDomain&, CompletionHandler<void()>&&);
    void clearUserInteraction(const TopFrameDomain&, CompletionHandler<void()>&&);
    void deleteWebsiteDataForRegistrableDomainsInAllPersistentDataStores(OptionSet<WebsiteDataType>, Vector<RegistrableDomain>&&, bool shouldNotifyPage, WebCore::IncludeHttpOnlyCookies, CompletionHandler<void(const HashSet<RegistrableDomain>&)>&&);
    void registrableDomainsWithWebsiteData(OptionSet<WebsiteDataType>, bool shouldNotifyPage, CompletionHandler<void(HashSet<RegistrableDomain>&&)>&&);
    bool grantStorageAccess(const SubFrameDomain&, const TopFrameDomain&, Optional<FrameID>, PageID);
    void hasHadUserInteraction(const RegistrableDomain&, CompletionHandler<void(bool)>&&);
    void hasStorageAccess(const SubFrameDomain&, const TopFrameDomain&, Optional<FrameID>, PageID, CompletionHandler<void(bool)>&& callback);
    bool hasStorageAccessForFrame(const SubFrameDomain&, const TopFrameDomain&, FrameID, PageID);
    void requestStorageAccess(const SubFrameDomain&, const TopFrameDomain&, Optional<FrameID>, PageID, CompletionHandler<void(StorageAccessStatus)>&&);
    void requestStorageAccessGranted(const SubFrameDomain&, const TopFrameDomain&, FrameID, PageID, CompletionHandler<void(bool)>&&);
    void requestUpdate();
    void setLastSeen(const RegistrableDomain&, Seconds, CompletionHandler<void()>&&);
    void setPrevalentResource(const RegistrableDomain&, CompletionHandler<void()>&&);
    void setVeryPrevalentResource(const RegistrableDomain&, CompletionHandler<void()>&&);
    void dumpResourceLoadStatistics(CompletionHandler<void(String)>&&);
    void isPrevalentResource(const RegistrableDomain&, CompletionHandler<void(bool)>&&);
    void isVeryPrevalentResource(const RegistrableDomain&, CompletionHandler<void(bool)>&&);
    void isRegisteredAsSubresourceUnder(const SubResourceDomain&, const TopFrameDomain&, CompletionHandler<void(bool)>&&);
    void isRegisteredAsSubFrameUnder(const SubFrameDomain&, const TopFrameDomain&, CompletionHandler<void(bool)>&&);
    void isRegisteredAsRedirectingTo(const RedirectedFromDomain&, const RedirectedToDomain&, CompletionHandler<void(bool)>&&);
    void clearPrevalentResource(const RegistrableDomain&, CompletionHandler<void()>&&);
    void setGrandfathered(const RegistrableDomain&, bool, CompletionHandler<void()>&&);
    void isGrandfathered(const RegistrableDomain&, CompletionHandler<void(bool)>&&);
    void removePrevalentDomains(const Vector<RegistrableDomain>&);
    void setNotifyPagesWhenDataRecordsWereScanned(bool, CompletionHandler<void()>&&);
    void setIsRunningTest(bool, CompletionHandler<void()>&&);
    void setSubframeUnderTopFrameDomain(const SubFrameDomain&, const TopFrameDomain&, CompletionHandler<void()>&&);
    void setSubresourceUnderTopFrameDomain(const SubResourceDomain&, const TopFrameDomain&, CompletionHandler<void()>&&);
    void setSubresourceUniqueRedirectTo(const SubResourceDomain&, const RedirectedToDomain&, CompletionHandler<void()>&&);
    void setSubresourceUniqueRedirectFrom(const SubResourceDomain&, const RedirectedFromDomain&, CompletionHandler<void()>&&);
    void setTopFrameUniqueRedirectTo(const TopFrameDomain&, const RedirectedToDomain&, CompletionHandler<void()>&&);
    void setTopFrameUniqueRedirectFrom(const TopFrameDomain&, const RedirectedFromDomain&, CompletionHandler<void()>&&);
    void scheduleCookieBlockingUpdate(CompletionHandler<void()>&&);
    void scheduleCookieBlockingUpdateForDomains(const Vector<RegistrableDomain>&, CompletionHandler<void()>&&);
    void scheduleClearBlockingStateForDomains(const Vector<RegistrableDomain>&, CompletionHandler<void()>&&);
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
    void setPrevalentResourceForDebugMode(const RegistrableDomain&, CompletionHandler<void()>&&);

    void logTestingEvent(const String&);
    void callGrantStorageAccessHandler(const SubFrameDomain&, const TopFrameDomain&, Optional<FrameID>, PageID, CompletionHandler<void(bool)>&&);
    void removeAllStorageAccess(CompletionHandler<void()>&&);
    void callUpdatePrevalentDomainsToBlockCookiesForHandler(const Vector<RegistrableDomain>&, CompletionHandler<void()>&&);
    void callRemoveDomainsHandler(const Vector<RegistrableDomain>&);
    void callHasStorageAccessForFrameHandler(const SubFrameDomain&, const TopFrameDomain&, FrameID, PageID, CompletionHandler<void(bool)>&&);

    void didCreateNetworkProcess();

    void notifyResourceLoadStatisticsProcessed();

    NetworkSession* networkSession() { return m_networkSession.get(); }

    void sendDiagnosticMessageWithValue(const String& message, const String& description, unsigned value, unsigned sigDigits, WebCore::ShouldSample) const;
    void notifyPageStatisticsTelemetryFinished(unsigned totalPrevalentResources, unsigned totalPrevalentResourcesWithUserInteraction, unsigned top3SubframeUnderTopFrameOrigins) const;

private:
    explicit WebResourceLoadStatisticsStore(NetworkSession&, const String&, ShouldIncludeLocalhost);

    void postTask(WTF::Function<void()>&&);
    static void postTaskReply(WTF::Function<void()>&&);

    // IPC::MessageReceiver.
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;

    // IPC message handlers.
    void resourceLoadStatisticsUpdated(Vector<ResourceLoadStatistics>&& origins);
    void requestStorageAccessUnderOpener(DomainInNeedOfStorageAccess&&, uint64_t openerPageID, OpenerDomain&&);

    void performDailyTasks();

    StorageAccessStatus storageAccessStatus(const String& subFramePrimaryDomain, const String& topFramePrimaryDomain);

    void flushAndDestroyPersistentStore();

    WeakPtr<NetworkSession> m_networkSession;
    Ref<WorkQueue> m_statisticsQueue;
    std::unique_ptr<ResourceLoadStatisticsStore> m_statisticsStore;
    std::unique_ptr<ResourceLoadStatisticsPersistentStorage> m_persistentStorage;

    RunLoop::Timer<WebResourceLoadStatisticsStore> m_dailyTasksTimer;

    bool m_hasScheduledProcessStats { false };

    bool m_firstNetworkProcessCreated { false };
};

} // namespace WebKit

#endif

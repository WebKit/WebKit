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

#include "config.h"
#include "WebResourceLoadStatisticsStore.h"

#if ENABLE(RESOURCE_LOAD_STATISTICS)

#include "APIDictionary.h"
#include "Logging.h"
#include "NetworkSession.h"
#include "ResourceLoadStatisticsMemoryStore.h"
#include "ResourceLoadStatisticsPersistentStorage.h"
#include "ShouldGrandfatherStatistics.h"
#include "StorageAccessStatus.h"
#include "WebFrameProxy.h"
#include "WebPageProxy.h"
#include "WebProcessMessages.h"
#include "WebProcessPool.h"
#include "WebProcessProxy.h"
#include "WebResourceLoadStatisticsStoreMessages.h"
#include "WebResourceLoadStatisticsTelemetry.h"
#include "WebsiteDataFetchOption.h"
#include <WebCore/DiagnosticLoggingClient.h>
#include <WebCore/DiagnosticLoggingKeys.h>
#include <WebCore/NetworkStorageSession.h>
#include <WebCore/ResourceLoadStatistics.h>
#include <wtf/CallbackAggregator.h>
#include <wtf/CrossThreadCopier.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/threads/BinarySemaphore.h>

namespace WebKit {
using namespace WebCore;

template<typename T> static inline String isolatedPrimaryDomain(const T& value)
{
    return ResourceLoadStatistics::primaryDomain(value).isolatedCopy();
}

const OptionSet<WebsiteDataType>& WebResourceLoadStatisticsStore::monitoredDataTypes()
{
    static NeverDestroyed<OptionSet<WebsiteDataType>> dataTypes(std::initializer_list<WebsiteDataType>({
        WebsiteDataType::Cookies,
        WebsiteDataType::DOMCache,
        WebsiteDataType::IndexedDBDatabases,
        WebsiteDataType::LocalStorage,
        WebsiteDataType::MediaKeys,
        WebsiteDataType::OfflineWebApplicationCache,
#if ENABLE(NETSCAPE_PLUGIN_API)
        WebsiteDataType::PlugInData,
#endif
        WebsiteDataType::SearchFieldRecentSearches,
        WebsiteDataType::SessionStorage,
#if ENABLE(SERVICE_WORKER)
        WebsiteDataType::ServiceWorkerRegistrations,
#endif
        WebsiteDataType::WebSQLDatabases,
    }));

    ASSERT(RunLoop::isMain());

    return dataTypes;
}

void WebResourceLoadStatisticsStore::setNotifyPagesWhenDataRecordsWereScanned(bool value)
{
    ASSERT(RunLoop::isMain());

    postTask([this, value] {
        if (m_memoryStore)
            m_memoryStore->setNotifyPagesWhenDataRecordsWereScanned(value);
    });
}

void WebResourceLoadStatisticsStore::setNotifyPagesWhenDataRecordsWereScanned(bool value, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    postTask([this, value, completionHandler = WTFMove(completionHandler)]() mutable {
        if (m_memoryStore)
            m_memoryStore->setNotifyPagesWhenDataRecordsWereScanned(value);
        
        postTaskReply(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::setShouldClassifyResourcesBeforeDataRecordsRemoval(bool value, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    postTask([this, value, completionHandler = WTFMove(completionHandler)]() mutable {
        if (m_memoryStore)
            m_memoryStore->setShouldClassifyResourcesBeforeDataRecordsRemoval(value);

        postTaskReply(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::setShouldSubmitTelemetry(bool value)
{
    ASSERT(RunLoop::isMain());

    postTask([this, value] {
        if (m_memoryStore)
            m_memoryStore->setShouldSubmitTelemetry(value);
    });
}

void WebResourceLoadStatisticsStore::setNotifyPagesWhenTelemetryWasCaptured(bool value, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    WebKit::WebResourceLoadStatisticsTelemetry::setNotifyPagesWhenTelemetryWasCaptured(value);
    completionHandler();
}

WebResourceLoadStatisticsStore::WebResourceLoadStatisticsStore(NetworkSession& networkSession, const String& resourceLoadStatisticsDirectory)
    : m_networkSession(makeWeakPtr(networkSession))
    , m_statisticsQueue(WorkQueue::create("WebResourceLoadStatisticsStore Process Data Queue", WorkQueue::Type::Serial, WorkQueue::QOS::Utility))
    , m_dailyTasksTimer(RunLoop::main(), this, &WebResourceLoadStatisticsStore::performDailyTasks)
{
    ASSERT(RunLoop::isMain());
    
    postTask([this, resourceLoadStatisticsDirectory = resourceLoadStatisticsDirectory.isolatedCopy()] {
        m_memoryStore = std::make_unique<ResourceLoadStatisticsMemoryStore>(*this, m_statisticsQueue);
        m_persistentStorage = std::make_unique<ResourceLoadStatisticsPersistentStorage>(*m_memoryStore, m_statisticsQueue, resourceLoadStatisticsDirectory);

        // FIXME(193297): This should be revised after the UIProcess version goes away.
        m_memoryStore->didCreateNetworkProcess();
    });
    
    m_dailyTasksTimer.startRepeating(24_h);
}

WebResourceLoadStatisticsStore::~WebResourceLoadStatisticsStore()
{
    ASSERT(RunLoop::isMain());

    flushAndDestroyPersistentStore();
}

inline void WebResourceLoadStatisticsStore::postTask(WTF::Function<void()>&& task)
{
    ASSERT(RunLoop::isMain());
    m_statisticsQueue->dispatch([protectedThis = makeRef(*this), task = WTFMove(task)] {
        task();
    });
}

inline void WebResourceLoadStatisticsStore::postTaskReply(WTF::Function<void()>&& reply)
{
    ASSERT(!RunLoop::isMain());
    RunLoop::main().dispatch(WTFMove(reply));
}

void WebResourceLoadStatisticsStore::flushAndDestroyPersistentStore()
{
    ASSERT(RunLoop::isMain());

    if (!m_persistentStorage && !m_memoryStore)
        return;

    // Make sure we destroy the persistent store on the background queue and wait for it to die
    // synchronously since it has a C++ reference to us. Blocking nature of this task allows us
    // to not maintain a WebResourceLoadStatisticsStore reference for the duration of dispatch,
    // avoiding double-deletion issues when this is invoked from the destructor.
    BinarySemaphore semaphore;
    m_statisticsQueue->dispatch([&semaphore, this] {
        m_persistentStorage = nullptr;
        m_memoryStore = nullptr;
        semaphore.signal();
    });
    semaphore.wait();
}

void WebResourceLoadStatisticsStore::setResourceLoadStatisticsDebugMode(bool value, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    postTask([this, value, completionHandler = WTFMove(completionHandler)]() mutable {
        if (m_memoryStore)
            m_memoryStore->setResourceLoadStatisticsDebugMode(value);
        postTaskReply(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::setPrevalentResourceForDebugMode(const String& primaryDomain, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    postTask([this, primaryDomain = isolatedPrimaryDomain(primaryDomain), completionHandler = WTFMove(completionHandler)]() mutable {
        if (m_memoryStore)
            m_memoryStore->setPrevalentResourceForDebugMode(primaryDomain);
        postTaskReply(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::scheduleStatisticsAndDataRecordsProcessing(CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    postTask([this, completionHandler = WTFMove(completionHandler)]() mutable {
        if (m_memoryStore)
            m_memoryStore->processStatisticsAndDataRecords();
        postTaskReply(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::requestUpdate()
{
    resourceLoadStatisticsUpdated({ });
}

void WebResourceLoadStatisticsStore::resourceLoadStatisticsUpdated(Vector<WebCore::ResourceLoadStatistics>&& origins)
{
    ASSERT(RunLoop::isMain());

    // It is safe to move the origins to the background queue without isolated copy here because this is an r-value
    // coming from IPC. ResourceLoadStatistics only contains strings which are safe to move to other threads as long
    // as nobody on this thread holds a reference to those strings.
    postTask([this, origins = WTFMove(origins)]() mutable {
        if (!m_memoryStore)
            return;

        m_memoryStore->mergeStatistics(WTFMove(origins));

        // We can cancel any pending request to process statistics since we're doing it synchronously below.
        m_memoryStore->cancelPendingStatisticsProcessingRequest();

        // Fire before processing statistics to propagate user interaction as fast as possible to the network process.
        m_memoryStore->updateCookieBlocking([]() { });
        m_memoryStore->processStatisticsAndDataRecords();
    });
}

void WebResourceLoadStatisticsStore::hasStorageAccess(const String& subFrameHost, const String& topFrameHost, Optional<uint64_t> frameID, uint64_t pageID, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(subFrameHost != topFrameHost);
    ASSERT(RunLoop::isMain());

    postTask([this, subFramePrimaryDomain = isolatedPrimaryDomain(subFrameHost), topFramePrimaryDomain = isolatedPrimaryDomain(topFrameHost), frameID, pageID, completionHandler = WTFMove(completionHandler)]() mutable {
        if (!m_memoryStore) {
            postTaskReply([completionHandler = WTFMove(completionHandler)]() mutable {
                completionHandler(false);
            });
            return;
        }
        m_memoryStore->hasStorageAccess(subFramePrimaryDomain, topFramePrimaryDomain, frameID, pageID, [completionHandler = WTFMove(completionHandler)](bool hasStorageAccess) mutable {
            postTaskReply([completionHandler = WTFMove(completionHandler), hasStorageAccess]() mutable {
                completionHandler(hasStorageAccess);
            });
        });
    });
}

bool WebResourceLoadStatisticsStore::hasStorageAccessForFrame(const String& resourceDomain, const String& firstPartyDomain, uint64_t frameID, uint64_t pageID)
{
    return m_networkSession ? m_networkSession->networkStorageSession().hasStorageAccess(resourceDomain, firstPartyDomain, frameID, pageID) : false;
}

void WebResourceLoadStatisticsStore::callHasStorageAccessForFrameHandler(const String& resourceDomain, const String& firstPartyDomain, uint64_t frameID, uint64_t pageID, CompletionHandler<void(bool hasAccess)>&& callback)
{
    ASSERT(RunLoop::isMain());

    if (m_networkSession) {
        callback(m_networkSession->networkStorageSession().hasStorageAccess(resourceDomain, firstPartyDomain, frameID, pageID));
        return;
    }

    callback(false);
}

void WebResourceLoadStatisticsStore::requestStorageAccessGranted(const String& subFrameHost, const String& topFrameHost, uint64_t frameID, uint64_t pageID, bool promptEnabled, CompletionHandler<void(bool)>&& completionHandler)
{
    auto statusHandler = [this, protectedThis = makeRef(*this), subFrameHost = isolatedPrimaryDomain(subFrameHost), topFrameHost = isolatedPrimaryDomain(topFrameHost), promptEnabled, frameID, pageID, completionHandler = WTFMove(completionHandler)](StorageAccessStatus status) mutable {
        switch (status) {
        case StorageAccessStatus::CannotRequestAccess:
            completionHandler(false);
            return;
        case StorageAccessStatus::RequiresUserPrompt:
            {
            ASSERT_UNUSED(promptEnabled, promptEnabled);
            auto subFramePrimaryDomain = ResourceLoadStatistics::primaryDomain(subFrameHost);
            auto topFramePrimaryDomain = ResourceLoadStatistics::primaryDomain(topFrameHost);
            CompletionHandler<void(bool)> requestConfirmationCompletionHandler = [this, protectedThis = makeRef(*this), subFrameHost = WTFMove(subFrameHost), topFrameHost = WTFMove(topFrameHost), frameID, pageID, completionHandler = WTFMove(completionHandler)] (bool userDidGrantAccess) mutable {
                if (userDidGrantAccess)
                    grantStorageAccess(WTFMove(subFrameHost), WTFMove(topFrameHost), frameID, pageID, userDidGrantAccess, WTFMove(completionHandler));
                else
                    completionHandler(false);
            };
            m_networkSession->networkProcess().parentProcessConnection()->sendWithAsyncReply(Messages::NetworkProcessProxy::RequestStorageAccessConfirm(pageID, frameID, subFramePrimaryDomain, topFramePrimaryDomain), WTFMove(requestConfirmationCompletionHandler));
            }
            return;
        case StorageAccessStatus::HasAccess:
            completionHandler(true);
            return;
        }
    };

    requestStorageAccess(subFrameHost, topFrameHost, frameID, pageID, promptEnabled, WTFMove(statusHandler));
}

void WebResourceLoadStatisticsStore::requestStorageAccess(const String& subFrameHost, const String& topFrameHost, Optional<uint64_t> frameID, uint64_t pageID, bool promptEnabled, CompletionHandler<void(StorageAccessStatus)>&& completionHandler)
{
    auto subFramePrimaryDomain = isolatedPrimaryDomain(subFrameHost);
    auto topFramePrimaryDomain = isolatedPrimaryDomain(topFrameHost);
    if (subFramePrimaryDomain == topFramePrimaryDomain) {
        completionHandler(StorageAccessStatus::HasAccess);
        return;
    }

    postTask([this, subFramePrimaryDomain = crossThreadCopy(subFramePrimaryDomain), topFramePrimaryDomain = crossThreadCopy(topFramePrimaryDomain), frameID, pageID, promptEnabled, completionHandler = WTFMove(completionHandler)]() mutable {
        if (!m_memoryStore) {
            postTaskReply([completionHandler = WTFMove(completionHandler)]() mutable {
                completionHandler(StorageAccessStatus::CannotRequestAccess);
            });
            return;
        }

        m_memoryStore->requestStorageAccess(WTFMove(subFramePrimaryDomain), WTFMove(topFramePrimaryDomain), frameID.value(), pageID, promptEnabled, [completionHandler = WTFMove(completionHandler)](StorageAccessStatus status) mutable {
            postTaskReply([completionHandler = WTFMove(completionHandler), status]() mutable {
                completionHandler(status);
            });
        });
    });
}

void WebResourceLoadStatisticsStore::requestStorageAccessUnderOpener(String&& primaryDomainInNeedOfStorageAccess, uint64_t openerPageID, String&& openerPrimaryDomain)
{
    ASSERT(RunLoop::isMain());

    // It is safe to move the strings to the background queue without isolated copy here because they are r-value references
    // coming from IPC. Strings which are safe to move to other threads as long as nobody on this thread holds a reference
    // to those strings.
    postTask([this, primaryDomainInNeedOfStorageAccess = WTFMove(primaryDomainInNeedOfStorageAccess), openerPageID, openerPrimaryDomain = WTFMove(openerPrimaryDomain)]() mutable {
        if (m_memoryStore)
            m_memoryStore->requestStorageAccessUnderOpener(WTFMove(primaryDomainInNeedOfStorageAccess), openerPageID, WTFMove(openerPrimaryDomain));
    });
}

void WebResourceLoadStatisticsStore::grantStorageAccess(const String& subFrameHost, const String& topFrameHost, uint64_t frameID, uint64_t pageID, bool userWasPromptedNow, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    postTask([this, subFrameHost = crossThreadCopy(subFrameHost), topFrameHost = crossThreadCopy(topFrameHost), frameID, pageID, userWasPromptedNow, completionHandler = WTFMove(completionHandler)]() mutable {
        if (!m_memoryStore) {
            postTaskReply([completionHandler = WTFMove(completionHandler)]() mutable {
                completionHandler(false);
            });
            return;
        }

        m_memoryStore->grantStorageAccess(WTFMove(subFrameHost), WTFMove(topFrameHost), frameID, pageID, userWasPromptedNow, [completionHandler = WTFMove(completionHandler)](bool wasGrantedAccess) mutable {
            postTaskReply([completionHandler = WTFMove(completionHandler), wasGrantedAccess]() mutable {
                completionHandler(wasGrantedAccess);
            });
        });
    });
}

bool WebResourceLoadStatisticsStore::grantStorageAccess(const String& resourceDomain, const String& firstPartyDomain, Optional<uint64_t> frameID, uint64_t pageID)
{
    bool isStorageGranted = false;

    if (m_networkSession) {
        m_networkSession->networkStorageSession().grantStorageAccess(resourceDomain, firstPartyDomain, frameID, pageID);
        ASSERT(m_networkSession->networkStorageSession().hasStorageAccess(resourceDomain, firstPartyDomain, frameID, pageID));
        isStorageGranted = true;
    }

    return isStorageGranted;
}

void WebResourceLoadStatisticsStore::callGrantStorageAccessHandler(const String& subFramePrimaryDomain, const String& topFramePrimaryDomain, Optional<uint64_t> frameID, uint64_t pageID, CompletionHandler<void(bool)>&& callback)
{
    ASSERT(RunLoop::isMain());

    callback(grantStorageAccess(subFramePrimaryDomain, topFramePrimaryDomain, frameID, pageID));
}

void WebResourceLoadStatisticsStore::didCreateNetworkProcess()
{
    ASSERT(RunLoop::isMain());

    postTask([this] {
        if (!m_memoryStore)
            return;
        m_memoryStore->didCreateNetworkProcess();
    });
}

void WebResourceLoadStatisticsStore::removeAllStorageAccess(CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    if (m_networkSession)
        m_networkSession->networkStorageSession().removeAllStorageAccess();

    completionHandler();
}

void WebResourceLoadStatisticsStore::applicationWillTerminate()
{
    flushAndDestroyPersistentStore();
}

void WebResourceLoadStatisticsStore::performDailyTasks()
{
    ASSERT(RunLoop::isMain());

    postTask([this] {
        if (!m_memoryStore)
            return;

        m_memoryStore->includeTodayAsOperatingDateIfNecessary();
        m_memoryStore->calculateAndSubmitTelemetry();
    });
}

void WebResourceLoadStatisticsStore::submitTelemetry(CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    postTask([this, completionHandler = WTFMove(completionHandler)]() mutable  {
        if (m_memoryStore)
            WebResourceLoadStatisticsTelemetry::calculateAndSubmit(*m_memoryStore);

        postTaskReply(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::logFrameNavigation(const WebFrameProxy& frame, const URL& pageURL, const WebCore::ResourceRequest& request, const URL& redirectURL)
{
    ASSERT(RunLoop::isMain());

    auto sourceURL = redirectURL;
    bool isRedirect = !redirectURL.isNull();
    if (!isRedirect) {
        sourceURL = frame.url();
        if (sourceURL.isNull())
            sourceURL = pageURL;
    }

    auto& targetURL = request.url();

    if (!targetURL.isValid() || !pageURL.isValid())
        return;

    auto targetHost = targetURL.host();
    auto mainFrameHost = pageURL.host();

    if (targetHost.isEmpty() || mainFrameHost.isEmpty() || targetHost == sourceURL.host())
        return;

    auto targetPrimaryDomain = ResourceLoadStatistics::primaryDomain(targetURL);
    auto mainFramePrimaryDomain = ResourceLoadStatistics::primaryDomain(pageURL);
    auto sourcePrimaryDomain = ResourceLoadStatistics::primaryDomain(sourceURL);

    logFrameNavigation(targetPrimaryDomain, mainFramePrimaryDomain, sourcePrimaryDomain, targetHost.toString(), mainFrameHost.toString(), isRedirect, frame.isMainFrame());
}

void WebResourceLoadStatisticsStore::logFrameNavigation(const String& targetPrimaryDomain, const String& mainFramePrimaryDomain, const String& sourcePrimaryDomain, const String& targetHost, const String& mainFrameHost, bool isRedirect, bool isMainFrame)
{
    postTask([this, targetPrimaryDomain = isolatedPrimaryDomain(targetPrimaryDomain), mainFramePrimaryDomain = isolatedPrimaryDomain(mainFramePrimaryDomain), sourcePrimaryDomain = isolatedPrimaryDomain(sourcePrimaryDomain), targetHost = isolatedPrimaryDomain(targetHost), mainFrameHost = isolatedPrimaryDomain(mainFrameHost), isRedirect, isMainFrame] {
        
        if (m_memoryStore)
            m_memoryStore->logFrameNavigation(targetPrimaryDomain, mainFramePrimaryDomain, sourcePrimaryDomain, targetHost, mainFrameHost, isRedirect, isMainFrame);
    });
}

void WebResourceLoadStatisticsStore::logWebSocketLoading(const String& targetPrimaryDomain, const String& mainFramePrimaryDomain, WallTime lastSeen, CompletionHandler<void()>&& completionHandler)
{
    postTask([this, targetPrimaryDomain = isolatedPrimaryDomain(targetPrimaryDomain), mainFramePrimaryDomain = isolatedPrimaryDomain(mainFramePrimaryDomain), lastSeen, completionHandler = WTFMove(completionHandler)]() mutable {
        if (m_memoryStore)
            m_memoryStore->logSubresourceLoading(targetPrimaryDomain, mainFramePrimaryDomain, lastSeen);

        postTaskReply(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::logSubresourceLoading(const String& targetPrimaryDomain, const String& mainFramePrimaryDomain, WallTime lastSeen, CompletionHandler<void()>&& completionHandler)
{
    postTask([this, targetPrimaryDomain = isolatedPrimaryDomain(targetPrimaryDomain), mainFramePrimaryDomain = isolatedPrimaryDomain(mainFramePrimaryDomain), lastSeen, completionHandler = WTFMove(completionHandler)]() mutable {
        if (m_memoryStore)
            m_memoryStore->logSubresourceLoading(targetPrimaryDomain, mainFramePrimaryDomain, lastSeen);
        
        postTaskReply(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::logSubresourceRedirect(const String& sourcePrimaryDomain, const String& targetPrimaryDomain, CompletionHandler<void()>&& completionHandler)
{
    postTask([this, sourcePrimaryDomain = isolatedPrimaryDomain(sourcePrimaryDomain), targetPrimaryDomain = isolatedPrimaryDomain(targetPrimaryDomain), completionHandler = WTFMove(completionHandler)]() mutable {
        if (m_memoryStore)
            m_memoryStore->logSubresourceRedirect(sourcePrimaryDomain, targetPrimaryDomain);
        
        postTaskReply(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::logUserInteraction(const String& targetPrimaryDomain, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    postTask([this, primaryDomain = isolatedPrimaryDomain(targetPrimaryDomain), completionHandler = WTFMove(completionHandler)]() mutable {
        if (m_memoryStore)
            m_memoryStore->logUserInteraction(primaryDomain);
        postTaskReply(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::clearUserInteraction(const String& targetPrimaryDomain, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    postTask([this, primaryDomain = isolatedPrimaryDomain(targetPrimaryDomain), completionHandler = WTFMove(completionHandler)]() mutable {
        if (m_memoryStore)
            m_memoryStore->clearUserInteraction(primaryDomain);
        postTaskReply(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::hasHadUserInteraction(const String& primaryDomain, CompletionHandler<void(bool)>&& completionHandler)
{
    postTask([this, primaryDomain = isolatedPrimaryDomain(primaryDomain), completionHandler = WTFMove(completionHandler)]() mutable {
        bool hadUserInteraction = m_memoryStore ? m_memoryStore->hasHadUserInteraction(primaryDomain) : false;
        postTaskReply([hadUserInteraction, completionHandler = WTFMove(completionHandler)]() mutable {
            completionHandler(hadUserInteraction);
        });
    });
}

void WebResourceLoadStatisticsStore::setLastSeen(const String& resourceDomain, Seconds seconds, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    postTask([this, resourceDomain = isolatedPrimaryDomain(resourceDomain), seconds, completionHandler = WTFMove(completionHandler)]() mutable {
        if (m_memoryStore)
            m_memoryStore->setLastSeen(resourceDomain, seconds);
        postTaskReply(WTFMove(completionHandler));
    });
}
    
void WebResourceLoadStatisticsStore::setPrevalentResource(const String& resourceDomain, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    postTask([this, resourceDomain = isolatedPrimaryDomain(resourceDomain), completionHandler = WTFMove(completionHandler)]() mutable {
        if (m_memoryStore)
            m_memoryStore->setPrevalentResource(resourceDomain);
        postTaskReply(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::setVeryPrevalentResource(const String& primaryDomain, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    postTask([this, primaryDomain = isolatedPrimaryDomain(primaryDomain), completionHandler = WTFMove(completionHandler)]() mutable {
        if (m_memoryStore)
            m_memoryStore->setVeryPrevalentResource(primaryDomain);
        postTaskReply(WTFMove(completionHandler));
    });
}
    
void WebResourceLoadStatisticsStore::dumpResourceLoadStatistics(CompletionHandler<void(String)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    postTask([this, completionHandler = WTFMove(completionHandler)]() mutable {
        String result = m_memoryStore ? m_memoryStore->dumpResourceLoadStatistics() : emptyString();
        postTaskReply([result = result.isolatedCopy(), completionHandler = WTFMove(completionHandler)]() mutable {
            completionHandler(result);
        });
    });
}

void WebResourceLoadStatisticsStore::isPrevalentResource(const String& primaryDomain, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    postTask([this, primaryDomain = isolatedPrimaryDomain(primaryDomain), completionHandler = WTFMove(completionHandler)]() mutable {
        bool isPrevalentResource = m_memoryStore ? m_memoryStore->isPrevalentResource(primaryDomain) : false;
        postTaskReply([isPrevalentResource, completionHandler = WTFMove(completionHandler)]() mutable {
            completionHandler(isPrevalentResource);
        });
    });
}
    
void WebResourceLoadStatisticsStore::isVeryPrevalentResource(const String& primaryDomain, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    postTask([this, primaryDomain = isolatedPrimaryDomain(primaryDomain), completionHandler = WTFMove(completionHandler)]() mutable {
        bool isVeryPrevalentResource = m_memoryStore ? m_memoryStore->isVeryPrevalentResource(primaryDomain) : false;
        postTaskReply([isVeryPrevalentResource, completionHandler = WTFMove(completionHandler)]() mutable {
            completionHandler(isVeryPrevalentResource);
        });
    });
}

void WebResourceLoadStatisticsStore::isRegisteredAsSubresourceUnder(const String& subresource, const String& topFrame, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    postTask([this, subresourcePrimaryDomain = isolatedPrimaryDomain(subresource), topFramePrimaryDomain = isolatedPrimaryDomain(topFrame), completionHandler = WTFMove(completionHandler)]() mutable {
        bool isRegisteredAsSubresourceUnder = m_memoryStore ? m_memoryStore->isRegisteredAsSubresourceUnder(subresourcePrimaryDomain, topFramePrimaryDomain) : false;
        postTaskReply([isRegisteredAsSubresourceUnder, completionHandler = WTFMove(completionHandler)]() mutable {
            completionHandler(isRegisteredAsSubresourceUnder);
        });
    });
}

void WebResourceLoadStatisticsStore::isRegisteredAsSubFrameUnder(const String& subFrame, const String& topFrame, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    postTask([this, subFramePrimaryDomain = isolatedPrimaryDomain(subFrame), topFramePrimaryDomain = isolatedPrimaryDomain(topFrame), completionHandler = WTFMove(completionHandler)]() mutable {
        bool isRegisteredAsSubFrameUnder = m_memoryStore ? m_memoryStore->isRegisteredAsSubFrameUnder(subFramePrimaryDomain, topFramePrimaryDomain) : false;
        postTaskReply([isRegisteredAsSubFrameUnder, completionHandler = WTFMove(completionHandler)]() mutable {
            completionHandler(isRegisteredAsSubFrameUnder);
        });
    });
}

void WebResourceLoadStatisticsStore::isRegisteredAsRedirectingTo(const String& hostRedirectedFrom, const String& hostRedirectedTo, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    postTask([this, hostRedirectedFromPrimaryDomain = isolatedPrimaryDomain(hostRedirectedFrom), hostRedirectedToPrimaryDomain = isolatedPrimaryDomain(hostRedirectedTo), completionHandler = WTFMove(completionHandler)]() mutable {
        bool isRegisteredAsRedirectingTo = m_memoryStore ? m_memoryStore->isRegisteredAsRedirectingTo(hostRedirectedFromPrimaryDomain, hostRedirectedToPrimaryDomain) : false;
        postTaskReply([isRegisteredAsRedirectingTo, completionHandler = WTFMove(completionHandler)]() mutable {
            completionHandler(isRegisteredAsRedirectingTo);
        });
    });
}

void WebResourceLoadStatisticsStore::clearPrevalentResource(const String& resourceDomain, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    postTask([this, resourceDomain = isolatedPrimaryDomain(resourceDomain), completionHandler = WTFMove(completionHandler)]() mutable {
        if (m_memoryStore)
            m_memoryStore->clearPrevalentResource(resourceDomain);
        postTaskReply(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::setGrandfathered(const String& domain, bool value, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    postTask([this, primaryDomain = isolatedPrimaryDomain(domain), value, completionHandler = WTFMove(completionHandler)]() mutable {
        if (m_memoryStore)
            m_memoryStore->setGrandfathered(primaryDomain, value);
        postTaskReply(WTFMove(completionHandler));
    });
}
    
void WebResourceLoadStatisticsStore::isGrandfathered(const String& primaryDomain, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    postTask([this, completionHandler = WTFMove(completionHandler), primaryDomain = isolatedPrimaryDomain(primaryDomain)]() mutable {
        bool isGrandFathered = m_memoryStore ? m_memoryStore->isGrandfathered(primaryDomain) : false;
        postTaskReply([isGrandFathered, completionHandler = WTFMove(completionHandler)]() mutable {
            completionHandler(isGrandFathered);
        });
    });
}

void WebResourceLoadStatisticsStore::setSubframeUnderTopFrameOrigin(const String& subframe, const String& topFrame, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    postTask([this, completionHandler = WTFMove(completionHandler), primaryTopFrameDomain = isolatedPrimaryDomain(topFrame), primarySubFrameDomain = isolatedPrimaryDomain(subframe)]() mutable {
        if (m_memoryStore)
            m_memoryStore->setSubframeUnderTopFrameOrigin(primarySubFrameDomain, primaryTopFrameDomain);
        postTaskReply(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::setSubresourceUnderTopFrameOrigin(const String& subresource, const String& topFrame, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    postTask([this, completionHandler = WTFMove(completionHandler), primaryTopFrameDomain = isolatedPrimaryDomain(topFrame), primarySubresourceDomain = isolatedPrimaryDomain(subresource)]() mutable {
        if (m_memoryStore)
            m_memoryStore->setSubresourceUnderTopFrameOrigin(primarySubresourceDomain, primaryTopFrameDomain);
        postTaskReply(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::setSubresourceUniqueRedirectTo(const String& subresource, const String& hostNameRedirectedTo, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    postTask([this, completionHandler = WTFMove(completionHandler), primaryRedirectDomain = isolatedPrimaryDomain(hostNameRedirectedTo), primarySubresourceDomain = isolatedPrimaryDomain(subresource)]() mutable {
        if (m_memoryStore)
            m_memoryStore->setSubresourceUniqueRedirectTo(primarySubresourceDomain, primaryRedirectDomain);
        postTaskReply(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::setSubresourceUniqueRedirectFrom(const String& subresource, const String& hostNameRedirectedFrom, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    postTask([this, completionHandler = WTFMove(completionHandler), primaryRedirectDomain = isolatedPrimaryDomain(hostNameRedirectedFrom), primarySubresourceDomain = isolatedPrimaryDomain(subresource)]() mutable {
        if (m_memoryStore)
            m_memoryStore->setSubresourceUniqueRedirectFrom(primarySubresourceDomain, primaryRedirectDomain);
        postTaskReply(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::setTopFrameUniqueRedirectTo(const String& topFrameHostName, const String& hostNameRedirectedTo, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    postTask([this, completionHandler = WTFMove(completionHandler), topFramePrimaryDomain = isolatedPrimaryDomain(topFrameHostName), primaryRedirectDomain = isolatedPrimaryDomain(hostNameRedirectedTo)]() mutable {
        if (m_memoryStore)
            m_memoryStore->setTopFrameUniqueRedirectTo(topFramePrimaryDomain, primaryRedirectDomain);
        postTaskReply(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::setTopFrameUniqueRedirectFrom(const String& topFrameHostName, const String& hostNameRedirectedFrom, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    postTask([this, completionHandler = WTFMove(completionHandler), topFramePrimaryDomain = isolatedPrimaryDomain(topFrameHostName), primaryRedirectDomain = isolatedPrimaryDomain(hostNameRedirectedFrom)]() mutable {
        if (m_memoryStore)
            m_memoryStore->setTopFrameUniqueRedirectFrom(topFramePrimaryDomain, primaryRedirectDomain);
        postTaskReply(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::scheduleCookieBlockingUpdate(CompletionHandler<void()>&& completionHandler)
{
    // Helper function used by testing system. Should only be called from the main thread.
    ASSERT(RunLoop::isMain());

    postTask([this, completionHandler = WTFMove(completionHandler)]() mutable {
        if (!m_memoryStore) {
            postTaskReply(WTFMove(completionHandler));
            return;
        }
        m_memoryStore->updateCookieBlocking([completionHandler = WTFMove(completionHandler)]() mutable {
            postTaskReply(WTFMove(completionHandler));
        });
    });
}

void WebResourceLoadStatisticsStore::scheduleCookieBlockingUpdateForDomains(const Vector<String>& domainsToBlock, CompletionHandler<void()>&& completionHandler)
{
    // Helper function used by testing system. Should only be called from the main thread.
    ASSERT(RunLoop::isMain());
    postTask([this, domainsToBlock = crossThreadCopy(domainsToBlock), completionHandler = WTFMove(completionHandler)]() mutable {
        if (!m_memoryStore) {
            postTaskReply(WTFMove(completionHandler));
            return;
        }

        m_memoryStore->updateCookieBlockingForDomains(domainsToBlock, [completionHandler = WTFMove(completionHandler)]() mutable {
            postTaskReply(WTFMove(completionHandler));
        });
    });
}

void WebResourceLoadStatisticsStore::scheduleClearBlockingStateForDomains(const Vector<String>& domains, CompletionHandler<void()>&& completionHandler)
{
    // Helper function used by testing system. Should only be called from the main thread.
    ASSERT(RunLoop::isMain());
    postTask([this, domains = crossThreadCopy(domains), completionHandler = WTFMove(completionHandler)]() mutable {
        if (!m_memoryStore) {
            postTaskReply(WTFMove(completionHandler));
            return;
        }

        m_memoryStore->clearBlockingStateForDomains(domains, [completionHandler = WTFMove(completionHandler)]() mutable {
            postTaskReply(WTFMove(completionHandler));
        });
    });
}

void WebResourceLoadStatisticsStore::scheduleClearInMemoryAndPersistent(ShouldGrandfatherStatistics shouldGrandfather, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    postTask([this, protectedThis = makeRef(*this), shouldGrandfather, completionHandler = WTFMove(completionHandler)]() mutable {
        if (m_persistentStorage)
            m_persistentStorage->clear();

        CompletionHandlerCallingScope completionHandlerCaller([completionHandler = WTFMove(completionHandler)]() mutable {
            postTaskReply(WTFMove(completionHandler));
        });

        if (m_memoryStore) {
            m_memoryStore->clear([this, protectedThis = protectedThis.copyRef(), shouldGrandfather, completionHandlerCaller = WTFMove(completionHandlerCaller)] () mutable {
                if (shouldGrandfather == ShouldGrandfatherStatistics::Yes) {
                    if (m_memoryStore)
                        m_memoryStore->grandfatherExistingWebsiteData(completionHandlerCaller.release());
                    else
                        RELEASE_LOG(ResourceLoadStatistics, "WebResourceLoadStatisticsStore::scheduleClearInMemoryAndPersistent After being cleared, m_memoryStore is null when trying to grandfather data.");
                }
            });
        } else {
            if (shouldGrandfather == ShouldGrandfatherStatistics::Yes)
                RELEASE_LOG(ResourceLoadStatistics, "WebResourceLoadStatisticsStore::scheduleClearInMemoryAndPersistent Before being cleared, m_memoryStore is null when trying to grandfather data.");
        }
    });
}

void WebResourceLoadStatisticsStore::scheduleClearInMemoryAndPersistent(WallTime modifiedSince, ShouldGrandfatherStatistics shouldGrandfather, CompletionHandler<void()>&& callback)
{
    ASSERT(RunLoop::isMain());

    // For now, be conservative and clear everything regardless of modifiedSince.
    UNUSED_PARAM(modifiedSince);
    scheduleClearInMemoryAndPersistent(shouldGrandfather, WTFMove(callback));
}

void WebResourceLoadStatisticsStore::setTimeToLiveUserInteraction(Seconds seconds, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    postTask([this, seconds, completionHandler = WTFMove(completionHandler)]() mutable {
        if (m_memoryStore)
            m_memoryStore->setTimeToLiveUserInteraction(seconds);
        postTaskReply(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::setMinimumTimeBetweenDataRecordsRemoval(Seconds seconds, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    postTask([this, seconds, completionHandler = WTFMove(completionHandler)]() mutable  {
        if (m_memoryStore)
            m_memoryStore->setMinimumTimeBetweenDataRecordsRemoval(seconds);

        postTaskReply(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::setGrandfatheringTime(Seconds seconds, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    postTask([this, seconds, completionHandler = WTFMove(completionHandler)]() mutable  {
        if (m_memoryStore)
            m_memoryStore->setGrandfatheringTime(seconds);

        postTaskReply(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::setCacheMaxAgeCap(Seconds seconds, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    ASSERT(seconds >= 0_s);
    
    if (m_networkSession)
        m_networkSession->networkStorageSession().setCacheMaxAgeCapForPrevalentResources(seconds);

    completionHandler();
}

void WebResourceLoadStatisticsStore::callUpdatePrevalentDomainsToBlockCookiesForHandler(const Vector<String>& domainsToBlock, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    if (m_networkSession)
        m_networkSession->networkStorageSession().setPrevalentDomainsToBlockCookiesFor(domainsToBlock);

    completionHandler();
}

void WebResourceLoadStatisticsStore::removePrevalentDomains(const Vector<String>& domains)
{
    if (m_networkSession)
        m_networkSession->networkStorageSession().removePrevalentDomains(domains);
}

void WebResourceLoadStatisticsStore::callRemoveDomainsHandler(const Vector<String>& domains)
{
    ASSERT(RunLoop::isMain());

    removePrevalentDomains(domains);
}
    
void WebResourceLoadStatisticsStore::setMaxStatisticsEntries(size_t maximumEntryCount, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    postTask([this, maximumEntryCount, completionHandler = WTFMove(completionHandler)]() mutable  {
        if (m_memoryStore)
            m_memoryStore->setMaxStatisticsEntries(maximumEntryCount);

        postTaskReply(WTFMove(completionHandler));
    });
}
    
void WebResourceLoadStatisticsStore::setPruneEntriesDownTo(size_t pruneTargetCount, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    postTask([this, pruneTargetCount, completionHandler = WTFMove(completionHandler)]() mutable  {
        if (m_memoryStore)
            m_memoryStore->setPruneEntriesDownTo(pruneTargetCount);

        postTaskReply(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::resetParametersToDefaultValues(CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    postTask([this, completionHandler = WTFMove(completionHandler)]() mutable {
        if (m_memoryStore)
            m_memoryStore->resetParametersToDefaultValues();

        postTaskReply(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::logTestingEvent(const String& event)
{
    ASSERT(RunLoop::isMain());

    if (m_networkSession)
        m_networkSession->networkProcess().parentProcessConnection()->send(Messages::NetworkProcessProxy::LogTestingEvent(m_networkSession->sessionID(), event), 0);
}

void WebResourceLoadStatisticsStore::notifyResourceLoadStatisticsProcessed()
{
    ASSERT(RunLoop::isMain());
    
    if (m_networkSession)
        m_networkSession->notifyResourceLoadStatisticsProcessed();
}

void WebResourceLoadStatisticsStore::deleteWebsiteDataForTopPrivatelyControlledDomainsInAllPersistentDataStores(OptionSet<WebsiteDataType> dataTypes, Vector<String>&& topPrivatelyControlledDomains, bool shouldNotifyPage, CompletionHandler<void(const HashSet<String>&)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    if (m_networkSession) {
        m_networkSession->deleteWebsiteDataForTopPrivatelyControlledDomainsInAllPersistentDataStores(dataTypes, WTFMove(topPrivatelyControlledDomains), shouldNotifyPage, WTFMove(completionHandler));
        return;
    }

    completionHandler({ });
}

void WebResourceLoadStatisticsStore::topPrivatelyControlledDomainsWithWebsiteData(OptionSet<WebsiteDataType> dataTypes, bool shouldNotifyPage, CompletionHandler<void(HashSet<String>&&)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    if (m_networkSession) {
        m_networkSession->topPrivatelyControlledDomainsWithWebsiteData(dataTypes, shouldNotifyPage, WTFMove(completionHandler));
        return;
    }

    completionHandler({ });
}

void WebResourceLoadStatisticsStore::sendDiagnosticMessageWithValue(const String& message, const String& description, unsigned value, unsigned sigDigits, WebCore::ShouldSample shouldSample) const
{
    if (m_networkSession)
        const_cast<WebResourceLoadStatisticsStore*>(this)->networkSession()->logDiagnosticMessageWithValue(message, description, value, sigDigits, shouldSample);
}

void WebResourceLoadStatisticsStore::notifyPageStatisticsTelemetryFinished(unsigned totalPrevalentResources, unsigned totalPrevalentResourcesWithUserInteraction, unsigned top3SubframeUnderTopFrameOrigins) const
{
    if (m_networkSession)
        const_cast<WebResourceLoadStatisticsStore*>(this)->networkSession()->notifyPageStatisticsTelemetryFinished(totalPrevalentResources, totalPrevalentResourcesWithUserInteraction, top3SubframeUnderTopFrameOrigins);
}

} // namespace WebKit

#endif

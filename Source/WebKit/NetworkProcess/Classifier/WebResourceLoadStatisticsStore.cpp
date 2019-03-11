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
#include "NetworkProcess.h"
#include "NetworkSession.h"
#include "ResourceLoadStatisticsDatabaseStore.h"
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
#include <WebCore/CookieJar.h>
#include <WebCore/DiagnosticLoggingClient.h>
#include <WebCore/DiagnosticLoggingKeys.h>
#include <WebCore/NetworkStorageSession.h>
#include <WebCore/ResourceLoadStatistics.h>
#include <WebCore/RuntimeEnabledFeatures.h>
#include <WebCore/SQLiteDatabase.h>
#include <WebCore/SQLiteStatement.h>
#include <wtf/CallbackAggregator.h>
#include <wtf/CrossThreadCopier.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/threads/BinarySemaphore.h>

namespace WebKit {
using namespace WebCore;

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
        if (m_statisticsStore)
            m_statisticsStore->setNotifyPagesWhenDataRecordsWereScanned(value);
    });
}

void WebResourceLoadStatisticsStore::setNotifyPagesWhenDataRecordsWereScanned(bool value, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    postTask([this, value, completionHandler = WTFMove(completionHandler)]() mutable {
        if (m_statisticsStore)
            m_statisticsStore->setNotifyPagesWhenDataRecordsWereScanned(value);

        postTaskReply(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::setIsRunningTest(bool value, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    postTask([this, value, completionHandler = WTFMove(completionHandler)]() mutable {
        if (m_statisticsStore)
            m_statisticsStore->setIsRunningTest(value);
        
        postTaskReply(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::setShouldClassifyResourcesBeforeDataRecordsRemoval(bool value, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    postTask([this, value, completionHandler = WTFMove(completionHandler)]() mutable {
        if (m_statisticsStore)
            m_statisticsStore->setShouldClassifyResourcesBeforeDataRecordsRemoval(value);

        postTaskReply(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::setShouldSubmitTelemetry(bool value)
{
    ASSERT(RunLoop::isMain());

    postTask([this, value] {
        if (m_statisticsStore)
            m_statisticsStore->setShouldSubmitTelemetry(value);
    });
}

void WebResourceLoadStatisticsStore::setNotifyPagesWhenTelemetryWasCaptured(bool value, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    WebKit::WebResourceLoadStatisticsTelemetry::setNotifyPagesWhenTelemetryWasCaptured(value);
    completionHandler();
}

WebResourceLoadStatisticsStore::WebResourceLoadStatisticsStore(NetworkSession& networkSession, const String& resourceLoadStatisticsDirectory, ShouldIncludeLocalhost shouldIncludeLocalhost)
    : m_networkSession(makeWeakPtr(networkSession))
    , m_statisticsQueue(WorkQueue::create("WebResourceLoadStatisticsStore Process Data Queue", WorkQueue::Type::Serial, WorkQueue::QOS::Utility))
    , m_dailyTasksTimer(RunLoop::main(), this, &WebResourceLoadStatisticsStore::performDailyTasks)
{
    ASSERT(RunLoop::isMain());
    
    postTask([this, resourceLoadStatisticsDirectory = resourceLoadStatisticsDirectory.isolatedCopy(), shouldIncludeLocalhost] {
        if (RuntimeEnabledFeatures::sharedFeatures().isITPDatabaseEnabled()) {
            m_statisticsStore = std::make_unique<ResourceLoadStatisticsDatabaseStore>(*this, m_statisticsQueue, shouldIncludeLocalhost, resourceLoadStatisticsDirectory);

            auto memoryStore = std::make_unique<ResourceLoadStatisticsMemoryStore>(*this, m_statisticsQueue, shouldIncludeLocalhost);
            auto persistentStore = std::make_unique<ResourceLoadStatisticsPersistentStorage>(*memoryStore, m_statisticsQueue, resourceLoadStatisticsDirectory);

            downcast<ResourceLoadStatisticsDatabaseStore>(*m_statisticsStore.get()).populateFromMemoryStore(*memoryStore);
        } else {
            m_statisticsStore = std::make_unique<ResourceLoadStatisticsMemoryStore>(*this, m_statisticsQueue, shouldIncludeLocalhost);
            m_persistentStorage = std::make_unique<ResourceLoadStatisticsPersistentStorage>(downcast<ResourceLoadStatisticsMemoryStore>(*m_statisticsStore), m_statisticsQueue, resourceLoadStatisticsDirectory);
        }

        // FIXME(193297): This should be revised after the UIProcess version goes away.
        m_statisticsStore->didCreateNetworkProcess();
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

    if (!m_persistentStorage && !m_statisticsStore)
        return;

    // Make sure we destroy the persistent store on the background queue and wait for it to die
    // synchronously since it has a C++ reference to us. Blocking nature of this task allows us
    // to not maintain a WebResourceLoadStatisticsStore reference for the duration of dispatch,
    // avoiding double-deletion issues when this is invoked from the destructor.
    BinarySemaphore semaphore;
    m_statisticsQueue->dispatch([&semaphore, this] {
        m_persistentStorage = nullptr;
        m_statisticsStore = nullptr;
        semaphore.signal();
    });
    semaphore.wait();
}

void WebResourceLoadStatisticsStore::setResourceLoadStatisticsDebugMode(bool value, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    postTask([this, value, completionHandler = WTFMove(completionHandler)]() mutable {
        if (m_statisticsStore)
            m_statisticsStore->setResourceLoadStatisticsDebugMode(value);
        postTaskReply(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::setPrevalentResourceForDebugMode(const RegistrableDomain& domain, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    postTask([this, domain = domain.isolatedCopy(), completionHandler = WTFMove(completionHandler)]() mutable {
        if (m_statisticsStore)
            m_statisticsStore->setPrevalentResourceForDebugMode(domain);
        postTaskReply(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::scheduleStatisticsAndDataRecordsProcessing(CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    postTask([this, completionHandler = WTFMove(completionHandler)]() mutable {
        if (m_statisticsStore)
            m_statisticsStore->processStatisticsAndDataRecords();
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
        if (!m_statisticsStore || !is<ResourceLoadStatisticsMemoryStore>(*m_statisticsStore))
            return;

        auto& memoryStore = downcast<ResourceLoadStatisticsMemoryStore>(*m_statisticsStore);
    
        memoryStore.mergeStatistics(WTFMove(origins));

        // We can cancel any pending request to process statistics since we're doing it synchronously below.
        memoryStore.cancelPendingStatisticsProcessingRequest();

        // Fire before processing statistics to propagate user interaction as fast as possible to the network process.
        memoryStore.updateCookieBlocking([]() { });
        memoryStore.processStatisticsAndDataRecords();
    });
}

void WebResourceLoadStatisticsStore::hasStorageAccess(const RegistrableDomain& subFrameDomain, const RegistrableDomain& topFrameDomain, Optional<uint64_t> frameID, uint64_t pageID, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(subFrameDomain != topFrameDomain);
    ASSERT(RunLoop::isMain());

    postTask([this, subFrameDomain = subFrameDomain.isolatedCopy(), topFrameDomain = topFrameDomain.isolatedCopy(), frameID, pageID, completionHandler = WTFMove(completionHandler)]() mutable {
        if (!m_statisticsStore) {
            postTaskReply([completionHandler = WTFMove(completionHandler)]() mutable {
                completionHandler(false);
            });
            return;
        }

        if (m_statisticsStore) {
            m_statisticsStore->hasStorageAccess(subFrameDomain, topFrameDomain, frameID, pageID, [completionHandler = WTFMove(completionHandler)](bool hasStorageAccess) mutable {
                postTaskReply([completionHandler = WTFMove(completionHandler), hasStorageAccess]() mutable {
                    completionHandler(hasStorageAccess);
                });
            });
        }
    });
}

bool WebResourceLoadStatisticsStore::hasStorageAccessForFrame(const RegistrableDomain& resourceDomain, const RegistrableDomain& firstPartyDomain, uint64_t frameID, uint64_t pageID)
{
    return m_networkSession ? m_networkSession->networkStorageSession().hasStorageAccess(resourceDomain, firstPartyDomain, frameID, pageID) : false;
}

void WebResourceLoadStatisticsStore::callHasStorageAccessForFrameHandler(const RegistrableDomain& resourceDomain, const RegistrableDomain& firstPartyDomain, uint64_t frameID, uint64_t pageID, CompletionHandler<void(bool hasAccess)>&& callback)
{
    ASSERT(RunLoop::isMain());

    if (m_networkSession) {
        callback(m_networkSession->networkStorageSession().hasStorageAccess(resourceDomain, firstPartyDomain, frameID, pageID));
        return;
    }

    callback(false);
}

void WebResourceLoadStatisticsStore::requestStorageAccessGranted(const RegistrableDomain& subFrameDomain, const RegistrableDomain& topFrameDomain, uint64_t frameID, uint64_t pageID, CompletionHandler<void(bool)>&& completionHandler)
{
    auto statusHandler = [this, protectedThis = makeRef(*this), subFrameDomain = subFrameDomain.isolatedCopy(), topFrameDomain = topFrameDomain.isolatedCopy(), frameID, pageID, completionHandler = WTFMove(completionHandler)](StorageAccessStatus status) mutable {
        switch (status) {
        case StorageAccessStatus::CannotRequestAccess:
            completionHandler(false);
            return;
        case StorageAccessStatus::RequiresUserPrompt:
            {
            CompletionHandler<void(bool)> requestConfirmationCompletionHandler = [this, protectedThis = protectedThis.copyRef(), subFrameDomain, topFrameDomain, frameID, pageID, completionHandler = WTFMove(completionHandler)] (bool userDidGrantAccess) mutable {
                if (userDidGrantAccess)
                    grantStorageAccess(subFrameDomain, topFrameDomain, frameID, pageID, userDidGrantAccess, WTFMove(completionHandler));
                else
                    completionHandler(false);
            };
            m_networkSession->networkProcess().parentProcessConnection()->sendWithAsyncReply(Messages::NetworkProcessProxy::RequestStorageAccessConfirm(pageID, frameID, subFrameDomain, topFrameDomain), WTFMove(requestConfirmationCompletionHandler));
            }
            return;
        case StorageAccessStatus::HasAccess:
            completionHandler(true);
            return;
        }
    };

    requestStorageAccess(subFrameDomain, topFrameDomain, frameID, pageID, WTFMove(statusHandler));
}

void WebResourceLoadStatisticsStore::requestStorageAccess(const RegistrableDomain& subFrameDomain, const RegistrableDomain& topFrameDomain, Optional<uint64_t> frameID, uint64_t pageID, CompletionHandler<void(StorageAccessStatus)>&& completionHandler)
{
    if (subFrameDomain == topFrameDomain) {
        completionHandler(StorageAccessStatus::HasAccess);
        return;
    }

    postTask([this, subFrameDomain = subFrameDomain.isolatedCopy(), topFrameDomain = topFrameDomain.isolatedCopy(), frameID, pageID, completionHandler = WTFMove(completionHandler)]() mutable {
        if (!m_statisticsStore) {
            postTaskReply([completionHandler = WTFMove(completionHandler)]() mutable {
                completionHandler(StorageAccessStatus::CannotRequestAccess);
            });
            return;
        }

        if (m_statisticsStore) {
            m_statisticsStore->requestStorageAccess(WTFMove(subFrameDomain), WTFMove(topFrameDomain), frameID.value(), pageID, [completionHandler = WTFMove(completionHandler)](StorageAccessStatus status) mutable {
                postTaskReply([completionHandler = WTFMove(completionHandler), status]() mutable {
                    completionHandler(status);
                });
            });
        }
    });
}

void WebResourceLoadStatisticsStore::requestStorageAccessUnderOpener(RegistrableDomain&& domainInNeedOfStorageAccess, uint64_t openerPageID, RegistrableDomain&& openerDomain)
{
    ASSERT(RunLoop::isMain());

    // It is safe to move the strings to the background queue without isolated copy here because they are r-value references
    // coming from IPC. Strings which are safe to move to other threads as long as nobody on this thread holds a reference
    // to those strings.
    postTask([this, domainInNeedOfStorageAccess = WTFMove(domainInNeedOfStorageAccess), openerPageID, openerDomain = WTFMove(openerDomain)]() mutable {
        if (m_statisticsStore)
            m_statisticsStore->requestStorageAccessUnderOpener(WTFMove(domainInNeedOfStorageAccess), openerPageID, WTFMove(openerDomain));
    });
}

void WebResourceLoadStatisticsStore::grantStorageAccess(const RegistrableDomain& subFrameDomain, const RegistrableDomain& topFrameDomain, uint64_t frameID, uint64_t pageID, bool userWasPromptedNow, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    postTask([this, subFrameDomain = subFrameDomain.isolatedCopy(), topFrameDomain = topFrameDomain.isolatedCopy(), frameID, pageID, userWasPromptedNow, completionHandler = WTFMove(completionHandler)]() mutable {
        if (!m_statisticsStore) {
            postTaskReply([completionHandler = WTFMove(completionHandler)]() mutable {
                completionHandler(false);
            });
            return;
        }

        if (m_statisticsStore) {
            m_statisticsStore->grantStorageAccess(WTFMove(subFrameDomain), WTFMove(topFrameDomain), frameID, pageID, userWasPromptedNow, [completionHandler = WTFMove(completionHandler)](bool wasGrantedAccess) mutable {
                postTaskReply([completionHandler = WTFMove(completionHandler), wasGrantedAccess]() mutable {
                    completionHandler(wasGrantedAccess);
                });
            });
        }
    });
}

bool WebResourceLoadStatisticsStore::grantStorageAccess(const RegistrableDomain& resourceDomain, const RegistrableDomain& firstPartyDomain, Optional<uint64_t> frameID, uint64_t pageID)
{
    bool isStorageGranted = false;

    if (m_networkSession) {
        m_networkSession->networkStorageSession().grantStorageAccess(resourceDomain, firstPartyDomain, frameID, pageID);
        ASSERT(m_networkSession->networkStorageSession().hasStorageAccess(resourceDomain, firstPartyDomain, frameID, pageID));
        isStorageGranted = true;
    }

    return isStorageGranted;
}

void WebResourceLoadStatisticsStore::callGrantStorageAccessHandler(const RegistrableDomain& subFrameDomain, const RegistrableDomain& topFrameDomain, Optional<uint64_t> frameID, uint64_t pageID, CompletionHandler<void(bool)>&& callback)
{
    ASSERT(RunLoop::isMain());

    callback(grantStorageAccess(subFrameDomain, topFrameDomain, frameID, pageID));
}

void WebResourceLoadStatisticsStore::didCreateNetworkProcess()
{
    ASSERT(RunLoop::isMain());

    postTask([this] {
        if (!m_statisticsStore)
            return;
        m_statisticsStore->didCreateNetworkProcess();
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
        if (m_statisticsStore) {
            m_statisticsStore->includeTodayAsOperatingDateIfNecessary();
            m_statisticsStore->calculateAndSubmitTelemetry();
        }
    });
}

void WebResourceLoadStatisticsStore::submitTelemetry(CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    postTask([this, completionHandler = WTFMove(completionHandler)]() mutable  {
        if (m_statisticsStore && is<ResourceLoadStatisticsMemoryStore>(*m_statisticsStore))
            WebResourceLoadStatisticsTelemetry::calculateAndSubmit(downcast<ResourceLoadStatisticsMemoryStore>(*m_statisticsStore));

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

    RegistrableDomain targetDomain { targetURL };
    RegistrableDomain topFrameDomain { pageURL };
    RegistrableDomain sourceDomain { sourceURL };

    logFrameNavigation(targetDomain, topFrameDomain, sourceDomain, isRedirect, frame.isMainFrame());
}

void WebResourceLoadStatisticsStore::logFrameNavigation(const RegistrableDomain& targetDomain, const RegistrableDomain& topFrameDomain, const RegistrableDomain& sourceDomain, bool isRedirect, bool isMainFrame)
{
    postTask([this, targetDomain = targetDomain.isolatedCopy(), topFrameDomain = topFrameDomain.isolatedCopy(), sourceDomain = sourceDomain.isolatedCopy(), isRedirect, isMainFrame] {
        if (m_statisticsStore)
            m_statisticsStore->logFrameNavigation(targetDomain, topFrameDomain, sourceDomain, isRedirect, isMainFrame);
    });
}

void WebResourceLoadStatisticsStore::logWebSocketLoading(const RegistrableDomain& targetDomain, const RegistrableDomain& topFrameDomain, WallTime lastSeen, CompletionHandler<void()>&& completionHandler)
{
    postTask([this, targetDomain = targetDomain.isolatedCopy(), topFrameDomain = topFrameDomain.isolatedCopy(), lastSeen, completionHandler = WTFMove(completionHandler)]() mutable {
        if (m_statisticsStore)
            m_statisticsStore->logSubresourceLoading(targetDomain, topFrameDomain, lastSeen);

        postTaskReply(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::logSubresourceLoading(const SubResourceDomain& targetDomain, const TopFrameDomain& topFrameDomain, WallTime lastSeen, CompletionHandler<void()>&& completionHandler)
{
    postTask([this, targetDomain = targetDomain.isolatedCopy(), topFrameDomain = topFrameDomain.isolatedCopy(), lastSeen, completionHandler = WTFMove(completionHandler)]() mutable {
        if (m_statisticsStore)
            m_statisticsStore->logSubresourceLoading(targetDomain, topFrameDomain, lastSeen);

        postTaskReply(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::logSubresourceRedirect(const RegistrableDomain& sourceDomain, const RegistrableDomain& targetDomain, CompletionHandler<void()>&& completionHandler)
{
    postTask([this, sourceDomain = sourceDomain.isolatedCopy(), targetDomain = targetDomain.isolatedCopy(), completionHandler = WTFMove(completionHandler)]() mutable {
        if (m_statisticsStore)
            m_statisticsStore->logSubresourceRedirect(sourceDomain, targetDomain);
        postTaskReply(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::logUserInteraction(const RegistrableDomain& domain, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    postTask([this, domain = domain.isolatedCopy(), completionHandler = WTFMove(completionHandler)]() mutable {
        if (m_statisticsStore)
            m_statisticsStore->logUserInteraction(domain);
        postTaskReply(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::logCrossSiteLoadWithLinkDecoration(const RegistrableDomain& fromDomain, const RegistrableDomain& toDomain, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    ASSERT(fromDomain != toDomain);
    
    postTask([this, fromDomain = fromDomain.isolatedCopy(), toDomain = toDomain.isolatedCopy(), completionHandler = WTFMove(completionHandler)]() mutable {
        if (m_statisticsStore)
            m_statisticsStore->logCrossSiteLoadWithLinkDecoration(fromDomain, toDomain);
        postTaskReply(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::clearUserInteraction(const RegistrableDomain& domain, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    postTask([this, domain = domain.isolatedCopy(), completionHandler = WTFMove(completionHandler)]() mutable {
        if (m_statisticsStore)
            m_statisticsStore->clearUserInteraction(domain);
        postTaskReply(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::hasHadUserInteraction(const RegistrableDomain& domain, CompletionHandler<void(bool)>&& completionHandler)
{
    postTask([this, domain, completionHandler = WTFMove(completionHandler)]() mutable {
        bool hadUserInteraction = m_statisticsStore ? m_statisticsStore->hasHadUserInteraction(domain) : false;
        postTaskReply([hadUserInteraction, completionHandler = WTFMove(completionHandler)]() mutable {
            completionHandler(hadUserInteraction);
        });
    });
}

void WebResourceLoadStatisticsStore::setLastSeen(const RegistrableDomain& domain, Seconds seconds, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    postTask([this, domain = domain.isolatedCopy(), seconds, completionHandler = WTFMove(completionHandler)]() mutable {
        if (m_statisticsStore)
            m_statisticsStore->setLastSeen(domain, seconds);
        postTaskReply(WTFMove(completionHandler));
    });
}
    
void WebResourceLoadStatisticsStore::setPrevalentResource(const RegistrableDomain& domain, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    postTask([this, domain = domain.isolatedCopy(), completionHandler = WTFMove(completionHandler)]() mutable {
        if (m_statisticsStore)
            m_statisticsStore->setPrevalentResource(domain);
        postTaskReply(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::setVeryPrevalentResource(const RegistrableDomain& domain, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    postTask([this, domain = domain.isolatedCopy(), completionHandler = WTFMove(completionHandler)]() mutable {
        if (m_statisticsStore)
            m_statisticsStore->setVeryPrevalentResource(domain);
        postTaskReply(WTFMove(completionHandler));
    });
}
    
void WebResourceLoadStatisticsStore::dumpResourceLoadStatistics(CompletionHandler<void(String)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    postTask([this, completionHandler = WTFMove(completionHandler)]() mutable {
        String result = m_statisticsStore ? m_statisticsStore->dumpResourceLoadStatistics() : emptyString();
        postTaskReply([result = result.isolatedCopy(), completionHandler = WTFMove(completionHandler)]() mutable {
            completionHandler(result);
        });
    });
}

void WebResourceLoadStatisticsStore::isPrevalentResource(const RegistrableDomain& domain, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    postTask([this, domain = domain.isolatedCopy(), completionHandler = WTFMove(completionHandler)]() mutable {
        bool isPrevalentResource = m_statisticsStore ? m_statisticsStore->isPrevalentResource(domain) : false;
        postTaskReply([isPrevalentResource, completionHandler = WTFMove(completionHandler)]() mutable {
            completionHandler(isPrevalentResource);
        });
    });
}
    
void WebResourceLoadStatisticsStore::isVeryPrevalentResource(const RegistrableDomain& domain, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    postTask([this, domain = domain.isolatedCopy(), completionHandler = WTFMove(completionHandler)]() mutable {
        bool isVeryPrevalentResource = m_statisticsStore ? m_statisticsStore->isVeryPrevalentResource(domain) : false;
        postTaskReply([isVeryPrevalentResource, completionHandler = WTFMove(completionHandler)]() mutable {
            completionHandler(isVeryPrevalentResource);
        });
    });
}

void WebResourceLoadStatisticsStore::isRegisteredAsSubresourceUnder(const RegistrableDomain& subresourceDomain, const RegistrableDomain& topFrameDomain, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    postTask([this, subresourceDomain = subresourceDomain.isolatedCopy(), topFrameDomain = topFrameDomain.isolatedCopy(), completionHandler = WTFMove(completionHandler)]() mutable {
        bool isRegisteredAsSubresourceUnder = m_statisticsStore ? m_statisticsStore->isRegisteredAsSubresourceUnder(subresourceDomain, topFrameDomain)
            : false;
        postTaskReply([isRegisteredAsSubresourceUnder, completionHandler = WTFMove(completionHandler)]() mutable {
            completionHandler(isRegisteredAsSubresourceUnder);
        });
    });
}

void WebResourceLoadStatisticsStore::isRegisteredAsSubFrameUnder(const RegistrableDomain& subFrameDomain, const RegistrableDomain& topFrameDomain, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    postTask([this, subFrameDomain = subFrameDomain.isolatedCopy(), topFrameDomain = topFrameDomain.isolatedCopy(), completionHandler = WTFMove(completionHandler)]() mutable {
        bool isRegisteredAsSubFrameUnder = m_statisticsStore ? m_statisticsStore->isRegisteredAsSubFrameUnder(subFrameDomain, topFrameDomain)
            : false;
        postTaskReply([isRegisteredAsSubFrameUnder, completionHandler = WTFMove(completionHandler)]() mutable {
            completionHandler(isRegisteredAsSubFrameUnder);
        });
    });
}

void WebResourceLoadStatisticsStore::isRegisteredAsRedirectingTo(const RegistrableDomain& domainRedirectedFrom, const RegistrableDomain& domainRedirectedTo, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    postTask([this, domainRedirectedFrom = domainRedirectedFrom.isolatedCopy(), domainRedirectedTo = domainRedirectedTo.isolatedCopy(), completionHandler = WTFMove(completionHandler)]() mutable {
        bool isRegisteredAsRedirectingTo = m_statisticsStore ? m_statisticsStore->isRegisteredAsRedirectingTo(domainRedirectedFrom, domainRedirectedTo)
            : false;
        postTaskReply([isRegisteredAsRedirectingTo, completionHandler = WTFMove(completionHandler)]() mutable {
            completionHandler(isRegisteredAsRedirectingTo);
        });
    });
}

void WebResourceLoadStatisticsStore::clearPrevalentResource(const RegistrableDomain& domain, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    postTask([this, domain = domain.isolatedCopy(), completionHandler = WTFMove(completionHandler)]() mutable {
        if (m_statisticsStore)
            m_statisticsStore->clearPrevalentResource(domain);
        postTaskReply(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::setGrandfathered(const RegistrableDomain& domain, bool value, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    postTask([this, domain = domain.isolatedCopy(), value, completionHandler = WTFMove(completionHandler)]() mutable {
        if (m_statisticsStore)
            m_statisticsStore->setGrandfathered(domain, value);
        postTaskReply(WTFMove(completionHandler));
    });
}
    
void WebResourceLoadStatisticsStore::isGrandfathered(const RegistrableDomain& domain, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    postTask([this, completionHandler = WTFMove(completionHandler), domain = domain.isolatedCopy()]() mutable {
        bool isGrandFathered = m_statisticsStore ? m_statisticsStore->isGrandfathered(domain)
            : false;
        postTaskReply([isGrandFathered, completionHandler = WTFMove(completionHandler)]() mutable {
            completionHandler(isGrandFathered);
        });
    });
}

void WebResourceLoadStatisticsStore::setSubframeUnderTopFrameDomain(const RegistrableDomain& subFrameDomain, const RegistrableDomain& topFrameDomain, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    postTask([this, completionHandler = WTFMove(completionHandler), subFrameDomain = subFrameDomain.isolatedCopy(), topFrameDomain = topFrameDomain.isolatedCopy()]() mutable {
        if (m_statisticsStore)
            m_statisticsStore->setSubframeUnderTopFrameDomain(subFrameDomain, topFrameDomain);
        postTaskReply(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::setSubresourceUnderTopFrameDomain(const RegistrableDomain& subresourceDomain, const RegistrableDomain& topFrameDomain, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    postTask([this, completionHandler = WTFMove(completionHandler), subresourceDomain = subresourceDomain.isolatedCopy(), topFrameDomain = topFrameDomain.isolatedCopy()]() mutable {
        if (m_statisticsStore)
            m_statisticsStore->setSubresourceUnderTopFrameDomain(subresourceDomain, topFrameDomain);
        postTaskReply(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::setSubresourceUniqueRedirectTo(const RegistrableDomain& subresourceDomain, const RegistrableDomain& domainRedirectedTo, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    postTask([this, completionHandler = WTFMove(completionHandler), subresourceDomain = subresourceDomain.isolatedCopy(), domainRedirectedTo = domainRedirectedTo.isolatedCopy()]() mutable {
        if (m_statisticsStore)
            m_statisticsStore->setSubresourceUniqueRedirectTo(subresourceDomain, domainRedirectedTo);
        postTaskReply(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::setSubresourceUniqueRedirectFrom(const RegistrableDomain& subresourceDomain, const RegistrableDomain& domainRedirectedFrom, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    postTask([this, completionHandler = WTFMove(completionHandler), subresourceDomain = subresourceDomain.isolatedCopy(), domainRedirectedFrom = domainRedirectedFrom.isolatedCopy()]() mutable {
        if (m_statisticsStore)
            m_statisticsStore->setSubresourceUniqueRedirectFrom(subresourceDomain, domainRedirectedFrom);
        postTaskReply(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::setTopFrameUniqueRedirectTo(const RegistrableDomain& topFrameDomain, const RegistrableDomain& domainRedirectedTo, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    postTask([this, completionHandler = WTFMove(completionHandler), topFrameDomain = topFrameDomain.isolatedCopy(), domainRedirectedTo = domainRedirectedTo.isolatedCopy()]() mutable {
        if (m_statisticsStore)
            m_statisticsStore->setTopFrameUniqueRedirectTo(topFrameDomain, domainRedirectedTo);
        postTaskReply(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::setTopFrameUniqueRedirectFrom(const RegistrableDomain& topFrameDomain, const RegistrableDomain& domainRedirectedFrom, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    postTask([this, completionHandler = WTFMove(completionHandler), topFrameDomain = topFrameDomain.isolatedCopy(), domainRedirectedFrom = domainRedirectedFrom.isolatedCopy()]() mutable {
        if (m_statisticsStore)
            m_statisticsStore->setTopFrameUniqueRedirectFrom(topFrameDomain, domainRedirectedFrom);
        postTaskReply(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::scheduleCookieBlockingUpdate(CompletionHandler<void()>&& completionHandler)
{
    // Helper function used by testing system. Should only be called from the main thread.
    ASSERT(RunLoop::isMain());

    postTask([this, completionHandler = WTFMove(completionHandler)]() mutable {
        if (!m_statisticsStore) {
            postTaskReply(WTFMove(completionHandler));
            return;
        }

        auto callbackAggregator = CallbackAggregator::create([completionHandler = WTFMove(completionHandler)] () mutable {
            postTaskReply(WTFMove(completionHandler));
        });
        
        if (m_statisticsStore)
            m_statisticsStore->updateCookieBlocking([callbackAggregator = callbackAggregator.copyRef()]() { });
    });
}

void WebResourceLoadStatisticsStore::scheduleCookieBlockingUpdateForDomains(const Vector<RegistrableDomain>& domainsToBlock, CompletionHandler<void()>&& completionHandler)
{
    // Helper function used by testing system. Should only be called from the main thread.
    ASSERT(RunLoop::isMain());
    postTask([this, domainsToBlock = crossThreadCopy(domainsToBlock), completionHandler = WTFMove(completionHandler)]() mutable {
        if (!m_statisticsStore) {
            postTaskReply(WTFMove(completionHandler));
            return;
        }

        auto callbackAggregator = CallbackAggregator::create([completionHandler = WTFMove(completionHandler)] () mutable {
            postTaskReply(WTFMove(completionHandler));
        });

        if (m_statisticsStore)
            m_statisticsStore->updateCookieBlockingForDomains(domainsToBlock, [callbackAggregator = callbackAggregator.copyRef()]() { });
    });
}

void WebResourceLoadStatisticsStore::scheduleClearBlockingStateForDomains(const Vector<RegistrableDomain>& domains, CompletionHandler<void()>&& completionHandler)
{
    // Helper function used by testing system. Should only be called from the main thread.
    ASSERT(RunLoop::isMain());
    postTask([this, domains = crossThreadCopy(domains), completionHandler = WTFMove(completionHandler)]() mutable {
        if (!m_statisticsStore) {
            postTaskReply(WTFMove(completionHandler));
            return;
        }

        auto callbackAggregator = CallbackAggregator::create([completionHandler = WTFMove(completionHandler)] () mutable {
            postTaskReply(WTFMove(completionHandler));
        });

        if (m_statisticsStore)
            m_statisticsStore->clearBlockingStateForDomains(domains, [callbackAggregator = callbackAggregator.copyRef()]() { });
    });
}

void WebResourceLoadStatisticsStore::scheduleClearInMemoryAndPersistent(ShouldGrandfatherStatistics shouldGrandfather, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    postTask([this, protectedThis = makeRef(*this), shouldGrandfather, completionHandler = WTFMove(completionHandler)]() mutable {
        if (m_persistentStorage)
            m_persistentStorage->clear();

        if (!m_statisticsStore) {
            if (shouldGrandfather == ShouldGrandfatherStatistics::Yes)
                RELEASE_LOG(ResourceLoadStatistics, "WebResourceLoadStatisticsStore::scheduleClearInMemoryAndPersistent Before being cleared, m_statisticsStore is null when trying to grandfather data.");

            postTaskReply(WTFMove(completionHandler));
            return;
        }

        auto callbackAggregator = CallbackAggregator::create([completionHandler = WTFMove(completionHandler)] () mutable {
            postTaskReply(WTFMove(completionHandler));
        });

        if (m_statisticsStore) {
            m_statisticsStore->clear([this, protectedThis = protectedThis.copyRef(), shouldGrandfather, callbackAggregator = callbackAggregator.copyRef()] () mutable {
                if (shouldGrandfather == ShouldGrandfatherStatistics::Yes) {
                    if (m_statisticsStore)
                        m_statisticsStore->grandfatherExistingWebsiteData([callbackAggregator = WTFMove(callbackAggregator)]() mutable { });
                    else
                        RELEASE_LOG(ResourceLoadStatistics, "WebResourceLoadStatisticsStore::scheduleClearInMemoryAndPersistent After being cleared, m_statisticsStore is null when trying to grandfather data.");
                }
            });
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
        if (m_statisticsStore)
            m_statisticsStore->setTimeToLiveUserInteraction(seconds);
        postTaskReply(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::setMinimumTimeBetweenDataRecordsRemoval(Seconds seconds, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    postTask([this, seconds, completionHandler = WTFMove(completionHandler)]() mutable  {
        if (m_statisticsStore)
            m_statisticsStore->setMinimumTimeBetweenDataRecordsRemoval(seconds);

        postTaskReply(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::setGrandfatheringTime(Seconds seconds, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    postTask([this, seconds, completionHandler = WTFMove(completionHandler)]() mutable  {
        if (m_statisticsStore)
            m_statisticsStore->setGrandfatheringTime(seconds);

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

void WebResourceLoadStatisticsStore::callUpdatePrevalentDomainsToBlockCookiesForHandler(const Vector<RegistrableDomain>& domainsToBlock, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    if (m_networkSession)
        m_networkSession->networkStorageSession().setPrevalentDomainsToBlockCookiesFor(domainsToBlock);

    completionHandler();
}

void WebResourceLoadStatisticsStore::removePrevalentDomains(const Vector<RegistrableDomain>& domains)
{
    if (m_networkSession)
        m_networkSession->networkStorageSession().removePrevalentDomains(domains);
}

void WebResourceLoadStatisticsStore::callRemoveDomainsHandler(const Vector<RegistrableDomain>& domains)
{
    ASSERT(RunLoop::isMain());

    removePrevalentDomains(domains);
}
    
void WebResourceLoadStatisticsStore::setMaxStatisticsEntries(size_t maximumEntryCount, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    postTask([this, maximumEntryCount, completionHandler = WTFMove(completionHandler)]() mutable  {
        if (m_statisticsStore)
            m_statisticsStore->setMaxStatisticsEntries(maximumEntryCount);

        postTaskReply(WTFMove(completionHandler));
    });
}
    
void WebResourceLoadStatisticsStore::setPruneEntriesDownTo(size_t pruneTargetCount, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    postTask([this, pruneTargetCount, completionHandler = WTFMove(completionHandler)]() mutable  {
        if (m_statisticsStore)
            m_statisticsStore->setPruneEntriesDownTo(pruneTargetCount);

        postTaskReply(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::resetParametersToDefaultValues(CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    postTask([this, completionHandler = WTFMove(completionHandler)]() mutable {
        if (m_statisticsStore)
            m_statisticsStore->resetParametersToDefaultValues();

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

void WebResourceLoadStatisticsStore::deleteWebsiteDataForRegistrableDomainsInAllPersistentDataStores(OptionSet<WebsiteDataType> dataTypes, Vector<RegistrableDomain>&& domains, bool shouldNotifyPage, IncludeHttpOnlyCookies includeHttpOnlyCookies, CompletionHandler<void(const HashSet<RegistrableDomain>&)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    if (m_networkSession) {
        m_networkSession->deleteWebsiteDataForRegistrableDomainsInAllPersistentDataStores(dataTypes, WTFMove(domains), shouldNotifyPage, includeHttpOnlyCookies, WTFMove(completionHandler));
        return;
    }

    completionHandler({ });
}

void WebResourceLoadStatisticsStore::registrableDomainsWithWebsiteData(OptionSet<WebsiteDataType> dataTypes, bool shouldNotifyPage, CompletionHandler<void(HashSet<RegistrableDomain>&&)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    if (m_networkSession) {
        m_networkSession->registrableDomainsWithWebsiteData(dataTypes, shouldNotifyPage, WTFMove(completionHandler));
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

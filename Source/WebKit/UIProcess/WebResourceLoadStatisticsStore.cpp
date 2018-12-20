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

#include "config.h"
#include "WebResourceLoadStatisticsStore.h"

#include "Logging.h"
#include "ResourceLoadStatisticsMemoryStore.h"
#include "ResourceLoadStatisticsPersistentStorage.h"
#include "WebFrameProxy.h"
#include "WebPageProxy.h"
#include "WebProcessMessages.h"
#include "WebProcessProxy.h"
#include "WebResourceLoadStatisticsStoreMessages.h"
#include "WebResourceLoadStatisticsTelemetry.h"
#include "WebsiteDataFetchOption.h"
#include "WebsiteDataStore.h"
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

void WebResourceLoadStatisticsStore::setShouldClassifyResourcesBeforeDataRecordsRemoval(bool value)
{
    ASSERT(RunLoop::isMain());

    postTask([this, value] {
        if (m_memoryStore)
            m_memoryStore->setShouldClassifyResourcesBeforeDataRecordsRemoval(value);
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

WebResourceLoadStatisticsStore::WebResourceLoadStatisticsStore(WebsiteDataStore& websiteDataStore)
    : m_websiteDataStore(makeWeakPtr(websiteDataStore))
    , m_statisticsQueue(WorkQueue::create("WebResourceLoadStatisticsStore Process Data Queue", WorkQueue::Type::Serial, WorkQueue::QOS::Utility))
    , m_dailyTasksTimer(RunLoop::main(), this, &WebResourceLoadStatisticsStore::performDailyTasks)
{
    ASSERT(RunLoop::isMain());

    postTask([this, resourceLoadStatisticsDirectory = websiteDataStore.resolvedResourceLoadStatisticsDirectory().isolatedCopy()] {
        m_memoryStore = std::make_unique<ResourceLoadStatisticsMemoryStore>(*this, m_statisticsQueue);
        m_persistentStorage = std::make_unique<ResourceLoadStatisticsPersistentStorage>(*m_memoryStore, m_statisticsQueue, resourceLoadStatisticsDirectory);
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

void WebResourceLoadStatisticsStore::setPrevalentResourceForDebugMode(const URL& url, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    postTask([this, primaryDomain = isolatedPrimaryDomain(url), completionHandler = WTFMove(completionHandler)]() mutable {
        if (m_memoryStore)
            m_memoryStore->setPrevalentResourceForDebugMode(primaryDomain);
        postTaskReply(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::scheduleStatisticsAndDataRecordsProcessing()
{
    ASSERT(RunLoop::isMain());

    postTask([this] {
        if (m_memoryStore)
            m_memoryStore->processStatisticsAndDataRecords();
    });
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

void WebResourceLoadStatisticsStore::hasStorageAccess(String&& subFrameHost, String&& topFrameHost, uint64_t frameID, uint64_t pageID, CompletionHandler<void (bool)>&& completionHandler)
{
    ASSERT(subFrameHost != topFrameHost);
    ASSERT(RunLoop::isMain());

    postTask([this, subFramePrimaryDomain = isolatedPrimaryDomain(subFrameHost), topFramePrimaryDomain = isolatedPrimaryDomain(topFrameHost), frameID, pageID, completionHandler = WTFMove(completionHandler)] () mutable {
        if (!m_memoryStore) {
            postTaskReply([completionHandler = WTFMove(completionHandler)] () mutable {
                completionHandler(false);
            });
            return;
        }
        m_memoryStore->hasStorageAccess(subFramePrimaryDomain, topFramePrimaryDomain, frameID, pageID, [completionHandler = WTFMove(completionHandler)](bool hasStorageAccess) mutable {
            postTaskReply([completionHandler = WTFMove(completionHandler), hasStorageAccess] () mutable {
                completionHandler(hasStorageAccess);
            });
        });
    });
}

void WebResourceLoadStatisticsStore::callHasStorageAccessForFrameHandler(const String& resourceDomain, const String& firstPartyDomain, uint64_t frameID, uint64_t pageID, CompletionHandler<void(bool hasAccess)>&& callback)
{
    ASSERT(RunLoop::isMain());

#if ENABLE(RESOURCE_LOAD_STATISTICS)
    if (m_websiteDataStore) {
        m_websiteDataStore->hasStorageAccessForFrameHandler(resourceDomain, firstPartyDomain, frameID, pageID, WTFMove(callback));
        return;
    }
#endif
    callback(false);
}

void WebResourceLoadStatisticsStore::requestStorageAccess(String&& subFrameHost, String&& topFrameHost, uint64_t frameID, uint64_t pageID, bool promptEnabled, CompletionHandler<void(StorageAccessStatus)>&& completionHandler)
{
    ASSERT(subFrameHost != topFrameHost);
    ASSERT(RunLoop::isMain());

    auto subFramePrimaryDomain = isolatedPrimaryDomain(subFrameHost);
    auto topFramePrimaryDomain = isolatedPrimaryDomain(topFrameHost);
    if (subFramePrimaryDomain == topFramePrimaryDomain) {
        completionHandler(StorageAccessStatus::HasAccess);
        return;
    }

    postTask([this, subFramePrimaryDomain = crossThreadCopy(subFramePrimaryDomain), topFramePrimaryDomain = crossThreadCopy(topFramePrimaryDomain), frameID, pageID, promptEnabled, completionHandler = WTFMove(completionHandler)] () mutable {
        if (!m_memoryStore) {
            postTaskReply([completionHandler = WTFMove(completionHandler)] () mutable {
                completionHandler(StorageAccessStatus::CannotRequestAccess);
            });
            return;
        }

        m_memoryStore->requestStorageAccess(WTFMove(subFramePrimaryDomain), WTFMove(topFramePrimaryDomain), frameID, pageID, promptEnabled, [completionHandler = WTFMove(completionHandler)](StorageAccessStatus status) mutable {
            postTaskReply([completionHandler = WTFMove(completionHandler), status] () mutable {
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

void WebResourceLoadStatisticsStore::grantStorageAccess(String&& subFrameHost, String&& topFrameHost, uint64_t frameID, uint64_t pageID, bool userWasPromptedNow, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    postTask([this, subFrameHost = crossThreadCopy(subFrameHost), topFrameHost = crossThreadCopy(topFrameHost), frameID, pageID, userWasPromptedNow, completionHandler = WTFMove(completionHandler)] () mutable {
        if (!m_memoryStore) {
            postTaskReply([completionHandler = WTFMove(completionHandler)] () mutable {
                completionHandler(false);
            });
            return;
        }

        m_memoryStore->grantStorageAccess(WTFMove(subFrameHost), WTFMove(topFrameHost), frameID, pageID, userWasPromptedNow, [completionHandler = WTFMove(completionHandler)](bool wasGrantedAccess) mutable {
            postTaskReply([completionHandler = WTFMove(completionHandler), wasGrantedAccess] () mutable {
                completionHandler(wasGrantedAccess);
            });
        });
    });
}

void WebResourceLoadStatisticsStore::callGrantStorageAccessHandler(const String& subFramePrimaryDomain, const String& topFramePrimaryDomain, Optional<uint64_t> frameID, uint64_t pageID, CompletionHandler<void(bool)>&& callback)
{
    ASSERT(RunLoop::isMain());

#if ENABLE(RESOURCE_LOAD_STATISTICS)
    if (m_websiteDataStore) {
        m_websiteDataStore->grantStorageAccessHandler(subFramePrimaryDomain, topFramePrimaryDomain, frameID, pageID, WTFMove(callback));
        return;
    }
#endif
    callback(false);
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

#if ENABLE(RESOURCE_LOAD_STATISTICS)
    if (m_websiteDataStore)
        m_websiteDataStore->removeAllStorageAccessHandler(WTFMove(completionHandler));
    else
        completionHandler();
#else
    completionHandler();
#endif
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

void WebResourceLoadStatisticsStore::submitTelemetry()
{
    ASSERT(RunLoop::isMain());

    postTask([this] {
        if (m_memoryStore)
            WebResourceLoadStatisticsTelemetry::calculateAndSubmit(*m_memoryStore);
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

    postTask([this, targetPrimaryDomain = targetPrimaryDomain.isolatedCopy(), mainFramePrimaryDomain = mainFramePrimaryDomain.isolatedCopy(), sourcePrimaryDomain = sourcePrimaryDomain.isolatedCopy(), targetHost = targetHost.toString().isolatedCopy(), mainFrameHost = mainFrameHost.toString().isolatedCopy(), isRedirect, isMainFrame = frame.isMainFrame()] {
        
        if (m_memoryStore)
            m_memoryStore->logFrameNavigation(targetPrimaryDomain, mainFramePrimaryDomain, sourcePrimaryDomain, targetHost, mainFrameHost, isRedirect, isMainFrame);
    });
}

void WebResourceLoadStatisticsStore::logUserInteraction(const URL& url, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    if (url.protocolIsAbout() || url.isEmpty()) {
        completionHandler();
        return;
    }

    postTask([this, primaryDomain = isolatedPrimaryDomain(url), completionHandler = WTFMove(completionHandler)]() mutable {
        if (m_memoryStore)
            m_memoryStore->logUserInteraction(primaryDomain);
        postTaskReply(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::clearUserInteraction(const URL& url, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    if (url.protocolIsAbout() || url.isEmpty()) {
        completionHandler();
        return;
    }

    postTask([this, primaryDomain = isolatedPrimaryDomain(url), completionHandler = WTFMove(completionHandler)]() mutable {
        if (m_memoryStore)
            m_memoryStore->clearUserInteraction(primaryDomain);
        postTaskReply(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::hasHadUserInteraction(const URL& url, CompletionHandler<void (bool)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    if (url.protocolIsAbout() || url.isEmpty()) {
        completionHandler(false);
        return;
    }

    postTask([this, primaryDomain = isolatedPrimaryDomain(url), completionHandler = WTFMove(completionHandler)] () mutable {
        bool hadUserInteraction = m_memoryStore ? m_memoryStore->hasHadUserInteraction(primaryDomain) : false;
        postTaskReply([hadUserInteraction, completionHandler = WTFMove(completionHandler)] () mutable {
            completionHandler(hadUserInteraction);
        });
    });
}

void WebResourceLoadStatisticsStore::setLastSeen(const URL& url, Seconds seconds, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    if (url.protocolIsAbout() || url.isEmpty()) {
        completionHandler();
        return;
    }

    postTask([this, primaryDomain = isolatedPrimaryDomain(url), seconds, completionHandler = WTFMove(completionHandler)]() mutable {
        if (m_memoryStore)
            m_memoryStore->setLastSeen(primaryDomain, seconds);
        postTaskReply(WTFMove(completionHandler));
    });
}
    
void WebResourceLoadStatisticsStore::setPrevalentResource(const URL& url, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    if (url.protocolIsAbout() || url.isEmpty()) {
        completionHandler();
        return;
    }

    postTask([this, primaryDomain = isolatedPrimaryDomain(url), completionHandler = WTFMove(completionHandler)]() mutable {
        if (m_memoryStore)
            m_memoryStore->setPrevalentResource(primaryDomain);
        postTaskReply(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::setVeryPrevalentResource(const URL& url, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    if (url.protocolIsAbout() || url.isEmpty()) {
        completionHandler();
        return;
    }

    postTask([this, primaryDomain = isolatedPrimaryDomain(url), completionHandler = WTFMove(completionHandler)]() mutable {
        if (m_memoryStore)
            m_memoryStore->setVeryPrevalentResource(primaryDomain);
        postTaskReply(WTFMove(completionHandler));
    });
}
    
void WebResourceLoadStatisticsStore::dumpResourceLoadStatistics(CompletionHandler<void(const String&)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    postTask([this, completionHandler = WTFMove(completionHandler)] () mutable {
        String result = m_memoryStore ? m_memoryStore->dumpResourceLoadStatistics() : emptyString();
        postTaskReply([result = result.isolatedCopy(), completionHandler = WTFMove(completionHandler)] () mutable {
            completionHandler(result);
        });
    });
}

void WebResourceLoadStatisticsStore::isPrevalentResource(const URL& url, CompletionHandler<void (bool)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    if (url.protocolIsAbout() || url.isEmpty()) {
        completionHandler(false);
        return;
    }

    postTask([this, primaryDomain = isolatedPrimaryDomain(url), completionHandler = WTFMove(completionHandler)] () mutable {
        bool isPrevalentResource = m_memoryStore ? m_memoryStore->isPrevalentResource(primaryDomain) : false;
        postTaskReply([isPrevalentResource, completionHandler = WTFMove(completionHandler)] () mutable {
            completionHandler(isPrevalentResource);
        });
    });
}

void WebResourceLoadStatisticsStore::isVeryPrevalentResource(const URL& url, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    if (url.protocolIsAbout() || url.isEmpty()) {
        completionHandler(false);
        return;
    }
    
    postTask([this, primaryDomain = isolatedPrimaryDomain(url), completionHandler = WTFMove(completionHandler)] () mutable {
        bool isVeryPrevalentResource = m_memoryStore ? m_memoryStore->isVeryPrevalentResource(primaryDomain) : false;
        postTaskReply([isVeryPrevalentResource, completionHandler = WTFMove(completionHandler)] () mutable {
            completionHandler(isVeryPrevalentResource);
        });
    });
}

void WebResourceLoadStatisticsStore::isRegisteredAsSubresourceUnder(const URL& subresource, const URL& topFrame, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    postTask([this, subresourcePrimaryDomain = isolatedPrimaryDomain(subresource), topFramePrimaryDomain = isolatedPrimaryDomain(topFrame), completionHandler = WTFMove(completionHandler)] () mutable {
        bool isRegisteredAsSubresourceUnder = m_memoryStore ? m_memoryStore->isRegisteredAsSubresourceUnder(subresourcePrimaryDomain, topFramePrimaryDomain) : false;
        postTaskReply([isRegisteredAsSubresourceUnder, completionHandler = WTFMove(completionHandler)] () mutable {
            completionHandler(isRegisteredAsSubresourceUnder);
        });
    });
}

void WebResourceLoadStatisticsStore::isRegisteredAsSubFrameUnder(const URL& subFrame, const URL& topFrame, CompletionHandler<void (bool)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    postTask([this, subFramePrimaryDomain = isolatedPrimaryDomain(subFrame), topFramePrimaryDomain = isolatedPrimaryDomain(topFrame), completionHandler = WTFMove(completionHandler)] () mutable {
        bool isRegisteredAsSubFrameUnder = m_memoryStore ? m_memoryStore->isRegisteredAsSubFrameUnder(subFramePrimaryDomain, topFramePrimaryDomain) : false;
        postTaskReply([isRegisteredAsSubFrameUnder, completionHandler = WTFMove(completionHandler)] () mutable {
            completionHandler(isRegisteredAsSubFrameUnder);
        });
    });
}

void WebResourceLoadStatisticsStore::isRegisteredAsRedirectingTo(const URL& hostRedirectedFrom, const URL& hostRedirectedTo, CompletionHandler<void (bool)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    postTask([this, hostRedirectedFromPrimaryDomain = isolatedPrimaryDomain(hostRedirectedFrom), hostRedirectedToPrimaryDomain = isolatedPrimaryDomain(hostRedirectedTo), completionHandler = WTFMove(completionHandler)] () mutable {
        bool isRegisteredAsRedirectingTo = m_memoryStore ? m_memoryStore->isRegisteredAsRedirectingTo(hostRedirectedFromPrimaryDomain, hostRedirectedToPrimaryDomain) : false;
        postTaskReply([isRegisteredAsRedirectingTo, completionHandler = WTFMove(completionHandler)] () mutable {
            completionHandler(isRegisteredAsRedirectingTo);
        });
    });
}

void WebResourceLoadStatisticsStore::clearPrevalentResource(const URL& url, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    if (url.protocolIsAbout() || url.isEmpty()) {
        completionHandler();
        return;
    }

    postTask([this, primaryDomain = isolatedPrimaryDomain(url), completionHandler = WTFMove(completionHandler)]() mutable {
        if (m_memoryStore)
            m_memoryStore->clearPrevalentResource(primaryDomain);
        postTaskReply(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::setGrandfathered(const URL& url, bool value)
{
    ASSERT(RunLoop::isMain());

    if (url.protocolIsAbout() || url.isEmpty())
        return;

    postTask([this, primaryDomain = isolatedPrimaryDomain(url), value] {
        if (m_memoryStore)
            m_memoryStore->setGrandfathered(primaryDomain, value);
    });
}

void WebResourceLoadStatisticsStore::isGrandfathered(const URL& url, CompletionHandler<void (bool)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    if (url.protocolIsAbout() || url.isEmpty()) {
        completionHandler(false);
        return;
    }

    postTask([this, completionHandler = WTFMove(completionHandler), primaryDomain = isolatedPrimaryDomain(url)] () mutable {
        bool isGrandFathered = m_memoryStore ? m_memoryStore->isGrandfathered(primaryDomain) : false;
        postTaskReply([isGrandFathered, completionHandler = WTFMove(completionHandler)] () mutable {
            completionHandler(isGrandFathered);
        });
    });
}

void WebResourceLoadStatisticsStore::setSubframeUnderTopFrameOrigin(const URL& subframe, const URL& topFrame)
{
    ASSERT(RunLoop::isMain());

    if (subframe.protocolIsAbout() || subframe.isEmpty() || topFrame.protocolIsAbout() || topFrame.isEmpty())
        return;

    postTask([this, primaryTopFrameDomain = isolatedPrimaryDomain(topFrame), primarySubFrameDomain = isolatedPrimaryDomain(subframe)] {
        if (m_memoryStore)
            m_memoryStore->setSubframeUnderTopFrameOrigin(primarySubFrameDomain, primaryTopFrameDomain);
    });
}

void WebResourceLoadStatisticsStore::setSubresourceUnderTopFrameOrigin(const URL& subresource, const URL& topFrame)
{
    ASSERT(RunLoop::isMain());

    if (subresource.protocolIsAbout() || subresource.isEmpty() || topFrame.protocolIsAbout() || topFrame.isEmpty())
        return;

    postTask([this, primaryTopFrameDomain = isolatedPrimaryDomain(topFrame), primarySubresourceDomain = isolatedPrimaryDomain(subresource)] {
        if (m_memoryStore)
            m_memoryStore->setSubresourceUnderTopFrameOrigin(primarySubresourceDomain, primaryTopFrameDomain);
    });
}

void WebResourceLoadStatisticsStore::setSubresourceUniqueRedirectTo(const URL& subresource, const URL& hostNameRedirectedTo)
{
    ASSERT(RunLoop::isMain());

    if (subresource.protocolIsAbout() || subresource.isEmpty() || hostNameRedirectedTo.protocolIsAbout() || hostNameRedirectedTo.isEmpty())
        return;

    postTask([this, primaryRedirectDomain = isolatedPrimaryDomain(hostNameRedirectedTo), primarySubresourceDomain = isolatedPrimaryDomain(subresource)] {
        if (m_memoryStore)
            m_memoryStore->setSubresourceUniqueRedirectTo(primarySubresourceDomain, primaryRedirectDomain);
    });
}

void WebResourceLoadStatisticsStore::setSubresourceUniqueRedirectFrom(const URL& subresource, const URL& hostNameRedirectedFrom)
{
    ASSERT(RunLoop::isMain());

    if (subresource.protocolIsAbout() || subresource.isEmpty() || hostNameRedirectedFrom.protocolIsAbout() || hostNameRedirectedFrom.isEmpty())
        return;
    
    postTask([this, primaryRedirectDomain = isolatedPrimaryDomain(hostNameRedirectedFrom), primarySubresourceDomain = isolatedPrimaryDomain(subresource)] {
        if (m_memoryStore)
            m_memoryStore->setSubresourceUniqueRedirectFrom(primarySubresourceDomain, primaryRedirectDomain);
    });
}

void WebResourceLoadStatisticsStore::setTopFrameUniqueRedirectTo(const URL& topFrameHostName, const URL& hostNameRedirectedTo)
{
    ASSERT(RunLoop::isMain());

    if (topFrameHostName.protocolIsAbout() || topFrameHostName.isEmpty() || hostNameRedirectedTo.protocolIsAbout() || hostNameRedirectedTo.isEmpty())
        return;
    
    postTask([this, primaryRedirectDomain = isolatedPrimaryDomain(hostNameRedirectedTo), topFramePrimaryDomain = isolatedPrimaryDomain(topFrameHostName)] {
        if (m_memoryStore)
            m_memoryStore->setTopFrameUniqueRedirectTo(topFramePrimaryDomain, primaryRedirectDomain);
    });
}

void WebResourceLoadStatisticsStore::setTopFrameUniqueRedirectFrom(const URL& topFrameHostName, const URL& hostNameRedirectedFrom)
{
    ASSERT(RunLoop::isMain());

    if (topFrameHostName.protocolIsAbout() || topFrameHostName.isEmpty() || hostNameRedirectedFrom.protocolIsAbout() || hostNameRedirectedFrom.isEmpty())
        return;
    
    postTask([this, primaryRedirectDomain = isolatedPrimaryDomain(hostNameRedirectedFrom), topFramePrimaryDomain = isolatedPrimaryDomain(topFrameHostName)] {
        if (m_memoryStore)
            m_memoryStore->setTopFrameUniqueRedirectFrom(topFramePrimaryDomain, primaryRedirectDomain);
    });
}

void WebResourceLoadStatisticsStore::scheduleCookieBlockingUpdate(CompletionHandler<void()>&& completionHandler)
{
    // Helper function used by testing system. Should only be called from the main thread.
    ASSERT(RunLoop::isMain());

    postTask([this, completionHandler = WTFMove(completionHandler)] () mutable {
        if (!m_memoryStore) {
            postTaskReply(WTFMove(completionHandler));
            return;
        }
        m_memoryStore->updateCookieBlocking([completionHandler = WTFMove(completionHandler)] () mutable {
            postTaskReply(WTFMove(completionHandler));
        });
    });
}

void WebResourceLoadStatisticsStore::scheduleCookieBlockingUpdateForDomains(const Vector<String>& domainsToBlock, CompletionHandler<void()>&& completionHandler)
{
    // Helper function used by testing system. Should only be called from the main thread.
    ASSERT(RunLoop::isMain());
    postTask([this, domainsToBlock = crossThreadCopy(domainsToBlock), completionHandler = WTFMove(completionHandler)] () mutable {
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
    postTask([this, domains = crossThreadCopy(domains), completionHandler = WTFMove(completionHandler)] () mutable {
        if (!m_memoryStore) {
            postTaskReply(WTFMove(completionHandler));
            return;
        }

        m_memoryStore->clearBlockingStateForDomains(domains, [completionHandler = WTFMove(completionHandler)]() mutable {
            postTaskReply(WTFMove(completionHandler));
        });
    });
}

void WebResourceLoadStatisticsStore::scheduleClearInMemoryAndPersistent(ShouldGrandfather shouldGrandfather, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    postTask([this, protectedThis = makeRef(*this), shouldGrandfather, completionHandler = WTFMove(completionHandler)] () mutable {
        if (m_persistentStorage)
            m_persistentStorage->clear();

        CompletionHandlerCallingScope completionHandlerCaller([completionHandler = WTFMove(completionHandler)]() mutable {
            postTaskReply(WTFMove(completionHandler));
        });

        if (m_memoryStore) {
            m_memoryStore->clear([this, protectedThis = protectedThis.copyRef(), shouldGrandfather, completionHandlerCaller = WTFMove(completionHandlerCaller)] () mutable {
                if (shouldGrandfather == ShouldGrandfather::Yes) {
                    if (m_memoryStore)
                        m_memoryStore->grandfatherExistingWebsiteData(completionHandlerCaller.release());
                    else
                        RELEASE_LOG(ResourceLoadStatistics, "WebResourceLoadStatisticsStore::scheduleClearInMemoryAndPersistent After being cleared, m_memoryStore is null when trying to grandfather data.");
                }
            });
        } else {
            if (shouldGrandfather == ShouldGrandfather::Yes)
                RELEASE_LOG(ResourceLoadStatistics, "WebResourceLoadStatisticsStore::scheduleClearInMemoryAndPersistent Before being cleared, m_memoryStore is null when trying to grandfather data.");
        }
    });
}

void WebResourceLoadStatisticsStore::scheduleClearInMemoryAndPersistent(WallTime modifiedSince, ShouldGrandfather shouldGrandfather, CompletionHandler<void()>&& callback)
{
    ASSERT(RunLoop::isMain());

    // For now, be conservative and clear everything regardless of modifiedSince.
    UNUSED_PARAM(modifiedSince);
    scheduleClearInMemoryAndPersistent(shouldGrandfather, WTFMove(callback));
}

void WebResourceLoadStatisticsStore::setTimeToLiveUserInteraction(Seconds seconds)
{
    ASSERT(RunLoop::isMain());
    postTask([this, seconds] {
        if (m_memoryStore)
            m_memoryStore->setTimeToLiveUserInteraction(seconds);
    });
}

void WebResourceLoadStatisticsStore::setMinimumTimeBetweenDataRecordsRemoval(Seconds seconds)
{
    ASSERT(RunLoop::isMain());
    postTask([this, seconds] {
        if (m_memoryStore)
            m_memoryStore->setMinimumTimeBetweenDataRecordsRemoval(seconds);
    });
}

void WebResourceLoadStatisticsStore::setGrandfatheringTime(Seconds seconds)
{
    ASSERT(RunLoop::isMain());
    postTask([this, seconds] {
        if (m_memoryStore)
            m_memoryStore->setGrandfatheringTime(seconds);
    });
}

void WebResourceLoadStatisticsStore::setCacheMaxAgeCap(Seconds seconds, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    ASSERT(seconds >= 0_s);
    
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    if (m_websiteDataStore) {
        m_websiteDataStore->setCacheMaxAgeCapForPrevalentResources(seconds, WTFMove(completionHandler));
        return;
    }
#endif
    completionHandler();
}

void WebResourceLoadStatisticsStore::callUpdatePrevalentDomainsToBlockCookiesForHandler(const Vector<String>& domainsToBlock, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

#if ENABLE(RESOURCE_LOAD_STATISTICS)
    if (m_websiteDataStore) {
        m_websiteDataStore->updatePrevalentDomainsToBlockCookiesFor(domainsToBlock, WTFMove(completionHandler));
        return;
    }
#endif
    completionHandler();
}

void WebResourceLoadStatisticsStore::callRemoveDomainsHandler(const Vector<String>& domains)
{
    ASSERT(RunLoop::isMain());

#if ENABLE(RESOURCE_LOAD_STATISTICS)
    if (m_websiteDataStore)
        m_websiteDataStore->removePrevalentDomains(domains);
#endif
}
    
void WebResourceLoadStatisticsStore::setMaxStatisticsEntries(size_t maximumEntryCount)
{
    ASSERT(RunLoop::isMain());
    postTask([this, maximumEntryCount] {
        if (m_memoryStore)
            m_memoryStore->setMaxStatisticsEntries(maximumEntryCount);
    });
}
    
void WebResourceLoadStatisticsStore::setPruneEntriesDownTo(size_t pruneTargetCount)
{
    ASSERT(RunLoop::isMain());

    postTask([this, pruneTargetCount] {
        if (m_memoryStore)
            m_memoryStore->setPruneEntriesDownTo(pruneTargetCount);
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

    if (m_statisticsTestingCallback)
        m_statisticsTestingCallback(event);
}

} // namespace WebKit

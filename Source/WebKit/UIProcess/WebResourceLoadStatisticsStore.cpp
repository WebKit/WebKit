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
#include <wtf/CrossThreadCopier.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/threads/BinarySemaphore.h>

namespace WebKit {
using namespace WebCore;

template<typename T> static inline String isolatedPrimaryDomain(const T& value)
{
    return ResourceLoadStatistics::primaryDomain(value).isolatedCopy();
}

static bool areDomainsAssociated(WebPageProxy* page, const String& firstDomain, const String& secondDomain)
{
    bool needsSiteSpecificQuirks = page && page->preferences().needsSiteSpecificQuirks();
    return ResourceLoadStatistics::areDomainsAssociated(needsSiteSpecificQuirks, firstDomain, secondDomain);
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

    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this), value] {
        if (m_memoryStore)
            m_memoryStore->setNotifyPagesWhenDataRecordsWereScanned(value);
    });
}

void WebResourceLoadStatisticsStore::setShouldClassifyResourcesBeforeDataRecordsRemoval(bool value)
{
    ASSERT(RunLoop::isMain());

    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this), value] {
        if (m_memoryStore)
            m_memoryStore->setShouldClassifyResourcesBeforeDataRecordsRemoval(value);
    });
}

void WebResourceLoadStatisticsStore::setShouldSubmitTelemetry(bool value)
{
    ASSERT(RunLoop::isMain());

    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this), value] {
        if (m_memoryStore)
            m_memoryStore->setShouldSubmitTelemetry(value);
    });
}

WebResourceLoadStatisticsStore::WebResourceLoadStatisticsStore(const String& resourceLoadStatisticsDirectory, Function<void(const String&)>&& testingCallback, bool isEphemeral, UpdatePrevalentDomainsToPartitionOrBlockCookiesHandler&& updatePrevalentDomainsToPartitionOrBlockCookiesHandler, HasStorageAccessForFrameHandler&& hasStorageAccessForFrameHandler, GrantStorageAccessHandler&& grantStorageAccessHandler, RemoveAllStorageAccessHandler&& removeAllStorageAccessHandler, RemovePrevalentDomainsHandler&& removeDomainsHandler)
    : m_statisticsQueue(WorkQueue::create("WebResourceLoadStatisticsStore Process Data Queue", WorkQueue::Type::Serial, WorkQueue::QOS::Utility))
    , m_updatePrevalentDomainsToPartitionOrBlockCookiesHandler(WTFMove(updatePrevalentDomainsToPartitionOrBlockCookiesHandler))
    , m_hasStorageAccessForFrameHandler(WTFMove(hasStorageAccessForFrameHandler))
    , m_grantStorageAccessHandler(WTFMove(grantStorageAccessHandler))
    , m_removeAllStorageAccessHandler(WTFMove(removeAllStorageAccessHandler))
    , m_removeDomainsHandler(WTFMove(removeDomainsHandler))
    , m_dailyTasksTimer(RunLoop::main(), this, &WebResourceLoadStatisticsStore::performDailyTasks)
    , m_statisticsTestingCallback(WTFMove(testingCallback))
{
    ASSERT(RunLoop::isMain());

    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this), isEphemeral, resourceLoadStatisticsDirectory = resourceLoadStatisticsDirectory.isolatedCopy()] {
        m_memoryStore = std::make_unique<ResourceLoadStatisticsMemoryStore>(*this, m_statisticsQueue);
        m_persistentStorage = std::make_unique<ResourceLoadStatisticsPersistentStorage>(*m_memoryStore, m_statisticsQueue, resourceLoadStatisticsDirectory, isEphemeral ? ResourceLoadStatisticsPersistentStorage::IsReadOnly::Yes : ResourceLoadStatisticsPersistentStorage::IsReadOnly::No);
    });

    m_statisticsQueue->dispatchAfter(5_s, [this, protectedThis = makeRef(*this)] {
        if (m_memoryStore)
            m_memoryStore->calculateAndSubmitTelemetry();
    });

    m_dailyTasksTimer.startRepeating(24_h);
}

WebResourceLoadStatisticsStore::~WebResourceLoadStatisticsStore()
{
    flushAndDestroyPersistentStore();
}

void WebResourceLoadStatisticsStore::flushAndDestroyPersistentStore()
{
    if (!m_persistentStorage && !m_memoryStore)
        return;

    // Make sure we destroy the persistent store on the background queue and wait for it to die
    // synchronously since it has a C++ reference to us.
    BinarySemaphore semaphore;
    m_statisticsQueue->dispatch([&semaphore, this] {
        m_persistentStorage = nullptr;
        m_memoryStore = nullptr;
        semaphore.signal();
    });
    semaphore.wait(WallTime::infinity());
}

void WebResourceLoadStatisticsStore::setResourceLoadStatisticsDebugMode(bool value)
{
    ASSERT(RunLoop::isMain());

    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this), value] {
        if (m_memoryStore)
            m_memoryStore->setResourceLoadStatisticsDebugMode(value);
    });
}

void WebResourceLoadStatisticsStore::scheduleStatisticsAndDataRecordsProcessing()
{
    ASSERT(RunLoop::isMain());

    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this)] {
        if (m_memoryStore)
            m_memoryStore->processStatisticsAndDataRecords();
    });
}

// On background queue due to IPC.
void WebResourceLoadStatisticsStore::resourceLoadStatisticsUpdated(Vector<WebCore::ResourceLoadStatistics>&& origins)
{
    ASSERT(!RunLoop::isMain());

    if (!m_memoryStore)
        return;

    m_memoryStore->mergeStatistics(WTFMove(origins));

    // We can cancel any pending request to process statistics since we're doing it synchronously below.
    m_memoryStore->cancelPendingStatisticsProcessingRequest();

    // Fire before processing statistics to propagate user interaction as fast as possible to the network process.
    m_memoryStore->updateCookiePartitioning([]() { });
    m_memoryStore->processStatisticsAndDataRecords();
}

void WebResourceLoadStatisticsStore::hasStorageAccess(String&& subFrameHost, String&& topFrameHost, uint64_t frameID, uint64_t pageID, CompletionHandler<void (bool)>&& completionHandler)
{
    ASSERT(subFrameHost != topFrameHost);
    ASSERT(RunLoop::isMain());

    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this), subFramePrimaryDomain = isolatedPrimaryDomain(subFrameHost), topFramePrimaryDomain = isolatedPrimaryDomain(topFrameHost), frameID, pageID, completionHandler = WTFMove(completionHandler)] () mutable {
        if (!m_memoryStore) {
            RunLoop::main().dispatch([completionHandler = WTFMove(completionHandler)] {
                completionHandler(false);
            });
            return;
        }
        m_memoryStore->hasStorageAccess(subFramePrimaryDomain, topFramePrimaryDomain, frameID, pageID, [completionHandler = WTFMove(completionHandler)](bool hasStorageAccess) mutable {
            RunLoop::main().dispatch([completionHandler = WTFMove(completionHandler), hasStorageAccess] {
                completionHandler(hasStorageAccess);
            });
        });
    });
}

void WebResourceLoadStatisticsStore::callHasStorageAccessForFrameHandler(const String& resourceDomain, const String& firstPartyDomain, uint64_t frameID, uint64_t pageID, Function<void(bool hasAccess)>&& callback)
{
    ASSERT(RunLoop::isMain());

    m_hasStorageAccessForFrameHandler(resourceDomain, firstPartyDomain, frameID, pageID, WTFMove(callback));
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

    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this), subFramePrimaryDomain = crossThreadCopy(subFramePrimaryDomain), topFramePrimaryDomain = crossThreadCopy(topFramePrimaryDomain), frameID, pageID, promptEnabled, completionHandler = WTFMove(completionHandler)] () mutable {
        if (!m_memoryStore) {
            RunLoop::main().dispatch([completionHandler = WTFMove(completionHandler)] {
                completionHandler(StorageAccessStatus::CannotRequestAccess);
            });
            return;
        }

        m_memoryStore->requestStorageAccess(WTFMove(subFramePrimaryDomain), WTFMove(topFramePrimaryDomain), frameID, pageID, promptEnabled, [completionHandler = WTFMove(completionHandler)](StorageAccessStatus status) mutable {
            RunLoop::main().dispatch([completionHandler = WTFMove(completionHandler), status] {
                completionHandler(status);
            });
        });
    });
}

// On background queue due to IPC.
void WebResourceLoadStatisticsStore::requestStorageAccessUnderOpener(String&& primaryDomainInNeedOfStorageAccess, uint64_t openerPageID, String&& openerPrimaryDomain, bool isTriggeredByUserGesture)
{
    ASSERT(!RunLoop::isMain());
    if (m_memoryStore)
        m_memoryStore->requestStorageAccessUnderOpener(WTFMove(primaryDomainInNeedOfStorageAccess), openerPageID, WTFMove(openerPrimaryDomain), isTriggeredByUserGesture);
}

void WebResourceLoadStatisticsStore::grantStorageAccess(String&& subFrameHost, String&& topFrameHost, uint64_t frameID, uint64_t pageID, bool userWasPromptedNow, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this), subFrameHost = crossThreadCopy(subFrameHost), topFrameHost = crossThreadCopy(topFrameHost), frameID, pageID, userWasPromptedNow, completionHandler = WTFMove(completionHandler)] () mutable {
        if (!m_memoryStore) {
            RunLoop::main().dispatch([completionHandler = WTFMove(completionHandler)] {
                completionHandler(false);
            });
            return;
        }

        m_memoryStore->grantStorageAccess(WTFMove(subFrameHost), WTFMove(topFrameHost), frameID, pageID, userWasPromptedNow, [completionHandler = WTFMove(completionHandler)](bool wasGrantedAccess) mutable {
            RunLoop::main().dispatch([completionHandler = WTFMove(completionHandler), wasGrantedAccess] {
                completionHandler(wasGrantedAccess);
            });
        });
    });
}

void WebResourceLoadStatisticsStore::callGrantStorageAccessHandler(const String& subFramePrimaryDomain, const String& topFramePrimaryDomain, std::optional<uint64_t> frameID, uint64_t pageID, CompletionHandler<void(bool)>&& callback)
{
    ASSERT(RunLoop::isMain());

    m_grantStorageAccessHandler(subFramePrimaryDomain, topFramePrimaryDomain, frameID, pageID, WTFMove(callback));
}

void WebResourceLoadStatisticsStore::removeAllStorageAccess()
{
    ASSERT(RunLoop::isMain());

    m_removeAllStorageAccessHandler();
}
    
void WebResourceLoadStatisticsStore::processWillOpenConnection(WebProcessProxy&, IPC::Connection& connection)
{
    connection.addWorkQueueMessageReceiver(Messages::WebResourceLoadStatisticsStore::messageReceiverName(), m_statisticsQueue.get(), this);
}

void WebResourceLoadStatisticsStore::processDidCloseConnection(WebProcessProxy&, IPC::Connection& connection)
{
    connection.removeWorkQueueMessageReceiver(Messages::WebResourceLoadStatisticsStore::messageReceiverName());
}

void WebResourceLoadStatisticsStore::applicationWillTerminate()
{
    flushAndDestroyPersistentStore();
}

void WebResourceLoadStatisticsStore::performDailyTasks()
{
    ASSERT(RunLoop::isMain());

    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this)] {
        if (!m_memoryStore)
            return;

        m_memoryStore->includeTodayAsOperatingDateIfNecessary();
        m_memoryStore->calculateAndSubmitTelemetry();
    });
}

void WebResourceLoadStatisticsStore::submitTelemetry()
{
    ASSERT(RunLoop::isMain());

    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this)] {
        if (m_memoryStore)
            WebResourceLoadStatisticsTelemetry::calculateAndSubmit(*m_memoryStore);
    });
}

void WebResourceLoadStatisticsStore::logFrameNavigation(const WebFrameProxy& frame, const URL& pageURL, const WebCore::ResourceRequest& request, const WebCore::URL& redirectURL)
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

    auto* page = frame.page();
    auto targetPrimaryDomain = ResourceLoadStatistics::primaryDomain(targetURL);
    auto mainFramePrimaryDomain = ResourceLoadStatistics::primaryDomain(pageURL);
    auto sourcePrimaryDomain = ResourceLoadStatistics::primaryDomain(sourceURL);

    bool areTargetAndMainFrameDomainsAssociated = areDomainsAssociated(page, targetPrimaryDomain, mainFramePrimaryDomain);
    bool areTargetAndSourceDomainsAssociated = areDomainsAssociated(page, targetPrimaryDomain, sourcePrimaryDomain);

    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this), targetPrimaryDomain = targetPrimaryDomain.isolatedCopy(), mainFramePrimaryDomain = mainFramePrimaryDomain.isolatedCopy(), sourcePrimaryDomain = sourcePrimaryDomain.isolatedCopy(), targetHost = targetHost.toString().isolatedCopy(), mainFrameHost = mainFrameHost.toString().isolatedCopy(), areTargetAndMainFrameDomainsAssociated, areTargetAndSourceDomainsAssociated, isRedirect, isMainFrame = frame.isMainFrame()] {
        if (m_memoryStore)
            m_memoryStore->logFrameNavigation(targetPrimaryDomain, mainFramePrimaryDomain, sourcePrimaryDomain, targetHost, mainFrameHost, areTargetAndMainFrameDomainsAssociated, areTargetAndSourceDomainsAssociated, isRedirect, isMainFrame);
    });
}

void WebResourceLoadStatisticsStore::logUserInteraction(const URL& url)
{
    ASSERT(RunLoop::isMain());

    if (url.isBlankURL() || url.isEmpty())
        return;

    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this), primaryDomain = isolatedPrimaryDomain(url)] {
        if (m_memoryStore)
            m_memoryStore->logUserInteraction(primaryDomain);
    });
}

void WebResourceLoadStatisticsStore::logNonRecentUserInteraction(const URL& url)
{
    ASSERT(RunLoop::isMain());

    if (url.isBlankURL() || url.isEmpty())
        return;
    
    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this), primaryDomain = isolatedPrimaryDomain(url)] {
        if (m_memoryStore)
            m_memoryStore->logNonRecentUserInteraction(primaryDomain);
    });
}

void WebResourceLoadStatisticsStore::clearUserInteraction(const URL& url)
{
    ASSERT(RunLoop::isMain());

    if (url.isBlankURL() || url.isEmpty())
        return;

    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this), primaryDomain = isolatedPrimaryDomain(url)] {
        if (m_memoryStore)
            m_memoryStore->clearUserInteraction(primaryDomain);
    });
}

void WebResourceLoadStatisticsStore::hasHadUserInteraction(const URL& url, CompletionHandler<void (bool)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    if (url.isBlankURL() || url.isEmpty()) {
        completionHandler(false);
        return;
    }

    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this), primaryDomain = isolatedPrimaryDomain(url), completionHandler = WTFMove(completionHandler)] () mutable {
        bool hadUserInteraction = m_memoryStore ? m_memoryStore->hasHadUserInteraction(primaryDomain) : false;
        RunLoop::main().dispatch([hadUserInteraction, completionHandler = WTFMove(completionHandler)] {
            completionHandler(hadUserInteraction);
        });
    });
}

void WebResourceLoadStatisticsStore::setLastSeen(const URL& url, Seconds seconds)
{
    ASSERT(RunLoop::isMain());

    if (url.isBlankURL() || url.isEmpty())
        return;
    
    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this), primaryDomain = isolatedPrimaryDomain(url), seconds] {
        if (m_memoryStore)
            m_memoryStore->setLastSeen(primaryDomain, seconds);
    });
}
    
void WebResourceLoadStatisticsStore::setPrevalentResource(const URL& url)
{
    ASSERT(RunLoop::isMain());

    if (url.isBlankURL() || url.isEmpty())
        return;

    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this), primaryDomain = isolatedPrimaryDomain(url)] {
        if (m_memoryStore)
            m_memoryStore->setPrevalentResource(primaryDomain);
    });
}

void WebResourceLoadStatisticsStore::setVeryPrevalentResource(const URL& url)
{
    ASSERT(RunLoop::isMain());

    if (url.isBlankURL() || url.isEmpty())
        return;
    
    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this), primaryDomain = isolatedPrimaryDomain(url)] {
        if (m_memoryStore)
            m_memoryStore->setVeryPrevalentResource(primaryDomain);
    });
}

void WebResourceLoadStatisticsStore::isPrevalentResource(const URL& url, CompletionHandler<void (bool)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    if (url.isBlankURL() || url.isEmpty()) {
        completionHandler(false);
        return;
    }

    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this), primaryDomain = isolatedPrimaryDomain(url), completionHandler = WTFMove(completionHandler)] () mutable {
        bool isPrevalentResource = m_memoryStore ? m_memoryStore->isPrevalentResource(primaryDomain) : false;
        RunLoop::main().dispatch([isPrevalentResource, completionHandler = WTFMove(completionHandler)] {
            completionHandler(isPrevalentResource);
        });
    });
}

void WebResourceLoadStatisticsStore::isVeryPrevalentResource(const URL& url, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    if (url.isBlankURL() || url.isEmpty()) {
        completionHandler(false);
        return;
    }
    
    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this), primaryDomain = isolatedPrimaryDomain(url), completionHandler = WTFMove(completionHandler)] () mutable {
        bool isVeryPrevalentResource = m_memoryStore ? m_memoryStore->isVeryPrevalentResource(primaryDomain) : false;
        RunLoop::main().dispatch([isVeryPrevalentResource, completionHandler = WTFMove(completionHandler)] {
            completionHandler(isVeryPrevalentResource);
        });
    });
}

void WebResourceLoadStatisticsStore::isRegisteredAsSubFrameUnder(const URL& subFrame, const URL& topFrame, CompletionHandler<void (bool)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this), subFramePrimaryDomain = isolatedPrimaryDomain(subFrame), topFramePrimaryDomain = isolatedPrimaryDomain(topFrame), completionHandler = WTFMove(completionHandler)] () mutable {
        bool isRegisteredAsSubFrameUnder = m_memoryStore ? m_memoryStore->isRegisteredAsSubFrameUnder(subFramePrimaryDomain, topFramePrimaryDomain) : false;
        RunLoop::main().dispatch([isRegisteredAsSubFrameUnder, completionHandler = WTFMove(completionHandler)] {
            completionHandler(isRegisteredAsSubFrameUnder);
        });
    });
}

void WebResourceLoadStatisticsStore::isRegisteredAsRedirectingTo(const URL& hostRedirectedFrom, const URL& hostRedirectedTo, CompletionHandler<void (bool)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this), hostRedirectedFromPrimaryDomain = isolatedPrimaryDomain(hostRedirectedFrom), hostRedirectedToPrimaryDomain = isolatedPrimaryDomain(hostRedirectedTo), completionHandler = WTFMove(completionHandler)] () mutable {
        bool isRegisteredAsRedirectingTo = m_memoryStore ? m_memoryStore->isRegisteredAsRedirectingTo(hostRedirectedFromPrimaryDomain, hostRedirectedToPrimaryDomain) : false;
        RunLoop::main().dispatch([isRegisteredAsRedirectingTo, completionHandler = WTFMove(completionHandler)] {
            completionHandler(isRegisteredAsRedirectingTo);
        });
    });
}

void WebResourceLoadStatisticsStore::clearPrevalentResource(const URL& url)
{
    ASSERT(RunLoop::isMain());

    if (url.isBlankURL() || url.isEmpty())
        return;

    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this), primaryDomain = isolatedPrimaryDomain(url)] {
        if (m_memoryStore)
            m_memoryStore->clearPrevalentResource(primaryDomain);
    });
}

void WebResourceLoadStatisticsStore::setGrandfathered(const URL& url, bool value)
{
    ASSERT(RunLoop::isMain());

    if (url.isBlankURL() || url.isEmpty())
        return;

    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this), primaryDomain = isolatedPrimaryDomain(url), value] {
        if (m_memoryStore)
            m_memoryStore->setGrandfathered(primaryDomain, value);
    });
}

void WebResourceLoadStatisticsStore::isGrandfathered(const URL& url, CompletionHandler<void (bool)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    if (url.isBlankURL() || url.isEmpty()) {
        completionHandler(false);
        return;
    }

    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler), primaryDomain = isolatedPrimaryDomain(url)] () mutable {
        bool isGrandFathered = m_memoryStore ? m_memoryStore->isGrandfathered(primaryDomain) : false;
        RunLoop::main().dispatch([isGrandFathered, completionHandler = WTFMove(completionHandler)] {
            completionHandler(isGrandFathered);
        });
    });
}

void WebResourceLoadStatisticsStore::setSubframeUnderTopFrameOrigin(const URL& subframe, const URL& topFrame)
{
    ASSERT(RunLoop::isMain());

    if (subframe.isBlankURL() || subframe.isEmpty() || topFrame.isBlankURL() || topFrame.isEmpty())
        return;

    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this), primaryTopFrameDomain = isolatedPrimaryDomain(topFrame), primarySubFrameDomain = isolatedPrimaryDomain(subframe)] {
        if (m_memoryStore)
            m_memoryStore->setSubframeUnderTopFrameOrigin(primarySubFrameDomain, primaryTopFrameDomain);
    });
}

void WebResourceLoadStatisticsStore::setSubresourceUnderTopFrameOrigin(const URL& subresource, const URL& topFrame)
{
    ASSERT(RunLoop::isMain());

    if (subresource.isBlankURL() || subresource.isEmpty() || topFrame.isBlankURL() || topFrame.isEmpty())
        return;

    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this), primaryTopFrameDomain = isolatedPrimaryDomain(topFrame), primarySubresourceDomain = isolatedPrimaryDomain(subresource)] {
        if (m_memoryStore)
            m_memoryStore->setSubresourceUnderTopFrameOrigin(primarySubresourceDomain, primaryTopFrameDomain);
    });
}

void WebResourceLoadStatisticsStore::setSubresourceUniqueRedirectTo(const URL& subresource, const URL& hostNameRedirectedTo)
{
    ASSERT(RunLoop::isMain());

    if (subresource.isBlankURL() || subresource.isEmpty() || hostNameRedirectedTo.isBlankURL() || hostNameRedirectedTo.isEmpty())
        return;

    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this), primaryRedirectDomain = isolatedPrimaryDomain(hostNameRedirectedTo), primarySubresourceDomain = isolatedPrimaryDomain(subresource)] {
        if (m_memoryStore)
            m_memoryStore->setSubresourceUniqueRedirectTo(primarySubresourceDomain, primaryRedirectDomain);
    });
}

void WebResourceLoadStatisticsStore::setSubresourceUniqueRedirectFrom(const URL& subresource, const URL& hostNameRedirectedFrom)
{
    ASSERT(RunLoop::isMain());

    if (subresource.isBlankURL() || subresource.isEmpty() || hostNameRedirectedFrom.isBlankURL() || hostNameRedirectedFrom.isEmpty())
        return;
    
    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this), primaryRedirectDomain = isolatedPrimaryDomain(hostNameRedirectedFrom), primarySubresourceDomain = isolatedPrimaryDomain(subresource)] {
        if (m_memoryStore)
            m_memoryStore->setSubresourceUniqueRedirectFrom(primarySubresourceDomain, primaryRedirectDomain);
    });
}

void WebResourceLoadStatisticsStore::setTopFrameUniqueRedirectTo(const URL& topFrameHostName, const URL& hostNameRedirectedTo)
{
    ASSERT(RunLoop::isMain());

    if (topFrameHostName.isBlankURL() || topFrameHostName.isEmpty() || hostNameRedirectedTo.isBlankURL() || hostNameRedirectedTo.isEmpty())
        return;
    
    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this), primaryRedirectDomain = isolatedPrimaryDomain(hostNameRedirectedTo), topFramePrimaryDomain = isolatedPrimaryDomain(topFrameHostName)] {
        if (m_memoryStore)
            m_memoryStore->setTopFrameUniqueRedirectTo(topFramePrimaryDomain, primaryRedirectDomain);
    });
}

void WebResourceLoadStatisticsStore::setTopFrameUniqueRedirectFrom(const URL& topFrameHostName, const URL& hostNameRedirectedFrom)
{
    ASSERT(RunLoop::isMain());

    if (topFrameHostName.isBlankURL() || topFrameHostName.isEmpty() || hostNameRedirectedFrom.isBlankURL() || hostNameRedirectedFrom.isEmpty())
        return;
    
    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this), primaryRedirectDomain = isolatedPrimaryDomain(hostNameRedirectedFrom), topFramePrimaryDomain = isolatedPrimaryDomain(topFrameHostName)] {
        if (m_memoryStore)
            m_memoryStore->setTopFrameUniqueRedirectFrom(topFramePrimaryDomain, primaryRedirectDomain);
    });
}

void WebResourceLoadStatisticsStore::scheduleCookiePartitioningUpdate(CompletionHandler<void()>&& completionHandler)
{
    // Helper function used by testing system. Should only be called from the main thread.
    ASSERT(RunLoop::isMain());

    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)] () mutable {
        if (!m_memoryStore) {
            RunLoop::main().dispatch([completionHandler = WTFMove(completionHandler)]() {
                completionHandler();
            });
            return;
        }
        m_memoryStore->updateCookiePartitioning([completionHandler = WTFMove(completionHandler)]() mutable {
            RunLoop::main().dispatch([completionHandler = WTFMove(completionHandler)]() {
                completionHandler();
            });
        });
    });
}

void WebResourceLoadStatisticsStore::scheduleCookiePartitioningUpdateForDomains(const Vector<String>& domainsToPartition, const Vector<String>& domainsToBlock, const Vector<String>& domainsToNeitherPartitionNorBlock, ShouldClearFirst shouldClearFirst, CompletionHandler<void()>&& completionHandler)
{
    // Helper function used by testing system. Should only be called from the main thread.
    ASSERT(RunLoop::isMain());
    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this), domainsToPartition = crossThreadCopy(domainsToPartition), domainsToBlock = crossThreadCopy(domainsToBlock), domainsToNeitherPartitionNorBlock = crossThreadCopy(domainsToNeitherPartitionNorBlock), shouldClearFirst, completionHandler = WTFMove(completionHandler)] () mutable {
        if (!m_memoryStore) {
            RunLoop::main().dispatch([completionHandler = WTFMove(completionHandler)]() {
                completionHandler();
            });
            return;
        }

        m_memoryStore->updateCookiePartitioningForDomains(domainsToPartition, domainsToBlock, domainsToNeitherPartitionNorBlock, shouldClearFirst, [completionHandler = WTFMove(completionHandler)]() mutable {
            RunLoop::main().dispatch([completionHandler = WTFMove(completionHandler)]() {
                completionHandler();
            });
        });
    });
}

void WebResourceLoadStatisticsStore::scheduleClearPartitioningStateForDomains(const Vector<String>& domains, CompletionHandler<void()>&& completionHandler)
{
    // Helper function used by testing system. Should only be called from the main thread.
    ASSERT(RunLoop::isMain());
    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this), domains = crossThreadCopy(domains), completionHandler = WTFMove(completionHandler)] () mutable {
        if (!m_memoryStore) {
            RunLoop::main().dispatch([completionHandler = WTFMove(completionHandler)]() {
                completionHandler();
            });
            return;
        }

        m_memoryStore->clearPartitioningStateForDomains(domains, [completionHandler = WTFMove(completionHandler)]() mutable {
            RunLoop::main().dispatch([completionHandler = WTFMove(completionHandler)]() {
                completionHandler();
            });
        });
    });
}

#if HAVE(CFNETWORK_STORAGE_PARTITIONING)
void WebResourceLoadStatisticsStore::scheduleCookiePartitioningStateReset()
{
    ASSERT(RunLoop::isMain());
    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this)] {
        if (m_memoryStore)
            m_memoryStore->resetCookiePartitioningState();
    });
}
#endif

void WebResourceLoadStatisticsStore::scheduleClearInMemory()
{
    ASSERT(RunLoop::isMain());
    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this)] {
        if (m_memoryStore)
            m_memoryStore->clear();
    });
}

void WebResourceLoadStatisticsStore::scheduleClearInMemoryAndPersistent(ShouldGrandfather shouldGrandfather, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this), shouldGrandfather, completionHandler = WTFMove(completionHandler)] () mutable {
        if (m_memoryStore)
            m_memoryStore->clear();
        if (m_persistentStorage)
            m_persistentStorage->clear();
        
        CompletionHandler<void()> callCompletionHandlerOnMainThread = [completionHandler = WTFMove(completionHandler)]() mutable {
            RunLoop::main().dispatch([completionHandler = WTFMove(completionHandler)] {
                completionHandler();
            });
        };

        if (shouldGrandfather == ShouldGrandfather::Yes && m_memoryStore)
            m_memoryStore->grandfatherExistingWebsiteData(WTFMove(callCompletionHandlerOnMainThread));
        else
            callCompletionHandlerOnMainThread();
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
    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this), seconds] {
        if (m_memoryStore)
            m_memoryStore->setTimeToLiveUserInteraction(seconds);
    });
}

void WebResourceLoadStatisticsStore::setTimeToLiveCookiePartitionFree(Seconds seconds)
{
    ASSERT(RunLoop::isMain());
    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this), seconds] {
        if (m_memoryStore)
            m_memoryStore->setTimeToLiveCookiePartitionFree(seconds);
    });
}

void WebResourceLoadStatisticsStore::setMinimumTimeBetweenDataRecordsRemoval(Seconds seconds)
{
    ASSERT(RunLoop::isMain());
    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this), seconds] {
        if (m_memoryStore)
            m_memoryStore->setMinimumTimeBetweenDataRecordsRemoval(seconds);
    });
}

void WebResourceLoadStatisticsStore::setGrandfatheringTime(Seconds seconds)
{
    ASSERT(RunLoop::isMain());
    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this), seconds] {
        if (m_memoryStore)
            m_memoryStore->setGrandfatheringTime(seconds);
    });
}

void WebResourceLoadStatisticsStore::callUpdatePrevalentDomainsToPartitionOrBlockCookiesHandler(const Vector<String>& domainsToPartition, const Vector<String>& domainsToBlock, const Vector<String>& domainsToNeitherPartitionNorBlock, ShouldClearFirst shouldClearFirst, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    m_updatePrevalentDomainsToPartitionOrBlockCookiesHandler(domainsToPartition, domainsToBlock, domainsToNeitherPartitionNorBlock, shouldClearFirst, WTFMove(completionHandler));
}

void WebResourceLoadStatisticsStore::callRemoveDomainsHandler(const Vector<String>& domains)
{
    ASSERT(RunLoop::isMain());

    m_removeDomainsHandler(domains);
}
    
void WebResourceLoadStatisticsStore::setMaxStatisticsEntries(size_t maximumEntryCount)
{
    ASSERT(RunLoop::isMain());
    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this), maximumEntryCount] {
        if (m_memoryStore)
            m_memoryStore->setMaxStatisticsEntries(maximumEntryCount);
    });
}
    
void WebResourceLoadStatisticsStore::setPruneEntriesDownTo(size_t pruneTargetCount)
{
    ASSERT(RunLoop::isMain());

    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this), pruneTargetCount] {
        if (m_memoryStore)
            m_memoryStore->setPruneEntriesDownTo(pruneTargetCount);
    });
}

void WebResourceLoadStatisticsStore::resetParametersToDefaultValues()
{
    ASSERT(RunLoop::isMain());

    m_statisticsQueue->dispatch([this, protectedThis(makeRef(*this))] {
        if (m_memoryStore)
            m_memoryStore->resetParametersToDefaultValues();
    });
}

void WebResourceLoadStatisticsStore::logTestingEvent(const String& event)
{
    ASSERT(RunLoop::isMain());

    if (m_statisticsTestingCallback)
        m_statisticsTestingCallback(event);
}

} // namespace WebKit

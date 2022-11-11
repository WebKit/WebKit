/*
 * Copyright (C) 2016-2020 Apple Inc. All rights reserved.
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

#if ENABLE(TRACKING_PREVENTION)

#include "APIDictionary.h"
#include "Logging.h"
#include "NetworkProcess.h"
#include "NetworkProcessProxyMessages.h"
#include "NetworkSession.h"
#include "PrivateClickMeasurementManager.h"
#include "ResourceLoadStatisticsDatabaseStore.h"
#include "ShouldGrandfatherStatistics.h"
#include "StorageAccessStatus.h"
#include "WebFrameProxy.h"
#include "WebPageProxy.h"
#include "WebProcessMessages.h"
#include "WebProcessPool.h"
#include "WebProcessProxy.h"
#include "WebsiteDataFetchOption.h"
#include <WebCore/CookieJar.h>
#include <WebCore/DiagnosticLoggingClient.h>
#include <WebCore/DiagnosticLoggingKeys.h>
#include <WebCore/NetworkStorageSession.h>
#include <WebCore/ResourceLoadStatistics.h>
#include <WebCore/SQLiteDatabase.h>
#include <WebCore/SQLiteFileSystem.h>
#include <WebCore/SQLiteStatement.h>
#include <wtf/CallbackAggregator.h>
#include <wtf/CrossThreadCopier.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/SuspendableWorkQueue.h>
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
        WebsiteDataType::SearchFieldRecentSearches,
        WebsiteDataType::SessionStorage,
#if ENABLE(SERVICE_WORKER)
        WebsiteDataType::ServiceWorkerRegistrations,
#endif
        WebsiteDataType::FileSystem,
    }));

    ASSERT(RunLoop::isMain());

    return dataTypes;
}

void WebResourceLoadStatisticsStore::setNotifyPagesWhenDataRecordsWereScanned(bool value)
{
    ASSERT(RunLoop::isMain());

    if (isEphemeral())
        return;

    postTask([this, value] {
        if (m_statisticsStore)
            m_statisticsStore->setNotifyPagesWhenDataRecordsWereScanned(value);
    });
}

void WebResourceLoadStatisticsStore::setNotifyPagesWhenDataRecordsWereScanned(bool value, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    if (isEphemeral()) {
        completionHandler();
        return;
    }

    postTask([this, value, completionHandler = WTFMove(completionHandler)]() mutable {
        if (m_statisticsStore)
            m_statisticsStore->setNotifyPagesWhenDataRecordsWereScanned(value);

        postTaskReply(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::setIsRunningTest(bool value, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    if (isEphemeral()) {
        completionHandler();
        return;
    }

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

static Ref<SuspendableWorkQueue> sharedStatisticsQueue()
{
    static NeverDestroyed<Ref<SuspendableWorkQueue>> queue(SuspendableWorkQueue::create("WebResourceLoadStatisticsStore Process Data Queue",  WorkQueue::QOS::Utility));
    return queue.get().copyRef();
}

WebResourceLoadStatisticsStore::WebResourceLoadStatisticsStore(NetworkSession& networkSession, const String& resourceLoadStatisticsDirectory, ShouldIncludeLocalhost shouldIncludeLocalhost, ResourceLoadStatistics::IsEphemeral isEphemeral)
    : m_networkSession(networkSession)
    , m_statisticsQueue(sharedStatisticsQueue())
    , m_dailyTasksTimer(RunLoop::main(), this, &WebResourceLoadStatisticsStore::performDailyTasks)
    , m_isEphemeral(isEphemeral)
{
    RELEASE_ASSERT(RunLoop::isMain());

    // No daily tasks needed for ephemeral sessions since no resource load statistics are collected.
    if (isEphemeral == ResourceLoadStatistics::IsEphemeral::Yes)
        return;

    if (!resourceLoadStatisticsDirectory.isEmpty()) {
        postTask([this, resourceLoadStatisticsDirectory = resourceLoadStatisticsDirectory.isolatedCopy(), shouldIncludeLocalhost, sessionID = networkSession.sessionID()] {
            m_statisticsStore = makeUnique<ResourceLoadStatisticsDatabaseStore>(*this, m_statisticsQueue, shouldIncludeLocalhost, resourceLoadStatisticsDirectory, sessionID);

            auto legacyPlistFilePath = FileSystem::pathByAppendingComponent(resourceLoadStatisticsDirectory, "full_browsing_session_resourceLog.plist"_s);
            if (FileSystem::fileExists(legacyPlistFilePath))
                FileSystem::deleteFile(legacyPlistFilePath);

            m_statisticsStore->didCreateNetworkProcess();
        });

        m_dailyTasksTimer.startRepeating(24_h);
    }
}

WebResourceLoadStatisticsStore::~WebResourceLoadStatisticsStore()
{
    RELEASE_ASSERT(RunLoop::isMain());
    RELEASE_ASSERT(!m_statisticsStore);
}

Ref<WebResourceLoadStatisticsStore> WebResourceLoadStatisticsStore::create(NetworkSession& networkSession, const String& resourceLoadStatisticsDirectory, ShouldIncludeLocalhost shouldIncludeLocalhost, WebCore::ResourceLoadStatistics::IsEphemeral isEphemeral)
{
    return adoptRef(*new WebResourceLoadStatisticsStore(networkSession, resourceLoadStatisticsDirectory, shouldIncludeLocalhost, isEphemeral));
}

void WebResourceLoadStatisticsStore::didDestroyNetworkSession(CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    auto callbackAggregator = CallbackAggregator::create([completionHandler = WTFMove(completionHandler)] () mutable {
        completionHandler();
    });

    m_networkSession = nullptr;
    destroyResourceLoadStatisticsStore([callbackAggregator] { });
}

inline void WebResourceLoadStatisticsStore::postTask(WTF::Function<void()>&& task)
{
    // Resource load statistics should not be captured for ephemeral sessions.
    RELEASE_ASSERT(!isEphemeral());

    ASSERT(RunLoop::isMain());
    m_statisticsQueue->dispatch([protectedThis = Ref { *this }, task = WTFMove(task)] {
        task();
    });
}

inline void WebResourceLoadStatisticsStore::postTaskReply(WTF::Function<void()>&& reply)
{
    ASSERT(!RunLoop::isMain());
    RunLoop::main().dispatch(WTFMove(reply));
}

void WebResourceLoadStatisticsStore::destroyResourceLoadStatisticsStore(CompletionHandler<void()>&& completionHandler)
{
    RELEASE_ASSERT(RunLoop::isMain());

    if (isEphemeral()) {
        completionHandler();
        return;
    }

    postTask([this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)]() mutable {
        m_statisticsStore = nullptr;
        postTaskReply(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::populateMemoryStoreFromDisk(CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    postTask([this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)]() mutable {
        if (m_statisticsStore && is<ResourceLoadStatisticsDatabaseStore>(*m_statisticsStore)) {
            auto& databaseStore = downcast<ResourceLoadStatisticsDatabaseStore>(*m_statisticsStore);
            if (databaseStore.isNewResourceLoadStatisticsDatabaseFile()) {
                m_statisticsStore->grandfatherExistingWebsiteData([protectedThis = WTFMove(protectedThis), completionHandler = WTFMove(completionHandler)]() mutable {
                    postTaskReply(WTFMove(completionHandler));
                });
                databaseStore.setIsNewResourceLoadStatisticsDatabaseFile(false);
            } else
                postTaskReply([this, protectedThis = WTFMove(protectedThis), completionHandler = WTFMove(completionHandler)]() mutable {
                    logTestingEvent("PopulatedWithoutGrandfathering"_s);
                    completionHandler();
                });
        } else
            postTaskReply(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::setResourceLoadStatisticsDebugMode(bool value, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    if (isEphemeral()) {
        completionHandler();
        return;
    }

    if (m_networkSession) {
        if (auto* storageSession = m_networkSession->networkStorageSession())
            storageSession->setTrackingPreventionDebugLoggingEnabled(value);
    }

    postTask([this, value, completionHandler = WTFMove(completionHandler)]() mutable {
        if (m_statisticsStore)
            m_statisticsStore->setResourceLoadStatisticsDebugMode(value);
        postTaskReply(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::setPrevalentResourceForDebugMode(RegistrableDomain&& domain, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    if (isEphemeral()) {
        completionHandler();
        return;
    }

    postTask([this, domain = WTFMove(domain).isolatedCopy(), completionHandler = WTFMove(completionHandler)]() mutable {
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

void WebResourceLoadStatisticsStore::statisticsDatabaseHasAllTables(CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    postTask([this, completionHandler = WTFMove(completionHandler)]() mutable { 
        if (!m_statisticsStore || !is<ResourceLoadStatisticsDatabaseStore>(*m_statisticsStore)) {
            completionHandler(false);
            ASSERT_NOT_REACHED();
            return;
        }
        auto missingTables = downcast<ResourceLoadStatisticsDatabaseStore>(*m_statisticsStore).checkForMissingTablesInSchema();
        postTaskReply([hasAllTables = missingTables ? false : true, completionHandler = WTFMove(completionHandler)] () mutable {
            completionHandler(hasAllTables);
        });
    });
}

void WebResourceLoadStatisticsStore::resourceLoadStatisticsUpdated(Vector<ResourceLoadStatistics>&& statistics, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    // It is safe to move the origins to the background queue without isolated copy here because this is an r-value
    // coming from IPC. ResourceLoadStatistics only contains strings which are safe to move to other threads as long
    // as nobody on this thread holds a reference to those strings.
    postTask([this, protectedThis = Ref { *this }, statistics = WTFMove(statistics), completionHandler = WTFMove(completionHandler)]() mutable {
        if (!m_statisticsStore) {
            postTaskReply(WTFMove(completionHandler));
            return;
        }

        m_statisticsStore->mergeStatistics(WTFMove(statistics));
        postTaskReply(WTFMove(completionHandler));
        // We can cancel any pending request to process statistics since we're doing it synchronously below.
        m_statisticsStore->cancelPendingStatisticsProcessingRequest();

        // Fire before processing statistics to propagate user interaction as fast as possible to the network process.
        m_statisticsStore->updateCookieBlocking([this, protectedThis]() {
            postTaskReply([this, protectedThis = protectedThis]() {
                logTestingEvent("Statistics Updated"_s);
            });
        });
        m_statisticsStore->processStatisticsAndDataRecords();
    });
}

void WebResourceLoadStatisticsStore::hasStorageAccess(RegistrableDomain&& subFrameDomain, RegistrableDomain&& topFrameDomain, std::optional<FrameIdentifier> frameID, PageIdentifier pageID, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(subFrameDomain != topFrameDomain);
    ASSERT(RunLoop::isMain());

    if (isEphemeral())
        return hasStorageAccessEphemeral(WTFMove(subFrameDomain), WTFMove(topFrameDomain), frameID, pageID, WTFMove(completionHandler));

    postTask([this, subFrameDomain = WTFMove(subFrameDomain).isolatedCopy(), topFrameDomain = WTFMove(topFrameDomain).isolatedCopy(), frameID, pageID, completionHandler = WTFMove(completionHandler)]() mutable {
        if (!m_statisticsStore) {
            postTaskReply([completionHandler = WTFMove(completionHandler)]() mutable {
                completionHandler(false);
            });
            return;
        }

        m_statisticsStore->hasStorageAccess(WTFMove(subFrameDomain), WTFMove(topFrameDomain), frameID, pageID, [completionHandler = WTFMove(completionHandler)](bool hasStorageAccess) mutable {
            postTaskReply([completionHandler = WTFMove(completionHandler), hasStorageAccess]() mutable {
                completionHandler(hasStorageAccess);
            });
        });
    });
}

void WebResourceLoadStatisticsStore::hasStorageAccessEphemeral(const RegistrableDomain& subFrameDomain, const RegistrableDomain& topFrameDomain, std::optional<FrameIdentifier> frameID, PageIdentifier pageID, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(isEphemeral());

    if (m_networkSession) {
        if (auto* storageSession = m_networkSession->networkStorageSession()) {
            completionHandler(storageSession->hasStorageAccess(subFrameDomain, topFrameDomain, frameID, pageID));
            return;
        }
    }
    completionHandler(false);
}

bool WebResourceLoadStatisticsStore::hasStorageAccessForFrame(const RegistrableDomain& resourceDomain, const RegistrableDomain& firstPartyDomain, FrameIdentifier frameID, PageIdentifier pageID)
{
    ASSERT(RunLoop::isMain());

    if (!m_networkSession)
        return false;

    if (auto* storageSession = m_networkSession->networkStorageSession())
        return storageSession->hasStorageAccess(resourceDomain, firstPartyDomain, frameID, pageID);

    return false;
}

void WebResourceLoadStatisticsStore::callHasStorageAccessForFrameHandler(const RegistrableDomain& resourceDomain, const RegistrableDomain& firstPartyDomain, FrameIdentifier frameID, PageIdentifier pageID, CompletionHandler<void(bool hasAccess)>&& callback)
{
    ASSERT(RunLoop::isMain());

    if (m_networkSession) {
        if (auto* storageSession = m_networkSession->networkStorageSession()) {
            callback(storageSession->hasStorageAccess(resourceDomain, firstPartyDomain, frameID, pageID));
            return;
        }
    }

    callback(false);
}

void WebResourceLoadStatisticsStore::requestStorageAccess(RegistrableDomain&& subFrameDomain, RegistrableDomain&& topFrameDomain, FrameIdentifier frameID, PageIdentifier webPageID, WebPageProxyIdentifier webPageProxyID, StorageAccessScope scope, CompletionHandler<void(RequestStorageAccessResult)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    if (subFrameDomain == topFrameDomain) {
        completionHandler({ StorageAccessWasGranted::Yes, StorageAccessPromptWasShown::No, scope, WTFMove(topFrameDomain), WTFMove(subFrameDomain) });
        return;
    }
    
    if (isEphemeral())
        return requestStorageAccessEphemeral(WTFMove(subFrameDomain), WTFMove(topFrameDomain), frameID, webPageID, webPageProxyID, scope, WTFMove(completionHandler));

    auto statusHandler = [this, protectedThis = Ref { *this }, subFrameDomain = subFrameDomain.isolatedCopy(), topFrameDomain = topFrameDomain.isolatedCopy(), frameID, webPageID, webPageProxyID, scope, completionHandler = WTFMove(completionHandler)](StorageAccessStatus status) mutable {
        switch (status) {
        case StorageAccessStatus::CannotRequestAccess:
            completionHandler({ StorageAccessWasGranted::No, StorageAccessPromptWasShown::No, scope, topFrameDomain, subFrameDomain });
            return;
        case StorageAccessStatus::RequiresUserPrompt:
            {
            if (!m_networkSession)
                return completionHandler({ StorageAccessWasGranted::No, StorageAccessPromptWasShown::No, scope, topFrameDomain, subFrameDomain });

            CompletionHandler<void(bool)> requestConfirmationCompletionHandler = [this, protectedThis, subFrameDomain, topFrameDomain, frameID, webPageID, scope, completionHandler = WTFMove(completionHandler)] (bool userDidGrantAccess) mutable {
                if (userDidGrantAccess)
                    grantStorageAccess(WTFMove(subFrameDomain), WTFMove(topFrameDomain), frameID, webPageID, StorageAccessPromptWasShown::Yes, scope, WTFMove(completionHandler));
                else
                    completionHandler({ StorageAccessWasGranted::No, StorageAccessPromptWasShown::Yes, scope, topFrameDomain, subFrameDomain });
            };
            m_networkSession->networkProcess().parentProcessConnection()->sendWithAsyncReply(Messages::NetworkProcessProxy::RequestStorageAccessConfirm(webPageProxyID, frameID, subFrameDomain, topFrameDomain), WTFMove(requestConfirmationCompletionHandler));
            }
            return;
        case StorageAccessStatus::HasAccess:
            completionHandler({ StorageAccessWasGranted::Yes, StorageAccessPromptWasShown::No, scope, topFrameDomain, subFrameDomain });
            return;
        }
    };

    postTask([this, subFrameDomain = WTFMove(subFrameDomain).isolatedCopy(), topFrameDomain = WTFMove(topFrameDomain).isolatedCopy(), frameID, webPageID, scope, statusHandler = WTFMove(statusHandler)]() mutable {
        if (!m_statisticsStore) {
            postTaskReply([statusHandler = WTFMove(statusHandler)]() mutable {
                statusHandler(StorageAccessStatus::CannotRequestAccess);
            });
            return;
        }

        m_statisticsStore->requestStorageAccess(WTFMove(subFrameDomain), WTFMove(topFrameDomain), frameID, webPageID, scope, [statusHandler = WTFMove(statusHandler)](StorageAccessStatus status) mutable {
            postTaskReply([statusHandler = WTFMove(statusHandler), status]() mutable {
                statusHandler(status);
            });
        });
    });
}

void WebResourceLoadStatisticsStore::requestStorageAccessEphemeral(const RegistrableDomain& subFrameDomain, const RegistrableDomain& topFrameDomain, FrameIdentifier frameID, PageIdentifier webPageID, WebPageProxyIdentifier webPageProxyID, StorageAccessScope scope, CompletionHandler<void(RequestStorageAccessResult)>&& completionHandler)
{
    ASSERT(isEphemeral());

    if (!m_networkSession || (!m_domainsWithEphemeralUserInteraction.contains(subFrameDomain) && !NetworkStorageSession::canRequestStorageAccessForLoginOrCompatibilityPurposesWithoutPriorUserInteraction(subFrameDomain, topFrameDomain)))
        return completionHandler({ StorageAccessWasGranted::No, StorageAccessPromptWasShown::No, scope, topFrameDomain, subFrameDomain });

    CompletionHandler<void(bool)> requestConfirmationCompletionHandler = [this, protectedThis = Ref { *this }, subFrameDomain, topFrameDomain, frameID, webPageID, scope, completionHandler = WTFMove(completionHandler)] (bool userDidGrantAccess) mutable {
        if (userDidGrantAccess)
            grantStorageAccessEphemeral(subFrameDomain, topFrameDomain, frameID, webPageID, StorageAccessPromptWasShown::Yes, scope, WTFMove(completionHandler));
        else
            completionHandler({ StorageAccessWasGranted::No, StorageAccessPromptWasShown::Yes, scope, topFrameDomain, subFrameDomain });
    };

    m_networkSession->networkProcess().parentProcessConnection()->sendWithAsyncReply(Messages::NetworkProcessProxy::RequestStorageAccessConfirm(webPageProxyID, frameID, subFrameDomain, topFrameDomain), WTFMove(requestConfirmationCompletionHandler));
}

void WebResourceLoadStatisticsStore::requestStorageAccessUnderOpener(RegistrableDomain&& domainInNeedOfStorageAccess, PageIdentifier openerPageID, RegistrableDomain&& openerDomain)
{
    ASSERT(RunLoop::isMain());

    if (isEphemeral())
        return requestStorageAccessUnderOpenerEphemeral(WTFMove(domainInNeedOfStorageAccess), openerPageID, WTFMove(openerDomain));

    // It is safe to move the strings to the background queue without isolated copy here because they are r-value references
    // coming from IPC. Strings which are safe to move to other threads as long as nobody on this thread holds a reference
    // to those strings.
    postTask([this, domainInNeedOfStorageAccess = WTFMove(domainInNeedOfStorageAccess), openerPageID, openerDomain = WTFMove(openerDomain)]() mutable {
        if (m_statisticsStore)
            m_statisticsStore->requestStorageAccessUnderOpener(WTFMove(domainInNeedOfStorageAccess), openerPageID, WTFMove(openerDomain));
    });
}

void WebResourceLoadStatisticsStore::requestStorageAccessUnderOpenerEphemeral(RegistrableDomain&& domainInNeedOfStorageAccess, PageIdentifier openerPageID, RegistrableDomain&& openerDomain)
{
    ASSERT(isEphemeral());

    if (m_networkSession) {
        if (auto* storageSession = m_networkSession->networkStorageSession())
            storageSession->grantStorageAccess(WTFMove(domainInNeedOfStorageAccess), WTFMove(openerDomain), std::nullopt, openerPageID);
    }
}

void WebResourceLoadStatisticsStore::grantStorageAccess(RegistrableDomain&& subFrameDomain, RegistrableDomain&& topFrameDomain, FrameIdentifier frameID, PageIdentifier pageID, StorageAccessPromptWasShown promptWasShown, StorageAccessScope scope, CompletionHandler<void(RequestStorageAccessResult)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    postTask([this, subFrameDomain = WTFMove(subFrameDomain).isolatedCopy(), topFrameDomain = WTFMove(topFrameDomain).isolatedCopy(), frameID, pageID, promptWasShown, scope, completionHandler = WTFMove(completionHandler)]() mutable {
        if (!m_statisticsStore) {
            postTaskReply([subFrameDomain = WTFMove(subFrameDomain).isolatedCopy(), topFrameDomain = WTFMove(topFrameDomain).isolatedCopy(), promptWasShown, scope, completionHandler = WTFMove(completionHandler)]() mutable {
                completionHandler({ StorageAccessWasGranted::No, promptWasShown, scope, topFrameDomain, subFrameDomain });
            });
            return;
        }

        m_statisticsStore->grantStorageAccess(WTFMove(subFrameDomain), WTFMove(topFrameDomain), frameID, pageID, promptWasShown, scope, [subFrameDomain = subFrameDomain.isolatedCopy(), topFrameDomain = topFrameDomain.isolatedCopy(), promptWasShown, scope, completionHandler = WTFMove(completionHandler)](StorageAccessWasGranted wasGrantedAccess) mutable {
            postTaskReply([subFrameDomain = WTFMove(subFrameDomain).isolatedCopy(), topFrameDomain = WTFMove(topFrameDomain).isolatedCopy(), wasGrantedAccess, promptWasShown, scope, completionHandler = WTFMove(completionHandler)]() mutable {
                completionHandler({ wasGrantedAccess, promptWasShown, scope, topFrameDomain, subFrameDomain });
            });
        });
    });
}

void WebResourceLoadStatisticsStore::grantStorageAccessEphemeral(const RegistrableDomain& subFrameDomain, const RegistrableDomain& topFrameDomain, FrameIdentifier frameID, PageIdentifier pageID, StorageAccessPromptWasShown promptWasShown, StorageAccessScope scope, CompletionHandler<void(RequestStorageAccessResult)>&& completionHandler)
{
    ASSERT(isEphemeral());

    if (m_networkSession) {
        if (auto* storageSession = m_networkSession->networkStorageSession()) {
            storageSession->grantStorageAccess(subFrameDomain, topFrameDomain, frameID, pageID);
            completionHandler({ StorageAccessWasGranted::Yes, promptWasShown, scope, topFrameDomain, subFrameDomain });
            return;
        }
    }
    completionHandler({ StorageAccessWasGranted::No, promptWasShown, scope, topFrameDomain, subFrameDomain });
}

StorageAccessWasGranted WebResourceLoadStatisticsStore::grantStorageAccessInStorageSession(const RegistrableDomain& resourceDomain, const RegistrableDomain& firstPartyDomain, std::optional<FrameIdentifier> frameID, PageIdentifier pageID, StorageAccessScope scope)
{
    ASSERT(RunLoop::isMain());

    bool isStorageGranted = false;

    if (m_networkSession) {
        if (auto* storageSession = m_networkSession->networkStorageSession()) {
            storageSession->grantStorageAccess(resourceDomain, firstPartyDomain, (scope == StorageAccessScope::PerFrame ? frameID : std::nullopt), pageID);
            ASSERT(storageSession->hasStorageAccess(resourceDomain, firstPartyDomain, frameID, pageID));
            isStorageGranted = true;
        }
    }

    return isStorageGranted ? StorageAccessWasGranted::Yes : StorageAccessWasGranted::No;
}

void WebResourceLoadStatisticsStore::callGrantStorageAccessHandler(const RegistrableDomain& subFrameDomain, const RegistrableDomain& topFrameDomain, std::optional<FrameIdentifier> frameID, PageIdentifier pageID, StorageAccessScope scope, CompletionHandler<void(StorageAccessWasGranted)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    completionHandler(grantStorageAccessInStorageSession(subFrameDomain, topFrameDomain, frameID, pageID, scope));
}

void WebResourceLoadStatisticsStore::hasCookies(const RegistrableDomain& domain, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    if (m_networkSession) {
        if (auto* storageSession = m_networkSession->networkStorageSession()) {
            storageSession->hasCookies(domain, WTFMove(completionHandler));
            return;
        }
    }
    
    completionHandler(false);
}

void WebResourceLoadStatisticsStore::setThirdPartyCookieBlockingMode(ThirdPartyCookieBlockingMode blockingMode)
{
    ASSERT(RunLoop::isMain());

    if (m_networkSession) {
        if (auto* storageSession = m_networkSession->networkStorageSession())
            storageSession->setThirdPartyCookieBlockingMode(blockingMode);
        else
            ASSERT_NOT_REACHED();
    }

    if (isEphemeral())
        return;

    postTask([this, blockingMode]() {
        if (!m_statisticsStore)
            return;

        m_statisticsStore->setThirdPartyCookieBlockingMode(blockingMode);
    });
}

void WebResourceLoadStatisticsStore::setSameSiteStrictEnforcementEnabled(SameSiteStrictEnforcementEnabled enabled)
{
    ASSERT(RunLoop::isMain());

    if (isEphemeral())
        return;

    postTask([this, enabled]() {
        if (!m_statisticsStore)
            return;

        m_statisticsStore->setSameSiteStrictEnforcementEnabled(enabled);
    });
}

void WebResourceLoadStatisticsStore::setFirstPartyWebsiteDataRemovalMode(FirstPartyWebsiteDataRemovalMode mode, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    if (isEphemeral()) {
        completionHandler();
        return;
    }

    postTask([this, mode, completionHandler = WTFMove(completionHandler)]() mutable {
        if (m_statisticsStore) {
            m_statisticsStore->setFirstPartyWebsiteDataRemovalMode(mode);
            if (mode == FirstPartyWebsiteDataRemovalMode::AllButCookiesReproTestingTimeout)
                m_statisticsStore->setIsRunningTest(true);
        }
        postTaskReply([completionHandler = WTFMove(completionHandler)]() mutable {
            completionHandler();
        });
    });
}

void WebResourceLoadStatisticsStore::setStandaloneApplicationDomain(const RegistrableDomain& domain, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    if (isEphemeral() || domain.isEmpty()) {
        completionHandler();
        return;
    }

    RELEASE_LOG(ResourceLoadStatistics, "WebResourceLoadStatisticsStore::setStandaloneApplicationDomain() called with non-empty domain.");

    postTask([this, domain = domain.isolatedCopy(), completionHandler = WTFMove(completionHandler)]() mutable {
        if (m_statisticsStore)
            m_statisticsStore->setStandaloneApplicationDomain(WTFMove(domain));
        postTaskReply([completionHandler = WTFMove(completionHandler)]() mutable {
            completionHandler();
        });
    });
}

#if ENABLE(APP_BOUND_DOMAINS)
void WebResourceLoadStatisticsStore::setAppBoundDomains(HashSet<RegistrableDomain>&& domains, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    if (isEphemeral() || domains.isEmpty()) {
        completionHandler();
        return;
    }

    auto domainsCopy = crossThreadCopy(domains);

    if (m_networkSession) {
        if (auto* storageSession = m_networkSession->networkStorageSession()) {
            storageSession->setAppBoundDomains(WTFMove(domains));
            storageSession->setThirdPartyCookieBlockingMode(ThirdPartyCookieBlockingMode::AllExceptBetweenAppBoundDomains);
        }
    }

    postTask([this, domains = WTFMove(domainsCopy), completionHandler = WTFMove(completionHandler)]() mutable {
        if (m_statisticsStore) {
            m_statisticsStore->setAppBoundDomains(WTFMove(domains));
            m_statisticsStore->setThirdPartyCookieBlockingMode(ThirdPartyCookieBlockingMode::AllExceptBetweenAppBoundDomains);
        }
        postTaskReply(WTFMove(completionHandler));
    });
}
#endif

#if ENABLE(MANAGED_DOMAINS)
void WebResourceLoadStatisticsStore::setManagedDomains(HashSet<RegistrableDomain>&& domains, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    if (isEphemeral() || domains.isEmpty()) {
        completionHandler();
        return;
    }

    auto domainsCopy = crossThreadCopy(domains);

    if (m_networkSession) {
        if (auto* storageSession = m_networkSession->networkStorageSession()) {
            storageSession->setManagedDomains(WTFMove(domains));
            storageSession->setThirdPartyCookieBlockingMode(ThirdPartyCookieBlockingMode::AllExceptManagedDomains);
        }
    }

    postTask([this, domains = WTFMove(domainsCopy), completionHandler = WTFMove(completionHandler)]() mutable {
        if (m_statisticsStore) {
            m_statisticsStore->setManagedDomains(WTFMove(domains));
            m_statisticsStore->setThirdPartyCookieBlockingMode(ThirdPartyCookieBlockingMode::AllExceptManagedDomains);
        }
        postTaskReply(WTFMove(completionHandler));
    });
}
#endif

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

    if (m_networkSession) {
        if (auto* storageSession = m_networkSession->networkStorageSession())
            storageSession->removeAllStorageAccess();
    }

    completionHandler();
}

void WebResourceLoadStatisticsStore::performDailyTasks()
{
    ASSERT(RunLoop::isMain());

    postTask([this] {
        if (m_statisticsStore) {
            m_statisticsStore->includeTodayAsOperatingDateIfNecessary();
            if (is<ResourceLoadStatisticsDatabaseStore>(*m_statisticsStore))
                downcast<ResourceLoadStatisticsDatabaseStore>(*m_statisticsStore).runIncrementalVacuumCommand();
        }
    });
}

void WebResourceLoadStatisticsStore::logFrameNavigation(RegistrableDomain&& targetDomain, RegistrableDomain&& topFrameDomain, RegistrableDomain&& sourceDomain, bool isRedirect, bool isMainFrame, Seconds delayAfterMainFrameDocumentLoad, bool wasPotentiallyInitiatedByUser)
{
    ASSERT(RunLoop::isMain());

    postTask([this, targetDomain = WTFMove(targetDomain).isolatedCopy(), topFrameDomain = WTFMove(topFrameDomain).isolatedCopy(), sourceDomain = WTFMove(sourceDomain).isolatedCopy(), isRedirect, isMainFrame, delayAfterMainFrameDocumentLoad, wasPotentiallyInitiatedByUser] {
        if (m_statisticsStore)
            m_statisticsStore->logFrameNavigation(targetDomain, topFrameDomain, sourceDomain, isRedirect, isMainFrame, delayAfterMainFrameDocumentLoad, wasPotentiallyInitiatedByUser);
    });
}

void WebResourceLoadStatisticsStore::logUserInteraction(RegistrableDomain&& domain, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    // User interactions need to be logged for ephemeral sessions to support the Storage Access API.
    if (isEphemeral())
        return logUserInteractionEphemeral(WTFMove(domain), WTFMove(completionHandler));

    postTask([this, domain = WTFMove(domain).isolatedCopy(), completionHandler = WTFMove(completionHandler)]() mutable {
        auto innerCompletionHandler = [completionHandler = WTFMove(completionHandler)]() mutable {
            postTaskReply(WTFMove(completionHandler));
        };
        if (m_statisticsStore) {
            m_statisticsStore->logUserInteraction(domain, WTFMove(innerCompletionHandler));
            return;
        }
        innerCompletionHandler();
    });
}

void WebResourceLoadStatisticsStore::logUserInteractionEphemeral(const RegistrableDomain& domain, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(isEphemeral());

    m_domainsWithEphemeralUserInteraction.add(domain);
    completionHandler();
}

void WebResourceLoadStatisticsStore::logCrossSiteLoadWithLinkDecoration(RegistrableDomain&& fromDomain, RegistrableDomain&& toDomain, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    ASSERT(fromDomain != toDomain);
    
    postTask([this, fromDomain = WTFMove(fromDomain).isolatedCopy(), toDomain = WTFMove(toDomain).isolatedCopy(), completionHandler = WTFMove(completionHandler)]() mutable {
        if (m_statisticsStore)
            m_statisticsStore->logCrossSiteLoadWithLinkDecoration(fromDomain, toDomain);
        postTaskReply(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::clearUserInteraction(RegistrableDomain&& domain, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    if (isEphemeral())
        return clearUserInteractionEphemeral(domain, WTFMove(completionHandler));

    postTask([this, domain = WTFMove(domain).isolatedCopy(), completionHandler = WTFMove(completionHandler)]() mutable {
        auto innerCompletionHandler = [completionHandler = WTFMove(completionHandler)]() mutable {
            postTaskReply(WTFMove(completionHandler));
        };
        if (m_statisticsStore) {
            m_statisticsStore->clearUserInteraction(domain, WTFMove(innerCompletionHandler));
            return;
        }
        innerCompletionHandler();
    });
}

void WebResourceLoadStatisticsStore::setTimeAdvanceForTesting(Seconds time, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    postTask([this, time, completionHandler = WTFMove(completionHandler)]() mutable {
        if (m_statisticsStore)
            m_statisticsStore->setTimeAdvanceForTesting(time);
        postTaskReply(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::clearUserInteractionEphemeral(const RegistrableDomain& domain, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(isEphemeral());

    m_domainsWithEphemeralUserInteraction.remove(domain);
    completionHandler();
}

void WebResourceLoadStatisticsStore::hasHadUserInteraction(RegistrableDomain&& domain, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    if (isEphemeral())
        return hasHadUserInteractionEphemeral(domain, WTFMove(completionHandler));

    postTask([this, domain = WTFMove(domain).isolatedCopy(), completionHandler = WTFMove(completionHandler)]() mutable {
        bool hadUserInteraction = m_statisticsStore ? m_statisticsStore->hasHadUserInteraction(domain, OperatingDatesWindow::Long) : false;
        postTaskReply([hadUserInteraction, completionHandler = WTFMove(completionHandler)]() mutable {
            completionHandler(hadUserInteraction);
        });
    });
}

void WebResourceLoadStatisticsStore::hasHadUserInteractionEphemeral(const RegistrableDomain& domain, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(isEphemeral());

    completionHandler(m_domainsWithEphemeralUserInteraction.contains(domain));
}

void WebResourceLoadStatisticsStore::setLastSeen(RegistrableDomain&& domain, Seconds seconds, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    postTask([this, domain = WTFMove(domain).isolatedCopy(), seconds, completionHandler = WTFMove(completionHandler)]() mutable {
        if (m_statisticsStore)
            m_statisticsStore->setLastSeen(domain, seconds);
        postTaskReply(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::mergeStatisticForTesting(RegistrableDomain&& domain, RegistrableDomain&& topFrameDomain1, RegistrableDomain&& topFrameDomain2, Seconds lastSeen, bool hadUserInteraction, Seconds mostRecentUserInteraction, bool isGrandfathered, bool isPrevalent, bool isVeryPrevalent, unsigned dataRecordsRemoved, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    postTask([this, domain = WTFMove(domain).isolatedCopy(), topFrameDomain1 = WTFMove(topFrameDomain1).isolatedCopy(), topFrameDomain2 = WTFMove(topFrameDomain2).isolatedCopy(), lastSeen, hadUserInteraction, mostRecentUserInteraction, isGrandfathered, isPrevalent, isVeryPrevalent, dataRecordsRemoved, completionHandler = WTFMove(completionHandler)]() mutable {
        if (m_statisticsStore) {
            ResourceLoadStatistics statistic(domain);
            statistic.lastSeen = WallTime::fromRawSeconds(lastSeen.seconds());
            statistic.hadUserInteraction = hadUserInteraction;
            statistic.mostRecentUserInteractionTime = WallTime::fromRawSeconds(mostRecentUserInteraction.seconds());
            statistic.grandfathered = isGrandfathered;
            statistic.isPrevalentResource = isPrevalent;
            statistic.isVeryPrevalentResource = isVeryPrevalent;
            statistic.dataRecordsRemoved = dataRecordsRemoved;
            
            HashSet<RegistrableDomain> topFrameDomains;
            
            if (!topFrameDomain1.isEmpty())
                topFrameDomains.add(topFrameDomain1);
            
            if (!topFrameDomain2.isEmpty())
                topFrameDomains.add(topFrameDomain2);

            statistic.subframeUnderTopFrameDomains = WTFMove(topFrameDomains);

            m_statisticsStore->mergeStatistics(Vector<ResourceLoadStatistics>::from(WTFMove(statistic)));
        }
        postTaskReply(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::isRelationshipOnlyInDatabaseOnce(RegistrableDomain&& subDomain, RegistrableDomain&& topDomain, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    postTask([this, subDomain = WTFMove(subDomain).isolatedCopy(), topDomain = WTFMove(topDomain).isolatedCopy(), completionHandler = WTFMove(completionHandler)]() mutable {
        if (!m_statisticsStore || !is<ResourceLoadStatisticsDatabaseStore>(*m_statisticsStore)) {
            completionHandler(false);
            return;
        }
        
        bool isRelationshipOnlyInDatabaseOnce = downcast<ResourceLoadStatisticsDatabaseStore>(*m_statisticsStore).isCorrectSubStatisticsCount(subDomain, topDomain);
        
        postTaskReply([isRelationshipOnlyInDatabaseOnce, completionHandler = WTFMove(completionHandler)]() mutable {
            completionHandler(isRelationshipOnlyInDatabaseOnce);
        });
    });
}
    
void WebResourceLoadStatisticsStore::setPrevalentResource(RegistrableDomain&& domain, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    postTask([this, domain = WTFMove(domain).isolatedCopy(), completionHandler = WTFMove(completionHandler)]() mutable {
        if (m_statisticsStore)
            m_statisticsStore->setPrevalentResource(domain);
        postTaskReply(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::setVeryPrevalentResource(RegistrableDomain&& domain, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    postTask([this, domain = WTFMove(domain).isolatedCopy(), completionHandler = WTFMove(completionHandler)]() mutable {
        if (m_statisticsStore)
            m_statisticsStore->setVeryPrevalentResource(domain);
        postTaskReply(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::setMostRecentWebPushInteractionTime(RegistrableDomain&& domain, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    postTask([this, completionHandler = WTFMove(completionHandler), domain = WTFMove(domain).isolatedCopy()] () mutable {
        if (m_statisticsStore)
            m_statisticsStore->setMostRecentWebPushInteractionTime(domain);
        postTaskReply(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::dumpResourceLoadStatistics(CompletionHandler<void(String&&)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    postTask([this, completionHandler = WTFMove(completionHandler)]() mutable {
        auto innerCompletionHandler = [completionHandler = WTFMove(completionHandler)](String&& result) mutable {
            postTaskReply([result = WTFMove(result).isolatedCopy(), completionHandler = WTFMove(completionHandler)]() mutable {
                completionHandler(WTFMove(result));
            });
        };
        if (!m_statisticsStore) {
            innerCompletionHandler(String { emptyString() });
            return;
        }
        m_statisticsStore->dumpResourceLoadStatistics(WTFMove(innerCompletionHandler));
    });
}

void WebResourceLoadStatisticsStore::isPrevalentResource(RegistrableDomain&& domain, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    if (isEphemeral()) {
        completionHandler(false);
        return;
    }

    postTask([this, domain = WTFMove(domain).isolatedCopy(), completionHandler = WTFMove(completionHandler)]() mutable {
        bool isPrevalentResource = m_statisticsStore ? m_statisticsStore->isPrevalentResource(domain) : false;
        postTaskReply([isPrevalentResource, completionHandler = WTFMove(completionHandler)]() mutable {
            completionHandler(isPrevalentResource);
        });
    });
}
    
void WebResourceLoadStatisticsStore::isVeryPrevalentResource(RegistrableDomain&& domain, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    postTask([this, domain = WTFMove(domain).isolatedCopy(), completionHandler = WTFMove(completionHandler)]() mutable {
        bool isVeryPrevalentResource = m_statisticsStore ? m_statisticsStore->isVeryPrevalentResource(domain) : false;
        postTaskReply([isVeryPrevalentResource, completionHandler = WTFMove(completionHandler)]() mutable {
            completionHandler(isVeryPrevalentResource);
        });
    });
}

void WebResourceLoadStatisticsStore::isRegisteredAsSubresourceUnder(RegistrableDomain&& subresourceDomain, RegistrableDomain&& topFrameDomain, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    postTask([this, subresourceDomain = WTFMove(subresourceDomain).isolatedCopy(), topFrameDomain = WTFMove(topFrameDomain).isolatedCopy(), completionHandler = WTFMove(completionHandler)]() mutable {
        bool isRegisteredAsSubresourceUnder = m_statisticsStore ? m_statisticsStore->isRegisteredAsSubresourceUnder(subresourceDomain, topFrameDomain)
            : false;
        postTaskReply([isRegisteredAsSubresourceUnder, completionHandler = WTFMove(completionHandler)]() mutable {
            completionHandler(isRegisteredAsSubresourceUnder);
        });
    });
}

void WebResourceLoadStatisticsStore::isRegisteredAsSubFrameUnder(RegistrableDomain&& subFrameDomain, RegistrableDomain&& topFrameDomain, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    postTask([this, subFrameDomain = WTFMove(subFrameDomain).isolatedCopy(), topFrameDomain = WTFMove(topFrameDomain).isolatedCopy(), completionHandler = WTFMove(completionHandler)]() mutable {
        bool isRegisteredAsSubFrameUnder = m_statisticsStore ? m_statisticsStore->isRegisteredAsSubFrameUnder(subFrameDomain, topFrameDomain)
            : false;
        postTaskReply([isRegisteredAsSubFrameUnder, completionHandler = WTFMove(completionHandler)]() mutable {
            completionHandler(isRegisteredAsSubFrameUnder);
        });
    });
}

void WebResourceLoadStatisticsStore::isRegisteredAsRedirectingTo(RegistrableDomain&& domainRedirectedFrom, RegistrableDomain&& domainRedirectedTo, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    postTask([this, domainRedirectedFrom = WTFMove(domainRedirectedFrom).isolatedCopy(), domainRedirectedTo = WTFMove(domainRedirectedTo).isolatedCopy(), completionHandler = WTFMove(completionHandler)]() mutable {
        bool isRegisteredAsRedirectingTo = m_statisticsStore ? m_statisticsStore->isRegisteredAsRedirectingTo(domainRedirectedFrom, domainRedirectedTo)
            : false;
        postTaskReply([isRegisteredAsRedirectingTo, completionHandler = WTFMove(completionHandler)]() mutable {
            completionHandler(isRegisteredAsRedirectingTo);
        });
    });
}

void WebResourceLoadStatisticsStore::clearPrevalentResource(RegistrableDomain&& domain, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    postTask([this, domain = WTFMove(domain).isolatedCopy(), completionHandler = WTFMove(completionHandler)]() mutable {
        if (m_statisticsStore)
            m_statisticsStore->clearPrevalentResource(domain);
        postTaskReply(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::setGrandfathered(RegistrableDomain&& domain, bool value, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    postTask([this, domain = WTFMove(domain).isolatedCopy(), value, completionHandler = WTFMove(completionHandler)]() mutable {
        if (m_statisticsStore)
            m_statisticsStore->setGrandfathered(domain, value);
        postTaskReply(WTFMove(completionHandler));
    });
}
    
void WebResourceLoadStatisticsStore::isGrandfathered(RegistrableDomain&& domain, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    postTask([this, completionHandler = WTFMove(completionHandler), domain = WTFMove(domain).isolatedCopy()]() mutable {
        bool isGrandFathered = m_statisticsStore ? m_statisticsStore->isGrandfathered(domain)
            : false;
        postTaskReply([isGrandFathered, completionHandler = WTFMove(completionHandler)]() mutable {
            completionHandler(isGrandFathered);
        });
    });
}

void WebResourceLoadStatisticsStore::setSubframeUnderTopFrameDomain(RegistrableDomain&& subFrameDomain, RegistrableDomain&& topFrameDomain, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    postTask([this, completionHandler = WTFMove(completionHandler), subFrameDomain = WTFMove(subFrameDomain).isolatedCopy(), topFrameDomain = WTFMove(topFrameDomain).isolatedCopy()]() mutable {
        if (m_statisticsStore)
            m_statisticsStore->setSubframeUnderTopFrameDomain(subFrameDomain, topFrameDomain);
        postTaskReply(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::setSubresourceUnderTopFrameDomain(RegistrableDomain&& subresourceDomain, RegistrableDomain&& topFrameDomain, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    postTask([this, completionHandler = WTFMove(completionHandler), subresourceDomain = WTFMove(subresourceDomain).isolatedCopy(), topFrameDomain = WTFMove(topFrameDomain).isolatedCopy()]() mutable {
        if (m_statisticsStore)
            m_statisticsStore->setSubresourceUnderTopFrameDomain(subresourceDomain, topFrameDomain);
        postTaskReply(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::setSubresourceUniqueRedirectTo(RegistrableDomain&& subresourceDomain, RegistrableDomain&& domainRedirectedTo, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    postTask([this, completionHandler = WTFMove(completionHandler), subresourceDomain = WTFMove(subresourceDomain).isolatedCopy(), domainRedirectedTo = WTFMove(domainRedirectedTo).isolatedCopy()]() mutable {
        if (m_statisticsStore)
            m_statisticsStore->setSubresourceUniqueRedirectTo(subresourceDomain, domainRedirectedTo);
        postTaskReply(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::setSubresourceUniqueRedirectFrom(RegistrableDomain&& subresourceDomain, RegistrableDomain&& domainRedirectedFrom, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    postTask([this, completionHandler = WTFMove(completionHandler), subresourceDomain = WTFMove(subresourceDomain).isolatedCopy(), domainRedirectedFrom = WTFMove(domainRedirectedFrom).isolatedCopy()]() mutable {
        if (m_statisticsStore)
            m_statisticsStore->setSubresourceUniqueRedirectFrom(subresourceDomain, domainRedirectedFrom);
        postTaskReply(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::setTopFrameUniqueRedirectTo(RegistrableDomain&& topFrameDomain, RegistrableDomain&& domainRedirectedTo, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    postTask([this, completionHandler = WTFMove(completionHandler), topFrameDomain = WTFMove(topFrameDomain).isolatedCopy(), domainRedirectedTo = WTFMove(domainRedirectedTo).isolatedCopy()]() mutable {
        if (m_statisticsStore)
            m_statisticsStore->setTopFrameUniqueRedirectTo(topFrameDomain, domainRedirectedTo);
        postTaskReply(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::setTopFrameUniqueRedirectFrom(RegistrableDomain&& topFrameDomain, RegistrableDomain&& domainRedirectedFrom, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    postTask([this, completionHandler = WTFMove(completionHandler), topFrameDomain = WTFMove(topFrameDomain).isolatedCopy(), domainRedirectedFrom = WTFMove(domainRedirectedFrom).isolatedCopy()]() mutable {
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

        m_statisticsStore->updateCookieBlocking([completionHandler = WTFMove(completionHandler)]() mutable {
            postTaskReply(WTFMove(completionHandler));
        });
    });
}

void WebResourceLoadStatisticsStore::scheduleClearInMemoryAndPersistent(ShouldGrandfatherStatistics shouldGrandfather, CompletionHandler<void()>&& completionHandler)
{
    if (isEphemeral())
        return clearInMemoryEphemeral(WTFMove(completionHandler));

    ASSERT(RunLoop::isMain());
    postTask([this, protectedThis = Ref { *this }, shouldGrandfather, completionHandler = WTFMove(completionHandler)]() mutable {
        if (!m_statisticsStore) {
            if (shouldGrandfather == ShouldGrandfatherStatistics::Yes)
                RELEASE_LOG(ResourceLoadStatistics, "WebResourceLoadStatisticsStore::scheduleClearInMemoryAndPersistent Before being cleared, m_statisticsStore is null when trying to grandfather data.");

            postTaskReply(WTFMove(completionHandler));
            return;
        }

        auto callbackAggregator = CallbackAggregator::create([completionHandler = WTFMove(completionHandler)] () mutable {
            postTaskReply(WTFMove(completionHandler));
        });

        m_statisticsStore->clear([this, protectedThis, shouldGrandfather, callbackAggregator] () mutable {
            if (shouldGrandfather == ShouldGrandfatherStatistics::Yes) {
                if (m_statisticsStore) {
                    m_statisticsStore->grandfatherExistingWebsiteData([callbackAggregator = WTFMove(callbackAggregator)]() mutable { });
                    if (is<ResourceLoadStatisticsDatabaseStore>(*m_statisticsStore))
                        downcast<ResourceLoadStatisticsDatabaseStore>(*m_statisticsStore).setIsNewResourceLoadStatisticsDatabaseFile(true);
                } else
                    RELEASE_LOG(ResourceLoadStatistics, "WebResourceLoadStatisticsStore::scheduleClearInMemoryAndPersistent After being cleared, m_statisticsStore is null when trying to grandfather data.");
            }
        });
        
        m_statisticsStore->cancelPendingStatisticsProcessingRequest();
    });
}

void WebResourceLoadStatisticsStore::scheduleClearInMemoryAndPersistent(WallTime modifiedSince, ShouldGrandfatherStatistics shouldGrandfather, CompletionHandler<void()>&& callback)
{
    ASSERT(RunLoop::isMain());

    // For now, be conservative and clear everything regardless of modifiedSince.
    UNUSED_PARAM(modifiedSince);
    scheduleClearInMemoryAndPersistent(shouldGrandfather, WTFMove(callback));
}

void WebResourceLoadStatisticsStore::clearInMemoryEphemeral(CompletionHandler<void()>&& completionHandler)
{
    m_domainsWithEphemeralUserInteraction.clear();
    if (auto* storageSession = m_networkSession->networkStorageSession())
        storageSession->removeAllStorageAccess();
    
    completionHandler();
}

void WebResourceLoadStatisticsStore::domainIDExistsInDatabase(int domainID, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    postTask([this, domainID, completionHandler = WTFMove(completionHandler)]() mutable {
        if (!m_statisticsStore || !is<ResourceLoadStatisticsDatabaseStore>(*m_statisticsStore)) {
            completionHandler(false);
            return;
        }
        auto& databaseStore = downcast<ResourceLoadStatisticsDatabaseStore>(*m_statisticsStore);
        bool domainIDExists = databaseStore.domainIDExistsInDatabase(domainID);
        postTaskReply([domainIDExists, completionHandler = WTFMove(completionHandler)]() mutable {
            completionHandler(domainIDExists);
        });
    });
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
    
    if (m_networkSession) {
        if (auto* storageSession = m_networkSession->networkStorageSession())
            storageSession->setCacheMaxAgeCapForPrevalentResources(seconds);
    }

    completionHandler();
}

bool WebResourceLoadStatisticsStore::needsUserInteractionQuirk(const RegistrableDomain& domain) const
{
    static NeverDestroyed<HashSet<RegistrableDomain>> quirks = [] {
        HashSet<RegistrableDomain> set;
        set.add(RegistrableDomain::uncheckedCreateFromRegistrableDomainString("kinja.com"_s));
        set.add(RegistrableDomain::uncheckedCreateFromRegistrableDomainString("youtube.com"_s));
        return set;
    }();

    return quirks.get().contains(domain);
}

void WebResourceLoadStatisticsStore::callUpdatePrevalentDomainsToBlockCookiesForHandler(const RegistrableDomainsToBlockCookiesFor& domainsToBlock, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    if (m_networkSession) {
        if (auto* storageSession = m_networkSession->networkStorageSession()) {
            storageSession->setPrevalentDomainsToBlockAndDeleteCookiesFor(domainsToBlock.domainsToBlockAndDeleteCookiesFor);
            storageSession->setPrevalentDomainsToBlockButKeepCookiesFor(domainsToBlock.domainsToBlockButKeepCookiesFor);
            storageSession->setDomainsWithUserInteractionAsFirstParty(domainsToBlock.domainsWithUserInteractionAsFirstParty);
        }

        HashSet<RegistrableDomain> domainsWithUserInteractionQuirk;
        for (auto& domain : domainsToBlock.domainsWithUserInteractionAsFirstParty) {
            if (needsUserInteractionQuirk(domain))
                domainsWithUserInteractionQuirk.add(domain);
        }

        if (m_domainsWithUserInteractionQuirk != domainsWithUserInteractionQuirk) {
            m_domainsWithUserInteractionQuirk = domainsWithUserInteractionQuirk;
            m_networkSession->networkProcess().parentProcessConnection()->send(Messages::NetworkProcessProxy::SetDomainsWithUserInteraction(domainsWithUserInteractionQuirk), 0);
        }

        HashMap<TopFrameDomain, SubResourceDomain> domainsWithStorageAccessQuirk;
        for (auto& firstPartyDomain : domainsToBlock.domainsWithStorageAccess.keys()) {
            auto requestingDomain = domainsToBlock.domainsWithStorageAccess.get(firstPartyDomain);
            if (NetworkStorageSession::loginDomainMatchesRequestingDomain(firstPartyDomain, requestingDomain))
                domainsWithStorageAccessQuirk.add(firstPartyDomain, requestingDomain);
        }

        if (m_domainsWithCrossPageStorageAccessQuirk != domainsWithStorageAccessQuirk) {
            if (m_networkSession) {
                if (auto* storageSession = m_networkSession->networkStorageSession())
                    storageSession->setDomainsWithCrossPageStorageAccess(domainsWithStorageAccessQuirk);
                m_networkSession->networkProcess().parentProcessConnection()->sendWithAsyncReply(Messages::NetworkProcessProxy::SetDomainsWithCrossPageStorageAccess(domainsWithStorageAccessQuirk), [this, domainsWithStorageAccessQuirk] () mutable {
                    m_domainsWithCrossPageStorageAccessQuirk = domainsWithStorageAccessQuirk;
                });
            }
        }
    }

    completionHandler();
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

    if (isEphemeral()) {
        completionHandler();
        return;
    }

#if ENABLE(APP_BOUND_DOMAINS)
    if (m_networkSession) {
        if (auto* storageSession = m_networkSession->networkStorageSession())
            storageSession->resetAppBoundDomains();
    }
#endif

#if ENABLE(MANAGED_DOMAINS)
    if (m_networkSession) {
        if (auto* storageSession = m_networkSession->networkStorageSession())
            storageSession->resetManagedDomains();
    }
#endif

    postTask([this, completionHandler = WTFMove(completionHandler)]() mutable {
        if (m_statisticsStore)
            m_statisticsStore->resetParametersToDefaultValues();

        postTaskReply(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::logTestingEvent(const String& event)
{
    ASSERT(RunLoop::isMain());

    if (m_networkSession && m_networkSession->enableResourceLoadStatisticsLogTestingEvent())
        m_networkSession->networkProcess().parentProcessConnection()->send(Messages::NetworkProcessProxy::LogTestingEvent(m_networkSession->sessionID(), event), 0);
}

void WebResourceLoadStatisticsStore::notifyResourceLoadStatisticsProcessed()
{
    ASSERT(RunLoop::isMain());
    
    if (m_networkSession)
        m_networkSession->notifyResourceLoadStatisticsProcessed();
}

NetworkSession* WebResourceLoadStatisticsStore::networkSession()
{
    ASSERT(RunLoop::isMain());
    return m_networkSession.get();
}

void WebResourceLoadStatisticsStore::invalidateAndCancel()
{
    ASSERT(RunLoop::isMain());
    m_networkSession = nullptr;
}

void WebResourceLoadStatisticsStore::removeDataForDomain(RegistrableDomain domain, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    postTask([this, domain = WTFMove(domain), completionHandler = WTFMove(completionHandler)]() mutable {
        if (m_statisticsStore)
            m_statisticsStore->removeDataForDomain(domain);

        postTaskReply(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::registrableDomains(CompletionHandler<void(Vector<RegistrableDomain>&&)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    if (isEphemeral()) {
        completionHandler({ });
        return;
    }

    postTask([this, completionHandler = WTFMove(completionHandler)]() mutable {
        auto domains = m_statisticsStore ? m_statisticsStore->allDomains() : Vector<RegistrableDomain>();
        postTaskReply([domains = crossThreadCopy(WTFMove(domains)), completionHandler = WTFMove(completionHandler)]() mutable {
            completionHandler(WTFMove(domains));
        });
    });
}

void WebResourceLoadStatisticsStore::deleteAndRestrictWebsiteDataForRegistrableDomains(OptionSet<WebsiteDataType> dataTypes, RegistrableDomainsToDeleteOrRestrictWebsiteDataFor&& domainsToDeleteAndRestrictWebsiteDataFor, bool shouldNotifyPage, CompletionHandler<void(HashSet<RegistrableDomain>&&)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    if (m_networkSession) {
        m_networkSession->deleteAndRestrictWebsiteDataForRegistrableDomains(dataTypes, WTFMove(domainsToDeleteAndRestrictWebsiteDataFor), shouldNotifyPage, WTFMove(completionHandler));
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
    ASSERT(RunLoop::isMain());
    if (m_networkSession)
        const_cast<WebResourceLoadStatisticsStore*>(this)->networkSession()->logDiagnosticMessageWithValue(message, description, value, sigDigits, shouldSample);
}

void WebResourceLoadStatisticsStore::aggregatedThirdPartyData(CompletionHandler<void(Vector<WebResourceLoadStatisticsStore::ThirdPartyData>&&)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    postTask([this, completionHandler = WTFMove(completionHandler)]() mutable {
        if (!m_statisticsStore) {
            postTaskReply([completionHandler = WTFMove(completionHandler)]() mutable {
                completionHandler({ });
            });
            return;
        }
        auto thirdPartyData = m_statisticsStore->aggregatedThirdPartyData();
        postTaskReply([thirdPartyData = WTFMove(thirdPartyData), completionHandler = WTFMove(completionHandler)]() mutable {
            completionHandler(WTFMove(thirdPartyData));
        });
    });
}

void WebResourceLoadStatisticsStore::suspend(CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    sharedStatisticsQueue()->suspend(ResourceLoadStatisticsDatabaseStore::interruptAllDatabases, WTFMove(completionHandler));
}

void WebResourceLoadStatisticsStore::resume()
{
    ASSERT(RunLoop::isMain());

    sharedStatisticsQueue()->resume();
}

void WebResourceLoadStatisticsStore::insertExpiredStatisticForTesting(RegistrableDomain&& domain, unsigned numberOfOperatingDaysPassed, bool hadUserInteraction, bool isScheduledForAllButCookieDataRemoval, bool isPrevalent, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    postTask([this, domain = WTFMove(domain).isolatedCopy(), numberOfOperatingDaysPassed, hadUserInteraction, isScheduledForAllButCookieDataRemoval, isPrevalent, completionHandler = WTFMove(completionHandler)]() mutable {
        if (m_statisticsStore)
            m_statisticsStore->insertExpiredStatisticForTesting(WTFMove(domain), numberOfOperatingDaysPassed, hadUserInteraction, isScheduledForAllButCookieDataRemoval, isPrevalent);
        postTaskReply(WTFMove(completionHandler));
    });
}

String WebResourceLoadStatisticsStore::ThirdPartyDataForSpecificFirstParty::toString() const
{
    return makeString("Has been granted storage access under ", firstPartyDomain.string(), ": ", storageAccessGranted ? '1' : '0', "; Has been seen under ", firstPartyDomain.string(), " in the last 24 hours: ", WallTime::now().secondsSinceEpoch() - timeLastUpdated < 24_h ? '1' : '0');
}

void WebResourceLoadStatisticsStore::ThirdPartyDataForSpecificFirstParty::encode(IPC::Encoder& encoder) const
{
    encoder << firstPartyDomain;
    encoder << storageAccessGranted;
    encoder << timeLastUpdated;
}

auto WebResourceLoadStatisticsStore::ThirdPartyDataForSpecificFirstParty::decode(IPC::Decoder& decoder) -> std::optional<ThirdPartyDataForSpecificFirstParty>
{
    std::optional<WebCore::RegistrableDomain> decodedDomain;
    decoder >> decodedDomain;
    if (!decodedDomain)
        return std::nullopt;

    std::optional<bool> decodedStorageAccess;
    decoder >> decodedStorageAccess;
    if (!decodedStorageAccess)
        return std::nullopt;

    std::optional<Seconds> decodedTimeLastUpdated;
    decoder >> decodedTimeLastUpdated;
    if (!decodedTimeLastUpdated)
        return std::nullopt;

    return {{ WTFMove(*decodedDomain), WTFMove(*decodedStorageAccess), WTFMove(*decodedTimeLastUpdated) }};
}

bool WebResourceLoadStatisticsStore::ThirdPartyDataForSpecificFirstParty::operator==(const ThirdPartyDataForSpecificFirstParty& other) const
{
    return firstPartyDomain == other.firstPartyDomain && storageAccessGranted == other.storageAccessGranted;
}

String WebResourceLoadStatisticsStore::ThirdPartyData::toString() const
{
    StringBuilder stringBuilder;
    stringBuilder.append("Third Party Registrable Domain: ", thirdPartyDomain.string(), "\n    {");
    for (auto firstParty : underFirstParties)
        stringBuilder.append("{ ", firstParty.toString(), " },");
    stringBuilder.append('}');
    return stringBuilder.toString();
}

void WebResourceLoadStatisticsStore::ThirdPartyData::encode(IPC::Encoder& encoder) const
{
    encoder << thirdPartyDomain;
    encoder << underFirstParties;
}

auto WebResourceLoadStatisticsStore::ThirdPartyData::decode(IPC::Decoder& decoder) -> std::optional<ThirdPartyData>
{
    std::optional<WebCore::RegistrableDomain> decodedDomain;
    decoder >> decodedDomain;
    if (!decodedDomain)
        return std::nullopt;

    std::optional<Vector<ThirdPartyDataForSpecificFirstParty>> decodedFirstParties;
    decoder >> decodedFirstParties;
    if (!decodedFirstParties)
        return std::nullopt;

    return {{ WTFMove(*decodedDomain), WTFMove(*decodedFirstParties) }};
}

bool WebResourceLoadStatisticsStore::ThirdPartyData::operator<(const ThirdPartyData &other) const
{
    return underFirstParties.size() < other.underFirstParties.size();
}

} // namespace WebKit

#endif

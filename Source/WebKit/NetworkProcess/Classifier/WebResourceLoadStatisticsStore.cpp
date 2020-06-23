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

#if ENABLE(RESOURCE_LOAD_STATISTICS)

#include "APIDictionary.h"
#include "Logging.h"
#include "NetworkProcess.h"
#include "NetworkProcessProxyMessages.h"
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
#include "WebResourceLoadStatisticsTelemetry.h"
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
#include <wtf/threads/BinarySemaphore.h>

namespace WebKit {
using namespace WebCore;

WebResourceLoadStatisticsStore::State WebResourceLoadStatisticsStore::suspendedState { WebResourceLoadStatisticsStore::State::Running };
Lock WebResourceLoadStatisticsStore::suspendedStateLock;
Condition WebResourceLoadStatisticsStore::suspendedStateChangeCondition;

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

static Ref<WorkQueue> sharedStatisticsQueue()
{
    static NeverDestroyed<Ref<WorkQueue>> queue(WorkQueue::create("WebResourceLoadStatisticsStore Process Data Queue", WorkQueue::Type::Serial, WorkQueue::QOS::Utility));
    return queue.get().copyRef();
}

WebResourceLoadStatisticsStore::WebResourceLoadStatisticsStore(NetworkSession& networkSession, const String& resourceLoadStatisticsDirectory, ShouldIncludeLocalhost shouldIncludeLocalhost, ResourceLoadStatistics::IsEphemeral isEphemeral)
    : m_networkSession(makeWeakPtr(networkSession))
    , m_statisticsQueue(sharedStatisticsQueue())
    , m_dailyTasksTimer(RunLoop::main(), this, &WebResourceLoadStatisticsStore::performDailyTasks)
    , m_isEphemeral(isEphemeral)
{
    RELEASE_ASSERT(RunLoop::isMain());

    // No daily tasks needed for ephemeral sessions since no resource load statistics are collected.
    if (isEphemeral == ResourceLoadStatistics::IsEphemeral::Yes)
        return;

    if (!resourceLoadStatisticsDirectory.isEmpty()) {
        postTask([this, databaseEnabled = networkSession.networkProcess().isITPDatabaseEnabled(), resourceLoadStatisticsDirectory = resourceLoadStatisticsDirectory.isolatedCopy(), shouldIncludeLocalhost, sessionID = networkSession.sessionID()] {
            if (databaseEnabled) {
                m_statisticsStore = makeUnique<ResourceLoadStatisticsDatabaseStore>(*this, m_statisticsQueue, shouldIncludeLocalhost, resourceLoadStatisticsDirectory, sessionID);

                auto memoryStore = makeUnique<ResourceLoadStatisticsMemoryStore>(*this, m_statisticsQueue, shouldIncludeLocalhost);
                downcast<ResourceLoadStatisticsDatabaseStore>(*m_statisticsStore.get()).populateFromMemoryStore(*memoryStore);

                auto legacyPlistFilePath = FileSystem::pathByAppendingComponent(resourceLoadStatisticsDirectory, "full_browsing_session_resourceLog.plist");
                if (FileSystem::fileExists(legacyPlistFilePath))
                    FileSystem::deleteFile(legacyPlistFilePath);

            } else {
                m_statisticsStore = makeUnique<ResourceLoadStatisticsMemoryStore>(*this, m_statisticsQueue, shouldIncludeLocalhost);
                m_persistentStorage = makeUnique<ResourceLoadStatisticsPersistentStorage>(downcast<ResourceLoadStatisticsMemoryStore>(*m_statisticsStore), m_statisticsQueue, resourceLoadStatisticsDirectory);

                SQLiteFileSystem::deleteDatabaseFile(FileSystem::pathByAppendingComponent(resourceLoadStatisticsDirectory, "observations.db"));
            }

            m_statisticsStore->didCreateNetworkProcess();
        });

        m_dailyTasksTimer.startRepeating(24_h);
    }
}

WebResourceLoadStatisticsStore::~WebResourceLoadStatisticsStore()
{
    RELEASE_ASSERT(RunLoop::isMain());
    RELEASE_ASSERT(!m_statisticsStore);
    RELEASE_ASSERT(!m_persistentStorage);
}

void WebResourceLoadStatisticsStore::didDestroyNetworkSession(CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    auto callbackAggregator = CallbackAggregator::create([completionHandler = WTFMove(completionHandler)] () mutable {
        completionHandler();
    });

    m_networkSession = nullptr;
    flushAndDestroyPersistentStore([callbackAggregator] { });
    destroyResourceLoadStatisticsStore([callbackAggregator] { });
}

inline void WebResourceLoadStatisticsStore::postTask(WTF::Function<void()>&& task)
{
    // Resource load statistics should not be captured for ephemeral sessions.
    RELEASE_ASSERT(!isEphemeral());

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

void WebResourceLoadStatisticsStore::destroyResourceLoadStatisticsStore(CompletionHandler<void()>&& completionHandler)
{
    RELEASE_ASSERT(RunLoop::isMain());

    if (isEphemeral()) {
        completionHandler();
        return;
    }

    postTask([this, protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)]() mutable {
        m_statisticsStore = nullptr;
        postTaskReply(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::flushAndDestroyPersistentStore(CompletionHandler<void()>&& completionHandler)
{
    RELEASE_ASSERT(RunLoop::isMain());

    // Make sure we destroy the persistent store on the background queue and stay alive until it
    // is destroyed because it has a C++ reference to us.
    m_statisticsQueue->dispatch([this, protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)] () mutable {
        m_persistentStorage = nullptr;
        RunLoop::main().dispatch(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::populateMemoryStoreFromDisk(CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    postTask([this, protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)]() mutable {
        if (m_persistentStorage)
            m_persistentStorage->populateMemoryStoreFromDisk([protectedThis = WTFMove(protectedThis), completionHandler = WTFMove(completionHandler)]() mutable {
                postTaskReply(WTFMove(completionHandler));
            });
        else if (m_statisticsStore && is<ResourceLoadStatisticsDatabaseStore>(*m_statisticsStore)) {
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

    postTask([this, value, completionHandler = WTFMove(completionHandler)]() mutable {
        if (m_statisticsStore)
            m_statisticsStore->setResourceLoadStatisticsDebugMode(value);
        postTaskReply(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::setPrevalentResourceForDebugMode(const RegistrableDomain& domain, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    if (isEphemeral()) {
        completionHandler();
        return;
    }

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

void WebResourceLoadStatisticsStore::resourceLoadStatisticsUpdated(Vector<ResourceLoadStatistics>&& statistics)
{
    ASSERT(RunLoop::isMain());

    // It is safe to move the origins to the background queue without isolated copy here because this is an r-value
    // coming from IPC. ResourceLoadStatistics only contains strings which are safe to move to other threads as long
    // as nobody on this thread holds a reference to those strings.
    postTask([this, protectedThis = makeRef(*this), statistics = WTFMove(statistics)]() mutable {
        if (!m_statisticsStore)
            return;

        ASSERT(suspendedState != State::Suspended);
        m_statisticsStore->mergeStatistics(WTFMove(statistics));

        // We can cancel any pending request to process statistics since we're doing it synchronously below.
        m_statisticsStore->cancelPendingStatisticsProcessingRequest();

        // Fire before processing statistics to propagate user interaction as fast as possible to the network process.
        m_statisticsStore->updateCookieBlocking([this, protectedThis = protectedThis.copyRef()]() {
            postTaskReply([this, protectedThis = protectedThis.copyRef()]() {
                logTestingEvent("Statistics Updated"_s);
            });
        });
        m_statisticsStore->processStatisticsAndDataRecords();
    });
}

void WebResourceLoadStatisticsStore::hasStorageAccess(const RegistrableDomain& subFrameDomain, const RegistrableDomain& topFrameDomain, Optional<FrameIdentifier> frameID, PageIdentifier pageID, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(subFrameDomain != topFrameDomain);
    ASSERT(RunLoop::isMain());

    if (isEphemeral())
        return hasStorageAccessEphemeral(subFrameDomain, topFrameDomain, frameID, pageID, WTFMove(completionHandler));

    postTask([this, subFrameDomain = subFrameDomain.isolatedCopy(), topFrameDomain = topFrameDomain.isolatedCopy(), frameID, pageID, completionHandler = WTFMove(completionHandler)]() mutable {
        if (!m_statisticsStore) {
            postTaskReply([completionHandler = WTFMove(completionHandler)]() mutable {
                completionHandler(false);
            });
            return;
        }

        m_statisticsStore->hasStorageAccess(subFrameDomain, topFrameDomain, frameID, pageID, [completionHandler = WTFMove(completionHandler)](bool hasStorageAccess) mutable {
            postTaskReply([completionHandler = WTFMove(completionHandler), hasStorageAccess]() mutable {
                completionHandler(hasStorageAccess);
            });
        });
    });
}

void WebResourceLoadStatisticsStore::hasStorageAccessEphemeral(const RegistrableDomain& subFrameDomain, const RegistrableDomain& topFrameDomain, Optional<FrameIdentifier> frameID, PageIdentifier pageID, CompletionHandler<void(bool)>&& completionHandler)
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

void WebResourceLoadStatisticsStore::requestStorageAccess(const RegistrableDomain& subFrameDomain, const RegistrableDomain& topFrameDomain, FrameIdentifier frameID, PageIdentifier webPageID, WebPageProxyIdentifier webPageProxyID, StorageAccessScope scope, CompletionHandler<void(RequestStorageAccessResult)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    if (subFrameDomain == topFrameDomain) {
        completionHandler({ StorageAccessWasGranted::Yes, StorageAccessPromptWasShown::No, scope, topFrameDomain, subFrameDomain });
        return;
    }
    
    if (isEphemeral())
        return requestStorageAccessEphemeral(subFrameDomain, topFrameDomain, frameID, webPageID, webPageProxyID, scope, WTFMove(completionHandler));

    auto statusHandler = [this, protectedThis = makeRef(*this), subFrameDomain = subFrameDomain.isolatedCopy(), topFrameDomain = topFrameDomain.isolatedCopy(), frameID, webPageID, webPageProxyID, scope, completionHandler = WTFMove(completionHandler)](StorageAccessStatus status) mutable {
        switch (status) {
        case StorageAccessStatus::CannotRequestAccess:
            completionHandler({ StorageAccessWasGranted::No, StorageAccessPromptWasShown::No, scope, topFrameDomain, subFrameDomain });
            return;
        case StorageAccessStatus::RequiresUserPrompt:
            {
            if (!m_networkSession)
                return completionHandler({ StorageAccessWasGranted::No, StorageAccessPromptWasShown::No, scope, topFrameDomain, subFrameDomain });

            CompletionHandler<void(bool)> requestConfirmationCompletionHandler = [this, protectedThis = protectedThis.copyRef(), subFrameDomain, topFrameDomain, frameID, webPageID, scope, completionHandler = WTFMove(completionHandler)] (bool userDidGrantAccess) mutable {
                if (userDidGrantAccess)
                    grantStorageAccess(subFrameDomain, topFrameDomain, frameID, webPageID, StorageAccessPromptWasShown::Yes, scope, WTFMove(completionHandler));
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

    postTask([this, subFrameDomain = subFrameDomain.isolatedCopy(), topFrameDomain = topFrameDomain.isolatedCopy(), frameID, webPageID, scope, statusHandler = WTFMove(statusHandler)]() mutable {
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

    if (!m_networkSession || !m_domainsWithEphemeralUserInteraction.contains(subFrameDomain))
        return completionHandler({ StorageAccessWasGranted::No, StorageAccessPromptWasShown::No, scope, topFrameDomain, subFrameDomain });

    CompletionHandler<void(bool)> requestConfirmationCompletionHandler = [this, protectedThis = makeRef(*this), subFrameDomain, topFrameDomain, frameID, webPageID, scope, completionHandler = WTFMove(completionHandler)] (bool userDidGrantAccess) mutable {
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
            storageSession->grantStorageAccess(WTFMove(domainInNeedOfStorageAccess), WTFMove(openerDomain), WTF::nullopt, openerPageID);
    }
}

void WebResourceLoadStatisticsStore::grantStorageAccess(const RegistrableDomain& subFrameDomain, const RegistrableDomain& topFrameDomain, FrameIdentifier frameID, PageIdentifier pageID, StorageAccessPromptWasShown promptWasShown, StorageAccessScope scope, CompletionHandler<void(RequestStorageAccessResult)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    postTask([this, subFrameDomain = subFrameDomain.isolatedCopy(), topFrameDomain = topFrameDomain.isolatedCopy(), frameID, pageID, promptWasShown, scope, completionHandler = WTFMove(completionHandler)]() mutable {
        if (!m_statisticsStore) {
            postTaskReply([subFrameDomain = subFrameDomain.isolatedCopy(), topFrameDomain = topFrameDomain.isolatedCopy(), promptWasShown, scope, completionHandler = WTFMove(completionHandler)]() mutable {
                completionHandler({ StorageAccessWasGranted::No, promptWasShown, scope, topFrameDomain, subFrameDomain });
            });
            return;
        }

        m_statisticsStore->grantStorageAccess(WTFMove(subFrameDomain), WTFMove(topFrameDomain), frameID, pageID, promptWasShown, scope, [subFrameDomain = subFrameDomain.isolatedCopy(), topFrameDomain = topFrameDomain.isolatedCopy(), promptWasShown, scope, completionHandler = WTFMove(completionHandler)](StorageAccessWasGranted wasGrantedAccess) mutable {
            postTaskReply([subFrameDomain = subFrameDomain.isolatedCopy(), topFrameDomain = topFrameDomain.isolatedCopy(), wasGrantedAccess, promptWasShown, scope, completionHandler = WTFMove(completionHandler)]() mutable {
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

StorageAccessWasGranted WebResourceLoadStatisticsStore::grantStorageAccessInStorageSession(const RegistrableDomain& resourceDomain, const RegistrableDomain& firstPartyDomain, Optional<FrameIdentifier> frameID, PageIdentifier pageID, StorageAccessScope scope)
{
    ASSERT(RunLoop::isMain());

    bool isStorageGranted = false;

    if (m_networkSession) {
        if (auto* storageSession = m_networkSession->networkStorageSession()) {
            storageSession->grantStorageAccess(resourceDomain, firstPartyDomain, (scope == StorageAccessScope::PerFrame ? frameID : WTF::nullopt), pageID);
            ASSERT(storageSession->hasStorageAccess(resourceDomain, firstPartyDomain, frameID, pageID));
            isStorageGranted = true;
        }
    }

    return isStorageGranted ? StorageAccessWasGranted::Yes : StorageAccessWasGranted::No;
}

void WebResourceLoadStatisticsStore::callGrantStorageAccessHandler(const RegistrableDomain& subFrameDomain, const RegistrableDomain& topFrameDomain, Optional<FrameIdentifier> frameID, PageIdentifier pageID, StorageAccessScope scope, CompletionHandler<void(StorageAccessWasGranted)>&& completionHandler)
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
            m_statisticsStore->calculateAndSubmitTelemetry();
        }
        if (is<ResourceLoadStatisticsDatabaseStore>(*m_statisticsStore))
            downcast<ResourceLoadStatisticsDatabaseStore>(*m_statisticsStore).runIncrementalVacuumCommand();
    });
}

void WebResourceLoadStatisticsStore::submitTelemetry(CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    postTask([this, completionHandler = WTFMove(completionHandler)]() mutable  {
        if (!m_statisticsStore)
            return;
        
        if (is<ResourceLoadStatisticsMemoryStore>(*m_statisticsStore))
            WebResourceLoadStatisticsTelemetry::calculateAndSubmit(downcast<ResourceLoadStatisticsMemoryStore>(*m_statisticsStore));
        else
            m_statisticsStore->calculateAndSubmitTelemetry();
        postTaskReply(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::logFrameNavigation(const RegistrableDomain& targetDomain, const RegistrableDomain& topFrameDomain, const RegistrableDomain& sourceDomain, bool isRedirect, bool isMainFrame, Seconds delayAfterMainFrameDocumentLoad, bool wasPotentiallyInitiatedByUser)
{
    ASSERT(RunLoop::isMain());

    postTask([this, targetDomain = targetDomain.isolatedCopy(), topFrameDomain = topFrameDomain.isolatedCopy(), sourceDomain = sourceDomain.isolatedCopy(), isRedirect, isMainFrame, delayAfterMainFrameDocumentLoad, wasPotentiallyInitiatedByUser] {
        if (m_statisticsStore)
            m_statisticsStore->logFrameNavigation(targetDomain, topFrameDomain, sourceDomain, isRedirect, isMainFrame, delayAfterMainFrameDocumentLoad, wasPotentiallyInitiatedByUser);
    });
}

void WebResourceLoadStatisticsStore::logUserInteraction(const RegistrableDomain& domain, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    // User interactions need to be logged for ephemeral sessions to support the Storage Access API.
    if (isEphemeral())
        return logUserInteractionEphemeral(domain, WTFMove(completionHandler));

    postTask([this, domain = domain.isolatedCopy(), completionHandler = WTFMove(completionHandler)]() mutable {
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

    if (isEphemeral())
        return clearUserInteractionEphemeral(domain, WTFMove(completionHandler));

    postTask([this, domain = domain.isolatedCopy(), completionHandler = WTFMove(completionHandler)]() mutable {
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

void WebResourceLoadStatisticsStore::clearUserInteractionEphemeral(const RegistrableDomain& domain, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(isEphemeral());

    m_domainsWithEphemeralUserInteraction.remove(domain);
    completionHandler();
}

void WebResourceLoadStatisticsStore::hasHadUserInteraction(const RegistrableDomain& domain, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    if (isEphemeral())
        return hasHadUserInteractionEphemeral(domain, WTFMove(completionHandler));

    postTask([this, domain = domain.isolatedCopy(), completionHandler = WTFMove(completionHandler)]() mutable {
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

void WebResourceLoadStatisticsStore::setLastSeen(const RegistrableDomain& domain, Seconds seconds, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    postTask([this, domain = domain.isolatedCopy(), seconds, completionHandler = WTFMove(completionHandler)]() mutable {
        if (m_statisticsStore)
            m_statisticsStore->setLastSeen(domain, seconds);
        postTaskReply(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::mergeStatisticForTesting(const RegistrableDomain& domain, const RegistrableDomain& topFrameDomain1, const RegistrableDomain& topFrameDomain2, Seconds lastSeen, bool hadUserInteraction, Seconds mostRecentUserInteraction, bool isGrandfathered, bool isPrevalent, bool isVeryPrevalent, unsigned dataRecordsRemoved, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    postTask([this, domain = domain.isolatedCopy(), topFrameDomain1 = topFrameDomain1.isolatedCopy(), topFrameDomain2 = topFrameDomain2.isolatedCopy(), lastSeen, hadUserInteraction, mostRecentUserInteraction, isGrandfathered, isPrevalent, isVeryPrevalent, dataRecordsRemoved, completionHandler = WTFMove(completionHandler)]() mutable {
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

            Vector<ResourceLoadStatistics> statistics;
            statistics.append(WTFMove(statistic));
            m_statisticsStore->mergeStatistics(WTFMove(statistics));
        }
        postTaskReply(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::isRelationshipOnlyInDatabaseOnce(const RegistrableDomain& subDomain, const RegistrableDomain& topDomain, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    postTask([this, subDomain = subDomain.isolatedCopy(), topDomain = topDomain.isolatedCopy(), completionHandler = WTFMove(completionHandler)]() mutable {
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
        auto innerCompletionHandler = [completionHandler = WTFMove(completionHandler)](const String& result) mutable {
            postTaskReply([result = result.isolatedCopy(), completionHandler = WTFMove(completionHandler)]() mutable {
                completionHandler(result);
            });
        };
        if (!m_statisticsStore) {
            innerCompletionHandler(emptyString());
            return;
        }
        m_statisticsStore->dumpResourceLoadStatistics(WTFMove(innerCompletionHandler));
    });
}

void WebResourceLoadStatisticsStore::isPrevalentResource(const RegistrableDomain& domain, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    if (isEphemeral()) {
        completionHandler(false);
        return;
    }

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

        m_statisticsStore->clear([this, protectedThis = protectedThis.copyRef(), shouldGrandfather, callbackAggregator = callbackAggregator.copyRef()] () mutable {
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

    if (m_networkSession) {
        if (auto* storageSession = m_networkSession->networkStorageSession())
            storageSession->resetAppBoundDomains();
    }

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
    postTask([this, completionHandler = WTFMove(completionHandler)]() mutable {
        auto domains = m_statisticsStore ? m_statisticsStore->allDomains() : Vector<RegistrableDomain>();
        postTaskReply([domains = crossThreadCopy(WTFMove(domains)), completionHandler = WTFMove(completionHandler)]() mutable {
            completionHandler(WTFMove(domains));
        });
    });
}

void WebResourceLoadStatisticsStore::deleteAndRestrictWebsiteDataForRegistrableDomains(OptionSet<WebsiteDataType> dataTypes, RegistrableDomainsToDeleteOrRestrictWebsiteDataFor&& domainsToDeleteAndRestrictWebsiteDataFor, bool shouldNotifyPage, CompletionHandler<void(const HashSet<RegistrableDomain>&)>&& completionHandler)
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

void WebResourceLoadStatisticsStore::notifyPageStatisticsTelemetryFinished(unsigned numberOfPrevalentResources, unsigned numberOfPrevalentResourcesWithUserInteraction, unsigned numberOfPrevalentResourcesWithoutUserInteraction, unsigned topPrevalentResourceWithUserInteractionDaysSinceUserInteraction, unsigned medianDaysSinceUserInteractionPrevalentResourceWithUserInteraction, unsigned top3NumberOfPrevalentResourcesWithUI, unsigned top3MedianSubFrameWithoutUI, unsigned top3MedianSubResourceWithoutUI, unsigned top3MedianUniqueRedirectsWithoutUI, unsigned top3MedianDataRecordsRemovedWithoutUI) const
{
    ASSERT(RunLoop::isMain());
    if (m_networkSession)
        const_cast<WebResourceLoadStatisticsStore*>(this)->networkSession()->notifyPageStatisticsTelemetryFinished(numberOfPrevalentResources, numberOfPrevalentResourcesWithUserInteraction, numberOfPrevalentResourcesWithoutUserInteraction, topPrevalentResourceWithUserInteractionDaysSinceUserInteraction, medianDaysSinceUserInteractionPrevalentResourceWithUserInteraction, top3NumberOfPrevalentResourcesWithUI, top3MedianSubFrameWithoutUI, top3MedianSubResourceWithoutUI, top3MedianUniqueRedirectsWithoutUI, top3MedianDataRecordsRemovedWithoutUI);
}

void WebResourceLoadStatisticsStore::aggregatedThirdPartyData(CompletionHandler<void(Vector<WebResourceLoadStatisticsStore::ThirdPartyData>&&)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    postTask([this, completionHandler = WTFMove(completionHandler)]() mutable  {
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
    CompletionHandlerCallingScope completionHandlerCaller(WTFMove(completionHandler));
    Locker<Lock> stateLocker(suspendedStateLock);
    if (suspendedState != State::Running)
        return;
    suspendedState = State::WillSuspend;

    sharedStatisticsQueue()->dispatch([completionHandler = completionHandlerCaller.release()] () mutable {

        for (auto& databaseStore : ResourceLoadStatisticsDatabaseStore::allStores())
            databaseStore->interrupt();
        
        Locker<Lock> stateLocker(suspendedStateLock);
        ASSERT(suspendedState != State::Suspended);

        if (suspendedState != State::WillSuspend) {
            postTaskReply(WTFMove(completionHandler));
            return;
        }

        suspendedState = State::Suspended;
        postTaskReply(WTFMove(completionHandler));

        while (suspendedState == State::Suspended)
            suspendedStateChangeCondition.wait(suspendedStateLock);
        ASSERT(suspendedState != State::Suspended);
    });
}

void WebResourceLoadStatisticsStore::resume()
{
    Locker<Lock> stateLocker(suspendedStateLock);
    auto previousState = suspendedState;
    suspendedState = State::Running;
    if (previousState == State::Suspended)
        suspendedStateChangeCondition.notifyOne();
}

void WebResourceLoadStatisticsStore::insertExpiredStatisticForTesting(const RegistrableDomain& domain, bool hadUserInteraction, bool isScheduledForAllButCookieDataRemoval, bool isPrevalent, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    postTask([this, domain = domain.isolatedCopy(), hadUserInteraction, isScheduledForAllButCookieDataRemoval, isPrevalent, completionHandler = WTFMove(completionHandler)]() mutable {
        if (m_statisticsStore)
            m_statisticsStore->insertExpiredStatisticForTesting(WTFMove(domain), hadUserInteraction, isScheduledForAllButCookieDataRemoval, isPrevalent);
        postTaskReply(WTFMove(completionHandler));
    });
}

} // namespace WebKit

#endif

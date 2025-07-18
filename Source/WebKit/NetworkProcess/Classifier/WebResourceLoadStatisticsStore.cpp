/*
 * Copyright (C) 2016-2025 Apple Inc. All rights reserved.
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

#include "APIDictionary.h"
#include "ITPThirdPartyData.h"
#include "Logging.h"
#include "NetworkProcess.h"
#include "NetworkProcessProxyMessages.h"
#include "NetworkSession.h"
#include "PrivateClickMeasurementManager.h"
#include "ResourceLoadStatisticsStore.h"
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
#include <WebCore/IsLoggedIn.h>
#include <WebCore/LoginStatus.h>
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
        WebsiteDataType::ServiceWorkerRegistrations,
        WebsiteDataType::FileSystem,
#if ENABLE(SCREEN_TIME)
        WebsiteDataType::ScreenTime
#endif
    }));

    ASSERT(RunLoop::isMain());

    return dataTypes;
}

void WebResourceLoadStatisticsStore::setIsRunningTest(bool value, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    if (isEphemeral()) {
        completionHandler();
        return;
    }

    postTask([value, completionHandler = WTFMove(completionHandler)](auto& store) mutable {
        if (RefPtr statisticsStore = store.m_statisticsStore)
            statisticsStore->setIsRunningTest(value);

        postTaskReply(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::setShouldClassifyResourcesBeforeDataRecordsRemoval(bool value, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    postTask([value, completionHandler = WTFMove(completionHandler)](auto& store) mutable {
        if (RefPtr statisticsStore = store.m_statisticsStore)
            statisticsStore->setShouldClassifyResourcesBeforeDataRecordsRemoval(value);

        postTaskReply(WTFMove(completionHandler));
    });
}

static Ref<SuspendableWorkQueue> sharedStatisticsQueue()
{
    static NeverDestroyed<Ref<SuspendableWorkQueue>> queue(SuspendableWorkQueue::create("WebResourceLoadStatisticsStore Process Data Queue"_s,  WorkQueue::QOS::Utility));
    return queue.get().copyRef();
}

WebResourceLoadStatisticsStore::WebResourceLoadStatisticsStore(NetworkSession& networkSession, const String& resourceLoadStatisticsDirectory, ShouldIncludeLocalhost shouldIncludeLocalhost, ResourceLoadStatistics::IsEphemeral isEphemeral)
    : m_networkSession(networkSession)
    , m_statisticsQueue(sharedStatisticsQueue())
    , m_dailyTasksTimer(RunLoop::mainSingleton(), this, &WebResourceLoadStatisticsStore::performDailyTasks)
    , m_isEphemeral(isEphemeral)
{
    RELEASE_ASSERT(RunLoop::isMain());

    // No daily tasks needed for ephemeral sessions since no resource load statistics are collected.
    if (isEphemeral == ResourceLoadStatistics::IsEphemeral::Yes)
        return;

    if (!resourceLoadStatisticsDirectory.isEmpty()) {
        postTask([resourceLoadStatisticsDirectory = resourceLoadStatisticsDirectory.isolatedCopy(), shouldIncludeLocalhost, sessionID = networkSession.sessionID()](auto& store) {
            Ref statisticsStore = ResourceLoadStatisticsStore::create(store, Ref { store.m_statisticsQueue }, shouldIncludeLocalhost, resourceLoadStatisticsDirectory, sessionID);
            store.m_statisticsStore = statisticsStore.copyRef();

            auto legacyPlistFilePath = FileSystem::pathByAppendingComponent(resourceLoadStatisticsDirectory, "full_browsing_session_resourceLog.plist"_s);
            if (FileSystem::fileExists(legacyPlistFilePath))
                FileSystem::deleteFile(legacyPlistFilePath);

            statisticsStore->didCreateNetworkProcess();
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

inline void WebResourceLoadStatisticsStore::postTask(WTF::Function<void(WebResourceLoadStatisticsStore&)>&& task)
{
    // Resource load statistics should not be captured for ephemeral sessions.
    RELEASE_ASSERT(!isEphemeral());

    ASSERT(RunLoop::isMain());
    m_statisticsQueue->dispatch([protectedThis = Ref { *this }, task = WTFMove(task)] {
        task(protectedThis.get());
    });
}

inline void WebResourceLoadStatisticsStore::postTaskReply(WTF::Function<void()>&& reply)
{
    ASSERT(!RunLoop::isMain());
    RunLoop::mainSingleton().dispatch(WTFMove(reply));
}

void WebResourceLoadStatisticsStore::destroyResourceLoadStatisticsStore(CompletionHandler<void()>&& completionHandler)
{
    RELEASE_ASSERT(RunLoop::isMain());

    if (isEphemeral()) {
        completionHandler();
        return;
    }

    postTask([completionHandler = WTFMove(completionHandler)](auto& store) mutable {
        store.m_statisticsStore = nullptr;
        postTaskReply(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::populateMemoryStoreFromDisk(CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    postTask([completionHandler = WTFMove(completionHandler)](auto& store) mutable {
        if (RefPtr statisticsStore = store.m_statisticsStore) {
            if (statisticsStore->isNewResourceLoadStatisticsDatabaseFile()) {
                statisticsStore->grandfatherExistingWebsiteData([completionHandler = WTFMove(completionHandler)]() mutable {
                    postTaskReply(WTFMove(completionHandler));
                });
                statisticsStore->setIsNewResourceLoadStatisticsDatabaseFile(false);
            } else
                postTaskReply([protectedThis = Ref { store }, completionHandler = WTFMove(completionHandler)]() mutable {
                    protectedThis->logTestingEvent("PopulatedWithoutGrandfathering"_s);
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

    if (CheckedPtr networkSession = m_networkSession.get()) {
        if (CheckedPtr storageSession = networkSession->networkStorageSession())
            storageSession->setTrackingPreventionDebugLoggingEnabled(value);
    }

    postTask([value, completionHandler = WTFMove(completionHandler)](auto& store) mutable {
        if (RefPtr statisticsStore = store.m_statisticsStore)
            statisticsStore->setResourceLoadStatisticsDebugMode(value);
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

    postTask([domain = WTFMove(domain).isolatedCopy(), completionHandler = WTFMove(completionHandler)](auto& store) mutable {
        if (RefPtr statisticsStore = store.m_statisticsStore)
            statisticsStore->setPrevalentResourceForDebugMode(domain);
        postTaskReply(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::scheduleStatisticsAndDataRecordsProcessing(CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    postTask([completionHandler = WTFMove(completionHandler)](auto& store) mutable {
        if (RefPtr statisticsStore = store.m_statisticsStore)
            statisticsStore->processStatisticsAndDataRecords();
        postTaskReply(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::statisticsDatabaseHasAllTables(CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    postTask([completionHandler = WTFMove(completionHandler)](auto& store) mutable {
        RefPtr statisticsStore = store.m_statisticsStore;
        if (!statisticsStore) {
            completionHandler(false);
            ASSERT_NOT_REACHED();
            return;
        }
        auto missingTables = statisticsStore->checkForMissingTablesInSchema();
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
    postTask([statistics = WTFMove(statistics), completionHandler = WTFMove(completionHandler)](auto& store) mutable {
        RefPtr statisticsStore = store.m_statisticsStore;
        if (!statisticsStore) {
            postTaskReply(WTFMove(completionHandler));
            return;
        }

        statisticsStore->mergeStatistics(WTFMove(statistics));
        postTaskReply(WTFMove(completionHandler));
        // We can cancel any pending request to process statistics since we're doing it synchronously below.
        statisticsStore->cancelPendingStatisticsProcessingRequest();

        // Fire before processing statistics to propagate user interaction as fast as possible to the network process.
        statisticsStore->updateCookieBlocking([protectedThis = Ref { store }]() {
            postTaskReply([protectedThis] {
                protectedThis->logTestingEvent("Statistics Updated"_s);
            });
        });
        statisticsStore->processStatisticsAndDataRecords();
    });
}

void WebResourceLoadStatisticsStore::hasStorageAccess(RegistrableDomain&& subFrameDomain, RegistrableDomain&& topFrameDomain, std::optional<FrameIdentifier> frameID, PageIdentifier pageID, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    if (isEphemeral())
        return hasStorageAccessEphemeral(WTFMove(subFrameDomain), WTFMove(topFrameDomain), frameID, pageID, WTFMove(completionHandler));

    CanRequestStorageAccessWithoutUserInteraction canRequestStorageAccessWithoutUserInteraction { CanRequestStorageAccessWithoutUserInteraction::No };
    if (CheckedPtr networkSession = m_networkSession.get()) {
        if (CheckedPtr storageSession = networkSession->networkStorageSession())
            canRequestStorageAccessWithoutUserInteraction = storageSession->canRequestStorageAccessForLoginOrCompatibilityPurposesWithoutPriorUserInteraction(subFrameDomain, topFrameDomain) ? CanRequestStorageAccessWithoutUserInteraction::Yes : CanRequestStorageAccessWithoutUserInteraction::No;
    }

    postTask([subFrameDomain = WTFMove(subFrameDomain).isolatedCopy(), topFrameDomain = WTFMove(topFrameDomain).isolatedCopy(), frameID, pageID, canRequestStorageAccessWithoutUserInteraction, completionHandler = WTFMove(completionHandler)](auto& store) mutable {
        RefPtr statisticsStore = store.m_statisticsStore;
        if (!statisticsStore) {
            postTaskReply([completionHandler = WTFMove(completionHandler)]() mutable {
                completionHandler(false);
            });
            return;
        }

        statisticsStore->hasStorageAccess(WTFMove(subFrameDomain), WTFMove(topFrameDomain), frameID, pageID, canRequestStorageAccessWithoutUserInteraction, [completionHandler = WTFMove(completionHandler)](bool hasStorageAccess) mutable {
            postTaskReply([completionHandler = WTFMove(completionHandler), hasStorageAccess]() mutable {
                completionHandler(hasStorageAccess);
            });
        });
    });
}

void WebResourceLoadStatisticsStore::hasStorageAccessEphemeral(const RegistrableDomain& subFrameDomain, const RegistrableDomain& topFrameDomain, std::optional<FrameIdentifier> frameID, PageIdentifier pageID, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(isEphemeral());

    if (CheckedPtr networkSession = m_networkSession.get()) {
        if (CheckedPtr storageSession = networkSession->networkStorageSession()) {
            completionHandler(storageSession->hasStorageAccess(subFrameDomain, topFrameDomain, frameID, pageID));
            return;
        }
    }
    completionHandler(false);
}

bool WebResourceLoadStatisticsStore::hasStorageAccessForFrame(const RegistrableDomain& resourceDomain, const RegistrableDomain& firstPartyDomain, FrameIdentifier frameID, PageIdentifier pageID)
{
    ASSERT(RunLoop::isMain());

    if (CheckedPtr networkSession = m_networkSession.get()) {
        if (CheckedPtr storageSession = networkSession->networkStorageSession())
            return storageSession->hasStorageAccess(resourceDomain, firstPartyDomain, frameID, pageID);
    }

    return false;
}

void WebResourceLoadStatisticsStore::callHasStorageAccessForFrameHandler(const RegistrableDomain& resourceDomain, const RegistrableDomain& firstPartyDomain, FrameIdentifier frameID, PageIdentifier pageID, CompletionHandler<void(bool hasAccess)>&& callback)
{
    ASSERT(RunLoop::isMain());

    if (CheckedPtr networkSession = m_networkSession.get()) {
        if (CheckedPtr storageSession = networkSession->networkStorageSession()) {
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

    CanRequestStorageAccessWithoutUserInteraction canRequestStorageAccessWithoutUserInteraction { CanRequestStorageAccessWithoutUserInteraction::No };
    std::optional<OrganizationStorageAccessPromptQuirk> storageAccessQuirk;
    if (CheckedPtr networkSession = m_networkSession.get()) {
        if (CheckedPtr storageSession = networkSession->networkStorageSession()) {
            canRequestStorageAccessWithoutUserInteraction = storageSession->canRequestStorageAccessForLoginOrCompatibilityPurposesWithoutPriorUserInteraction(subFrameDomain, topFrameDomain) ? CanRequestStorageAccessWithoutUserInteraction::Yes : CanRequestStorageAccessWithoutUserInteraction::No;
            storageAccessQuirk = storageSession->storageAccessQuirkForDomainPair(topFrameDomain, subFrameDomain);
        }
    }
    
    if (isEphemeral())
        return requestStorageAccessEphemeral(WTFMove(subFrameDomain), WTFMove(topFrameDomain), frameID, webPageID, webPageProxyID, scope, canRequestStorageAccessWithoutUserInteraction, WTFMove(storageAccessQuirk), WTFMove(completionHandler));

    auto statusHandler = [this, protectedThis = Ref { *this }, subFrameDomain = subFrameDomain.isolatedCopy(), topFrameDomain = topFrameDomain.isolatedCopy(), frameID, webPageID, webPageProxyID, scope, storageAccessQuirk = WTFMove(storageAccessQuirk), completionHandler = WTFMove(completionHandler)](StorageAccessStatus status) mutable {
        switch (status) {
        case StorageAccessStatus::CannotRequestAccess:
            completionHandler({ StorageAccessWasGranted::No, StorageAccessPromptWasShown::No, scope, topFrameDomain, subFrameDomain });
            return;
        case StorageAccessStatus::RequiresUserPrompt: {
            CheckedPtr networkSession = m_networkSession.get();
            if (!networkSession)
                return completionHandler({ StorageAccessWasGranted::No, StorageAccessPromptWasShown::No, scope, topFrameDomain, subFrameDomain });

            CompletionHandler<void(bool)> requestConfirmationCompletionHandler = [this, protectedThis, subFrameDomain, topFrameDomain, frameID, webPageID, scope, completionHandler = WTFMove(completionHandler)] (bool userDidGrantAccess) mutable {
                if (userDidGrantAccess)
                    grantStorageAccess(WTFMove(subFrameDomain), WTFMove(topFrameDomain), frameID, webPageID, StorageAccessPromptWasShown::Yes, scope, WTFMove(completionHandler));
                else
                    completionHandler({ StorageAccessWasGranted::No, StorageAccessPromptWasShown::Yes, scope, topFrameDomain, subFrameDomain });
            };

            networkSession->networkProcess().protectedParentProcessConnection()->sendWithAsyncReply(Messages::NetworkProcessProxy::RequestStorageAccessConfirm(webPageProxyID, frameID, subFrameDomain, topFrameDomain, storageAccessQuirk), WTFMove(requestConfirmationCompletionHandler));
            return;
        }
        case StorageAccessStatus::HasAccess:
            completionHandler({ storageAccessWasGrantedValueForFrame(frameID, subFrameDomain), StorageAccessPromptWasShown::No, scope, topFrameDomain, subFrameDomain });
            return;
        }
    };

    postTask([subFrameDomain = WTFMove(subFrameDomain).isolatedCopy(), topFrameDomain = WTFMove(topFrameDomain).isolatedCopy(), frameID, webPageID, scope, canRequestStorageAccessWithoutUserInteraction, statusHandler = WTFMove(statusHandler)](auto& store) mutable {
        RefPtr statisticsStore = store.m_statisticsStore;
        if (!statisticsStore) {
            postTaskReply([statusHandler = WTFMove(statusHandler)]() mutable {
                statusHandler(StorageAccessStatus::CannotRequestAccess);
            });
            return;
        }

        statisticsStore->requestStorageAccess(WTFMove(subFrameDomain), WTFMove(topFrameDomain), frameID, webPageID, scope, canRequestStorageAccessWithoutUserInteraction, [statusHandler = WTFMove(statusHandler)](StorageAccessStatus status) mutable {
            postTaskReply([statusHandler = WTFMove(statusHandler), status]() mutable {
                statusHandler(status);
            });
        });
    });
}

void WebResourceLoadStatisticsStore::setLoginStatus(RegistrableDomain&& domain, IsLoggedIn loggedInStatus, std::optional<LoginStatus>&& lastAuthentication, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    if (loggedInStatus == IsLoggedIn::LoggedIn) {
        auto loginStatusToSet = lastAuthentication && lastAuthentication->hasExpired() ? std::nullopt : std::optional(WTFMove(lastAuthentication));
        if (loginStatusToSet)
            loginStatusToSet->setTimeToLive(WebCore::LoginStatus::TimeToLiveLong);
        auto pair = std::make_pair(loggedInStatus, loginStatusToSet);
        m_loginStatus.set(domain, WTFMove(pair));
    } else
        m_loginStatus.remove(domain);
    completionHandler();
}

void WebResourceLoadStatisticsStore::isLoggedIn(RegistrableDomain&& domain, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    auto it = m_loginStatus.find(domain);
    completionHandler(it != m_loginStatus.end() && it->value.first == IsLoggedIn::LoggedIn);
}

void WebResourceLoadStatisticsStore::requestStorageAccessEphemeral(const RegistrableDomain& subFrameDomain, const RegistrableDomain& topFrameDomain, FrameIdentifier frameID, PageIdentifier webPageID, WebPageProxyIdentifier webPageProxyID, StorageAccessScope scope, CanRequestStorageAccessWithoutUserInteraction canRequestStorageAccessWithoutUserInteraction, std::optional<OrganizationStorageAccessPromptQuirk>&& storageAccessPromptQuirk, CompletionHandler<void(RequestStorageAccessResult)>&& completionHandler)
{
    ASSERT(isEphemeral());

    CheckedPtr networkSession = m_networkSession.get();
    if (!networkSession || (!m_domainsWithEphemeralUserInteraction.contains(subFrameDomain) && canRequestStorageAccessWithoutUserInteraction == CanRequestStorageAccessWithoutUserInteraction::No))
        return completionHandler({ StorageAccessWasGranted::No, StorageAccessPromptWasShown::No, scope, topFrameDomain, subFrameDomain });

    CompletionHandler<void(bool)> requestConfirmationCompletionHandler = [this, protectedThis = Ref { *this }, subFrameDomain, topFrameDomain, frameID, webPageID, scope, completionHandler = WTFMove(completionHandler)] (bool userDidGrantAccess) mutable {
        if (userDidGrantAccess)
            grantStorageAccessEphemeral(subFrameDomain, topFrameDomain, frameID, webPageID, StorageAccessPromptWasShown::Yes, scope, WTFMove(completionHandler));
        else
            completionHandler({ StorageAccessWasGranted::No, StorageAccessPromptWasShown::Yes, scope, topFrameDomain, subFrameDomain });
    };

    networkSession->networkProcess().protectedParentProcessConnection()->sendWithAsyncReply(Messages::NetworkProcessProxy::RequestStorageAccessConfirm(webPageProxyID, frameID, subFrameDomain, topFrameDomain, WTFMove(storageAccessPromptQuirk)), WTFMove(requestConfirmationCompletionHandler));
}

void WebResourceLoadStatisticsStore::requestStorageAccessUnderOpener(RegistrableDomain&& domainInNeedOfStorageAccess, PageIdentifier openerPageID, RegistrableDomain&& openerDomain)
{
    ASSERT(RunLoop::isMain());

    if (isEphemeral())
        return requestStorageAccessUnderOpenerEphemeral(WTFMove(domainInNeedOfStorageAccess), openerPageID, WTFMove(openerDomain));

    CanRequestStorageAccessWithoutUserInteraction canRequestStorageAccessWithoutUserInteraction { CanRequestStorageAccessWithoutUserInteraction::No };
    if (CheckedPtr networkSession = m_networkSession.get()) {
        if (CheckedPtr storageSession = networkSession->networkStorageSession())
            canRequestStorageAccessWithoutUserInteraction = storageSession->canRequestStorageAccessForLoginOrCompatibilityPurposesWithoutPriorUserInteraction(domainInNeedOfStorageAccess, openerDomain) ? CanRequestStorageAccessWithoutUserInteraction::Yes : CanRequestStorageAccessWithoutUserInteraction::No;
    }

    // It is safe to move the strings to the background queue without isolated copy here because they are r-value references
    // coming from IPC. Strings which are safe to move to other threads as long as nobody on this thread holds a reference
    // to those strings.
    postTask([domainInNeedOfStorageAccess = WTFMove(domainInNeedOfStorageAccess), openerPageID, openerDomain = WTFMove(openerDomain), canRequestStorageAccessWithoutUserInteraction](auto& store) mutable {
        if (RefPtr statisticsStore = store.m_statisticsStore)
            statisticsStore->requestStorageAccessUnderOpener(WTFMove(domainInNeedOfStorageAccess), openerPageID, WTFMove(openerDomain), canRequestStorageAccessWithoutUserInteraction);
    });
}

void WebResourceLoadStatisticsStore::requestStorageAccessUnderOpenerEphemeral(RegistrableDomain&& domainInNeedOfStorageAccess, PageIdentifier openerPageID, RegistrableDomain&& openerDomain)
{
    ASSERT(isEphemeral());

    if (CheckedPtr networkSession = m_networkSession.get()) {
        if (CheckedPtr storageSession = networkSession->networkStorageSession())
            storageSession->grantStorageAccess(WTFMove(domainInNeedOfStorageAccess), WTFMove(openerDomain), std::nullopt, openerPageID);
    }
}

void WebResourceLoadStatisticsStore::grantStorageAccess(RegistrableDomain&& subFrameDomain, RegistrableDomain&& topFrameDomain, FrameIdentifier frameID, PageIdentifier pageID, StorageAccessPromptWasShown promptWasShown, StorageAccessScope scope, CompletionHandler<void(RequestStorageAccessResult)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    postTask([subFrameDomain = WTFMove(subFrameDomain).isolatedCopy(), topFrameDomain = WTFMove(topFrameDomain).isolatedCopy(), frameID, pageID, promptWasShown, scope, completionHandler = WTFMove(completionHandler)](auto& store) mutable {
        RefPtr statisticsStore = store.m_statisticsStore;
        if (!statisticsStore) {
            postTaskReply([subFrameDomain = WTFMove(subFrameDomain).isolatedCopy(), topFrameDomain = WTFMove(topFrameDomain).isolatedCopy(), promptWasShown, scope, completionHandler = WTFMove(completionHandler)]() mutable {
                completionHandler({ StorageAccessWasGranted::No, promptWasShown, scope, topFrameDomain, subFrameDomain });
            });
            return;
        }

        statisticsStore->grantStorageAccess(WTFMove(subFrameDomain), WTFMove(topFrameDomain), frameID, pageID, promptWasShown, scope, [weakStore = ThreadSafeWeakPtr { store }, frameID, subFrameDomain = subFrameDomain.isolatedCopy(), topFrameDomain = topFrameDomain.isolatedCopy(), promptWasShown, scope, completionHandler = WTFMove(completionHandler)](StorageAccessWasGranted wasGrantedAccess) mutable {
            postTaskReply([weakStore = WTFMove(weakStore), frameID, subFrameDomain = WTFMove(subFrameDomain).isolatedCopy(), topFrameDomain = WTFMove(topFrameDomain).isolatedCopy(), wasGrantedAccess, promptWasShown, scope, completionHandler = WTFMove(completionHandler)]() mutable {
                RefPtr store { weakStore.get() };
                if (store && wasGrantedAccess == StorageAccessWasGranted::Yes) {
                    completionHandler({ store->storageAccessWasGrantedValueForFrame(frameID, subFrameDomain), promptWasShown, scope, topFrameDomain, subFrameDomain });
                    return;
                }
                completionHandler({ wasGrantedAccess, promptWasShown, scope, topFrameDomain, subFrameDomain });
            });
        });
    });
}

void WebResourceLoadStatisticsStore::grantStorageAccessEphemeral(const RegistrableDomain& subFrameDomain, const RegistrableDomain& topFrameDomain, FrameIdentifier frameID, PageIdentifier pageID, StorageAccessPromptWasShown promptWasShown, StorageAccessScope scope, CompletionHandler<void(RequestStorageAccessResult)>&& completionHandler)
{
    ASSERT(isEphemeral());

    if (CheckedPtr networkSession = m_networkSession.get()) {
        if (CheckedPtr storageSession = networkSession->networkStorageSession()) {
            storageSession->grantStorageAccess(subFrameDomain, topFrameDomain, frameID, pageID);
            completionHandler({ storageAccessWasGrantedValueForFrame(frameID, subFrameDomain), promptWasShown, scope, topFrameDomain, subFrameDomain });
            return;
        }
    }
    completionHandler({ StorageAccessWasGranted::No, promptWasShown, scope, topFrameDomain, subFrameDomain });
}

StorageAccessWasGranted WebResourceLoadStatisticsStore::grantStorageAccessInStorageSession(const RegistrableDomain& resourceDomain, const RegistrableDomain& firstPartyDomain, std::optional<FrameIdentifier> frameID, PageIdentifier pageID, StorageAccessScope scope)
{
    ASSERT(RunLoop::isMain());

    bool isStorageGranted = false;

    if (CheckedPtr networkSession = m_networkSession.get()) {
        if (CheckedPtr storageSession = networkSession->networkStorageSession()) {
            storageSession->grantStorageAccess(resourceDomain, firstPartyDomain, (scope == StorageAccessScope::PerFrame ? frameID : std::nullopt), pageID);
            ASSERT(storageSession->hasStorageAccess(resourceDomain, firstPartyDomain, frameID, pageID));
            isStorageGranted = true;
        }
    }

    if (!isStorageGranted)
        return StorageAccessWasGranted::No;

    if (!frameID)
        return StorageAccessWasGranted::Yes;

    return storageAccessWasGrantedValueForFrame(*frameID, resourceDomain);
}

void WebResourceLoadStatisticsStore::callGrantStorageAccessHandler(const RegistrableDomain& subFrameDomain, const RegistrableDomain& topFrameDomain, std::optional<FrameIdentifier> frameID, PageIdentifier pageID, StorageAccessScope scope, CompletionHandler<void(StorageAccessWasGranted)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    completionHandler(grantStorageAccessInStorageSession(subFrameDomain, topFrameDomain, frameID, pageID, scope));
}

void WebResourceLoadStatisticsStore::hasCookies(const RegistrableDomain& domain, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    if (CheckedPtr networkSession = m_networkSession.get()) {
        if (CheckedPtr storageSession = networkSession->networkStorageSession()) {
            storageSession->hasCookies(domain, WTFMove(completionHandler));
            return;
        }
    }
    
    completionHandler(false);
}

void WebResourceLoadStatisticsStore::setThirdPartyCookieBlockingMode(ThirdPartyCookieBlockingMode blockingMode)
{
    ASSERT(RunLoop::isMain());

    if (CheckedPtr networkSession = m_networkSession.get()) {
        if (CheckedPtr storageSession = networkSession->networkStorageSession())
            storageSession->setThirdPartyCookieBlockingMode(blockingMode);
        else
            ASSERT_NOT_REACHED();
    }

    if (isEphemeral())
        return;

    postTask([blockingMode](auto& store) {
        if (RefPtr statisticsStore = store.m_statisticsStore)
            statisticsStore->setThirdPartyCookieBlockingMode(blockingMode);
    });
}

void WebResourceLoadStatisticsStore::setSameSiteStrictEnforcementEnabled(SameSiteStrictEnforcementEnabled enabled)
{
    ASSERT(RunLoop::isMain());

    if (isEphemeral())
        return;

    postTask([enabled](auto& store) {
        if (RefPtr statisticsStore = store.m_statisticsStore)
            statisticsStore->setSameSiteStrictEnforcementEnabled(enabled);
    });
}

void WebResourceLoadStatisticsStore::setFirstPartyWebsiteDataRemovalMode(FirstPartyWebsiteDataRemovalMode mode, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    if (isEphemeral()) {
        completionHandler();
        return;
    }

    postTask([mode, completionHandler = WTFMove(completionHandler)](auto& store) mutable {
        if (RefPtr statisticsStore = store.m_statisticsStore) {
            statisticsStore->setFirstPartyWebsiteDataRemovalMode(mode);
            if (mode == FirstPartyWebsiteDataRemovalMode::AllButCookiesReproTestingTimeout)
                statisticsStore->setIsRunningTest(true);
        }
        postTaskReply([completionHandler = WTFMove(completionHandler)]() mutable {
            completionHandler();
        });
    });
}

void WebResourceLoadStatisticsStore::setPersistedDomains(const HashSet<RegistrableDomain>& domains)
{
    ASSERT(RunLoop::isMain());

    if (isEphemeral() || domains.isEmpty())
        return;

    postTask([domains = crossThreadCopy(domains)](auto& store) mutable {
        if (RefPtr statisticsStore = store.m_statisticsStore)
            statisticsStore->setPersistedDomains(WTFMove(domains));
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

    postTask([domain = domain.isolatedCopy(), completionHandler = WTFMove(completionHandler)](auto& store) mutable {
        if (RefPtr statisticsStore = store.m_statisticsStore)
            statisticsStore->setStandaloneApplicationDomain(WTFMove(domain));
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

    if (CheckedPtr networkSession = m_networkSession.get()) {
        if (CheckedPtr storageSession = networkSession->networkStorageSession()) {
            storageSession->setAppBoundDomains(WTFMove(domains));
            storageSession->setThirdPartyCookieBlockingMode(ThirdPartyCookieBlockingMode::AllExceptBetweenAppBoundDomains);
        }
    }

    postTask([domains = WTFMove(domainsCopy), completionHandler = WTFMove(completionHandler)](auto& store) mutable {
        if (RefPtr statisticsStore = store.m_statisticsStore) {
            statisticsStore->setAppBoundDomains(WTFMove(domains));
            statisticsStore->setThirdPartyCookieBlockingMode(ThirdPartyCookieBlockingMode::AllExceptBetweenAppBoundDomains);
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

    if (CheckedPtr networkSession = m_networkSession.get()) {
        if (CheckedPtr storageSession = networkSession->networkStorageSession()) {
            storageSession->setManagedDomains(WTFMove(domains));
            storageSession->setThirdPartyCookieBlockingMode(ThirdPartyCookieBlockingMode::AllExceptManagedDomains);
        }
    }

    postTask([domains = WTFMove(domainsCopy), completionHandler = WTFMove(completionHandler)](auto& store) mutable {
        if (RefPtr statisticsStore = store.m_statisticsStore) {
            statisticsStore->setManagedDomains(WTFMove(domains));
            statisticsStore->setThirdPartyCookieBlockingMode(ThirdPartyCookieBlockingMode::AllExceptManagedDomains);
        }
        postTaskReply(WTFMove(completionHandler));
    });
}
#endif

void WebResourceLoadStatisticsStore::didCreateNetworkProcess()
{
    ASSERT(RunLoop::isMain());

    postTask([](auto& store) {
        if (RefPtr statisticsStore = store.m_statisticsStore)
            statisticsStore->didCreateNetworkProcess();
    });
}

void WebResourceLoadStatisticsStore::removeAllStorageAccess(CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    if (CheckedPtr networkSession = m_networkSession.get()) {
        if (CheckedPtr storageSession = networkSession->networkStorageSession())
            storageSession->removeAllStorageAccess();
    }

    completionHandler();
}

void WebResourceLoadStatisticsStore::performDailyTasks()
{
    ASSERT(RunLoop::isMain());
    RELEASE_LOG(ResourceLoadStatistics, "WebResourceLoadStatisticsStore::performDailyTasks");

    postTask([](auto& store) {
        if (RefPtr statisticsStore = store.m_statisticsStore) {
            statisticsStore->includeTodayAsOperatingDateIfNecessary();
            statisticsStore->runIncrementalVacuumCommand();
        }
    });
}

void WebResourceLoadStatisticsStore::logFrameNavigation(RegistrableDomain&& targetDomain, RegistrableDomain&& topFrameDomain, RegistrableDomain&& sourceDomain, bool isRedirect, bool isMainFrame, Seconds delayAfterMainFrameDocumentLoad, bool wasPotentiallyInitiatedByUser)
{
    ASSERT(RunLoop::isMain());

    postTask([targetDomain = WTFMove(targetDomain).isolatedCopy(), topFrameDomain = WTFMove(topFrameDomain).isolatedCopy(), sourceDomain = WTFMove(sourceDomain).isolatedCopy(), isRedirect, isMainFrame, delayAfterMainFrameDocumentLoad, wasPotentiallyInitiatedByUser](auto& store) {
        if (RefPtr statisticsStore = store.m_statisticsStore)
            statisticsStore->logFrameNavigation(targetDomain, topFrameDomain, sourceDomain, isRedirect, isMainFrame, delayAfterMainFrameDocumentLoad, wasPotentiallyInitiatedByUser);
    });
}

void WebResourceLoadStatisticsStore::logUserInteraction(RegistrableDomain&& domain, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    // User interactions need to be logged for ephemeral sessions to support the Storage Access API.
    if (isEphemeral())
        return logUserInteractionEphemeral(WTFMove(domain), WTFMove(completionHandler));

    postTask([domain = WTFMove(domain).isolatedCopy(), completionHandler = WTFMove(completionHandler)](auto& store) mutable {
        auto innerCompletionHandler = [completionHandler = WTFMove(completionHandler)]() mutable {
            postTaskReply(WTFMove(completionHandler));
        };
        if (RefPtr statisticsStore = store.m_statisticsStore) {
            statisticsStore->logUserInteraction(domain, WTFMove(innerCompletionHandler));
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

void WebResourceLoadStatisticsStore::logCrossSiteLoadWithLinkDecoration(RegistrableDomain&& fromDomain, RegistrableDomain&& toDomain, DidFilterKnownLinkDecoration didFilterKnownLinkDecoration, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    ASSERT(fromDomain != toDomain);
    
    postTask([fromDomain = WTFMove(fromDomain).isolatedCopy(), toDomain = WTFMove(toDomain).isolatedCopy(), didFilterKnownLinkDecoration, completionHandler = WTFMove(completionHandler)](auto& store) mutable {
        if (RefPtr statisticsStore = store.m_statisticsStore)
            statisticsStore->logCrossSiteLoadWithLinkDecoration(fromDomain, toDomain, didFilterKnownLinkDecoration);
        postTaskReply(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::clearUserInteraction(RegistrableDomain&& domain, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    if (isEphemeral())
        return clearUserInteractionEphemeral(domain, WTFMove(completionHandler));

    postTask([domain = WTFMove(domain).isolatedCopy(), completionHandler = WTFMove(completionHandler)](auto& store) mutable {
        auto innerCompletionHandler = [completionHandler = WTFMove(completionHandler)]() mutable {
            postTaskReply(WTFMove(completionHandler));
        };
        if (RefPtr statisticsStore = store.m_statisticsStore) {
            statisticsStore->clearUserInteraction(domain, WTFMove(innerCompletionHandler));
            return;
        }
        innerCompletionHandler();
    });
}

void WebResourceLoadStatisticsStore::setTimeAdvanceForTesting(Seconds time, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    postTask([time, completionHandler = WTFMove(completionHandler)](auto& store) mutable {
        if (RefPtr statisticsStore = store.m_statisticsStore)
            statisticsStore->setTimeAdvanceForTesting(time);
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

    postTask([domain = WTFMove(domain).isolatedCopy(), completionHandler = WTFMove(completionHandler)](auto& store) mutable {
        bool hadUserInteraction = store.m_statisticsStore ? RefPtr { store.m_statisticsStore }->hasHadUserInteraction(domain, OperatingDatesWindow::Long) : false;
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
    
    postTask([domain = WTFMove(domain).isolatedCopy(), seconds, completionHandler = WTFMove(completionHandler)](auto& store) mutable {
        if (RefPtr statisticsStore = store.m_statisticsStore)
            statisticsStore->setLastSeen(domain, seconds);
        postTaskReply(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::mergeStatisticForTesting(RegistrableDomain&& domain, RegistrableDomain&& topFrameDomain1, RegistrableDomain&& topFrameDomain2, Seconds lastSeen, bool hadUserInteraction, Seconds mostRecentUserInteraction, bool isGrandfathered, bool isPrevalent, bool isVeryPrevalent, unsigned dataRecordsRemoved, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    postTask([domain = WTFMove(domain).isolatedCopy(), topFrameDomain1 = WTFMove(topFrameDomain1).isolatedCopy(), topFrameDomain2 = WTFMove(topFrameDomain2).isolatedCopy(), lastSeen, hadUserInteraction, mostRecentUserInteraction, isGrandfathered, isPrevalent, isVeryPrevalent, dataRecordsRemoved, completionHandler = WTFMove(completionHandler)](auto& store) mutable {
        if (RefPtr statisticsStore = store.m_statisticsStore) {
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

            statisticsStore->mergeStatistics(Vector<ResourceLoadStatistics>::from(WTFMove(statistic)));
        }
        postTaskReply(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::isRelationshipOnlyInDatabaseOnce(RegistrableDomain&& subDomain, RegistrableDomain&& topDomain, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    postTask([subDomain = WTFMove(subDomain).isolatedCopy(), topDomain = WTFMove(topDomain).isolatedCopy(), completionHandler = WTFMove(completionHandler)](auto& store) mutable {
        RefPtr statisticsStore = store.m_statisticsStore;
        if (!statisticsStore) {
            completionHandler(false);
            return;
        }
        
        bool isRelationshipOnlyInDatabaseOnce = statisticsStore->isCorrectSubStatisticsCount(subDomain, topDomain);
        
        postTaskReply([isRelationshipOnlyInDatabaseOnce, completionHandler = WTFMove(completionHandler)]() mutable {
            completionHandler(isRelationshipOnlyInDatabaseOnce);
        });
    });
}
    
void WebResourceLoadStatisticsStore::setPrevalentResource(RegistrableDomain&& domain, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    postTask([domain = WTFMove(domain).isolatedCopy(), completionHandler = WTFMove(completionHandler)](auto& store) mutable {
        if (RefPtr statisticsStore = store.m_statisticsStore)
            statisticsStore->setPrevalentResource(domain);
        postTaskReply(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::setVeryPrevalentResource(RegistrableDomain&& domain, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    postTask([domain = WTFMove(domain).isolatedCopy(), completionHandler = WTFMove(completionHandler)](auto& store) mutable {
        if (RefPtr statisticsStore = store.m_statisticsStore)
            statisticsStore->setVeryPrevalentResource(domain);
        postTaskReply(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::setMostRecentWebPushInteractionTime(RegistrableDomain&& domain, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    postTask([completionHandler = WTFMove(completionHandler), domain = WTFMove(domain).isolatedCopy()](auto& store) mutable {
        if (RefPtr statisticsStore = store.m_statisticsStore)
            statisticsStore->setMostRecentWebPushInteractionTime(domain);
        postTaskReply(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::dumpResourceLoadStatistics(CompletionHandler<void(String&&)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    postTask([completionHandler = WTFMove(completionHandler)](auto& store) mutable {
        auto innerCompletionHandler = [completionHandler = WTFMove(completionHandler)](const String& result) mutable {
            postTaskReply([result = result.isolatedCopy(), completionHandler = WTFMove(completionHandler)]() mutable {
                completionHandler(WTFMove(result));
            });
        };
        if (RefPtr statisticsStore = store.m_statisticsStore)
            statisticsStore->dumpResourceLoadStatistics(WTFMove(innerCompletionHandler));
        else
            innerCompletionHandler(String { emptyString() });
    });
}

void WebResourceLoadStatisticsStore::isPrevalentResource(RegistrableDomain&& domain, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    if (isEphemeral()) {
        completionHandler(false);
        return;
    }

    postTask([domain = WTFMove(domain).isolatedCopy(), completionHandler = WTFMove(completionHandler)](auto& store) mutable {
        bool isPrevalentResource = store.m_statisticsStore && RefPtr { store.m_statisticsStore }->isPrevalentResource(domain);
        postTaskReply([isPrevalentResource, completionHandler = WTFMove(completionHandler)]() mutable {
            completionHandler(isPrevalentResource);
        });
    });
}
    
void WebResourceLoadStatisticsStore::isVeryPrevalentResource(RegistrableDomain&& domain, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    postTask([domain = WTFMove(domain).isolatedCopy(), completionHandler = WTFMove(completionHandler)](auto& store) mutable {
        bool isVeryPrevalentResource = store.m_statisticsStore && RefPtr { store.m_statisticsStore }->isVeryPrevalentResource(domain);
        postTaskReply([isVeryPrevalentResource, completionHandler = WTFMove(completionHandler)]() mutable {
            completionHandler(isVeryPrevalentResource);
        });
    });
}

void WebResourceLoadStatisticsStore::isRegisteredAsSubresourceUnder(RegistrableDomain&& subresourceDomain, RegistrableDomain&& topFrameDomain, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    postTask([subresourceDomain = WTFMove(subresourceDomain).isolatedCopy(), topFrameDomain = WTFMove(topFrameDomain).isolatedCopy(), completionHandler = WTFMove(completionHandler)](auto& store) mutable {
        bool isRegisteredAsSubresourceUnder = store.m_statisticsStore && RefPtr { store.m_statisticsStore }->isRegisteredAsSubresourceUnder(subresourceDomain, topFrameDomain);
        postTaskReply([isRegisteredAsSubresourceUnder, completionHandler = WTFMove(completionHandler)]() mutable {
            completionHandler(isRegisteredAsSubresourceUnder);
        });
    });
}

void WebResourceLoadStatisticsStore::isRegisteredAsSubFrameUnder(RegistrableDomain&& subFrameDomain, RegistrableDomain&& topFrameDomain, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    postTask([subFrameDomain = WTFMove(subFrameDomain).isolatedCopy(), topFrameDomain = WTFMove(topFrameDomain).isolatedCopy(), completionHandler = WTFMove(completionHandler)](auto& store) mutable {
        bool isRegisteredAsSubFrameUnder = store.m_statisticsStore && RefPtr { store.m_statisticsStore }->isRegisteredAsSubFrameUnder(subFrameDomain, topFrameDomain);
        postTaskReply([isRegisteredAsSubFrameUnder, completionHandler = WTFMove(completionHandler)]() mutable {
            completionHandler(isRegisteredAsSubFrameUnder);
        });
    });
}

void WebResourceLoadStatisticsStore::isRegisteredAsRedirectingTo(RegistrableDomain&& domainRedirectedFrom, RegistrableDomain&& domainRedirectedTo, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    postTask([domainRedirectedFrom = WTFMove(domainRedirectedFrom).isolatedCopy(), domainRedirectedTo = WTFMove(domainRedirectedTo).isolatedCopy(), completionHandler = WTFMove(completionHandler)](auto& store) mutable {
        bool isRegisteredAsRedirectingTo = store.m_statisticsStore && RefPtr { store.m_statisticsStore }->isRegisteredAsRedirectingTo(domainRedirectedFrom, domainRedirectedTo);
        postTaskReply([isRegisteredAsRedirectingTo, completionHandler = WTFMove(completionHandler)]() mutable {
            completionHandler(isRegisteredAsRedirectingTo);
        });
    });
}

void WebResourceLoadStatisticsStore::clearPrevalentResource(RegistrableDomain&& domain, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    postTask([domain = WTFMove(domain).isolatedCopy(), completionHandler = WTFMove(completionHandler)](auto& store) mutable {
        if (RefPtr statisticsStore = store.m_statisticsStore)
            statisticsStore->clearPrevalentResource(domain);
        postTaskReply(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::setGrandfathered(RegistrableDomain&& domain, bool value, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    postTask([domain = WTFMove(domain).isolatedCopy(), value, completionHandler = WTFMove(completionHandler)](auto& store) mutable {
        if (RefPtr statisticsStore = store.m_statisticsStore)
            statisticsStore->setGrandfathered(domain, value);
        postTaskReply(WTFMove(completionHandler));
    });
}
    
void WebResourceLoadStatisticsStore::isGrandfathered(RegistrableDomain&& domain, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    postTask([completionHandler = WTFMove(completionHandler), domain = WTFMove(domain).isolatedCopy()](auto& store) mutable {
        bool isGrandFathered = store.m_statisticsStore && RefPtr { store.m_statisticsStore }->isGrandfathered(domain);
        postTaskReply([isGrandFathered, completionHandler = WTFMove(completionHandler)]() mutable {
            completionHandler(isGrandFathered);
        });
    });
}

void WebResourceLoadStatisticsStore::setSubframeUnderTopFrameDomain(RegistrableDomain&& subFrameDomain, RegistrableDomain&& topFrameDomain, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    postTask([completionHandler = WTFMove(completionHandler), subFrameDomain = WTFMove(subFrameDomain).isolatedCopy(), topFrameDomain = WTFMove(topFrameDomain).isolatedCopy()](auto& store) mutable {
        if (RefPtr statisticsStore = store.m_statisticsStore)
            statisticsStore->setSubframeUnderTopFrameDomain(subFrameDomain, topFrameDomain);
        postTaskReply(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::setSubresourceUnderTopFrameDomain(RegistrableDomain&& subresourceDomain, RegistrableDomain&& topFrameDomain, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    postTask([completionHandler = WTFMove(completionHandler), subresourceDomain = WTFMove(subresourceDomain).isolatedCopy(), topFrameDomain = WTFMove(topFrameDomain).isolatedCopy()](auto& store) mutable {
        if (RefPtr statisticsStore = store.m_statisticsStore)
            statisticsStore->setSubresourceUnderTopFrameDomain(subresourceDomain, topFrameDomain);
        postTaskReply(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::setSubresourceUniqueRedirectTo(RegistrableDomain&& subresourceDomain, RegistrableDomain&& domainRedirectedTo, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    postTask([completionHandler = WTFMove(completionHandler), subresourceDomain = WTFMove(subresourceDomain).isolatedCopy(), domainRedirectedTo = WTFMove(domainRedirectedTo).isolatedCopy()](auto& store) mutable {
        if (RefPtr statisticsStore = store.m_statisticsStore)
            statisticsStore->setSubresourceUniqueRedirectTo(subresourceDomain, domainRedirectedTo);
        postTaskReply(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::setSubresourceUniqueRedirectFrom(RegistrableDomain&& subresourceDomain, RegistrableDomain&& domainRedirectedFrom, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    postTask([completionHandler = WTFMove(completionHandler), subresourceDomain = WTFMove(subresourceDomain).isolatedCopy(), domainRedirectedFrom = WTFMove(domainRedirectedFrom).isolatedCopy()](auto& store) mutable {
        if (RefPtr statisticsStore = store.m_statisticsStore)
            statisticsStore->setSubresourceUniqueRedirectFrom(subresourceDomain, domainRedirectedFrom);
        postTaskReply(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::setTopFrameUniqueRedirectTo(RegistrableDomain&& topFrameDomain, RegistrableDomain&& domainRedirectedTo, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    postTask([completionHandler = WTFMove(completionHandler), topFrameDomain = WTFMove(topFrameDomain).isolatedCopy(), domainRedirectedTo = WTFMove(domainRedirectedTo).isolatedCopy()](auto& store) mutable {
        if (RefPtr statisticsStore = store.m_statisticsStore)
            statisticsStore->setTopFrameUniqueRedirectTo(topFrameDomain, domainRedirectedTo);
        postTaskReply(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::setTopFrameUniqueRedirectFrom(RegistrableDomain&& topFrameDomain, RegistrableDomain&& domainRedirectedFrom, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    postTask([completionHandler = WTFMove(completionHandler), topFrameDomain = WTFMove(topFrameDomain).isolatedCopy(), domainRedirectedFrom = WTFMove(domainRedirectedFrom).isolatedCopy()](auto& store) mutable {
        if (RefPtr statisticsStore = store.m_statisticsStore)
            statisticsStore->setTopFrameUniqueRedirectFrom(topFrameDomain, domainRedirectedFrom);
        postTaskReply(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::scheduleCookieBlockingUpdate(CompletionHandler<void()>&& completionHandler)
{
    // Helper function used by testing system. Should only be called from the main thread.
    ASSERT(RunLoop::isMain());

    postTask([completionHandler = WTFMove(completionHandler)](auto& store) mutable {
        RefPtr statisticsStore = store.m_statisticsStore;
        if (!statisticsStore) {
            postTaskReply(WTFMove(completionHandler));
            return;
        }

        statisticsStore->updateCookieBlocking([completionHandler = WTFMove(completionHandler)]() mutable {
            postTaskReply(WTFMove(completionHandler));
        });
    });
}

void WebResourceLoadStatisticsStore::scheduleClearInMemoryAndPersistent(ShouldGrandfatherStatistics shouldGrandfather, CompletionHandler<void()>&& completionHandler)
{
    if (isEphemeral())
        return clearInMemoryEphemeral(WTFMove(completionHandler));

    ASSERT(RunLoop::isMain());
    postTask([shouldGrandfather, completionHandler = WTFMove(completionHandler)](auto& store) mutable {
        RefPtr statisticsStore = store.m_statisticsStore;
        if (!statisticsStore) {
            if (shouldGrandfather == ShouldGrandfatherStatistics::Yes)
                RELEASE_LOG(ResourceLoadStatistics, "WebResourceLoadStatisticsStore::scheduleClearInMemoryAndPersistent Before being cleared, m_statisticsStore is null when trying to grandfather data.");

            postTaskReply(WTFMove(completionHandler));
            return;
        }

        auto callbackAggregator = CallbackAggregator::create([completionHandler = WTFMove(completionHandler)] () mutable {
            postTaskReply(WTFMove(completionHandler));
        });

        statisticsStore->clear([protectedThis = Ref { store }, shouldGrandfather, callbackAggregator] () mutable {
            if (shouldGrandfather == ShouldGrandfatherStatistics::Yes) {
                if (RefPtr statisticsStore = protectedThis->m_statisticsStore) {
                    statisticsStore->grandfatherExistingWebsiteData([callbackAggregator = WTFMove(callbackAggregator)]() mutable { });
                    statisticsStore->setIsNewResourceLoadStatisticsDatabaseFile(true);
                } else
                    RELEASE_LOG(ResourceLoadStatistics, "WebResourceLoadStatisticsStore::scheduleClearInMemoryAndPersistent After being cleared, m_statisticsStore is null when trying to grandfather data.");
            }
        });
        
        statisticsStore->cancelPendingStatisticsProcessingRequest();
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

    if (CheckedPtr networkSession = m_networkSession.get()) {
        if (CheckedPtr storageSession = networkSession->networkStorageSession())
            storageSession->removeAllStorageAccess();
    }

    completionHandler();
}

void WebResourceLoadStatisticsStore::domainIDExistsInDatabase(int domainID, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    postTask([domainID, completionHandler = WTFMove(completionHandler)](auto& store) mutable {
        RefPtr statisticsStore = store.m_statisticsStore;
        if (!statisticsStore) {
            completionHandler(false);
            return;
        }
        bool domainIDExists = statisticsStore->domainIDExistsInDatabase(domainID);
        postTaskReply([domainIDExists, completionHandler = WTFMove(completionHandler)]() mutable {
            completionHandler(domainIDExists);
        });
    });
}

void WebResourceLoadStatisticsStore::setTimeToLiveUserInteraction(Seconds seconds, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    postTask([seconds, completionHandler = WTFMove(completionHandler)](auto& store) mutable {
        if (RefPtr statisticsStore = store.m_statisticsStore)
            statisticsStore->setTimeToLiveUserInteraction(seconds);
        postTaskReply(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::setMinimumTimeBetweenDataRecordsRemoval(Seconds seconds, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    postTask([seconds, completionHandler = WTFMove(completionHandler)](auto& store) mutable  {
        if (RefPtr statisticsStore = store.m_statisticsStore)
            statisticsStore->setMinimumTimeBetweenDataRecordsRemoval(seconds);

        postTaskReply(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::setGrandfatheringTime(Seconds seconds, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    postTask([seconds, completionHandler = WTFMove(completionHandler)](auto& store) mutable  {
        if (RefPtr statisticsStore = store.m_statisticsStore)
            statisticsStore->setGrandfatheringTime(seconds);

        postTaskReply(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::setCacheMaxAgeCap(Seconds seconds, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    ASSERT(seconds >= 0_s);
    
    if (CheckedPtr networkSession = m_networkSession.get()) {
        if (CheckedPtr storageSession = networkSession->networkStorageSession())
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

    if (CheckedPtr networkSession = m_networkSession.get()) {
        if (CheckedPtr storageSession = networkSession->networkStorageSession()) {
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
            networkSession->networkProcess().protectedParentProcessConnection()->send(Messages::NetworkProcessProxy::SetDomainsWithUserInteraction(domainsWithUserInteractionQuirk), 0);
        }

        HashMap<TopFrameDomain, Vector<SubResourceDomain>> domainsWithStorageAccessQuirk;
        for (auto& [firstPartyDomain, requestingDomains] : domainsToBlock.domainsWithStorageAccess) {
            for (auto& requestingDomain : requestingDomains) {
                if (NetworkStorageSession::loginDomainMatchesRequestingDomain(firstPartyDomain, requestingDomain))
                    domainsWithStorageAccessQuirk.add(firstPartyDomain, Vector<SubResourceDomain> { }).iterator->value.append(requestingDomain);
            }
        }

        if (m_domainsWithCrossPageStorageAccessQuirk != domainsWithStorageAccessQuirk) {
            if (CheckedPtr storageSession = networkSession->networkStorageSession())
                storageSession->setDomainsWithCrossPageStorageAccess(domainsWithStorageAccessQuirk);
            networkSession->networkProcess().protectedParentProcessConnection()->sendWithAsyncReply(Messages::NetworkProcessProxy::SetDomainsWithCrossPageStorageAccess(domainsWithStorageAccessQuirk), [this, protectedThis = Ref { *this }, domainsWithStorageAccessQuirk] () mutable {
                m_domainsWithCrossPageStorageAccessQuirk = domainsWithStorageAccessQuirk;
            });
        }
    }

    completionHandler();
}

void WebResourceLoadStatisticsStore::setMaxStatisticsEntries(size_t maximumEntryCount, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    postTask([maximumEntryCount, completionHandler = WTFMove(completionHandler)](auto& store) mutable  {
        if (RefPtr statisticsStore = store.m_statisticsStore)
            statisticsStore->setMaxStatisticsEntries(maximumEntryCount);

        postTaskReply(WTFMove(completionHandler));
    });
}
    
void WebResourceLoadStatisticsStore::setPruneEntriesDownTo(size_t pruneTargetCount, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    postTask([pruneTargetCount, completionHandler = WTFMove(completionHandler)](auto& store) mutable  {
        if (RefPtr statisticsStore = store.m_statisticsStore)
            statisticsStore->setPruneEntriesDownTo(pruneTargetCount);

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
    if (CheckedPtr networkSession = m_networkSession.get()) {
        if (CheckedPtr storageSession = networkSession->networkStorageSession())
            storageSession->resetAppBoundDomains();
    }
#endif

#if ENABLE(MANAGED_DOMAINS)
    if (CheckedPtr networkSession = m_networkSession.get()) {
        if (CheckedPtr storageSession = networkSession->networkStorageSession())
            storageSession->resetManagedDomains();
    }
#endif

    postTask([completionHandler = WTFMove(completionHandler)](auto& store) mutable {
        if (RefPtr statisticsStore = store.m_statisticsStore)
            statisticsStore->resetParametersToDefaultValues();

        postTaskReply(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::logTestingEvent(const String& event)
{
    ASSERT(RunLoop::isMain());

    CheckedPtr networkSession = m_networkSession.get();
    if (networkSession && networkSession->enableResourceLoadStatisticsLogTestingEvent())
        networkSession->networkProcess().protectedParentProcessConnection()->send(Messages::NetworkProcessProxy::LogTestingEvent(m_networkSession->sessionID(), event), 0);
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
    postTask([domain = WTFMove(domain), completionHandler = WTFMove(completionHandler)](auto& store) mutable {
        if (RefPtr statisticsStore = store.m_statisticsStore)
            statisticsStore->removeDataForDomain(domain);

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

    postTask([completionHandler = WTFMove(completionHandler)](auto& store) mutable {
        auto domains = store.m_statisticsStore ? RefPtr { store.m_statisticsStore }->allDomains() : Vector<RegistrableDomain>();
        postTaskReply([domains = crossThreadCopy(WTFMove(domains)), completionHandler = WTFMove(completionHandler)]() mutable {
            completionHandler(WTFMove(domains));
        });
    });
}

void WebResourceLoadStatisticsStore::registrableDomainsWithLastAccessedTime(CompletionHandler<void(std::optional<HashMap<RegistrableDomain, WallTime>>&&)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    if (isEphemeral()) {
        completionHandler(std::nullopt);
        return;
    }

    postTask([completionHandler = WTFMove(completionHandler)](auto& store) mutable {
        std::optional<HashMap<RegistrableDomain, WallTime>> result;
        if (RefPtr statisticsStore = store.m_statisticsStore)
            result = statisticsStore->allDomainsWithLastAccessedTime();
        postTaskReply([result = crossThreadCopy(WTFMove(result)), completionHandler = WTFMove(completionHandler)]() mutable {
            completionHandler(WTFMove(result));
        });
    });
}

void WebResourceLoadStatisticsStore::registrableDomainsExemptFromWebsiteDataDeletion(CompletionHandler<void(HashSet<RegistrableDomain>&&)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    if (isEphemeral()) {
        completionHandler({ });
        return;
    }

    postTask([completionHandler = WTFMove(completionHandler)](auto& store) mutable {
        HashSet<RegistrableDomain> result;
        if (RefPtr statisticsStore = store.m_statisticsStore)
            result = statisticsStore->domainsExemptFromWebsiteDataDeletion();
        postTaskReply([result = crossThreadCopy(WTFMove(result)), completionHandler = WTFMove(completionHandler)]() mutable {
            completionHandler(WTFMove(result));
        });
    });
}

void WebResourceLoadStatisticsStore::deleteAndRestrictWebsiteDataForRegistrableDomains(OptionSet<WebsiteDataType> dataTypes, RegistrableDomainsToDeleteOrRestrictWebsiteDataFor&& domainsToDeleteAndRestrictWebsiteDataFor, CompletionHandler<void(HashSet<RegistrableDomain>&&)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    if (CheckedPtr networkSession = m_networkSession.get()) {
        networkSession->deleteAndRestrictWebsiteDataForRegistrableDomains(dataTypes, WTFMove(domainsToDeleteAndRestrictWebsiteDataFor), WTFMove(completionHandler));
        return;
    }

    completionHandler({ });
}

void WebResourceLoadStatisticsStore::registrableDomainsWithWebsiteData(OptionSet<WebsiteDataType> dataTypes, CompletionHandler<void(HashSet<RegistrableDomain>&&)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    if (CheckedPtr networkSession = m_networkSession.get()) {
        networkSession->registrableDomainsWithWebsiteData(dataTypes, WTFMove(completionHandler));
        return;
    }

    completionHandler({ });
}

void WebResourceLoadStatisticsStore::aggregatedThirdPartyData(CompletionHandler<void(Vector<ITPThirdPartyData>&&)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    postTask([completionHandler = WTFMove(completionHandler)](auto& store) mutable {
        RefPtr statisticsStore = store.m_statisticsStore;
        if (!statisticsStore) {
            postTaskReply([completionHandler = WTFMove(completionHandler)]() mutable {
                completionHandler({ });
            });
            return;
        }
        auto thirdPartyData = statisticsStore->aggregatedThirdPartyData();
        postTaskReply([thirdPartyData = WTFMove(thirdPartyData), completionHandler = WTFMove(completionHandler)]() mutable {
            completionHandler(WTFMove(thirdPartyData));
        });
    });
}

void WebResourceLoadStatisticsStore::suspend(CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    sharedStatisticsQueue()->suspend(ResourceLoadStatisticsStore::interruptAllDatabases, WTFMove(completionHandler));
}

void WebResourceLoadStatisticsStore::resume()
{
    ASSERT(RunLoop::isMain());

    sharedStatisticsQueue()->resume();
}

void WebResourceLoadStatisticsStore::insertExpiredStatisticForTesting(RegistrableDomain&& domain, unsigned numberOfOperatingDaysPassed, bool hadUserInteraction, bool isScheduledForAllButCookieDataRemoval, bool isPrevalent, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    postTask([domain = WTFMove(domain).isolatedCopy(), numberOfOperatingDaysPassed, hadUserInteraction, isScheduledForAllButCookieDataRemoval, isPrevalent, completionHandler = WTFMove(completionHandler)](auto& store) mutable {
        if (RefPtr statisticsStore = store.m_statisticsStore)
            statisticsStore->insertExpiredStatisticForTesting(WTFMove(domain), numberOfOperatingDaysPassed, hadUserInteraction, isScheduledForAllButCookieDataRemoval, isPrevalent);
        postTaskReply(WTFMove(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::recordFrameLoadForStorageAccess(WebPageProxyIdentifier webPageProxyID, WebCore::FrameIdentifier frameID, const WebCore::RegistrableDomain& domain)
{
    auto currentTime = WallTime::now();
    StorageAccessRequestRecordKey key { frameID, domain };
    auto& recordValue = m_storageAccessRequestRecords.ensure(key, [&]() {
        return StorageAccessRequestRecordValue { webPageProxyID, { }, currentTime };
    }).iterator->value;
    ASSERT(recordValue.webPageProxyID == webPageProxyID);
    recordValue.lastLoadTime = currentTime;
}

void WebResourceLoadStatisticsStore::clearFrameLoadRecordsForStorageAccess(WebCore::FrameIdentifier frameID)
{
    m_storageAccessRequestRecords.removeIf([&](auto& record) {
        return record.key.first == frameID;
    });
}

void WebResourceLoadStatisticsStore::clearFrameLoadRecordsForStorageAccess(WebPageProxyIdentifier webPageProxyID)
{
    m_storageAccessRequestRecords.removeIf([&](auto& record) {
        return record.value.webPageProxyID == webPageProxyID;
    });
}

StorageAccessWasGranted WebResourceLoadStatisticsStore::storageAccessWasGrantedValueForFrame(WebCore::FrameIdentifier frameID, const WebCore::RegistrableDomain& domain)
{
    StorageAccessRequestRecordKey key { frameID, domain };
    auto iter = m_storageAccessRequestRecords.find(key);
    if (iter == m_storageAccessRequestRecords.end())
        return StorageAccessWasGranted::Yes;

    auto& value = iter->value;
    if (!value.lastRequestTime)
        value.lastRequestTime = WallTime::now();

    return value.lastRequestTime.value() < value.lastLoadTime ? StorageAccessWasGranted::Yes : StorageAccessWasGranted::YesWithException;
}

} // namespace WebKit

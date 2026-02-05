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
#include "StorageAccessPermissionChangeObserver.h"
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
#include <WebCore/PermissionState.h>
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
        WebsiteDataType::SearchFieldRecentSearches,
        WebsiteDataType::SessionStorage,
        WebsiteDataType::ServiceWorkerRegistrations,
        WebsiteDataType::FileSystem,
#if ENABLE(SCREEN_TIME)
        WebsiteDataType::ScreenTime,
#endif
        WebsiteDataType::EnhancedSecurityRecord
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

    postTask([value, completionHandler = WTF::move(completionHandler)](auto& store) mutable {
        if (RefPtr statisticsStore = store.m_statisticsStore)
            statisticsStore->setIsRunningTest(value);

        postTaskReply(WTF::move(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::setShouldClassifyResourcesBeforeDataRecordsRemoval(bool value, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    postTask([value, completionHandler = WTF::move(completionHandler)](auto& store) mutable {
        if (RefPtr statisticsStore = store.m_statisticsStore)
            statisticsStore->setShouldClassifyResourcesBeforeDataRecordsRemoval(value);

        postTaskReply(WTF::move(completionHandler));
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
    , m_dailyTasksTimer(RunLoop::mainSingleton(), "WebResourceLoadStatisticsStore::DailyTasksTimer"_s, this, &WebResourceLoadStatisticsStore::performDailyTasks)
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

    auto callbackAggregator = CallbackAggregator::create([completionHandler = WTF::move(completionHandler)] () mutable {
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
    m_statisticsQueue->dispatch([protectedThis = Ref { *this }, task = WTF::move(task)] {
        task(protectedThis.get());
    });
}

inline void WebResourceLoadStatisticsStore::postTaskReply(WTF::Function<void()>&& reply)
{
    ASSERT(!RunLoop::isMain());
    RunLoop::mainSingleton().dispatch(WTF::move(reply));
}

void WebResourceLoadStatisticsStore::destroyResourceLoadStatisticsStore(CompletionHandler<void()>&& completionHandler)
{
    RELEASE_ASSERT(RunLoop::isMain());

    if (isEphemeral()) {
        completionHandler();
        return;
    }

    postTask([completionHandler = WTF::move(completionHandler)](auto& store) mutable {
        store.m_statisticsStore = nullptr;
        postTaskReply(WTF::move(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::populateMemoryStoreFromDisk(CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    postTask([completionHandler = WTF::move(completionHandler)](auto& store) mutable {
        if (RefPtr statisticsStore = store.m_statisticsStore) {
            if (statisticsStore->isNewResourceLoadStatisticsDatabaseFile()) {
                statisticsStore->grandfatherExistingWebsiteData([completionHandler = WTF::move(completionHandler)]() mutable {
                    postTaskReply(WTF::move(completionHandler));
                });
                statisticsStore->setIsNewResourceLoadStatisticsDatabaseFile(false);
            } else
                postTaskReply([protectedThis = Ref { store }, completionHandler = WTF::move(completionHandler)]() mutable {
                    protectedThis->logTestingEvent("PopulatedWithoutGrandfathering"_s);
                    completionHandler();
                });
        } else
            postTaskReply(WTF::move(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::loadWebsitesWithUserInteraction(CompletionHandler<void(HashSet<RegistrableDomain>&&)>&& completionHandler)
{
    if (isEphemeral())
        return completionHandler({ });

    ASSERT(RunLoop::isMain());
    postTask([completionHandler = WTF::move(completionHandler)](auto& store) mutable {
        HashSet<RegistrableDomain> domains;
        if (RefPtr statisticsStore = store.m_statisticsStore)
            domains = statisticsStore->loadWebsitesWithUserInteraction();
        store.postTaskReply([domains = crossThreadCopy(WTF::move(domains)), completionHandler = WTF::move(completionHandler)] mutable {
            completionHandler(WTF::move(domains));
        });
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

    postTask([value, completionHandler = WTF::move(completionHandler)](auto& store) mutable {
        if (RefPtr statisticsStore = store.m_statisticsStore)
            statisticsStore->setResourceLoadStatisticsDebugMode(value);
        postTaskReply(WTF::move(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::setPrevalentResourceForDebugMode(RegistrableDomain&& domain, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    if (isEphemeral()) {
        completionHandler();
        return;
    }

    postTask([domain = WTF::move(domain).isolatedCopy(), completionHandler = WTF::move(completionHandler)](auto& store) mutable {
        if (RefPtr statisticsStore = store.m_statisticsStore)
            statisticsStore->setPrevalentResourceForDebugMode(domain);
        postTaskReply(WTF::move(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::scheduleStatisticsAndDataRecordsProcessing(CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    postTask([completionHandler = WTF::move(completionHandler)](auto& store) mutable {
        if (RefPtr statisticsStore = store.m_statisticsStore) {
            statisticsStore->processStatisticsAndDataRecords([weakStore = ThreadSafeWeakPtr { store }, completionHandler = WTF::move(completionHandler)] () mutable {
                if (RefPtr store = weakStore.get())
                    store->postTaskReply(WTF::move(completionHandler));
                else
                    completionHandler();
            });
        } else
            postTaskReply(WTF::move(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::statisticsDatabaseHasAllTables(CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    postTask([completionHandler = WTF::move(completionHandler)](auto& store) mutable {
        RefPtr statisticsStore = store.m_statisticsStore;
        if (!statisticsStore) {
            completionHandler(false);
            ASSERT_NOT_REACHED();
            return;
        }
        auto missingTables = statisticsStore->checkForMissingTablesInSchema();
        postTaskReply([hasAllTables = missingTables ? false : true, completionHandler = WTF::move(completionHandler)] () mutable {
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
    postTask([statistics = WTF::move(statistics), completionHandler = WTF::move(completionHandler)](auto& store) mutable {
        RefPtr statisticsStore = store.m_statisticsStore;
        if (!statisticsStore) {
            postTaskReply(WTF::move(completionHandler));
            return;
        }

        statisticsStore->mergeStatistics(WTF::move(statistics));
        postTaskReply(WTF::move(completionHandler));
        // We can cancel any pending request to process statistics since we're doing it synchronously below.
        statisticsStore->cancelPendingStatisticsProcessingRequest();

        // Fire before processing statistics to propagate user interaction as fast as possible to the network process.
        statisticsStore->updateCookieBlocking([protectedThis = Ref { store }]() {
            postTaskReply([protectedThis] {
                protectedThis->logTestingEvent("Statistics Updated"_s);
            });
        });
        statisticsStore->processStatisticsAndDataRecords([] { });
    });
}

void WebResourceLoadStatisticsStore::hasStorageAccess(RegistrableDomain&& subFrameDomain, RegistrableDomain&& topFrameDomain, std::optional<FrameIdentifier> frameID, PageIdentifier pageID, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    if (isEphemeral())
        return hasStorageAccessEphemeral(WTF::move(subFrameDomain), WTF::move(topFrameDomain), frameID, pageID, WTF::move(completionHandler));

    CanRequestStorageAccessWithoutUserInteraction canRequestStorageAccessWithoutUserInteraction { CanRequestStorageAccessWithoutUserInteraction::No };
    if (CheckedPtr networkSession = m_networkSession.get()) {
        if (CheckedPtr storageSession = networkSession->networkStorageSession())
            canRequestStorageAccessWithoutUserInteraction = storageSession->canRequestStorageAccessForLoginOrCompatibilityPurposesWithoutPriorUserInteraction(subFrameDomain, topFrameDomain) ? CanRequestStorageAccessWithoutUserInteraction::Yes : CanRequestStorageAccessWithoutUserInteraction::No;
    }

    postTask([subFrameDomain = WTF::move(subFrameDomain).isolatedCopy(), topFrameDomain = WTF::move(topFrameDomain).isolatedCopy(), frameID, pageID, canRequestStorageAccessWithoutUserInteraction, completionHandler = WTF::move(completionHandler)](auto& store) mutable {
        RefPtr statisticsStore = store.m_statisticsStore;
        if (!statisticsStore) {
            postTaskReply([completionHandler = WTF::move(completionHandler)]() mutable {
                completionHandler(false);
            });
            return;
        }

        statisticsStore->hasStorageAccess(WTF::move(subFrameDomain), WTF::move(topFrameDomain), frameID, pageID, canRequestStorageAccessWithoutUserInteraction, [completionHandler = WTF::move(completionHandler)](bool hasStorageAccess) mutable {
            postTaskReply([completionHandler = WTF::move(completionHandler), hasStorageAccess]() mutable {
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

void WebResourceLoadStatisticsStore::requestStorageAccess(RegistrableDomain&& subFrameDomain, RegistrableDomain&& topFrameDomain, FrameIdentifier frameID, PageIdentifier webPageID, WebPageProxyIdentifier webPageProxyID, StorageAccessScope scope, HasOrShouldIgnoreUserGesture hasOrShouldIgnoreUserGesture, CompletionHandler<void(RequestStorageAccessResult)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    if (subFrameDomain == topFrameDomain) {
        completionHandler({ StorageAccessWasGranted::Yes, StorageAccessPromptWasShown::No, scope, WTF::move(topFrameDomain), WTF::move(subFrameDomain) });
        return;
    }

    if (hasOrShouldIgnoreUserGesture == HasOrShouldIgnoreUserGesture::No) {
        auto it = m_domainsGrantedStorageAccessPermissionInPage.find(webPageProxyID);
        if (it == m_domainsGrantedStorageAccessPermissionInPage.end() || !it->value.contains({ topFrameDomain, subFrameDomain }))
            return completionHandler({ StorageAccessWasGranted::No, StorageAccessPromptWasShown::No, scope, topFrameDomain, subFrameDomain });
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
        return requestStorageAccessEphemeral(WTF::move(subFrameDomain), WTF::move(topFrameDomain), frameID, webPageID, webPageProxyID, scope, canRequestStorageAccessWithoutUserInteraction, WTF::move(storageAccessQuirk), WTF::move(completionHandler));

    auto statusHandler = [this, protectedThis = Ref { *this }, subFrameDomain = subFrameDomain.isolatedCopy(), topFrameDomain = topFrameDomain.isolatedCopy(), frameID, webPageID, webPageProxyID, scope, storageAccessQuirk = WTF::move(storageAccessQuirk), completionHandler = WTF::move(completionHandler)](StorageAccessStatus status) mutable {
        switch (status) {
        case StorageAccessStatus::CannotRequestAccess:
            completionHandler({ StorageAccessWasGranted::No, StorageAccessPromptWasShown::No, scope, topFrameDomain, subFrameDomain });
            return;
        case StorageAccessStatus::RequiresUserPrompt: {
            CheckedPtr networkSession = m_networkSession.get();
            if (!networkSession)
                return completionHandler({ StorageAccessWasGranted::No, StorageAccessPromptWasShown::No, scope, topFrameDomain, subFrameDomain });

            CompletionHandler<void(bool)> requestConfirmationCompletionHandler = [this, protectedThis, subFrameDomain, topFrameDomain, frameID, webPageID, webPageProxyID, scope, completionHandler = WTF::move(completionHandler)] (bool userDidGrantAccess) mutable {
                if (userDidGrantAccess)
                    grantStorageAccess(WTF::move(subFrameDomain), WTF::move(topFrameDomain), frameID, webPageID, webPageProxyID, StorageAccessPromptWasShown::Yes, scope, WTF::move(completionHandler));
                else
                    completionHandler({ StorageAccessWasGranted::No, StorageAccessPromptWasShown::Yes, scope, topFrameDomain, subFrameDomain });
            };

            protect(networkSession->networkProcess().parentProcessConnection())->sendWithAsyncReply(Messages::NetworkProcessProxy::RequestStorageAccessConfirm(webPageProxyID, frameID, subFrameDomain, topFrameDomain, storageAccessQuirk), WTF::move(requestConfirmationCompletionHandler));
            return;
        }
        case StorageAccessStatus::HasAccess:
            completionHandler({ storageAccessWasGrantedValueForFrame(frameID, subFrameDomain), StorageAccessPromptWasShown::No, scope, topFrameDomain, subFrameDomain });
            return;
        }
    };

    postTask([subFrameDomain = WTF::move(subFrameDomain).isolatedCopy(), topFrameDomain = WTF::move(topFrameDomain).isolatedCopy(), frameID, webPageID, scope, canRequestStorageAccessWithoutUserInteraction, statusHandler = WTF::move(statusHandler)](auto& store) mutable {
        RefPtr statisticsStore = store.m_statisticsStore;
        if (!statisticsStore) {
            postTaskReply([statusHandler = WTF::move(statusHandler)]() mutable {
                statusHandler(StorageAccessStatus::CannotRequestAccess);
            });
            return;
        }

        statisticsStore->requestStorageAccess(WTF::move(subFrameDomain), WTF::move(topFrameDomain), frameID, webPageID, scope, canRequestStorageAccessWithoutUserInteraction, [statusHandler = WTF::move(statusHandler)](StorageAccessStatus status) mutable {
            postTaskReply([statusHandler = WTF::move(statusHandler), status]() mutable {
                statusHandler(status);
            });
        });
    });
}

void WebResourceLoadStatisticsStore::queryStorageAccessPermission(SubFrameDomain&& subFrameDomain, TopFrameDomain&& topFrameDomain, std::optional<WebPageProxyIdentifier> webPageProxyID, CompletionHandler<void(PermissionState)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    if (isEphemeral()) {
        if (webPageProxyID) {
            if (auto it = m_domainsGrantedStorageAccessPermissionInPage.find(*webPageProxyID); it != m_domainsGrantedStorageAccessPermissionInPage.end())
                return completionHandler(it->value.contains({ topFrameDomain, subFrameDomain }) ? PermissionState::Granted : PermissionState::Prompt);
        }
        return completionHandler(PermissionState::Prompt);
    }

    postTask([subFrameDomain = WTF::move(subFrameDomain).isolatedCopy(), topFrameDomain = WTF::move(topFrameDomain).isolatedCopy(), completionHandler = WTF::move(completionHandler)](auto& store) mutable {
        RefPtr statisticsStore = store.m_statisticsStore;
        if (!statisticsStore) {
            return postTaskReply([completionHandler = WTF::move(completionHandler)] mutable {
                completionHandler(PermissionState::Denied);
            });
        }

        statisticsStore->queryStorageAccessPermission(WTF::move(subFrameDomain), WTF::move(topFrameDomain), [completionHandler = WTF::move(completionHandler)](PermissionState permissionState) mutable {
            postTaskReply([completionHandler = WTF::move(completionHandler), permissionState] mutable {
                completionHandler(permissionState);
            });
        });
    });
}

void WebResourceLoadStatisticsStore::startListeningForStorageAccessPermissionChanges(StorageAccessPermissionChangeObserver& observer, TopFrameDomain&& topFrameDomain, SubFrameDomain&& subFrameDomain)
{
    m_storageAccessPermissionChangeObservers.ensure({ WTF::move(topFrameDomain), WTF::move(subFrameDomain) }, [] {
        return WeakHashSet<StorageAccessPermissionChangeObserver> { };
    }).iterator->value.add(observer);
}

void WebResourceLoadStatisticsStore::stopListeningForStorageAccessPermissionChanges(StorageAccessPermissionChangeObserver& observer, TopFrameDomain&& topFrameDomain, SubFrameDomain&& subFrameDomain)
{
    if (auto it = m_storageAccessPermissionChangeObservers.find({ WTF::move(topFrameDomain), WTF::move(subFrameDomain) }); it != m_storageAccessPermissionChangeObservers.end())
        it->value.remove(observer);
}

void WebResourceLoadStatisticsStore::stopListeningForStorageAccessPermissionChanges(StorageAccessPermissionChangeObserver& observer)
{
    m_storageAccessPermissionChangeObservers.removeIf([&](auto& entry) {
        entry.value.remove(observer);
        return entry.value.isEmptyIgnoringNullReferences();
    });
}

void WebResourceLoadStatisticsStore::setLoginStatus(RegistrableDomain&& domain, IsLoggedIn loggedInStatus, std::optional<LoginStatus>&& lastAuthentication, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    if (loggedInStatus == IsLoggedIn::LoggedIn) {
        auto loginStatusToSet = lastAuthentication && lastAuthentication->hasExpired() ? std::nullopt : std::optional(WTF::move(lastAuthentication));
        if (loginStatusToSet)
            loginStatusToSet->setTimeToLive(WebCore::LoginStatus::TimeToLiveLong);
        auto pair = std::make_pair(loggedInStatus, loginStatusToSet);
        m_loginStatus.set(domain, WTF::move(pair));
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

    CompletionHandler<void(bool)> requestConfirmationCompletionHandler = [this, protectedThis = Ref { *this }, subFrameDomain, topFrameDomain, frameID, webPageID, scope, completionHandler = WTF::move(completionHandler)] (bool userDidGrantAccess) mutable {
        if (userDidGrantAccess)
            grantStorageAccessEphemeral(subFrameDomain, topFrameDomain, frameID, webPageID, StorageAccessPromptWasShown::Yes, scope, WTF::move(completionHandler));
        else
            completionHandler({ StorageAccessWasGranted::No, StorageAccessPromptWasShown::Yes, scope, topFrameDomain, subFrameDomain });
    };

    protect(networkSession->networkProcess().parentProcessConnection())->sendWithAsyncReply(Messages::NetworkProcessProxy::RequestStorageAccessConfirm(webPageProxyID, frameID, subFrameDomain, topFrameDomain, WTF::move(storageAccessPromptQuirk)), WTF::move(requestConfirmationCompletionHandler));
}

void WebResourceLoadStatisticsStore::requestStorageAccessUnderOpener(RegistrableDomain&& domainInNeedOfStorageAccess, PageIdentifier openerPageID, RegistrableDomain&& openerDomain)
{
    ASSERT(RunLoop::isMain());

    if (isEphemeral())
        return requestStorageAccessUnderOpenerEphemeral(WTF::move(domainInNeedOfStorageAccess), openerPageID, WTF::move(openerDomain));

    CanRequestStorageAccessWithoutUserInteraction canRequestStorageAccessWithoutUserInteraction { CanRequestStorageAccessWithoutUserInteraction::No };
    if (CheckedPtr networkSession = m_networkSession.get()) {
        if (CheckedPtr storageSession = networkSession->networkStorageSession())
            canRequestStorageAccessWithoutUserInteraction = storageSession->canRequestStorageAccessForLoginOrCompatibilityPurposesWithoutPriorUserInteraction(domainInNeedOfStorageAccess, openerDomain) ? CanRequestStorageAccessWithoutUserInteraction::Yes : CanRequestStorageAccessWithoutUserInteraction::No;
    }

    // It is safe to move the strings to the background queue without isolated copy here because they are r-value references
    // coming from IPC. Strings which are safe to move to other threads as long as nobody on this thread holds a reference
    // to those strings.
    postTask([domainInNeedOfStorageAccess = WTF::move(domainInNeedOfStorageAccess), openerPageID, openerDomain = WTF::move(openerDomain), canRequestStorageAccessWithoutUserInteraction](auto& store) mutable {
        if (RefPtr statisticsStore = store.m_statisticsStore)
            statisticsStore->requestStorageAccessUnderOpener(WTF::move(domainInNeedOfStorageAccess), openerPageID, WTF::move(openerDomain), canRequestStorageAccessWithoutUserInteraction);
    });
}

void WebResourceLoadStatisticsStore::requestStorageAccessUnderOpenerEphemeral(RegistrableDomain&& domainInNeedOfStorageAccess, PageIdentifier openerPageID, RegistrableDomain&& openerDomain)
{
    ASSERT(isEphemeral());

    if (CheckedPtr networkSession = m_networkSession.get()) {
        if (CheckedPtr storageSession = networkSession->networkStorageSession())
            storageSession->grantStorageAccess(WTF::move(domainInNeedOfStorageAccess), WTF::move(openerDomain), std::nullopt, openerPageID);
    }
}

void WebResourceLoadStatisticsStore::grantStorageAccess(RegistrableDomain&& subFrameDomain, RegistrableDomain&& topFrameDomain, FrameIdentifier frameID, PageIdentifier pageID, WebPageProxyIdentifier webPageProxyID, StorageAccessPromptWasShown promptWasShown, StorageAccessScope scope, CompletionHandler<void(RequestStorageAccessResult)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    if (promptWasShown == StorageAccessPromptWasShown::Yes)
        wasGrantedStorageAccessPermissionInPage(webPageProxyID, topFrameDomain, subFrameDomain);

    postTask([subFrameDomain = WTF::move(subFrameDomain).isolatedCopy(), topFrameDomain = WTF::move(topFrameDomain).isolatedCopy(), frameID, pageID, promptWasShown, scope, completionHandler = WTF::move(completionHandler)](auto& store) mutable {
        RefPtr statisticsStore = store.m_statisticsStore;
        if (!statisticsStore) {
            postTaskReply([subFrameDomain = WTF::move(subFrameDomain).isolatedCopy(), topFrameDomain = WTF::move(topFrameDomain).isolatedCopy(), promptWasShown, scope, completionHandler = WTF::move(completionHandler)]() mutable {
                completionHandler({ StorageAccessWasGranted::No, promptWasShown, scope, topFrameDomain, subFrameDomain });
            });
            return;
        }

        statisticsStore->grantStorageAccess(WTF::move(subFrameDomain), WTF::move(topFrameDomain), frameID, pageID, promptWasShown, scope, [weakStore = ThreadSafeWeakPtr { store }, frameID, subFrameDomain = subFrameDomain.isolatedCopy(), topFrameDomain = topFrameDomain.isolatedCopy(), promptWasShown, scope, completionHandler = WTF::move(completionHandler)](StorageAccessWasGranted wasGrantedAccess) mutable {
            postTaskReply([weakStore = WTF::move(weakStore), frameID, subFrameDomain = WTF::move(subFrameDomain).isolatedCopy(), topFrameDomain = WTF::move(topFrameDomain).isolatedCopy(), wasGrantedAccess, promptWasShown, scope, completionHandler = WTF::move(completionHandler)]() mutable {
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
            storageSession->hasCookies(domain, WTF::move(completionHandler));
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

    postTask([mode, completionHandler = WTF::move(completionHandler)](auto& store) mutable {
        if (RefPtr statisticsStore = store.m_statisticsStore) {
            statisticsStore->setFirstPartyWebsiteDataRemovalMode(mode);
            if (mode == FirstPartyWebsiteDataRemovalMode::AllButCookiesReproTestingTimeout)
                statisticsStore->setIsRunningTest(true);
        }
        postTaskReply([completionHandler = WTF::move(completionHandler)]() mutable {
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
            statisticsStore->setPersistedDomains(WTF::move(domains));
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

    postTask([domain = domain.isolatedCopy(), completionHandler = WTF::move(completionHandler)](auto& store) mutable {
        if (RefPtr statisticsStore = store.m_statisticsStore)
            statisticsStore->setStandaloneApplicationDomain(WTF::move(domain));
        postTaskReply([completionHandler = WTF::move(completionHandler)]() mutable {
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
            storageSession->setAppBoundDomains(WTF::move(domains));
            storageSession->setThirdPartyCookieBlockingMode(ThirdPartyCookieBlockingMode::AllExceptBetweenAppBoundDomains);
        }
    }

    postTask([domains = WTF::move(domainsCopy), completionHandler = WTF::move(completionHandler)](auto& store) mutable {
        if (RefPtr statisticsStore = store.m_statisticsStore) {
            statisticsStore->setAppBoundDomains(WTF::move(domains));
            statisticsStore->setThirdPartyCookieBlockingMode(ThirdPartyCookieBlockingMode::AllExceptBetweenAppBoundDomains);
        }
        postTaskReply(WTF::move(completionHandler));
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
            storageSession->setManagedDomains(WTF::move(domains));
            storageSession->setThirdPartyCookieBlockingMode(ThirdPartyCookieBlockingMode::AllExceptManagedDomains);
        }
    }

    postTask([domains = WTF::move(domainsCopy), completionHandler = WTF::move(completionHandler)](auto& store) mutable {
        if (RefPtr statisticsStore = store.m_statisticsStore) {
            statisticsStore->setManagedDomains(WTF::move(domains));
            statisticsStore->setThirdPartyCookieBlockingMode(ThirdPartyCookieBlockingMode::AllExceptManagedDomains);
        }
        postTaskReply(WTF::move(completionHandler));
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

    postTask([targetDomain = WTF::move(targetDomain).isolatedCopy(), topFrameDomain = WTF::move(topFrameDomain).isolatedCopy(), sourceDomain = WTF::move(sourceDomain).isolatedCopy(), isRedirect, isMainFrame, delayAfterMainFrameDocumentLoad, wasPotentiallyInitiatedByUser](auto& store) {
        if (RefPtr statisticsStore = store.m_statisticsStore)
            statisticsStore->logFrameNavigation(targetDomain, topFrameDomain, sourceDomain, isRedirect, isMainFrame, delayAfterMainFrameDocumentLoad, wasPotentiallyInitiatedByUser);
    });
}

void WebResourceLoadStatisticsStore::logUserInteraction(RegistrableDomain&& domain, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    // User interactions need to be logged for ephemeral sessions to support the Storage Access API.
    if (isEphemeral())
        return logUserInteractionEphemeral(WTF::move(domain), WTF::move(completionHandler));

    postTask([domain = WTF::move(domain).isolatedCopy(), completionHandler = WTF::move(completionHandler)](auto& store) mutable {
        auto innerCompletionHandler = [completionHandler = WTF::move(completionHandler)]() mutable {
            postTaskReply(WTF::move(completionHandler));
        };
        if (RefPtr statisticsStore = store.m_statisticsStore) {
            statisticsStore->logUserInteraction(domain, WTF::move(innerCompletionHandler));
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
    
    postTask([fromDomain = WTF::move(fromDomain).isolatedCopy(), toDomain = WTF::move(toDomain).isolatedCopy(), didFilterKnownLinkDecoration, completionHandler = WTF::move(completionHandler)](auto& store) mutable {
        if (RefPtr statisticsStore = store.m_statisticsStore)
            statisticsStore->logCrossSiteLoadWithLinkDecoration(fromDomain, toDomain, didFilterKnownLinkDecoration);
        postTaskReply(WTF::move(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::clearUserInteraction(RegistrableDomain&& domain, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    if (isEphemeral())
        return clearUserInteractionEphemeral(domain, WTF::move(completionHandler));

    postTask([domain = WTF::move(domain).isolatedCopy(), completionHandler = WTF::move(completionHandler)](auto& store) mutable {
        auto innerCompletionHandler = [completionHandler = WTF::move(completionHandler)]() mutable {
            postTaskReply(WTF::move(completionHandler));
        };
        if (RefPtr statisticsStore = store.m_statisticsStore) {
            statisticsStore->clearUserInteraction(domain, WTF::move(innerCompletionHandler));
            return;
        }
        innerCompletionHandler();
    });
}

void WebResourceLoadStatisticsStore::setTimeAdvanceForTesting(Seconds time, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    postTask([time, completionHandler = WTF::move(completionHandler)](auto& store) mutable {
        if (RefPtr statisticsStore = store.m_statisticsStore)
            statisticsStore->setTimeAdvanceForTesting(time);
        postTaskReply(WTF::move(completionHandler));
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
        return hasHadUserInteractionEphemeral(domain, WTF::move(completionHandler));

    postTask([domain = WTF::move(domain).isolatedCopy(), completionHandler = WTF::move(completionHandler)](auto& store) mutable {
        bool hadUserInteraction = store.m_statisticsStore ? RefPtr { store.m_statisticsStore }->hasHadUserInteraction(domain, OperatingDatesWindow::Long) : false;
        postTaskReply([hadUserInteraction, completionHandler = WTF::move(completionHandler)]() mutable {
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
    
    postTask([domain = WTF::move(domain).isolatedCopy(), seconds, completionHandler = WTF::move(completionHandler)](auto& store) mutable {
        if (RefPtr statisticsStore = store.m_statisticsStore)
            statisticsStore->setLastSeen(domain, seconds);
        postTaskReply(WTF::move(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::mergeStatisticForTesting(RegistrableDomain&& domain, RegistrableDomain&& topFrameDomain1, RegistrableDomain&& topFrameDomain2, Seconds lastSeen, bool hadUserInteraction, Seconds mostRecentUserInteraction, bool isGrandfathered, bool isPrevalent, bool isVeryPrevalent, unsigned dataRecordsRemoved, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    postTask([domain = WTF::move(domain).isolatedCopy(), topFrameDomain1 = WTF::move(topFrameDomain1).isolatedCopy(), topFrameDomain2 = WTF::move(topFrameDomain2).isolatedCopy(), lastSeen, hadUserInteraction, mostRecentUserInteraction, isGrandfathered, isPrevalent, isVeryPrevalent, dataRecordsRemoved, completionHandler = WTF::move(completionHandler)](auto& store) mutable {
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

            statistic.subframeUnderTopFrameDomains = WTF::move(topFrameDomains);

            statisticsStore->mergeStatistics(Vector<ResourceLoadStatistics>::from(WTF::move(statistic)));
        }
        postTaskReply(WTF::move(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::isRelationshipOnlyInDatabaseOnce(RegistrableDomain&& subDomain, RegistrableDomain&& topDomain, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    postTask([subDomain = WTF::move(subDomain).isolatedCopy(), topDomain = WTF::move(topDomain).isolatedCopy(), completionHandler = WTF::move(completionHandler)](auto& store) mutable {
        RefPtr statisticsStore = store.m_statisticsStore;
        if (!statisticsStore) {
            completionHandler(false);
            return;
        }
        
        bool isRelationshipOnlyInDatabaseOnce = statisticsStore->isCorrectSubStatisticsCount(subDomain, topDomain);
        
        postTaskReply([isRelationshipOnlyInDatabaseOnce, completionHandler = WTF::move(completionHandler)]() mutable {
            completionHandler(isRelationshipOnlyInDatabaseOnce);
        });
    });
}
    
void WebResourceLoadStatisticsStore::setPrevalentResource(RegistrableDomain&& domain, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    postTask([domain = WTF::move(domain).isolatedCopy(), completionHandler = WTF::move(completionHandler)](auto& store) mutable {
        if (RefPtr statisticsStore = store.m_statisticsStore)
            statisticsStore->setPrevalentResource(domain);
        postTaskReply(WTF::move(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::setVeryPrevalentResource(RegistrableDomain&& domain, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    postTask([domain = WTF::move(domain).isolatedCopy(), completionHandler = WTF::move(completionHandler)](auto& store) mutable {
        if (RefPtr statisticsStore = store.m_statisticsStore)
            statisticsStore->setVeryPrevalentResource(domain);
        postTaskReply(WTF::move(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::setMostRecentWebPushInteractionTime(RegistrableDomain&& domain, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    postTask([completionHandler = WTF::move(completionHandler), domain = WTF::move(domain).isolatedCopy()](auto& store) mutable {
        if (RefPtr statisticsStore = store.m_statisticsStore)
            statisticsStore->setMostRecentWebPushInteractionTime(domain);
        postTaskReply(WTF::move(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::dumpResourceLoadStatistics(CompletionHandler<void(String&&)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    postTask([completionHandler = WTF::move(completionHandler)](auto& store) mutable {
        auto innerCompletionHandler = [completionHandler = WTF::move(completionHandler)](const String& result) mutable {
            postTaskReply([result = result.isolatedCopy(), completionHandler = WTF::move(completionHandler)]() mutable {
                completionHandler(WTF::move(result));
            });
        };
        if (RefPtr statisticsStore = store.m_statisticsStore)
            statisticsStore->dumpResourceLoadStatistics(WTF::move(innerCompletionHandler));
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

    postTask([domain = WTF::move(domain).isolatedCopy(), completionHandler = WTF::move(completionHandler)](auto& store) mutable {
        bool isPrevalentResource = store.m_statisticsStore && RefPtr { store.m_statisticsStore }->isPrevalentResource(domain);
        postTaskReply([isPrevalentResource, completionHandler = WTF::move(completionHandler)]() mutable {
            completionHandler(isPrevalentResource);
        });
    });
}
    
void WebResourceLoadStatisticsStore::isVeryPrevalentResource(RegistrableDomain&& domain, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    postTask([domain = WTF::move(domain).isolatedCopy(), completionHandler = WTF::move(completionHandler)](auto& store) mutable {
        bool isVeryPrevalentResource = store.m_statisticsStore && RefPtr { store.m_statisticsStore }->isVeryPrevalentResource(domain);
        postTaskReply([isVeryPrevalentResource, completionHandler = WTF::move(completionHandler)]() mutable {
            completionHandler(isVeryPrevalentResource);
        });
    });
}

void WebResourceLoadStatisticsStore::isRegisteredAsSubresourceUnder(RegistrableDomain&& subresourceDomain, RegistrableDomain&& topFrameDomain, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    postTask([subresourceDomain = WTF::move(subresourceDomain).isolatedCopy(), topFrameDomain = WTF::move(topFrameDomain).isolatedCopy(), completionHandler = WTF::move(completionHandler)](auto& store) mutable {
        bool isRegisteredAsSubresourceUnder = store.m_statisticsStore && RefPtr { store.m_statisticsStore }->isRegisteredAsSubresourceUnder(subresourceDomain, topFrameDomain);
        postTaskReply([isRegisteredAsSubresourceUnder, completionHandler = WTF::move(completionHandler)]() mutable {
            completionHandler(isRegisteredAsSubresourceUnder);
        });
    });
}

void WebResourceLoadStatisticsStore::isRegisteredAsSubFrameUnder(RegistrableDomain&& subFrameDomain, RegistrableDomain&& topFrameDomain, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    postTask([subFrameDomain = WTF::move(subFrameDomain).isolatedCopy(), topFrameDomain = WTF::move(topFrameDomain).isolatedCopy(), completionHandler = WTF::move(completionHandler)](auto& store) mutable {
        bool isRegisteredAsSubFrameUnder = store.m_statisticsStore && RefPtr { store.m_statisticsStore }->isRegisteredAsSubFrameUnder(subFrameDomain, topFrameDomain);
        postTaskReply([isRegisteredAsSubFrameUnder, completionHandler = WTF::move(completionHandler)]() mutable {
            completionHandler(isRegisteredAsSubFrameUnder);
        });
    });
}

void WebResourceLoadStatisticsStore::isRegisteredAsRedirectingTo(RegistrableDomain&& domainRedirectedFrom, RegistrableDomain&& domainRedirectedTo, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    postTask([domainRedirectedFrom = WTF::move(domainRedirectedFrom).isolatedCopy(), domainRedirectedTo = WTF::move(domainRedirectedTo).isolatedCopy(), completionHandler = WTF::move(completionHandler)](auto& store) mutable {
        bool isRegisteredAsRedirectingTo = store.m_statisticsStore && RefPtr { store.m_statisticsStore }->isRegisteredAsRedirectingTo(domainRedirectedFrom, domainRedirectedTo);
        postTaskReply([isRegisteredAsRedirectingTo, completionHandler = WTF::move(completionHandler)]() mutable {
            completionHandler(isRegisteredAsRedirectingTo);
        });
    });
}

void WebResourceLoadStatisticsStore::clearPrevalentResource(RegistrableDomain&& domain, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    postTask([domain = WTF::move(domain).isolatedCopy(), completionHandler = WTF::move(completionHandler)](auto& store) mutable {
        if (RefPtr statisticsStore = store.m_statisticsStore)
            statisticsStore->clearPrevalentResource(domain);
        postTaskReply(WTF::move(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::setGrandfathered(RegistrableDomain&& domain, bool value, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    postTask([domain = WTF::move(domain).isolatedCopy(), value, completionHandler = WTF::move(completionHandler)](auto& store) mutable {
        if (RefPtr statisticsStore = store.m_statisticsStore)
            statisticsStore->setGrandfathered(domain, value);
        postTaskReply(WTF::move(completionHandler));
    });
}
    
void WebResourceLoadStatisticsStore::isGrandfathered(RegistrableDomain&& domain, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    postTask([completionHandler = WTF::move(completionHandler), domain = WTF::move(domain).isolatedCopy()](auto& store) mutable {
        bool isGrandFathered = store.m_statisticsStore && RefPtr { store.m_statisticsStore }->isGrandfathered(domain);
        postTaskReply([isGrandFathered, completionHandler = WTF::move(completionHandler)]() mutable {
            completionHandler(isGrandFathered);
        });
    });
}

void WebResourceLoadStatisticsStore::setSubframeUnderTopFrameDomain(RegistrableDomain&& subFrameDomain, RegistrableDomain&& topFrameDomain, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    postTask([completionHandler = WTF::move(completionHandler), subFrameDomain = WTF::move(subFrameDomain).isolatedCopy(), topFrameDomain = WTF::move(topFrameDomain).isolatedCopy()](auto& store) mutable {
        if (RefPtr statisticsStore = store.m_statisticsStore)
            statisticsStore->setSubframeUnderTopFrameDomain(subFrameDomain, topFrameDomain);
        postTaskReply(WTF::move(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::setSubresourceUnderTopFrameDomain(RegistrableDomain&& subresourceDomain, RegistrableDomain&& topFrameDomain, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    postTask([completionHandler = WTF::move(completionHandler), subresourceDomain = WTF::move(subresourceDomain).isolatedCopy(), topFrameDomain = WTF::move(topFrameDomain).isolatedCopy()](auto& store) mutable {
        if (RefPtr statisticsStore = store.m_statisticsStore)
            statisticsStore->setSubresourceUnderTopFrameDomain(subresourceDomain, topFrameDomain);
        postTaskReply(WTF::move(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::setSubresourceUniqueRedirectTo(RegistrableDomain&& subresourceDomain, RegistrableDomain&& domainRedirectedTo, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    postTask([completionHandler = WTF::move(completionHandler), subresourceDomain = WTF::move(subresourceDomain).isolatedCopy(), domainRedirectedTo = WTF::move(domainRedirectedTo).isolatedCopy()](auto& store) mutable {
        if (RefPtr statisticsStore = store.m_statisticsStore)
            statisticsStore->setSubresourceUniqueRedirectTo(subresourceDomain, domainRedirectedTo);
        postTaskReply(WTF::move(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::setSubresourceUniqueRedirectFrom(RegistrableDomain&& subresourceDomain, RegistrableDomain&& domainRedirectedFrom, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    postTask([completionHandler = WTF::move(completionHandler), subresourceDomain = WTF::move(subresourceDomain).isolatedCopy(), domainRedirectedFrom = WTF::move(domainRedirectedFrom).isolatedCopy()](auto& store) mutable {
        if (RefPtr statisticsStore = store.m_statisticsStore)
            statisticsStore->setSubresourceUniqueRedirectFrom(subresourceDomain, domainRedirectedFrom);
        postTaskReply(WTF::move(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::setTopFrameUniqueRedirectTo(RegistrableDomain&& topFrameDomain, RegistrableDomain&& domainRedirectedTo, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    postTask([completionHandler = WTF::move(completionHandler), topFrameDomain = WTF::move(topFrameDomain).isolatedCopy(), domainRedirectedTo = WTF::move(domainRedirectedTo).isolatedCopy()](auto& store) mutable {
        if (RefPtr statisticsStore = store.m_statisticsStore)
            statisticsStore->setTopFrameUniqueRedirectTo(topFrameDomain, domainRedirectedTo);
        postTaskReply(WTF::move(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::setTopFrameUniqueRedirectFrom(RegistrableDomain&& topFrameDomain, RegistrableDomain&& domainRedirectedFrom, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    postTask([completionHandler = WTF::move(completionHandler), topFrameDomain = WTF::move(topFrameDomain).isolatedCopy(), domainRedirectedFrom = WTF::move(domainRedirectedFrom).isolatedCopy()](auto& store) mutable {
        if (RefPtr statisticsStore = store.m_statisticsStore)
            statisticsStore->setTopFrameUniqueRedirectFrom(topFrameDomain, domainRedirectedFrom);
        postTaskReply(WTF::move(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::scheduleCookieBlockingUpdate(CompletionHandler<void()>&& completionHandler)
{
    // Helper function used by testing system. Should only be called from the main thread.
    ASSERT(RunLoop::isMain());

    postTask([completionHandler = WTF::move(completionHandler)](auto& store) mutable {
        RefPtr statisticsStore = store.m_statisticsStore;
        if (!statisticsStore) {
            postTaskReply(WTF::move(completionHandler));
            return;
        }

        statisticsStore->updateCookieBlocking([completionHandler = WTF::move(completionHandler)]() mutable {
            postTaskReply(WTF::move(completionHandler));
        });
    });
}

void WebResourceLoadStatisticsStore::scheduleClearInMemoryAndPersistent(ShouldGrandfatherStatistics shouldGrandfather, CompletionHandler<void()>&& completionHandler)
{
    if (isEphemeral())
        return clearInMemoryEphemeral(WTF::move(completionHandler));

    ASSERT(RunLoop::isMain());
    postTask([shouldGrandfather, completionHandler = WTF::move(completionHandler)](auto& store) mutable {
        RefPtr statisticsStore = store.m_statisticsStore;
        if (!statisticsStore) {
            if (shouldGrandfather == ShouldGrandfatherStatistics::Yes)
                RELEASE_LOG(ResourceLoadStatistics, "WebResourceLoadStatisticsStore::scheduleClearInMemoryAndPersistent Before being cleared, m_statisticsStore is null when trying to grandfather data.");

            postTaskReply(WTF::move(completionHandler));
            return;
        }

        auto callbackAggregator = CallbackAggregator::create([completionHandler = WTF::move(completionHandler)] () mutable {
            postTaskReply(WTF::move(completionHandler));
        });

        statisticsStore->clear([protectedThis = Ref { store }, shouldGrandfather, callbackAggregator] () mutable {
            if (shouldGrandfather == ShouldGrandfatherStatistics::Yes) {
                if (RefPtr statisticsStore = protectedThis->m_statisticsStore) {
                    statisticsStore->grandfatherExistingWebsiteData([callbackAggregator = WTF::move(callbackAggregator)]() mutable { });
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
    scheduleClearInMemoryAndPersistent(shouldGrandfather, WTF::move(callback));
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

    postTask([domainID, completionHandler = WTF::move(completionHandler)](auto& store) mutable {
        RefPtr statisticsStore = store.m_statisticsStore;
        if (!statisticsStore) {
            completionHandler(false);
            return;
        }
        bool domainIDExists = statisticsStore->domainIDExistsInDatabase(domainID);
        postTaskReply([domainIDExists, completionHandler = WTF::move(completionHandler)]() mutable {
            completionHandler(domainIDExists);
        });
    });
}

void WebResourceLoadStatisticsStore::setTimeToLiveUserInteraction(Seconds seconds, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    postTask([seconds, completionHandler = WTF::move(completionHandler)](auto& store) mutable {
        if (RefPtr statisticsStore = store.m_statisticsStore)
            statisticsStore->setTimeToLiveUserInteraction(seconds);
        postTaskReply(WTF::move(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::setMinimumTimeBetweenDataRecordsRemoval(Seconds seconds, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    postTask([seconds, completionHandler = WTF::move(completionHandler)](auto& store) mutable  {
        if (RefPtr statisticsStore = store.m_statisticsStore)
            statisticsStore->setMinimumTimeBetweenDataRecordsRemoval(seconds);

        postTaskReply(WTF::move(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::setGrandfatheringTime(Seconds seconds, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    postTask([seconds, completionHandler = WTF::move(completionHandler)](auto& store) mutable  {
        if (RefPtr statisticsStore = store.m_statisticsStore)
            statisticsStore->setGrandfatheringTime(seconds);

        postTaskReply(WTF::move(completionHandler));
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
            protect(networkSession->networkProcess().parentProcessConnection())->send(Messages::NetworkProcessProxy::SetDomainsWithUserInteraction(domainsWithUserInteractionQuirk), 0);
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
            protect(networkSession->networkProcess().parentProcessConnection())->sendWithAsyncReply(Messages::NetworkProcessProxy::SetDomainsWithCrossPageStorageAccess(domainsWithStorageAccessQuirk), [this, protectedThis = Ref { *this }, domainsWithStorageAccessQuirk] () mutable {
                m_domainsWithCrossPageStorageAccessQuirk = domainsWithStorageAccessQuirk;
            });
        }
    }

    completionHandler();
}

void WebResourceLoadStatisticsStore::setMaxStatisticsEntries(size_t maximumEntryCount, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    postTask([maximumEntryCount, completionHandler = WTF::move(completionHandler)](auto& store) mutable  {
        if (RefPtr statisticsStore = store.m_statisticsStore)
            statisticsStore->setMaxStatisticsEntries(maximumEntryCount);

        postTaskReply(WTF::move(completionHandler));
    });
}
    
void WebResourceLoadStatisticsStore::setPruneEntriesDownTo(size_t pruneTargetCount, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    postTask([pruneTargetCount, completionHandler = WTF::move(completionHandler)](auto& store) mutable  {
        if (RefPtr statisticsStore = store.m_statisticsStore)
            statisticsStore->setPruneEntriesDownTo(pruneTargetCount);

        postTaskReply(WTF::move(completionHandler));
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

    postTask([completionHandler = WTF::move(completionHandler)](auto& store) mutable {
        if (RefPtr statisticsStore = store.m_statisticsStore)
            statisticsStore->resetParametersToDefaultValues();

        postTaskReply(WTF::move(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::logTestingEvent(const String& event)
{
    ASSERT(RunLoop::isMain());

    CheckedPtr networkSession = m_networkSession.get();
    if (networkSession && networkSession->enableResourceLoadStatisticsLogTestingEvent())
        protect(networkSession->networkProcess().parentProcessConnection())->send(Messages::NetworkProcessProxy::LogTestingEvent(m_networkSession->sessionID(), event), 0);
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
    for (auto it = m_domainsGrantedStorageAccessPermissionInPage.begin(); it != m_domainsGrantedStorageAccessPermissionInPage.end(); ++it) {
        it->value.removeIf([&domain](const auto& pair) {
            return pair.first == domain;
        });
    }

    ASSERT(RunLoop::isMain());
    postTask([domain = WTF::move(domain), completionHandler = WTF::move(completionHandler)](auto& store) mutable {
        if (RefPtr statisticsStore = store.m_statisticsStore)
            statisticsStore->removeDataForDomain(domain);

        postTaskReply(WTF::move(completionHandler));
    });
}

void WebResourceLoadStatisticsStore::registrableDomains(CompletionHandler<void(Vector<RegistrableDomain>&&)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    if (isEphemeral()) {
        completionHandler({ });
        return;
    }

    postTask([completionHandler = WTF::move(completionHandler)](auto& store) mutable {
        auto domains = store.m_statisticsStore ? RefPtr { store.m_statisticsStore }->allDomains() : Vector<RegistrableDomain>();
        postTaskReply([domains = crossThreadCopy(WTF::move(domains)), completionHandler = WTF::move(completionHandler)]() mutable {
            completionHandler(WTF::move(domains));
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

    postTask([completionHandler = WTF::move(completionHandler)](auto& store) mutable {
        std::optional<HashMap<RegistrableDomain, WallTime>> result;
        if (RefPtr statisticsStore = store.m_statisticsStore)
            result = statisticsStore->allDomainsWithLastAccessedTime();
        postTaskReply([result = crossThreadCopy(WTF::move(result)), completionHandler = WTF::move(completionHandler)]() mutable {
            completionHandler(WTF::move(result));
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

    postTask([completionHandler = WTF::move(completionHandler)](auto& store) mutable {
        HashSet<RegistrableDomain> result;
        if (RefPtr statisticsStore = store.m_statisticsStore)
            result = statisticsStore->domainsExemptFromWebsiteDataDeletion();
        postTaskReply([result = crossThreadCopy(WTF::move(result)), completionHandler = WTF::move(completionHandler)]() mutable {
            completionHandler(WTF::move(result));
        });
    });
}

void WebResourceLoadStatisticsStore::deleteAndRestrictWebsiteDataForRegistrableDomains(OptionSet<WebsiteDataType> dataTypes, RegistrableDomainsToDeleteOrRestrictWebsiteDataFor&& domainsToDeleteAndRestrictWebsiteDataFor, CompletionHandler<void(HashSet<RegistrableDomain>&&)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    if (CheckedPtr networkSession = m_networkSession.get()) {
        networkSession->deleteAndRestrictWebsiteDataForRegistrableDomains(dataTypes, WTF::move(domainsToDeleteAndRestrictWebsiteDataFor), WTF::move(completionHandler));
        return;
    }

    completionHandler({ });
}

void WebResourceLoadStatisticsStore::registrableDomainsWithWebsiteData(OptionSet<WebsiteDataType> dataTypes, CompletionHandler<void(HashSet<RegistrableDomain>&&)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    if (CheckedPtr networkSession = m_networkSession.get()) {
        networkSession->registrableDomainsWithWebsiteData(dataTypes, WTF::move(completionHandler));
        return;
    }

    completionHandler({ });
}

void WebResourceLoadStatisticsStore::aggregatedThirdPartyData(CompletionHandler<void(Vector<ITPThirdPartyData>&&)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    postTask([completionHandler = WTF::move(completionHandler)](auto& store) mutable {
        RefPtr statisticsStore = store.m_statisticsStore;
        if (!statisticsStore) {
            postTaskReply([completionHandler = WTF::move(completionHandler)]() mutable {
                completionHandler({ });
            });
            return;
        }
        auto thirdPartyData = statisticsStore->aggregatedThirdPartyData();
        postTaskReply([thirdPartyData = WTF::move(thirdPartyData), completionHandler = WTF::move(completionHandler)]() mutable {
            completionHandler(WTF::move(thirdPartyData));
        });
    });
}

void WebResourceLoadStatisticsStore::suspend(CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    sharedStatisticsQueue()->suspend(ResourceLoadStatisticsStore::interruptAllDatabases, WTF::move(completionHandler));
}

void WebResourceLoadStatisticsStore::resume()
{
    ASSERT(RunLoop::isMain());

    sharedStatisticsQueue()->resume();
}

void WebResourceLoadStatisticsStore::insertExpiredStatisticForTesting(RegistrableDomain&& domain, unsigned numberOfOperatingDaysPassed, bool hadUserInteraction, bool isScheduledForAllButCookieDataRemoval, bool isPrevalent, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    postTask([domain = WTF::move(domain).isolatedCopy(), numberOfOperatingDaysPassed, hadUserInteraction, isScheduledForAllButCookieDataRemoval, isPrevalent, completionHandler = WTF::move(completionHandler)](auto& store) mutable {
        if (RefPtr statisticsStore = store.m_statisticsStore)
            statisticsStore->insertExpiredStatisticForTesting(WTF::move(domain), numberOfOperatingDaysPassed, hadUserInteraction, isScheduledForAllButCookieDataRemoval, isPrevalent);
        postTaskReply(WTF::move(completionHandler));
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

void WebResourceLoadStatisticsStore::setStorageAccessPermissionForTesting(bool granted, WebPageProxyIdentifier webPageProxyID, RegistrableDomain&& topFrameDomain, RegistrableDomain&& subFrameDomain, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    if (granted)
        wasGrantedStorageAccessPermissionInPage(webPageProxyID, topFrameDomain, subFrameDomain);
    else
        wasRevokedStorageAccessPermissionInPage(webPageProxyID);

    if (isEphemeral())
        return completionHandler();

    postTask([granted, subFrameDomain = WTF::move(subFrameDomain).isolatedCopy(), topFrameDomain = WTF::move(topFrameDomain).isolatedCopy(), completionHandler = WTF::move(completionHandler)](auto& store) mutable {
        RefPtr statisticsStore = store.m_statisticsStore;
        if (!statisticsStore)
            return postTaskReply(WTF::move(completionHandler));

        Ref callbackAggregator = CallbackAggregator::create([completionHandler = WTF::move(completionHandler)] mutable {
            postTaskReply(WTF::move(completionHandler));
        });

        if (granted) {
            statisticsStore->logUserInteraction(subFrameDomain, [callbackAggregator] { });
            statisticsStore->grantStorageAccessPermission(topFrameDomain, subFrameDomain);
        } else {
            statisticsStore->clearUserInteraction(subFrameDomain, [callbackAggregator] { });
            statisticsStore->revokeStorageAccessPermission(subFrameDomain);
        }
    });
}

void WebResourceLoadStatisticsStore::wasGrantedStorageAccessPermissionInPage(WebPageProxyIdentifier webPageProxyID, const RegistrableDomain& topFrameDomain, const RegistrableDomain& subFrameDomain)
{
    auto result = m_domainsGrantedStorageAccessPermissionInPage.ensure(webPageProxyID, [] {
        return HashSet<std::pair<TopFrameDomain, SubFrameDomain>>();
    }).iterator->value.add({ topFrameDomain, subFrameDomain });

    if (result.isNewEntry) {
        if (auto it = m_storageAccessPermissionChangeObservers.find({ topFrameDomain, subFrameDomain }); it != m_storageAccessPermissionChangeObservers.end()) {
            for (Ref observer : it->value)
                observer->storageAccessPermissionChanged(topFrameDomain, subFrameDomain);
        }
    }
}

void WebResourceLoadStatisticsStore::wasRevokedStorageAccessPermissionInPage(WebPageProxyIdentifier webPageProxyID)
{
    m_domainsGrantedStorageAccessPermissionInPage.remove(webPageProxyID);
}

} // namespace WebKit

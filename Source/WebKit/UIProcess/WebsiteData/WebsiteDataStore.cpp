/*
 * Copyright (C) 2014-2019 Apple Inc. All rights reserved.
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
#include "WebsiteDataStore.h"

#include "APIProcessPoolConfiguration.h"
#include "APIWebsiteDataRecord.h"
#include "APIWebsiteDataStore.h"
#include "AuthenticatorManager.h"
#include "DeviceIdHashSaltStorage.h"
#include "MockAuthenticatorManager.h"
#include "NetworkProcessMessages.h"
#include "ShouldGrandfatherStatistics.h"
#include "StorageAccessStatus.h"
#include "StorageManager.h"
#include "WebProcessCache.h"
#include "WebProcessMessages.h"
#include "WebProcessPool.h"
#include "WebResourceLoadStatisticsStore.h"
#include "WebResourceLoadStatisticsStoreMessages.h"
#include "WebsiteData.h"
#include "WebsiteDataStoreClient.h"
#include "WebsiteDataStoreParameters.h"
#include <WebCore/ApplicationCacheStorage.h>
#include <WebCore/DatabaseTracker.h>
#include <WebCore/HTMLMediaElement.h>
#include <WebCore/OriginLock.h>
#include <WebCore/RegistrableDomain.h>
#include <WebCore/SecurityOrigin.h>
#include <WebCore/SecurityOriginData.h>
#include <wtf/CallbackAggregator.h>
#include <wtf/CompletionHandler.h>
#include <wtf/CrossThreadCopier.h>
#include <wtf/FileSystem.h>
#include <wtf/ProcessPrivilege.h>
#include <wtf/RunLoop.h>

#if ENABLE(NETSCAPE_PLUGIN_API)
#include "PluginProcessManager.h"
#endif

#if HAVE(SEC_KEY_PROXY)
#include "SecKeyProxyStore.h"
#endif

namespace WebKit {

static bool allowsWebsiteDataRecordsForAllOrigins;
void WebsiteDataStore::allowWebsiteDataRecordsForAllOrigins()
{
    allowsWebsiteDataRecordsForAllOrigins = true;
}

static HashMap<PAL::SessionID, WebsiteDataStore*>& nonDefaultDataStores()
{
    RELEASE_ASSERT(isUIThread());
    static NeverDestroyed<HashMap<PAL::SessionID, WebsiteDataStore*>> map;
    return map;
}

Ref<WebsiteDataStore> WebsiteDataStore::createNonPersistent()
{
    return adoptRef(*new WebsiteDataStore(PAL::SessionID::generateEphemeralSessionID()));
}

Ref<WebsiteDataStore> WebsiteDataStore::create(Ref<WebsiteDataStoreConfiguration>&& configuration, PAL::SessionID sessionID)
{
    return adoptRef(*new WebsiteDataStore(WTFMove(configuration), sessionID));
}

WebsiteDataStore::WebsiteDataStore(Ref<WebsiteDataStoreConfiguration>&& configuration, PAL::SessionID sessionID)
    : m_sessionID(sessionID)
    , m_resolvedConfiguration(WTFMove(configuration))
    , m_configuration(m_resolvedConfiguration->copy())
    , m_storageManager(StorageManager::create(m_configuration->localStorageDirectory()))
    , m_deviceIdHashSaltStorage(DeviceIdHashSaltStorage::create(isPersistent() ? m_configuration->deviceIdHashSaltsStorageDirectory() : String()))
    , m_queue(WorkQueue::create("com.apple.WebKit.WebsiteDataStore"))
    , m_sourceApplicationBundleIdentifier(m_configuration->sourceApplicationBundleIdentifier())
    , m_sourceApplicationSecondaryIdentifier(m_configuration->sourceApplicationSecondaryIdentifier())
#if ENABLE(WEB_AUTHN)
    , m_authenticatorManager(makeUniqueRef<AuthenticatorManager>())
#endif
    , m_client(makeUniqueRef<WebsiteDataStoreClient>())
{
#if HAVE(LOAD_OPTIMIZER)
WEBSITEDATASTORE_LOADOPTIMIZER_ADDITIONS_2
#endif
    WTF::setProcessPrivileges(allPrivileges());
    maybeRegisterWithSessionIDMap();
    platformInitialize();

    ASSERT(RunLoop::isMain());
}

WebsiteDataStore::WebsiteDataStore(PAL::SessionID sessionID)
    : m_sessionID(sessionID)
    , m_resolvedConfiguration(WebsiteDataStoreConfiguration::create())
    , m_configuration(m_resolvedConfiguration->copy())
    , m_storageManager(StorageManager::create({ }))
    , m_deviceIdHashSaltStorage(DeviceIdHashSaltStorage::create(isPersistent() ? m_configuration->deviceIdHashSaltsStorageDirectory() : String()))
    , m_queue(WorkQueue::create("com.apple.WebKit.WebsiteDataStore"))
#if ENABLE(WEB_AUTHN)
    , m_authenticatorManager(makeUniqueRef<AuthenticatorManager>())
#endif
    , m_client(makeUniqueRef<WebsiteDataStoreClient>())
{
#if HAVE(LOAD_OPTIMIZER)
WEBSITEDATASTORE_LOADOPTIMIZER_ADDITIONS_2
#endif
    maybeRegisterWithSessionIDMap();
    platformInitialize();

    ASSERT(RunLoop::isMain());
}

WebsiteDataStore::~WebsiteDataStore()
{
    ASSERT(RunLoop::isMain());

    platformDestroy();

    if (m_sessionID.isValid() && m_sessionID != PAL::SessionID::defaultSessionID()) {
        ASSERT(nonDefaultDataStores().get(m_sessionID) == this);
        nonDefaultDataStores().remove(m_sessionID);
        for (auto& processPool : WebProcessPool::allProcessPools()) {
            if (auto* networkProcess = processPool->networkProcess())
                networkProcess->removeSession(m_sessionID);
        }
    }
}

void WebsiteDataStore::maybeRegisterWithSessionIDMap()
{
    if (m_sessionID.isValid() && m_sessionID != PAL::SessionID::defaultSessionID()) {
        auto result = nonDefaultDataStores().add(m_sessionID, this);
        ASSERT_UNUSED(result, result.isNewEntry);
    }
}

WebsiteDataStore* WebsiteDataStore::existingNonDefaultDataStoreForSessionID(PAL::SessionID sessionID)
{
    return sessionID.isValid() && sessionID != PAL::SessionID::defaultSessionID() ? nonDefaultDataStores().get(sessionID) : nullptr;
}

WebProcessPool* WebsiteDataStore::processPoolForCookieStorageOperations()
{
    auto pools = processPools(1, false);
    return pools.isEmpty() ? nullptr : pools.begin()->get();
}

void WebsiteDataStore::resolveDirectoriesIfNecessary()
{
    if (m_hasResolvedDirectories)
        return;
    m_hasResolvedDirectories = true;

    // Resolve directory paths.
    if (!m_configuration->applicationCacheDirectory().isEmpty())
        m_resolvedConfiguration->setApplicationCacheDirectory(resolveAndCreateReadWriteDirectoryForSandboxExtension(m_configuration->applicationCacheDirectory()));
    if (!m_configuration->mediaCacheDirectory().isEmpty())
        m_resolvedConfiguration->setMediaCacheDirectory(resolveAndCreateReadWriteDirectoryForSandboxExtension(m_configuration->mediaCacheDirectory()));
    if (!m_configuration->mediaKeysStorageDirectory().isEmpty())
        m_resolvedConfiguration->setMediaKeysStorageDirectory(resolveAndCreateReadWriteDirectoryForSandboxExtension(m_configuration->mediaKeysStorageDirectory()));
    if (!m_configuration->webSQLDatabaseDirectory().isEmpty())
        m_resolvedConfiguration->setWebSQLDatabaseDirectory(resolveAndCreateReadWriteDirectoryForSandboxExtension(m_configuration->webSQLDatabaseDirectory()));
    if (!m_configuration->indexedDBDatabaseDirectory().isEmpty())
        m_resolvedConfiguration->setIndexedDBDatabaseDirectory(resolveAndCreateReadWriteDirectoryForSandboxExtension(m_configuration->indexedDBDatabaseDirectory()));
    if (!m_configuration->deviceIdHashSaltsStorageDirectory().isEmpty())
        m_resolvedConfiguration->setDeviceIdHashSaltsStorageDirectory(resolveAndCreateReadWriteDirectoryForSandboxExtension(m_configuration->deviceIdHashSaltsStorageDirectory()));
    if (!m_configuration->resourceLoadStatisticsDirectory().isEmpty())
        m_resolvedConfiguration->setResourceLoadStatisticsDirectory(resolveAndCreateReadWriteDirectoryForSandboxExtension(m_configuration->resourceLoadStatisticsDirectory()));
    if (!m_configuration->serviceWorkerRegistrationDirectory().isEmpty() && m_resolvedConfiguration->serviceWorkerRegistrationDirectory().isEmpty())
        m_resolvedConfiguration->setServiceWorkerRegistrationDirectory(resolveAndCreateReadWriteDirectoryForSandboxExtension(m_configuration->serviceWorkerRegistrationDirectory()));
    if (!m_configuration->javaScriptConfigurationDirectory().isEmpty())
        m_resolvedConfiguration->setJavaScriptConfigurationDirectory(resolvePathForSandboxExtension(m_configuration->javaScriptConfigurationDirectory()));
    if (!m_configuration->cacheStorageDirectory().isEmpty() && m_resolvedConfiguration->cacheStorageDirectory().isEmpty())
        m_resolvedConfiguration->setCacheStorageDirectory(resolvePathForSandboxExtension(m_configuration->cacheStorageDirectory()));

    // Resolve directories for file paths.
    if (!m_configuration->cookieStorageFile().isEmpty()) {
        m_resolvedConfiguration->setCookieStorageFile(resolveAndCreateReadWriteDirectoryForSandboxExtension(FileSystem::directoryName(m_configuration->cookieStorageFile())));
        m_resolvedConfiguration->setCookieStorageFile(FileSystem::pathByAppendingComponent(m_resolvedConfiguration->cookieStorageFile(), FileSystem::pathGetFileName(m_configuration->cookieStorageFile())));
    }
}

void WebsiteDataStore::cloneSessionData(WebPageProxy& sourcePage, WebPageProxy& newPage)
{
    auto& sourceDataStore = sourcePage.websiteDataStore();
    auto& newDataStore = newPage.websiteDataStore();

    // FIXME: Handle this.
    if (&sourceDataStore != &newDataStore)
        return;

    if (!sourceDataStore.m_storageManager)
        return;

    sourceDataStore.m_storageManager->cloneSessionStorageNamespace(sourcePage.pageID(), newPage.pageID());
}

enum class ProcessAccessType {
    None,
    OnlyIfLaunched,
    Launch,
};

static ProcessAccessType computeNetworkProcessAccessTypeForDataFetch(OptionSet<WebsiteDataType> dataTypes, bool isNonPersistentStore)
{
    ProcessAccessType processAccessType = ProcessAccessType::None;

    if (dataTypes.contains(WebsiteDataType::Cookies)) {
        if (isNonPersistentStore)
            processAccessType = std::max(processAccessType, ProcessAccessType::OnlyIfLaunched);
        else
            processAccessType = std::max(processAccessType, ProcessAccessType::Launch);
    }

    if (dataTypes.contains(WebsiteDataType::DiskCache) && !isNonPersistentStore)
        processAccessType = std::max(processAccessType, ProcessAccessType::Launch);

    if (dataTypes.contains(WebsiteDataType::DOMCache))
        processAccessType = std::max(processAccessType, ProcessAccessType::Launch);
    
    if (dataTypes.contains(WebsiteDataType::IndexedDBDatabases) && !isNonPersistentStore)
        processAccessType = std::max(processAccessType, ProcessAccessType::Launch);

#if ENABLE(SERVICE_WORKER)
    if (dataTypes.contains(WebsiteDataType::ServiceWorkerRegistrations) && !isNonPersistentStore)
        processAccessType = std::max(processAccessType, ProcessAccessType::Launch);
#endif

    return processAccessType;
}

static ProcessAccessType computeWebProcessAccessTypeForDataFetch(OptionSet<WebsiteDataType> dataTypes, bool isNonPersistentStore)
{
    UNUSED_PARAM(isNonPersistentStore);

    ProcessAccessType processAccessType = ProcessAccessType::None;

    if (dataTypes.contains(WebsiteDataType::MemoryCache))
        return ProcessAccessType::OnlyIfLaunched;

    return processAccessType;
}

void WebsiteDataStore::fetchData(OptionSet<WebsiteDataType> dataTypes, OptionSet<WebsiteDataFetchOption> fetchOptions, Function<void(Vector<WebsiteDataRecord>)>&& completionHandler)
{
    fetchDataAndApply(dataTypes, fetchOptions, nullptr, WTFMove(completionHandler));
}

void WebsiteDataStore::fetchDataAndApply(OptionSet<WebsiteDataType> dataTypes, OptionSet<WebsiteDataFetchOption> fetchOptions, RefPtr<WorkQueue>&& queue, Function<void(Vector<WebsiteDataRecord>)>&& apply)
{
    struct CallbackAggregator final : ThreadSafeRefCounted<CallbackAggregator> {
        CallbackAggregator(OptionSet<WebsiteDataFetchOption> fetchOptions, RefPtr<WorkQueue>&& queue, Function<void(Vector<WebsiteDataRecord>)>&& apply, WebsiteDataStore& dataStore)
            : fetchOptions(fetchOptions)
            , queue(WTFMove(queue))
            , apply(WTFMove(apply))
            , protectedDataStore(dataStore)
        {
            ASSERT(RunLoop::isMain());
        }

        ~CallbackAggregator()
        {
            ASSERT(!pendingCallbacks);

            // Make sure the data store gets destroyed on the main thread even though the CallbackAggregator can get destroyed on a background queue.
            RunLoop::main().dispatch([protectedDataStore = WTFMove(protectedDataStore)] { });
        }

        void addPendingCallback()
        {
            pendingCallbacks++;
        }

        void removePendingCallback(WebsiteData websiteData)
        {
            ASSERT(pendingCallbacks);
            --pendingCallbacks;

            for (auto& entry : websiteData.entries) {
                auto displayName = WebsiteDataRecord::displayNameForOrigin(entry.origin);
                if (!displayName) {
                    if (!allowsWebsiteDataRecordsForAllOrigins)
                        continue;

                    displayName = makeString(entry.origin.protocol, " ", entry.origin.host);
                }

                auto& record = m_websiteDataRecords.add(displayName, WebsiteDataRecord { }).iterator->value;
                if (!record.displayName)
                    record.displayName = WTFMove(displayName);

                record.add(entry.type, entry.origin);

                if (fetchOptions.contains(WebsiteDataFetchOption::ComputeSizes)) {
                    if (!record.size)
                        record.size = WebsiteDataRecord::Size { 0, { } };

                    record.size->totalSize += entry.size;
                    record.size->typeSizes.add(static_cast<unsigned>(entry.type), 0).iterator->value += entry.size;
                }
            }

            for (auto& hostName : websiteData.hostNamesWithCookies) {
                auto displayName = WebsiteDataRecord::displayNameForCookieHostName(hostName);
                if (!displayName)
                    continue;

                auto& record = m_websiteDataRecords.add(displayName, WebsiteDataRecord { }).iterator->value;
                if (!record.displayName)
                    record.displayName = WTFMove(displayName);

                record.addCookieHostName(hostName);
            }

#if ENABLE(NETSCAPE_PLUGIN_API)
            for (auto& hostName : websiteData.hostNamesWithPluginData) {
                auto displayName = WebsiteDataRecord::displayNameForHostName(hostName);
                if (!displayName)
                    continue;

                auto& record = m_websiteDataRecords.add(displayName, WebsiteDataRecord { }).iterator->value;
                if (!record.displayName)
                    record.displayName = WTFMove(displayName);

                record.addPluginDataHostName(hostName);
            }
#endif

            for (auto& origin : websiteData.originsWithCredentials) {
                auto& record = m_websiteDataRecords.add(origin, WebsiteDataRecord { }).iterator->value;
                
                record.addOriginWithCredential(origin);
            }

            for (auto& hostName : websiteData.hostNamesWithHSTSCache) {
                auto displayName = WebsiteDataRecord::displayNameForHostName(hostName);
                if (!displayName)
                    continue;
                
                auto& record = m_websiteDataRecords.add(displayName, WebsiteDataRecord { }).iterator->value;
                if (!record.displayName)
                    record.displayName = WTFMove(displayName);

                record.addHSTSCacheHostname(hostName);
            }

            callIfNeeded();
        }

        void callIfNeeded()
        {
            if (pendingCallbacks)
                return;

            Vector<WebsiteDataRecord> records;
            records.reserveInitialCapacity(m_websiteDataRecords.size());
            for (auto& record : m_websiteDataRecords.values())
                records.uncheckedAppend(WTFMove(record));

            auto processRecords = [apply = WTFMove(apply), records = WTFMove(records)] () mutable {
                apply(WTFMove(records));
            };

            if (queue)
                queue->dispatch(WTFMove(processRecords));
            else
                RunLoop::main().dispatch(WTFMove(processRecords));
        }

        const OptionSet<WebsiteDataFetchOption> fetchOptions;

        unsigned pendingCallbacks = 0;
        RefPtr<WorkQueue> queue;
        Function<void(Vector<WebsiteDataRecord>)> apply;

        HashMap<String, WebsiteDataRecord> m_websiteDataRecords;
        Ref<WebsiteDataStore> protectedDataStore;
    };

    RefPtr<CallbackAggregator> callbackAggregator = adoptRef(new CallbackAggregator(fetchOptions, WTFMove(queue), WTFMove(apply), *this));

#if ENABLE(VIDEO)
    if (dataTypes.contains(WebsiteDataType::DiskCache)) {
        callbackAggregator->addPendingCallback();
        m_queue->dispatch([mediaCacheDirectory = m_configuration->mediaCacheDirectory().isolatedCopy(), callbackAggregator] {
            // FIXME: Make HTMLMediaElement::originsInMediaCache return a collection of SecurityOriginDatas.
            HashSet<RefPtr<WebCore::SecurityOrigin>> origins = WebCore::HTMLMediaElement::originsInMediaCache(mediaCacheDirectory);
            WebsiteData websiteData;
            
            for (auto& origin : origins) {
                WebsiteData::Entry entry { origin->data(), WebsiteDataType::DiskCache, 0 };
                websiteData.entries.append(WTFMove(entry));
            }
            
            RunLoop::main().dispatch([callbackAggregator, origins = WTFMove(origins), websiteData = WTFMove(websiteData)]() mutable {
                callbackAggregator->removePendingCallback(WTFMove(websiteData));
            });
        });
    }
#endif

    auto networkProcessAccessType = computeNetworkProcessAccessTypeForDataFetch(dataTypes, !isPersistent());
    if (networkProcessAccessType != ProcessAccessType::None) {
        for (auto& processPool : processPools()) {
            switch (networkProcessAccessType) {
            case ProcessAccessType::OnlyIfLaunched:
                if (!processPool->networkProcess())
                    continue;
                break;

            case ProcessAccessType::Launch:
                processPool->ensureNetworkProcess(this);
                break;

            case ProcessAccessType::None:
                ASSERT_NOT_REACHED();
            }

            callbackAggregator->addPendingCallback();
            processPool->networkProcess()->fetchWebsiteData(m_sessionID, dataTypes, fetchOptions, [callbackAggregator, processPool](WebsiteData websiteData) {
                callbackAggregator->removePendingCallback(WTFMove(websiteData));
            });
        }
    }

    auto webProcessAccessType = computeWebProcessAccessTypeForDataFetch(dataTypes, !isPersistent());
    if (webProcessAccessType != ProcessAccessType::None) {
        for (auto& process : processes()) {
            switch (webProcessAccessType) {
            case ProcessAccessType::OnlyIfLaunched:
                if (!process->canSendMessage())
                    continue;
                break;

            case ProcessAccessType::Launch:
                // FIXME: Handle this.
                ASSERT_NOT_REACHED();
                break;

            case ProcessAccessType::None:
                ASSERT_NOT_REACHED();
            }

            callbackAggregator->addPendingCallback();
            process->fetchWebsiteData(m_sessionID, dataTypes, [callbackAggregator](WebsiteData websiteData) {
                callbackAggregator->removePendingCallback(WTFMove(websiteData));
            });
        }
    }

    if (dataTypes.contains(WebsiteDataType::SessionStorage) && m_storageManager) {
        callbackAggregator->addPendingCallback();

        m_storageManager->getSessionStorageOrigins([callbackAggregator](HashSet<WebCore::SecurityOriginData>&& origins) {
            WebsiteData websiteData;

            while (!origins.isEmpty())
                websiteData.entries.append(WebsiteData::Entry { origins.takeAny(), WebsiteDataType::SessionStorage, 0 });

            callbackAggregator->removePendingCallback(WTFMove(websiteData));
        });
    }

    if (dataTypes.contains(WebsiteDataType::LocalStorage) && m_storageManager) {
        callbackAggregator->addPendingCallback();

        m_storageManager->getLocalStorageOrigins([callbackAggregator](HashSet<WebCore::SecurityOriginData>&& origins) {
            WebsiteData websiteData;

            while (!origins.isEmpty())
                websiteData.entries.append(WebsiteData::Entry { origins.takeAny(), WebsiteDataType::LocalStorage, 0 });

            callbackAggregator->removePendingCallback(WTFMove(websiteData));
        });
    }

    if (dataTypes.contains(WebsiteDataType::DeviceIdHashSalt)) {
        callbackAggregator->addPendingCallback();

        m_deviceIdHashSaltStorage->getDeviceIdHashSaltOrigins([callbackAggregator](auto&& origins) {
            WebsiteData websiteData;

            while (!origins.isEmpty())
                websiteData.entries.append(WebsiteData::Entry { origins.takeAny(), WebsiteDataType::DeviceIdHashSalt, 0 });

            callbackAggregator->removePendingCallback(WTFMove(websiteData));
        });
    }

    if (dataTypes.contains(WebsiteDataType::OfflineWebApplicationCache) && isPersistent()) {
        callbackAggregator->addPendingCallback();

        m_queue->dispatch([fetchOptions, applicationCacheDirectory = m_configuration->applicationCacheDirectory().isolatedCopy(), applicationCacheFlatFileSubdirectoryName = m_configuration->applicationCacheFlatFileSubdirectoryName().isolatedCopy(), callbackAggregator] {
            auto storage = WebCore::ApplicationCacheStorage::create(applicationCacheDirectory, applicationCacheFlatFileSubdirectoryName);

            WebsiteData websiteData;

            // FIXME: getOriginsWithCache should return a collection of SecurityOriginDatas.
            auto origins = storage->originsWithCache();

            for (auto& origin : origins) {
                uint64_t size = fetchOptions.contains(WebsiteDataFetchOption::ComputeSizes) ? storage->diskUsageForOrigin(origin) : 0;
                WebsiteData::Entry entry { origin->data(), WebsiteDataType::OfflineWebApplicationCache, size };

                websiteData.entries.append(WTFMove(entry));
            }

            RunLoop::main().dispatch([callbackAggregator, origins = WTFMove(origins), websiteData = WTFMove(websiteData)]() mutable {
                callbackAggregator->removePendingCallback(WTFMove(websiteData));
            });
        });
    }

    if (dataTypes.contains(WebsiteDataType::WebSQLDatabases) && isPersistent()) {
        callbackAggregator->addPendingCallback();

        m_queue->dispatch([webSQLDatabaseDirectory = m_configuration->webSQLDatabaseDirectory().isolatedCopy(), callbackAggregator] {
            auto origins = WebCore::DatabaseTracker::trackerWithDatabasePath(webSQLDatabaseDirectory)->origins();
            RunLoop::main().dispatch([callbackAggregator, origins = WTFMove(origins)]() mutable {
                WebsiteData websiteData;
                for (auto& origin : origins)
                    websiteData.entries.append(WebsiteData::Entry { WTFMove(origin), WebsiteDataType::WebSQLDatabases, 0 });
                callbackAggregator->removePendingCallback(WTFMove(websiteData));
            });
        });
    }

    if (dataTypes.contains(WebsiteDataType::MediaKeys) && isPersistent()) {
        callbackAggregator->addPendingCallback();

        m_queue->dispatch([mediaKeysStorageDirectory = m_configuration->mediaKeysStorageDirectory().isolatedCopy(), callbackAggregator] {
            auto origins = mediaKeyOrigins(mediaKeysStorageDirectory);

            RunLoop::main().dispatch([callbackAggregator, origins = WTFMove(origins)]() mutable {
                WebsiteData websiteData;
                for (auto& origin : origins)
                    websiteData.entries.append(WebsiteData::Entry { origin, WebsiteDataType::MediaKeys, 0 });

                callbackAggregator->removePendingCallback(WTFMove(websiteData));
            });
        });
    }

#if ENABLE(NETSCAPE_PLUGIN_API)
    if (dataTypes.contains(WebsiteDataType::PlugInData) && isPersistent()) {
        class State {
        public:
            static void fetchData(Ref<CallbackAggregator>&& callbackAggregator, Vector<PluginModuleInfo>&& plugins)
            {
                new State(WTFMove(callbackAggregator), WTFMove(plugins));
            }

        private:
            State(Ref<CallbackAggregator>&& callbackAggregator, Vector<PluginModuleInfo>&& plugins)
                : m_callbackAggregator(WTFMove(callbackAggregator))
                , m_plugins(WTFMove(plugins))
            {
                m_callbackAggregator->addPendingCallback();

                fetchWebsiteDataForNextPlugin();
            }

            ~State()
            {
                ASSERT(m_plugins.isEmpty());
            }

            void fetchWebsiteDataForNextPlugin()
            {
                if (m_plugins.isEmpty()) {
                    WebsiteData websiteData;
                    websiteData.hostNamesWithPluginData = WTFMove(m_hostNames);

                    m_callbackAggregator->removePendingCallback(WTFMove(websiteData));

                    delete this;
                    return;
                }

                auto plugin = m_plugins.takeLast();
                PluginProcessManager::singleton().fetchWebsiteData(plugin, m_callbackAggregator->fetchOptions, [this](Vector<String> hostNames) {
                    for (auto& hostName : hostNames)
                        m_hostNames.add(WTFMove(hostName));
                    fetchWebsiteDataForNextPlugin();
                });
            }

            Ref<CallbackAggregator> m_callbackAggregator;
            Vector<PluginModuleInfo> m_plugins;
            HashSet<String> m_hostNames;
        };

        State::fetchData(*callbackAggregator, plugins());
    }
#endif

    callbackAggregator->callIfNeeded();
}

static ProcessAccessType computeNetworkProcessAccessTypeForDataRemoval(OptionSet<WebsiteDataType> dataTypes, bool isNonPersistentStore)
{
    ProcessAccessType processAccessType = ProcessAccessType::None;

    if (dataTypes.contains(WebsiteDataType::Cookies)) {
        if (isNonPersistentStore)
            processAccessType = std::max(processAccessType, ProcessAccessType::OnlyIfLaunched);
        else
            processAccessType = std::max(processAccessType, ProcessAccessType::Launch);
    }

    if (dataTypes.contains(WebsiteDataType::DiskCache) && !isNonPersistentStore)
        processAccessType = std::max(processAccessType, ProcessAccessType::Launch);

    if (dataTypes.contains(WebsiteDataType::HSTSCache))
        processAccessType = std::max(processAccessType, ProcessAccessType::Launch);

    if (dataTypes.contains(WebsiteDataType::Credentials))
        processAccessType = std::max(processAccessType, ProcessAccessType::Launch);

    if (dataTypes.contains(WebsiteDataType::DOMCache))
        processAccessType = std::max(processAccessType, ProcessAccessType::Launch);

    if (dataTypes.contains(WebsiteDataType::IndexedDBDatabases) && !isNonPersistentStore)
        processAccessType = std::max(processAccessType, ProcessAccessType::Launch);
    
#if ENABLE(SERVICE_WORKER)
    if (dataTypes.contains(WebsiteDataType::ServiceWorkerRegistrations) && !isNonPersistentStore)
        processAccessType = std::max(processAccessType, ProcessAccessType::Launch);
#endif

    return processAccessType;
}

static ProcessAccessType computeWebProcessAccessTypeForDataRemoval(OptionSet<WebsiteDataType> dataTypes, bool isNonPersistentStore)
{
    UNUSED_PARAM(isNonPersistentStore);

    ProcessAccessType processAccessType = ProcessAccessType::None;

    if (dataTypes.contains(WebsiteDataType::MemoryCache))
        processAccessType = std::max(processAccessType, ProcessAccessType::OnlyIfLaunched);

    if (dataTypes.contains(WebsiteDataType::Credentials))
        processAccessType = std::max(processAccessType, ProcessAccessType::OnlyIfLaunched);

    return processAccessType;
}

void WebsiteDataStore::removeData(OptionSet<WebsiteDataType> dataTypes, WallTime modifiedSince, Function<void()>&& completionHandler)
{
    struct CallbackAggregator : ThreadSafeRefCounted<CallbackAggregator> {
        CallbackAggregator(WebsiteDataStore& dataStore, Function<void()>&& completionHandler)
            : completionHandler(WTFMove(completionHandler))
            , protectedDataStore(dataStore)
        {
            ASSERT(RunLoop::isMain());
        }

        ~CallbackAggregator()
        {
            // Make sure the data store gets destroyed on the main thread even though the CallbackAggregator can get destroyed on a background queue.
            RunLoop::main().dispatch([protectedDataStore = WTFMove(protectedDataStore)] { });
        }

        void addPendingCallback()
        {
            pendingCallbacks++;
        }

        void removePendingCallback()
        {
            ASSERT(pendingCallbacks);
            --pendingCallbacks;

            callIfNeeded();
        }

        void callIfNeeded()
        {
            if (!pendingCallbacks)
                RunLoop::main().dispatch(WTFMove(completionHandler));
        }

        unsigned pendingCallbacks = 0;
        Function<void()> completionHandler;
        Ref<WebsiteDataStore> protectedDataStore;
    };

    RefPtr<CallbackAggregator> callbackAggregator = adoptRef(new CallbackAggregator(*this, WTFMove(completionHandler)));

#if ENABLE(VIDEO)
    if (dataTypes.contains(WebsiteDataType::DiskCache)) {
        callbackAggregator->addPendingCallback();
        m_queue->dispatch([modifiedSince, mediaCacheDirectory = m_configuration->mediaCacheDirectory().isolatedCopy(), callbackAggregator] {
            WebCore::HTMLMediaElement::clearMediaCache(mediaCacheDirectory, modifiedSince);
            
            WTF::RunLoop::main().dispatch([callbackAggregator] {
                callbackAggregator->removePendingCallback();
            });
        });
    }
#endif

#if ENABLE(RESOURCE_LOAD_STATISTICS)
    bool didNotifyNetworkProcessToDeleteWebsiteData = false;
#endif
    auto networkProcessAccessType = computeNetworkProcessAccessTypeForDataRemoval(dataTypes, !isPersistent());
    if (networkProcessAccessType != ProcessAccessType::None) {
        for (auto& processPool : processPools()) {
            switch (networkProcessAccessType) {
            case ProcessAccessType::OnlyIfLaunched:
                if (!processPool->networkProcess())
                    continue;
                break;

            case ProcessAccessType::Launch:
                processPool->ensureNetworkProcess(this);
                break;

            case ProcessAccessType::None:
                ASSERT_NOT_REACHED();
            }

            callbackAggregator->addPendingCallback();
            processPool->networkProcess()->deleteWebsiteData(m_sessionID, dataTypes, modifiedSince, [callbackAggregator, processPool] {
                callbackAggregator->removePendingCallback();
            });
#if ENABLE(RESOURCE_LOAD_STATISTICS)
            didNotifyNetworkProcessToDeleteWebsiteData = true;
#endif
        }
    }

    auto webProcessAccessType = computeWebProcessAccessTypeForDataRemoval(dataTypes, !isPersistent());
    if (webProcessAccessType != ProcessAccessType::None) {
        for (auto& processPool : processPools()) {
            processPool->clearSuspendedPages(AllowProcessCaching::No);
            processPool->webProcessCache().clear();
        }

        for (auto& process : processes()) {
            switch (webProcessAccessType) {
            case ProcessAccessType::OnlyIfLaunched:
                if (!process->canSendMessage())
                    continue;
                break;

            case ProcessAccessType::Launch:
                // FIXME: Handle this.
                ASSERT_NOT_REACHED();
                break;

            case ProcessAccessType::None:
                ASSERT_NOT_REACHED();
            }

            callbackAggregator->addPendingCallback();
            process->deleteWebsiteData(m_sessionID, dataTypes, modifiedSince, [callbackAggregator] {
                callbackAggregator->removePendingCallback();
            });
        }
    }

    if (dataTypes.contains(WebsiteDataType::SessionStorage) && m_storageManager) {
        callbackAggregator->addPendingCallback();

        m_storageManager->deleteSessionStorageOrigins([callbackAggregator] {
            callbackAggregator->removePendingCallback();
        });
    }

    if (dataTypes.contains(WebsiteDataType::LocalStorage) && m_storageManager) {
        callbackAggregator->addPendingCallback();

        m_storageManager->deleteLocalStorageOriginsModifiedSince(modifiedSince, [callbackAggregator] {
            callbackAggregator->removePendingCallback();
        });
    }

    if (dataTypes.contains(WebsiteDataType::DeviceIdHashSalt) || (dataTypes.contains(WebsiteDataType::Cookies))) {
        callbackAggregator->addPendingCallback();

        m_deviceIdHashSaltStorage->deleteDeviceIdHashSaltOriginsModifiedSince(modifiedSince, [callbackAggregator] {
            callbackAggregator->removePendingCallback();
        });
    }

    if (dataTypes.contains(WebsiteDataType::OfflineWebApplicationCache) && isPersistent()) {
        callbackAggregator->addPendingCallback();

        m_queue->dispatch([applicationCacheDirectory = m_configuration->applicationCacheDirectory().isolatedCopy(), applicationCacheFlatFileSubdirectoryName = m_configuration->applicationCacheFlatFileSubdirectoryName().isolatedCopy(), callbackAggregator] {
            auto storage = WebCore::ApplicationCacheStorage::create(applicationCacheDirectory, applicationCacheFlatFileSubdirectoryName);

            storage->deleteAllCaches();

            WTF::RunLoop::main().dispatch([callbackAggregator] {
                callbackAggregator->removePendingCallback();
            });
        });
    }

    if (dataTypes.contains(WebsiteDataType::WebSQLDatabases) && isPersistent()) {
        callbackAggregator->addPendingCallback();

        m_queue->dispatch([webSQLDatabaseDirectory = m_configuration->webSQLDatabaseDirectory().isolatedCopy(), callbackAggregator, modifiedSince] {
            WebCore::DatabaseTracker::trackerWithDatabasePath(webSQLDatabaseDirectory)->deleteDatabasesModifiedSince(modifiedSince);

            RunLoop::main().dispatch([callbackAggregator] {
                callbackAggregator->removePendingCallback();
            });
        });
    }

    if (dataTypes.contains(WebsiteDataType::MediaKeys) && isPersistent()) {
        callbackAggregator->addPendingCallback();

        m_queue->dispatch([mediaKeysStorageDirectory = m_configuration->mediaKeysStorageDirectory().isolatedCopy(), callbackAggregator, modifiedSince] {
            removeMediaKeys(mediaKeysStorageDirectory, modifiedSince);

            RunLoop::main().dispatch([callbackAggregator] {
                callbackAggregator->removePendingCallback();
            });
        });
    }

    if (dataTypes.contains(WebsiteDataType::SearchFieldRecentSearches) && isPersistent()) {
        callbackAggregator->addPendingCallback();

        m_queue->dispatch([modifiedSince, callbackAggregator] {
            platformRemoveRecentSearches(modifiedSince);

            RunLoop::main().dispatch([callbackAggregator] {
                callbackAggregator->removePendingCallback();
            });
        });
    }

#if ENABLE(NETSCAPE_PLUGIN_API)
    if (dataTypes.contains(WebsiteDataType::PlugInData) && isPersistent()) {
        class State {
        public:
            static void deleteData(Ref<CallbackAggregator>&& callbackAggregator, Vector<PluginModuleInfo>&& plugins, WallTime modifiedSince)
            {
                new State(WTFMove(callbackAggregator), WTFMove(plugins), modifiedSince);
            }

        private:
            State(Ref<CallbackAggregator>&& callbackAggregator, Vector<PluginModuleInfo>&& plugins, WallTime modifiedSince)
                : m_callbackAggregator(WTFMove(callbackAggregator))
                , m_plugins(WTFMove(plugins))
                , m_modifiedSince(modifiedSince)
            {
                m_callbackAggregator->addPendingCallback();

                deleteWebsiteDataForNextPlugin();
            }

            ~State()
            {
                ASSERT(m_plugins.isEmpty());
            }

            void deleteWebsiteDataForNextPlugin()
            {
                if (m_plugins.isEmpty()) {
                    m_callbackAggregator->removePendingCallback();

                    delete this;
                    return;
                }

                auto plugin = m_plugins.takeLast();
                PluginProcessManager::singleton().deleteWebsiteData(plugin, m_modifiedSince, [this] {
                    deleteWebsiteDataForNextPlugin();
                });
            }

            Ref<CallbackAggregator> m_callbackAggregator;
            Vector<PluginModuleInfo> m_plugins;
            WallTime m_modifiedSince;
        };

        State::deleteData(*callbackAggregator, plugins(), modifiedSince);
    }
#endif

#if ENABLE(RESOURCE_LOAD_STATISTICS)
    if (dataTypes.contains(WebsiteDataType::ResourceLoadStatistics)) {
        if (!didNotifyNetworkProcessToDeleteWebsiteData) {
            for (auto& processPool : processPools()) {
                if (auto* process = processPool->networkProcess()) {
                    callbackAggregator->addPendingCallback();
                    process->deleteWebsiteData(m_sessionID, dataTypes, modifiedSince, [callbackAggregator] {
                        callbackAggregator->removePendingCallback();
                    });
                }
            }
        }

        callbackAggregator->addPendingCallback();
        clearResourceLoadStatisticsInWebProcesses([callbackAggregator] {
            callbackAggregator->removePendingCallback();
        });
    }
#endif

    // There's a chance that we don't have any pending callbacks. If so, we want to dispatch the completion handler right away.
    callbackAggregator->callIfNeeded();
}

void WebsiteDataStore::removeData(OptionSet<WebsiteDataType> dataTypes, const Vector<WebsiteDataRecord>& dataRecords, Function<void()>&& completionHandler)
{
    Vector<WebCore::SecurityOriginData> origins;

    for (const auto& dataRecord : dataRecords) {
        for (auto& origin : dataRecord.origins)
            origins.append(origin);
    }

    struct CallbackAggregator : ThreadSafeRefCounted<CallbackAggregator> {
        CallbackAggregator(WebsiteDataStore& dataStore, Function<void()>&& completionHandler)
            : completionHandler(WTFMove(completionHandler))
            , protectedDataStore(dataStore)
        {
            ASSERT(RunLoop::isMain());
        }

        ~CallbackAggregator()
        {
            // Make sure the data store gets destroyed on the main thread even though the CallbackAggregator can get destroyed on a background queue.
            RunLoop::main().dispatch([protectedDataStore = WTFMove(protectedDataStore)] { });
        }

        void addPendingCallback()
        {
            pendingCallbacks++;
        }

        void removePendingCallback()
        {
            ASSERT(pendingCallbacks);
            --pendingCallbacks;

            callIfNeeded();
        }

        void callIfNeeded()
        {
            if (!pendingCallbacks)
                RunLoop::main().dispatch(WTFMove(completionHandler));
        }

        unsigned pendingCallbacks = 0;
        Function<void()> completionHandler;
        Ref<WebsiteDataStore> protectedDataStore;
    };

    RefPtr<CallbackAggregator> callbackAggregator = adoptRef(new CallbackAggregator(*this, WTFMove(completionHandler)));
    
    if (dataTypes.contains(WebsiteDataType::DiskCache)) {
        HashSet<WebCore::SecurityOriginData> origins;
        for (const auto& dataRecord : dataRecords) {
            for (const auto& origin : dataRecord.origins)
                origins.add(origin);
        }
        
#if ENABLE(VIDEO)
        callbackAggregator->addPendingCallback();
        m_queue->dispatch([origins = WTFMove(origins), mediaCacheDirectory = m_configuration->mediaCacheDirectory().isolatedCopy(), callbackAggregator] {

            // FIXME: Move SecurityOrigin::toRawString to SecurityOriginData and
            // make HTMLMediaElement::clearMediaCacheForOrigins take SecurityOriginData.
            HashSet<RefPtr<WebCore::SecurityOrigin>> securityOrigins;
            for (auto& origin : origins)
                securityOrigins.add(origin.securityOrigin());
            WebCore::HTMLMediaElement::clearMediaCacheForOrigins(mediaCacheDirectory, securityOrigins);
            
            WTF::RunLoop::main().dispatch([callbackAggregator] {
                callbackAggregator->removePendingCallback();
            });
        });
#endif
    }
    
    auto networkProcessAccessType = computeNetworkProcessAccessTypeForDataRemoval(dataTypes, !isPersistent());
    if (networkProcessAccessType != ProcessAccessType::None) {
        for (auto& processPool : processPools()) {
            switch (networkProcessAccessType) {
            case ProcessAccessType::OnlyIfLaunched:
                if (!processPool->networkProcess())
                    continue;
                break;

            case ProcessAccessType::Launch:
                processPool->ensureNetworkProcess(this);
                break;

            case ProcessAccessType::None:
                ASSERT_NOT_REACHED();
            }

            Vector<String> cookieHostNames;
            Vector<String> HSTSCacheHostNames;
            for (const auto& dataRecord : dataRecords) {
                for (auto& hostName : dataRecord.cookieHostNames)
                    cookieHostNames.append(hostName);
                for (auto& hostName : dataRecord.HSTSCacheHostNames)
                    HSTSCacheHostNames.append(hostName);
            }

            callbackAggregator->addPendingCallback();
            processPool->networkProcess()->deleteWebsiteDataForOrigins(m_sessionID, dataTypes, origins, cookieHostNames, HSTSCacheHostNames, [callbackAggregator, processPool] {
                callbackAggregator->removePendingCallback();
            });
        }
    }

    auto webProcessAccessType = computeWebProcessAccessTypeForDataRemoval(dataTypes, !isPersistent());
    if (webProcessAccessType != ProcessAccessType::None) {
        for (auto& process : processes()) {
            switch (webProcessAccessType) {
            case ProcessAccessType::OnlyIfLaunched:
                if (!process->canSendMessage())
                    continue;
                break;

            case ProcessAccessType::Launch:
                // FIXME: Handle this.
                ASSERT_NOT_REACHED();
                break;

            case ProcessAccessType::None:
                ASSERT_NOT_REACHED();
            }

            callbackAggregator->addPendingCallback();

            process->deleteWebsiteDataForOrigins(m_sessionID, dataTypes, origins, [callbackAggregator] {
                callbackAggregator->removePendingCallback();
            });
        }
    }

    if (dataTypes.contains(WebsiteDataType::SessionStorage) && m_storageManager) {
        callbackAggregator->addPendingCallback();

        m_storageManager->deleteSessionStorageEntriesForOrigins(origins, [callbackAggregator] {
            callbackAggregator->removePendingCallback();
        });
    }

    if (dataTypes.contains(WebsiteDataType::LocalStorage) && m_storageManager) {
        callbackAggregator->addPendingCallback();

        m_storageManager->deleteLocalStorageEntriesForOrigins(origins, [callbackAggregator] {
            callbackAggregator->removePendingCallback();
        });
    }

    if (dataTypes.contains(WebsiteDataType::DeviceIdHashSalt) || (dataTypes.contains(WebsiteDataType::Cookies))) {
        callbackAggregator->addPendingCallback();

        m_deviceIdHashSaltStorage->deleteDeviceIdHashSaltForOrigins(origins, [callbackAggregator] {
            callbackAggregator->removePendingCallback();
        });
    }

    if (dataTypes.contains(WebsiteDataType::OfflineWebApplicationCache) && isPersistent()) {
        HashSet<WebCore::SecurityOriginData> origins;
        for (const auto& dataRecord : dataRecords) {
            for (const auto& origin : dataRecord.origins)
                origins.add(origin);
        }

        callbackAggregator->addPendingCallback();
        m_queue->dispatch([origins = WTFMove(origins), applicationCacheDirectory = m_configuration->applicationCacheDirectory().isolatedCopy(), applicationCacheFlatFileSubdirectoryName = m_configuration->applicationCacheFlatFileSubdirectoryName().isolatedCopy(), callbackAggregator] {
            auto storage = WebCore::ApplicationCacheStorage::create(applicationCacheDirectory, applicationCacheFlatFileSubdirectoryName);

            for (const auto& origin : origins)
                storage->deleteCacheForOrigin(origin.securityOrigin());

            WTF::RunLoop::main().dispatch([callbackAggregator] {
                callbackAggregator->removePendingCallback();
            });
        });
    }

    if (dataTypes.contains(WebsiteDataType::WebSQLDatabases) && isPersistent()) {
        HashSet<WebCore::SecurityOriginData> origins;
        for (const auto& dataRecord : dataRecords) {
            for (const auto& origin : dataRecord.origins)
                origins.add(origin);
        }

        callbackAggregator->addPendingCallback();
        m_queue->dispatch([origins = WTFMove(origins), callbackAggregator, webSQLDatabaseDirectory = m_configuration->webSQLDatabaseDirectory().isolatedCopy()] {
            auto databaseTracker = WebCore::DatabaseTracker::trackerWithDatabasePath(webSQLDatabaseDirectory);
            for (auto& origin : origins)
                databaseTracker->deleteOrigin(origin);
            RunLoop::main().dispatch([callbackAggregator] {
                callbackAggregator->removePendingCallback();
            });
        });
    }

    if (dataTypes.contains(WebsiteDataType::MediaKeys) && isPersistent()) {
        HashSet<WebCore::SecurityOriginData> origins;
        for (const auto& dataRecord : dataRecords) {
            for (const auto& origin : dataRecord.origins)
                origins.add(origin);
        }

        callbackAggregator->addPendingCallback();
        m_queue->dispatch([mediaKeysStorageDirectory = m_configuration->mediaKeysStorageDirectory().isolatedCopy(), callbackAggregator, origins = WTFMove(origins)] {

            removeMediaKeys(mediaKeysStorageDirectory, origins);

            RunLoop::main().dispatch([callbackAggregator] {
                callbackAggregator->removePendingCallback();
            });
        });
    }

#if ENABLE(NETSCAPE_PLUGIN_API)
    if (dataTypes.contains(WebsiteDataType::PlugInData) && isPersistent()) {
        Vector<String> hostNames;
        for (const auto& dataRecord : dataRecords) {
            for (const auto& hostName : dataRecord.pluginDataHostNames)
                hostNames.append(hostName);
        }


        class State {
        public:
            static void deleteData(Ref<CallbackAggregator>&& callbackAggregator, Vector<PluginModuleInfo>&& plugins, Vector<String>&& hostNames)
            {
                new State(WTFMove(callbackAggregator), WTFMove(plugins), WTFMove(hostNames));
            }

        private:
            State(Ref<CallbackAggregator>&& callbackAggregator, Vector<PluginModuleInfo>&& plugins, Vector<String>&& hostNames)
                : m_callbackAggregator(WTFMove(callbackAggregator))
                , m_plugins(WTFMove(plugins))
                , m_hostNames(WTFMove(hostNames))
            {
                m_callbackAggregator->addPendingCallback();

                deleteWebsiteDataForNextPlugin();
            }

            ~State()
            {
                ASSERT(m_plugins.isEmpty());
            }

            void deleteWebsiteDataForNextPlugin()
            {
                if (m_plugins.isEmpty()) {
                    m_callbackAggregator->removePendingCallback();

                    delete this;
                    return;
                }

                auto plugin = m_plugins.takeLast();
                PluginProcessManager::singleton().deleteWebsiteDataForHostNames(plugin, m_hostNames, [this] {
                    deleteWebsiteDataForNextPlugin();
                });
            }

            Ref<CallbackAggregator> m_callbackAggregator;
            Vector<PluginModuleInfo> m_plugins;
            Vector<String> m_hostNames;
        };

        if (!hostNames.isEmpty())
            State::deleteData(*callbackAggregator, plugins(), WTFMove(hostNames));
    }
#endif

    // There's a chance that we don't have any pending callbacks. If so, we want to dispatch the completion handler right away.
    callbackAggregator->callIfNeeded();
}

#if ENABLE(RESOURCE_LOAD_STATISTICS)
void WebsiteDataStore::setMaxStatisticsEntries(size_t maximumEntryCount, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));

    for (auto& processPool : processPools())
        processPool->ensureNetworkProcess().setMaxStatisticsEntries(m_sessionID, maximumEntryCount, [processPool, callbackAggregator = callbackAggregator.copyRef()] { });
}

void WebsiteDataStore::setPruneEntriesDownTo(size_t pruneTargetCount, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));

    for (auto& processPool : processPools()) {
        processPool->ensureNetworkProcess().setPruneEntriesDownTo(m_sessionID, pruneTargetCount, [processPool, callbackAggregator = callbackAggregator.copyRef()] { });
    }
}

void WebsiteDataStore::setGrandfatheringTime(Seconds seconds, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));

    for (auto& processPool : processPools())
        processPool->ensureNetworkProcess().setGrandfatheringTime(m_sessionID, seconds, [processPool, callbackAggregator = callbackAggregator.copyRef()] { });
}

void WebsiteDataStore::setMinimumTimeBetweenDataRecordsRemoval(Seconds seconds, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));

    for (auto& processPool : processPools())
        processPool->ensureNetworkProcess().setMinimumTimeBetweenDataRecordsRemoval(m_sessionID, seconds, [processPool, callbackAggregator = callbackAggregator.copyRef()] { });
}
    
void WebsiteDataStore::dumpResourceLoadStatistics(CompletionHandler<void(const String&)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    for (auto& processPool : processPools()) {
        if (auto* process = processPool->networkProcess()) {
            process->dumpResourceLoadStatistics(m_sessionID, WTFMove(completionHandler));
            RELEASE_ASSERT(processPools().size() == 1);
            break;
        }
    }
}

void WebsiteDataStore::isPrevalentResource(const URL& url, CompletionHandler<void(bool isPrevalent)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    if (url.protocolIsAbout() || url.isEmpty()) {
        completionHandler(false);
        return;
    }

    for (auto& processPool : processPools()) {
        if (auto* process = processPool->networkProcess()) {
            process->isPrevalentResource(m_sessionID, RegistrableDomain { url }, WTFMove(completionHandler));
            RELEASE_ASSERT(processPools().size() == 1);
            break;
        }
    }
}

void WebsiteDataStore::setPrevalentResource(const URL& url, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    if (url.protocolIsAbout() || url.isEmpty()) {
        completionHandler();
        return;
    }
    
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));

    for (auto& processPool : processPools()) {
        processPool->ensureNetworkProcess().setPrevalentResource(m_sessionID, RegistrableDomain { url }, [processPool, callbackAggregator = callbackAggregator.copyRef()] { });
    }
}

void WebsiteDataStore::setPrevalentResourceForDebugMode(const URL& url, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    if (url.protocolIsAbout() || url.isEmpty()) {
        completionHandler();
        return;
    }
    
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));
    
    for (auto& processPool : processPools()) {
        if (auto* process = processPool->networkProcess())
            process->setPrevalentResourceForDebugMode(m_sessionID, RegistrableDomain { url }, [processPool, callbackAggregator = callbackAggregator.copyRef()] { });
    }
}

void WebsiteDataStore::isVeryPrevalentResource(const URL& url, CompletionHandler<void(bool isVeryPrevalent)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    if (url.protocolIsAbout() || url.isEmpty()) {
        completionHandler(false);
        return;
    }
    
    for (auto& processPool : processPools()) {
        if (auto* process = processPool->networkProcess()) {
            process->isVeryPrevalentResource(m_sessionID, RegistrableDomain { url }, WTFMove(completionHandler));
            ASSERT(processPools().size() == 1);
            break;
        }
    }
}

void WebsiteDataStore::setVeryPrevalentResource(const URL& url, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    if (url.protocolIsAbout() || url.isEmpty()) {
        completionHandler();
        return;
    }
    
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));
    
    for (auto& processPool : processPools()) {
        if (auto* process = processPool->networkProcess())
            process->setVeryPrevalentResource(m_sessionID, RegistrableDomain { url }, [processPool, callbackAggregator = callbackAggregator.copyRef()] { });
    }
}

void WebsiteDataStore::setShouldClassifyResourcesBeforeDataRecordsRemoval(bool value, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));

    for (auto& processPool : processPools())
        processPool->ensureNetworkProcess().setShouldClassifyResourcesBeforeDataRecordsRemoval(m_sessionID, value, [processPool, callbackAggregator = callbackAggregator.copyRef()] { });
}

void WebsiteDataStore::setSubframeUnderTopFrameDomain(const URL& subFrameURL, const URL& topFrameURL, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    if (subFrameURL.protocolIsAbout() || subFrameURL.isEmpty() || topFrameURL.protocolIsAbout() || topFrameURL.isEmpty()) {
        completionHandler();
        return;
    }

    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));
    
    for (auto& processPool : processPools()) {
        if (auto* process = processPool->networkProcess())
            process->setSubframeUnderTopFrameDomain(m_sessionID, RegistrableDomain { subFrameURL }, RegistrableDomain { topFrameURL }, [processPool, callbackAggregator = callbackAggregator.copyRef()] { });
    }
}

void WebsiteDataStore::isRegisteredAsSubFrameUnder(const URL& subFrameURL, const URL& topFrameURL, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    for (auto& processPool : processPools()) {
        if (auto* process = processPool->networkProcess()) {
            process->isRegisteredAsSubFrameUnder(m_sessionID, RegistrableDomain { subFrameURL }, RegistrableDomain { topFrameURL }, WTFMove(completionHandler));
            ASSERT(processPools().size() == 1);
            break;
        }
    }
}

void WebsiteDataStore::setSubresourceUnderTopFrameDomain(const URL& subresourceURL, const URL& topFrameURL, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    if (subresourceURL.protocolIsAbout() || subresourceURL.isEmpty() || topFrameURL.protocolIsAbout() || topFrameURL.isEmpty()) {
        completionHandler();
        return;
    }

    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));
    
    for (auto& processPool : processPools()) {
        if (auto* process = processPool->networkProcess())
            process->setSubresourceUnderTopFrameDomain(m_sessionID, RegistrableDomain { subresourceURL }, RegistrableDomain { topFrameURL }, [processPool, callbackAggregator = callbackAggregator.copyRef()] { });
    }
}

void WebsiteDataStore::isRegisteredAsSubresourceUnder(const URL& subresourceURL, const URL& topFrameURL, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    for (auto& processPool : processPools()) {
        if (auto* process = processPool->networkProcess()) {
            process->isRegisteredAsSubresourceUnder(m_sessionID, RegistrableDomain { subresourceURL }, RegistrableDomain { topFrameURL }, WTFMove(completionHandler));
            ASSERT(processPools().size() == 1);
            break;
        }
    }
}

void WebsiteDataStore::setSubresourceUniqueRedirectTo(const URL& subresourceURL, const URL& urlRedirectedTo, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    if (subresourceURL.protocolIsAbout() || subresourceURL.isEmpty() || urlRedirectedTo.protocolIsAbout() || urlRedirectedTo.isEmpty()) {
        completionHandler();
        return;
    }

    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));
    
    for (auto& processPool : processPools()) {
        if (auto* process = processPool->networkProcess())
            process->setSubresourceUniqueRedirectTo(m_sessionID, RegistrableDomain { subresourceURL }, RegistrableDomain { urlRedirectedTo }, [processPool, callbackAggregator = callbackAggregator.copyRef()] { });
    }
}

void WebsiteDataStore::setSubresourceUniqueRedirectFrom(const URL& subresourceURL, const URL& urlRedirectedFrom, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    if (subresourceURL.protocolIsAbout() || subresourceURL.isEmpty() || urlRedirectedFrom.protocolIsAbout() || urlRedirectedFrom.isEmpty()) {
        completionHandler();
        return;
    }

    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));
    
    for (auto& processPool : processPools()) {
        if (auto* process = processPool->networkProcess())
            process->setSubresourceUniqueRedirectFrom(m_sessionID, RegistrableDomain { subresourceURL }, RegistrableDomain { urlRedirectedFrom }, [processPool, callbackAggregator = callbackAggregator.copyRef()] { });
    }
}

void WebsiteDataStore::setTopFrameUniqueRedirectTo(const URL& topFrameURL, const URL& urlRedirectedTo, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    if (topFrameURL.protocolIsAbout() || topFrameURL.isEmpty() || urlRedirectedTo.protocolIsAbout() || urlRedirectedTo.isEmpty()) {
        completionHandler();
        return;
    }

    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));
    
    for (auto& processPool : processPools()) {
        if (auto* process = processPool->networkProcess())
            process->setTopFrameUniqueRedirectTo(m_sessionID, RegistrableDomain { topFrameURL }, RegistrableDomain { urlRedirectedTo }, [processPool, callbackAggregator = callbackAggregator.copyRef()] { });
    }
}

void WebsiteDataStore::setTopFrameUniqueRedirectFrom(const URL& topFrameURL, const URL& urlRedirectedFrom, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    if (topFrameURL.protocolIsAbout() || topFrameURL.isEmpty() || urlRedirectedFrom.protocolIsAbout() || urlRedirectedFrom.isEmpty()) {
        completionHandler();
        return;
    }

    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));
    
    for (auto& processPool : processPools()) {
        if (auto* process = processPool->networkProcess())
            process->setTopFrameUniqueRedirectFrom(m_sessionID, RegistrableDomain { topFrameURL }, RegistrableDomain { urlRedirectedFrom }, [processPool, callbackAggregator = callbackAggregator.copyRef()] { });
    }
}

void WebsiteDataStore::isRegisteredAsRedirectingTo(const URL& urlRedirectedFrom, const URL& urlRedirectedTo, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    for (auto& processPool : processPools()) {
        if (auto* process = processPool->networkProcess()) {
            process->isRegisteredAsRedirectingTo(m_sessionID, RegistrableDomain { urlRedirectedFrom }, RegistrableDomain { urlRedirectedTo }, WTFMove(completionHandler));
            ASSERT(processPools().size() == 1);
            break;
        }
    }
}

void WebsiteDataStore::clearPrevalentResource(const URL& url, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
        
    if (url.protocolIsAbout() || url.isEmpty()) {
        completionHandler();
        return;
    }

    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));

    for (auto& processPool : processPools()) {
        if (auto* process = processPool->networkProcess())
            process->clearPrevalentResource(m_sessionID, RegistrableDomain { url }, [processPool, callbackAggregator = callbackAggregator.copyRef()] { });
    }
}

void WebsiteDataStore::resetParametersToDefaultValues(CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));
    
    for (auto& processPool : processPools()) {
        if (auto* process = processPool->networkProcess())
            process->resetParametersToDefaultValues(m_sessionID, [processPool, callbackAggregator = callbackAggregator.copyRef()] { });
    }
}

void WebsiteDataStore::submitTelemetry()
{
    ASSERT(RunLoop::isMain());
    
    for (auto& processPool : processPools()) {
        if (auto* process = processPool->networkProcess())
            process->submitTelemetry(m_sessionID, [] { });
    }
}

void WebsiteDataStore::scheduleClearInMemoryAndPersistent(WallTime modifiedSince, ShouldGrandfatherStatistics shouldGrandfather, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));

    for (auto& processPool : processPools()) {
        if (auto* process = processPool->networkProcess())
            process->scheduleClearInMemoryAndPersistent(m_sessionID, modifiedSince, shouldGrandfather, [processPool, callbackAggregator = callbackAggregator.copyRef()] { });
    }
}

void WebsiteDataStore::scheduleClearInMemoryAndPersistent(ShouldGrandfatherStatistics shouldGrandfather, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));

    for (auto& processPool : processPools()) {
        if (auto* process = processPool->networkProcess())
            process->scheduleClearInMemoryAndPersistent(m_sessionID, { }, shouldGrandfather, [processPool, callbackAggregator = callbackAggregator.copyRef()] { });
    }
}

void WebsiteDataStore::scheduleCookieBlockingUpdate(CompletionHandler<void()>&& completionHandler)
{
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));

    for (auto& processPool : processPools()) {
        if (auto* process = processPool->networkProcess())
            process->scheduleCookieBlockingUpdate(m_sessionID, [processPool, callbackAggregator = callbackAggregator.copyRef()] { });
    }
}

void WebsiteDataStore::scheduleStatisticsAndDataRecordsProcessing(CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));
    
    for (auto& processPool : processPools()) {
        if (auto* process = processPool->networkProcess())
            process->scheduleStatisticsAndDataRecordsProcessing(m_sessionID, [processPool, callbackAggregator = callbackAggregator.copyRef()] { });
    }
}

void WebsiteDataStore::setLastSeen(const URL& url, Seconds seconds, CompletionHandler<void()>&& completionHandler)
{
    if (url.protocolIsAbout() || url.isEmpty()) {
        completionHandler();
        return;
    }

    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));

    for (auto& processPool : processPools()) {
        if (auto* process = processPool->networkProcess())
            process->setLastSeen(m_sessionID, RegistrableDomain { url }, seconds, [processPool, callbackAggregator = callbackAggregator.copyRef()] { });
    }
}

void WebsiteDataStore::setNotifyPagesWhenDataRecordsWereScanned(bool value, CompletionHandler<void()>&& completionHandler)
{
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));

    for (auto& processPool : processPools())
        processPool->ensureNetworkProcess().setNotifyPagesWhenDataRecordsWereScanned(m_sessionID, value, [processPool, callbackAggregator = callbackAggregator.copyRef()] { });
}

void WebsiteDataStore::setIsRunningResourceLoadStatisticsTest(bool value, CompletionHandler<void()>&& completionHandler)
{
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));
    
    for (auto& processPool : processPools())
        processPool->ensureNetworkProcess().setIsRunningResourceLoadStatisticsTest(m_sessionID, value, [processPool, callbackAggregator = callbackAggregator.copyRef()] { });
}

void WebsiteDataStore::setNotifyPagesWhenTelemetryWasCaptured(bool value, CompletionHandler<void()>&& completionHandler)
{
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));
    
    for (auto& processPool : processPools())
        processPool->ensureNetworkProcess().setNotifyPagesWhenTelemetryWasCaptured(m_sessionID, value, [processPool, callbackAggregator = callbackAggregator.copyRef()] { });
}

void WebsiteDataStore::getAllStorageAccessEntries(uint64_t pageID, CompletionHandler<void(Vector<String>&& domains)>&& completionHandler)
{
    auto* webPage = WebProcessProxy::webPage(pageID);
    if (!webPage) {
        completionHandler({ });
        return;
    }
    
    auto& networkProcess = webPage->process().processPool().ensureNetworkProcess();
    networkProcess.getAllStorageAccessEntries(m_sessionID, WTFMove(completionHandler));
}

void WebsiteDataStore::hasStorageAccess(const String& subFrameHost, const String& topFrameHost, uint64_t frameID, uint64_t pageID, CompletionHandler<void(bool)>&& completionHandler)
{
    if (!resourceLoadStatisticsEnabled()) {
        completionHandler(false);
        return;
    }

    auto* webPage = WebProcessProxy::webPage(pageID);
    if (!webPage) {
        completionHandler(false);
        return;
    }

    if (auto networkProcess = webPage->process().processPool().networkProcess())
        networkProcess->hasStorageAccess(m_sessionID, RegistrableDomain::uncheckedCreateFromHost(subFrameHost), RegistrableDomain::uncheckedCreateFromHost(topFrameHost), frameID, pageID, WTFMove(completionHandler));
}

void WebsiteDataStore::requestStorageAccess(const String& subFrameHost, const String& topFrameHost, uint64_t frameID, uint64_t pageID, CompletionHandler<void(StorageAccessStatus)>&& completionHandler)
{
    if (!resourceLoadStatisticsEnabled()) {
        completionHandler(StorageAccessStatus::CannotRequestAccess);
        return;
    }
    
    auto* webPage = WebProcessProxy::webPage(pageID);
    if (!webPage) {
        completionHandler(StorageAccessStatus::CannotRequestAccess);
        return;
    }

    if (auto networkProcess = webPage->process().processPool().networkProcess())
        networkProcess->requestStorageAccess(m_sessionID, RegistrableDomain::uncheckedCreateFromHost(subFrameHost), RegistrableDomain::uncheckedCreateFromHost(topFrameHost), frameID, pageID, WTFMove(completionHandler));
}

void WebsiteDataStore::grantStorageAccess(String&& subFrameHost, String&& topFrameHost, uint64_t frameID, uint64_t pageID, bool userWasPrompted, CompletionHandler<void(bool)>&& completionHandler)
{
    if (!resourceLoadStatisticsEnabled()) {
        completionHandler(false);
        return;
    }
    
    auto* webPage = WebProcessProxy::webPage(pageID);
    if (!webPage) {
        completionHandler(false);
        return;
    }

    if (auto networkProcess = webPage->process().processPool().networkProcess())
        networkProcess->grantStorageAccess(m_sessionID, RegistrableDomain::uncheckedCreateFromHost(subFrameHost), RegistrableDomain::uncheckedCreateFromHost(topFrameHost), frameID, pageID, userWasPrompted, WTFMove(completionHandler));
}

void WebsiteDataStore::setTimeToLiveUserInteraction(Seconds seconds, CompletionHandler<void()>&& completionHandler)
{
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));
    
    for (auto& processPool : processPools())
        processPool->ensureNetworkProcess().setTimeToLiveUserInteraction(m_sessionID, seconds, [callbackAggregator = callbackAggregator.copyRef()] { });
}

void WebsiteDataStore::logUserInteraction(const URL& url, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    if (url.protocolIsAbout() || url.isEmpty()) {
        completionHandler();
        return;
    }
    
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));

    for (auto& processPool : processPools()) {
        if (auto* process = processPool->networkProcess())
            process->logUserInteraction(m_sessionID, RegistrableDomain { url }, [callbackAggregator = callbackAggregator.copyRef()] { });
    }
}

void WebsiteDataStore::hasHadUserInteraction(const URL& url, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    if (url.protocolIsAbout() || url.isEmpty()) {
        completionHandler(false);
        return;
    }
    
    for (auto& processPool : processPools()) {
        if (auto* process = processPool->networkProcess()) {
            process->hasHadUserInteraction(m_sessionID, RegistrableDomain { url }, WTFMove(completionHandler));
            ASSERT(processPools().size() == 1);
            break;
        }
    }
}

void WebsiteDataStore::clearUserInteraction(const URL& url, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    if (url.protocolIsAbout() || url.isEmpty()) {
        completionHandler();
        return;
    }
    
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));

    for (auto& processPool : processPools()) {
        if (auto* process = processPool->networkProcess())
            process->clearUserInteraction(m_sessionID, RegistrableDomain { url }, [callbackAggregator = callbackAggregator.copyRef()] { });
    }
}

void WebsiteDataStore::setGrandfathered(const URL& url, bool isGrandfathered, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    if (url.protocolIsAbout() || url.isEmpty()) {
        completionHandler();
        return;
    }
    
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));
    
    for (auto& processPool : processPools()) {
        if (auto* process = processPool->networkProcess())
            process->setGrandfathered(m_sessionID, RegistrableDomain { url }, isGrandfathered, [callbackAggregator = callbackAggregator.copyRef()] { });
    }
}

void WebsiteDataStore::resetCrossSiteLoadsWithLinkDecorationForTesting(CompletionHandler<void()>&& completionHandler)
{
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));
    
    for (auto& processPool : processPools()) {
        if (auto* networkProcess = processPool->networkProcess())
            networkProcess->resetCrossSiteLoadsWithLinkDecorationForTesting(m_sessionID, [callbackAggregator = callbackAggregator.copyRef()] { });
    }
}

void WebsiteDataStore::deleteCookiesForTesting(const URL& url, bool includeHttpOnlyCookies, CompletionHandler<void()>&& completionHandler)
{
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));
    
    for (auto& processPool : processPools()) {
        if (auto* networkProcess = processPool->networkProcess())
            networkProcess->deleteCookiesForTesting(m_sessionID, RegistrableDomain { url }, includeHttpOnlyCookies, [callbackAggregator = callbackAggregator.copyRef()] { });
    }
}
#endif // ENABLE(RESOURCE_LOAD_STATISTICS)

void WebsiteDataStore::setCacheMaxAgeCapForPrevalentResources(Seconds seconds, CompletionHandler<void()>&& completionHandler)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));
    
    for (auto& processPool : processPools())
        processPool->ensureNetworkProcess().setCacheMaxAgeCapForPrevalentResources(m_sessionID, seconds, [callbackAggregator = callbackAggregator.copyRef()] { });
#else
    UNUSED_PARAM(seconds);
    completionHandler();
#endif
}

void WebsiteDataStore::resetCacheMaxAgeCapForPrevalentResources(CompletionHandler<void()>&& completionHandler)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));
    
    for (auto& processPool : processPools()) {
        if (auto* networkProcess = processPool->networkProcess())
            networkProcess->resetCacheMaxAgeCapForPrevalentResources(m_sessionID, [callbackAggregator = callbackAggregator.copyRef()] { });
    }
#else
    completionHandler();
#endif
}

void WebsiteDataStore::webPageWasAdded(WebPageProxy& webPageProxy)
{
    if (m_storageManager)
        m_storageManager->createSessionStorageNamespace(webPageProxy.pageID(), std::numeric_limits<unsigned>::max());
}

void WebsiteDataStore::webPageWasInvalidated(WebPageProxy& webPageProxy)
{
    if (m_storageManager)
        m_storageManager->destroySessionStorageNamespace(webPageProxy.pageID());
}

void WebsiteDataStore::webProcessWillOpenConnection(WebProcessProxy& webProcessProxy, IPC::Connection& connection)
{
    if (m_storageManager)
        m_storageManager->processWillOpenConnection(webProcessProxy, connection);
}

void WebsiteDataStore::webPageWillOpenConnection(WebPageProxy& webPageProxy, IPC::Connection& connection)
{
    if (m_storageManager)
        m_storageManager->addAllowedSessionStorageNamespaceConnection(webPageProxy.pageID(), connection);
}

void WebsiteDataStore::webPageDidCloseConnection(WebPageProxy& webPageProxy, IPC::Connection& connection)
{
    if (m_storageManager)
        m_storageManager->removeAllowedSessionStorageNamespaceConnection(webPageProxy.pageID(), connection);
}

void WebsiteDataStore::webProcessDidCloseConnection(WebProcessProxy& webProcessProxy, IPC::Connection& connection)
{
    if (m_storageManager)
        m_storageManager->processDidCloseConnection(webProcessProxy, connection);
}

bool WebsiteDataStore::isAssociatedProcessPool(WebProcessPool& processPool) const
{
    if (auto* processPoolDataStore = processPool.websiteDataStore())
        return &processPoolDataStore->websiteDataStore() == this;
    return false;
}

HashSet<RefPtr<WebProcessPool>> WebsiteDataStore::processPools(size_t count, bool ensureAPoolExists) const
{
    HashSet<RefPtr<WebProcessPool>> processPools;
    for (auto& process : processes())
        processPools.add(&process->processPool());

    if (processPools.isEmpty()) {
        // Check if we're one of the legacy data stores.
        for (auto& processPool : WebProcessPool::allProcessPools()) {
            if (!isAssociatedProcessPool(*processPool))
                continue;

            processPools.add(processPool);

            if (processPools.size() == count)
                break;
        }
    }

    if (processPools.isEmpty() && count && ensureAPoolExists) {
        auto processPool = WebProcessPool::create(API::ProcessPoolConfiguration::createWithWebsiteDataStoreConfiguration(m_configuration));
        processPools.add(processPool.ptr());
    }

    return processPools;
}

#if ENABLE(NETSCAPE_PLUGIN_API)
Vector<PluginModuleInfo> WebsiteDataStore::plugins() const
{
    Vector<PluginModuleInfo> plugins;

    for (auto& processPool : processPools()) {
        for (auto& plugin : processPool->pluginInfoStore().plugins())
            plugins.append(plugin);
    }

    return plugins;
}
#endif

static String computeMediaKeyFile(const String& mediaKeyDirectory)
{
    return FileSystem::pathByAppendingComponent(mediaKeyDirectory, "SecureStop.plist");
}

Vector<WebCore::SecurityOriginData> WebsiteDataStore::mediaKeyOrigins(const String& mediaKeysStorageDirectory)
{
    ASSERT(!mediaKeysStorageDirectory.isEmpty());

    Vector<WebCore::SecurityOriginData> origins;

    for (const auto& originPath : FileSystem::listDirectory(mediaKeysStorageDirectory, "*")) {
        auto mediaKeyFile = computeMediaKeyFile(originPath);
        if (!FileSystem::fileExists(mediaKeyFile))
            continue;

        auto mediaKeyIdentifier = FileSystem::pathGetFileName(originPath);

        if (auto securityOrigin = WebCore::SecurityOriginData::fromDatabaseIdentifier(mediaKeyIdentifier))
            origins.append(*securityOrigin);
    }

    return origins;
}

void WebsiteDataStore::removeMediaKeys(const String& mediaKeysStorageDirectory, WallTime modifiedSince)
{
    ASSERT(!mediaKeysStorageDirectory.isEmpty());

    for (const auto& mediaKeyDirectory : FileSystem::listDirectory(mediaKeysStorageDirectory, "*")) {
        auto mediaKeyFile = computeMediaKeyFile(mediaKeyDirectory);

        auto modificationTime = FileSystem::getFileModificationTime(mediaKeyFile);
        if (!modificationTime)
            continue;

        if (modificationTime.value() < modifiedSince)
            continue;

        FileSystem::deleteFile(mediaKeyFile);
        FileSystem::deleteEmptyDirectory(mediaKeyDirectory);
    }
}

void WebsiteDataStore::removeMediaKeys(const String& mediaKeysStorageDirectory, const HashSet<WebCore::SecurityOriginData>& origins)
{
    ASSERT(!mediaKeysStorageDirectory.isEmpty());

    for (const auto& origin : origins) {
        auto mediaKeyDirectory = FileSystem::pathByAppendingComponent(mediaKeysStorageDirectory, origin.databaseIdentifier());
        auto mediaKeyFile = computeMediaKeyFile(mediaKeyDirectory);

        FileSystem::deleteFile(mediaKeyFile);
        FileSystem::deleteEmptyDirectory(mediaKeyDirectory);
    }
}

bool WebsiteDataStore::resourceLoadStatisticsEnabled() const
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    return m_resourceLoadStatisticsEnabled;
#else
    return false;
#endif
}

bool WebsiteDataStore::resourceLoadStatisticsDebugMode() const
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    return m_resourceLoadStatisticsDebugMode;
#else
    return false;
#endif
}

void WebsiteDataStore::setResourceLoadStatisticsEnabled(bool enabled)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    if (m_sessionID.isEphemeral() || enabled == resourceLoadStatisticsEnabled())
        return;

    if (enabled) {
        enableResourceLoadStatisticsAndSetTestingCallback(nullptr);
        return;
    }

    for (auto& processPool : processPools(std::numeric_limits<size_t>::max(), false)) {
        processPool->setResourceLoadStatisticsEnabled(false);
        processPool->sendToNetworkingProcess(Messages::NetworkProcess::SetResourceLoadStatisticsEnabled(false));
    }
#else
    UNUSED_PARAM(enabled);
#endif
}

void WebsiteDataStore::setResourceLoadStatisticsDebugMode(bool enabled)
{
    setResourceLoadStatisticsDebugMode(enabled, []() { });
}

void WebsiteDataStore::setResourceLoadStatisticsDebugMode(bool enabled, CompletionHandler<void()>&& completionHandler)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    m_resourceLoadStatisticsDebugMode = enabled;

    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));
    
    for (auto& processPool : processPools())
        processPool->ensureNetworkProcess().setResourceLoadStatisticsDebugMode(m_sessionID, enabled, [callbackAggregator = callbackAggregator.copyRef()] { });
#else
    UNUSED_PARAM(enabled);
    UNUSED_PARAM(completionHandler);
#endif
}

#if ENABLE(RESOURCE_LOAD_STATISTICS)
void WebsiteDataStore::enableResourceLoadStatisticsAndSetTestingCallback(Function<void (const String&)>&& callback)
{
    ASSERT(!m_sessionID.isEphemeral());

    m_resourceLoadStatisticsEnabled = true;
    setStatisticsTestingCallback(WTFMove(callback));

    resolveDirectoriesIfNecessary();

    for (auto& processPool : processPools(std::numeric_limits<size_t>::max(), false)) {
        processPool->setResourceLoadStatisticsEnabled(true);
        processPool->sendToNetworkingProcess(Messages::NetworkProcess::SetResourceLoadStatisticsEnabled(true));
    }
}

void WebsiteDataStore::logTestingEvent(const String& event)
{
    ASSERT(RunLoop::isMain());
    
    if (m_statisticsTestingCallback)
        m_statisticsTestingCallback(event);
}

void WebsiteDataStore::clearResourceLoadStatisticsInWebProcesses(CompletionHandler<void()>&& callback)
{
    if (resourceLoadStatisticsEnabled()) {
        for (auto& processPool : processPools())
            processPool->clearResourceLoadStatistics();
    }
    callback();
}
#endif

Vector<WebCore::Cookie> WebsiteDataStore::pendingCookies() const
{
    return copyToVector(m_pendingCookies);
}

void WebsiteDataStore::addPendingCookie(const WebCore::Cookie& cookie)
{
    m_pendingCookies.add(cookie);
}

void WebsiteDataStore::removePendingCookie(const WebCore::Cookie& cookie)
{
    m_pendingCookies.remove(cookie);
}
    
void WebsiteDataStore::clearPendingCookies()
{
    m_pendingCookies.clear();
}

#if !PLATFORM(COCOA)
WebsiteDataStoreParameters WebsiteDataStore::parameters()
{
    WebsiteDataStoreParameters parameters;
    parameters.networkSessionParameters.sessionID = m_sessionID;

    resolveDirectoriesIfNecessary();

#if ENABLE(INDEXED_DATABASE)
    parameters.indexedDatabaseDirectory = resolvedIndexedDatabaseDirectory();
    if (!parameters.indexedDatabaseDirectory.isEmpty())
        SandboxExtension::createHandleForReadWriteDirectory(parameters.indexedDatabaseDirectory, parameters.indexedDatabaseDirectoryExtensionHandle);
#endif

#if ENABLE(SERVICE_WORKER)
    parameters.serviceWorkerRegistrationDirectory = resolvedServiceWorkerRegistrationDirectory();
    if (!parameters.serviceWorkerRegistrationDirectory.isEmpty())
        SandboxExtension::createHandleForReadWriteDirectory(parameters.serviceWorkerRegistrationDirectory, parameters.serviceWorkerRegistrationDirectoryExtensionHandle);
#endif

    platformSetNetworkParameters(parameters);

    return parameters;
}
#endif

#if HAVE(SEC_KEY_PROXY)
void WebsiteDataStore::addSecKeyProxyStore(Ref<SecKeyProxyStore>&& store)
{
    m_secKeyProxyStores.append(WTFMove(store));
}
#endif

#if ENABLE(WEB_AUTHN)
void WebsiteDataStore::setMockWebAuthenticationConfiguration(MockWebAuthenticationConfiguration&& configuration)
{
    if (!m_authenticatorManager->isMock()) {
        m_authenticatorManager = makeUniqueRef<MockAuthenticatorManager>(WTFMove(configuration));
        return;
    }
    static_cast<MockAuthenticatorManager*>(&m_authenticatorManager)->setTestConfiguration(WTFMove(configuration));
}
#endif

API::HTTPCookieStore& WebsiteDataStore::cookieStore()
{
    if (!m_cookieStore)
        m_cookieStore = API::HTTPCookieStore::create(*this);

    return *m_cookieStore;
}

void WebsiteDataStore::didCreateNetworkProcess()
{
}

bool WebsiteDataStore::setSourceApplicationSecondaryIdentifier(String&& identifier)
{
    if (!m_allowedToSetApplicationIdentifiers)
        return false;
    m_sourceApplicationSecondaryIdentifier = WTFMove(identifier);
    return true;
}

bool WebsiteDataStore::setSourceApplicationBundleIdentifier(String&& identifier)
{
    if (!m_allowedToSetApplicationIdentifiers)
        return false;
    m_sourceApplicationBundleIdentifier = WTFMove(identifier);
    return true;
}

}

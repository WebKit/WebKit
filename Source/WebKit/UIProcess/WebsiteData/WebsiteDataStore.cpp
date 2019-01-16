/*
 * Copyright (C) 2014-2017 Apple Inc. All rights reserved.
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
#include "StorageManager.h"
#include "WebCookieManagerProxy.h"
#include "WebProcessMessages.h"
#include "WebProcessPool.h"
#include "WebResourceLoadStatisticsStore.h"
#include "WebResourceLoadStatisticsStoreMessages.h"
#include "WebsiteData.h"
#include "WebsiteDataStoreParameters.h"
#include <WebCore/ApplicationCacheStorage.h>
#include <WebCore/DatabaseTracker.h>
#include <WebCore/FileSystem.h>
#include <WebCore/HTMLMediaElement.h>
#include <WebCore/OriginLock.h>
#include <WebCore/SecurityOrigin.h>
#include <WebCore/SecurityOriginData.h>
#include <wtf/CallbackAggregator.h>
#include <wtf/CompletionHandler.h>
#include <wtf/CrossThreadCopier.h>
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
#if ENABLE(WEB_AUTHN)
    , m_authenticatorManager(makeUniqueRef<AuthenticatorManager>())
#endif
{
    WTF::setProcessPrivileges(allPrivileges());
    maybeRegisterWithSessionIDMap();
    platformInitialize();

    ASSERT(RunLoop::isMain());
}

WebsiteDataStore::WebsiteDataStore(PAL::SessionID sessionID)
    : m_sessionID(sessionID)
    , m_resolvedConfiguration(WebsiteDataStoreConfiguration::create())
    , m_configuration(m_resolvedConfiguration->copy())
    , m_deviceIdHashSaltStorage(DeviceIdHashSaltStorage::create(isPersistent() ? m_configuration->deviceIdHashSaltsStorageDirectory() : String()))
    , m_queue(WorkQueue::create("com.apple.WebKit.WebsiteDataStore"))
#if ENABLE(WEB_AUTHN)
    , m_authenticatorManager(makeUniqueRef<AuthenticatorManager>())
#endif
{
    maybeRegisterWithSessionIDMap();
    platformInitialize();

    ASSERT(RunLoop::isMain());
}

WebsiteDataStore::~WebsiteDataStore()
{
    ASSERT(RunLoop::isMain());

    platformDestroy();

    unregisterWebResourceLoadStatisticsStoreAsMessageReceiver();

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
        m_resolvedConfiguration->setCookieStorageFile(resolveAndCreateReadWriteDirectoryForSandboxExtension(WebCore::FileSystem::directoryName(m_configuration->cookieStorageFile())));
        m_resolvedConfiguration->setCookieStorageFile(WebCore::FileSystem::pathByAppendingComponent(m_resolvedConfiguration->cookieStorageFile(), WebCore::FileSystem::pathGetFileName(m_configuration->cookieStorageFile())));
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

void WebsiteDataStore::fetchDataForTopPrivatelyControlledDomains(OptionSet<WebsiteDataType> dataTypes, OptionSet<WebsiteDataFetchOption> fetchOptions, const Vector<String>& topPrivatelyControlledDomains, Function<void(Vector<WebsiteDataRecord>&&, HashSet<String>&&)>&& completionHandler)
{
    fetchDataAndApply(dataTypes, fetchOptions, m_queue.copyRef(), [topPrivatelyControlledDomains = crossThreadCopy(topPrivatelyControlledDomains), completionHandler = WTFMove(completionHandler)] (auto&& existingDataRecords) mutable {
        ASSERT(!RunLoop::isMain());

        Vector<WebsiteDataRecord> matchingDataRecords;
        HashSet<String> domainsWithMatchingDataRecords;
        for (auto&& dataRecord : existingDataRecords) {
            for (auto& topPrivatelyControlledDomain : topPrivatelyControlledDomains) {
                if (dataRecord.matchesTopPrivatelyControlledDomain(topPrivatelyControlledDomain)) {
                    matchingDataRecords.append(WTFMove(dataRecord));
                    domainsWithMatchingDataRecords.add(topPrivatelyControlledDomain.isolatedCopy());
                    break;
                }
            }
        }
        RunLoop::main().dispatch([completionHandler = WTFMove(completionHandler), matchingDataRecords = WTFMove(matchingDataRecords), domainsWithMatchingDataRecords = WTFMove(domainsWithMatchingDataRecords)] () mutable {
            completionHandler(WTFMove(matchingDataRecords), WTFMove(domainsWithMatchingDataRecords));
        });
    });
}
    
void WebsiteDataStore::topPrivatelyControlledDomainsWithWebsiteData(OptionSet<WebsiteDataType> dataTypes, OptionSet<WebsiteDataFetchOption> fetchOptions, Function<void(HashSet<String>&&)>&& completionHandler)
{
    fetchData(dataTypes, fetchOptions, [completionHandler = WTFMove(completionHandler), protectedThis = makeRef(*this)](auto&& existingDataRecords) {
        HashSet<String> domainsWithDataRecords;
        for (auto&& dataRecord : existingDataRecords) {
            String domain = dataRecord.topPrivatelyControlledDomain();
            if (domain.isEmpty())
                continue;
            domainsWithDataRecords.add(WTFMove(domain));
        }
        completionHandler(WTFMove(domainsWithDataRecords));
    });
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

    if (dataTypes.contains(WebsiteDataType::ResourceLoadStatistics) && m_resourceLoadStatistics) {
        auto deletedTypesRaw = dataTypes.toRaw();
        auto monitoredTypesRaw = WebResourceLoadStatisticsStore::monitoredDataTypes().toRaw();
        
        // If we are deleting all of the data types that the resource load statistics store monitors
        // we do not need to re-grandfather old data.
        callbackAggregator->addPendingCallback();
        if ((monitoredTypesRaw & deletedTypesRaw) == monitoredTypesRaw)
            m_resourceLoadStatistics->scheduleClearInMemoryAndPersistent(modifiedSince, WebResourceLoadStatisticsStore::ShouldGrandfather::No, [callbackAggregator] {
                callbackAggregator->removePendingCallback();
            });
        else
            m_resourceLoadStatistics->scheduleClearInMemoryAndPersistent(modifiedSince, WebResourceLoadStatisticsStore::ShouldGrandfather::Yes, [callbackAggregator] {
                callbackAggregator->removePendingCallback();
            });

        callbackAggregator->addPendingCallback();
        clearResourceLoadStatisticsInWebProcesses([callbackAggregator] {
            callbackAggregator->removePendingCallback();
        });
    }

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

    // FIXME <rdar://problem/33491222>; scheduleClearInMemoryAndPersistent does not have a completion handler,
    // so the completion handler for this removeData() call can be called prematurely.
    if (dataTypes.contains(WebsiteDataType::ResourceLoadStatistics) && m_resourceLoadStatistics) {
        auto deletedTypesRaw = dataTypes.toRaw();
        auto monitoredTypesRaw = WebResourceLoadStatisticsStore::monitoredDataTypes().toRaw();

        // If we are deleting all of the data types that the resource load statistics store monitors
        // we do not need to re-grandfather old data.
        callbackAggregator->addPendingCallback();
        if ((monitoredTypesRaw & deletedTypesRaw) == monitoredTypesRaw)
            m_resourceLoadStatistics->scheduleClearInMemoryAndPersistent(WebResourceLoadStatisticsStore::ShouldGrandfather::No, [callbackAggregator] {
                callbackAggregator->removePendingCallback();
            });
        else
            m_resourceLoadStatistics->scheduleClearInMemoryAndPersistent(WebResourceLoadStatisticsStore::ShouldGrandfather::Yes, [callbackAggregator] {
                callbackAggregator->removePendingCallback();
            });

        callbackAggregator->addPendingCallback();
        clearResourceLoadStatisticsInWebProcesses([callbackAggregator] {
            callbackAggregator->removePendingCallback();
        });
    }

    // There's a chance that we don't have any pending callbacks. If so, we want to dispatch the completion handler right away.
    callbackAggregator->callIfNeeded();
}

void WebsiteDataStore::removeDataForTopPrivatelyControlledDomains(OptionSet<WebsiteDataType> dataTypes, OptionSet<WebsiteDataFetchOption> fetchOptions, const Vector<String>& topPrivatelyControlledDomains, Function<void(HashSet<String>&&)>&& completionHandler)
{
    fetchDataForTopPrivatelyControlledDomains(dataTypes, fetchOptions, topPrivatelyControlledDomains, [dataTypes, completionHandler = WTFMove(completionHandler), this, protectedThis = makeRef(*this)](Vector<WebsiteDataRecord>&& websiteDataRecords, HashSet<String>&& domainsWithDataRecords) mutable {
        this->removeData(dataTypes, websiteDataRecords, [domainsWithDataRecords = WTFMove(domainsWithDataRecords), completionHandler = WTFMove(completionHandler)]() mutable {
            completionHandler(WTFMove(domainsWithDataRecords));
        });
    });
}

#if ENABLE(RESOURCE_LOAD_STATISTICS)
void WebsiteDataStore::updatePrevalentDomainsToBlockCookiesFor(const Vector<String>& domainsToBlock, CompletionHandler<void()>&& completionHandler)
{
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));

    for (auto& processPool : processPools()) {
        if (auto* process = processPool->networkProcess())
            process->updatePrevalentDomainsToBlockCookiesFor(m_sessionID, domainsToBlock, [processPool, callbackAggregator = callbackAggregator.copyRef()] { });
    }
}

void WebsiteDataStore::setAgeCapForClientSideCookies(Optional<Seconds> seconds, CompletionHandler<void()>&& completionHandler)
{
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));
    
    for (auto& processPool : processPools()) {
        if (auto* process = processPool->networkProcess())
            process->setAgeCapForClientSideCookies(m_sessionID, seconds, [processPool, callbackAggregator = callbackAggregator.copyRef()] { });
    }
}

void WebsiteDataStore::hasStorageAccessForFrameHandler(const String& resourceDomain, const String& firstPartyDomain, uint64_t frameID, uint64_t pageID, CompletionHandler<void(bool hasAccess)>&& completionHandler)
{
    auto* webPage = WebProcessProxy::webPage(pageID);
    if (!webPage) {
        completionHandler(false);
        return;
    }

    auto& networkProcess = webPage->process().processPool().ensureNetworkProcess();
    networkProcess.hasStorageAccessForFrame(m_sessionID, resourceDomain, firstPartyDomain, frameID, pageID, WTFMove(completionHandler));
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

void WebsiteDataStore::grantStorageAccessHandler(const String& resourceDomain, const String& firstPartyDomain, Optional<uint64_t> frameID, uint64_t pageID, CompletionHandler<void(bool wasGranted)>&& completionHandler)
{
    auto* webPage = WebProcessProxy::webPage(pageID);
    if (!webPage) {
        completionHandler(false);
        return;
    }

    auto& networkProcess = webPage->process().processPool().ensureNetworkProcess();
    networkProcess.grantStorageAccess(m_sessionID, resourceDomain, firstPartyDomain, frameID, pageID, WTFMove(completionHandler));
}

void WebsiteDataStore::removeAllStorageAccessHandler(CompletionHandler<void()>&& completionHandler)
{
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));
    
    for (auto& processPool : processPools()) {
        if (auto networkProcess = processPool->networkProcess())
            networkProcess->removeAllStorageAccess(m_sessionID, [processPool, callbackAggregator = callbackAggregator.copyRef()] { });
    }
}

void WebsiteDataStore::removePrevalentDomains(const Vector<String>& domains)
{
    for (auto& processPool : processPools())
        processPool->sendToNetworkingProcess(Messages::NetworkProcess::RemovePrevalentDomains(m_sessionID, domains));
}

void WebsiteDataStore::hasStorageAccess(String&& subFrameHost, String&& topFrameHost, uint64_t frameID, uint64_t pageID, CompletionHandler<void(bool)>&& completionHandler)
{
    if (!resourceLoadStatisticsEnabled()) {
        completionHandler(false);
        return;
    }
    
    m_resourceLoadStatistics->hasStorageAccess(WTFMove(subFrameHost), WTFMove(topFrameHost), frameID, pageID, WTFMove(completionHandler));
}

void WebsiteDataStore::requestStorageAccess(String&& subFrameHost, String&& topFrameHost, uint64_t frameID, uint64_t pageID, bool promptEnabled, CompletionHandler<void(StorageAccessStatus)>&& completionHandler)
{
    if (!resourceLoadStatisticsEnabled()) {
        completionHandler(StorageAccessStatus::CannotRequestAccess);
        return;
    }
    
    m_resourceLoadStatistics->requestStorageAccess(WTFMove(subFrameHost), WTFMove(topFrameHost), frameID, pageID, promptEnabled, WTFMove(completionHandler));
}

void WebsiteDataStore::grantStorageAccess(String&& subFrameHost, String&& topFrameHost, uint64_t frameID, uint64_t pageID, bool userWasPrompted, CompletionHandler<void(bool)>&& completionHandler)
{
    if (!resourceLoadStatisticsEnabled()) {
        completionHandler(false);
        return;
    }
    
    m_resourceLoadStatistics->grantStorageAccess(WTFMove(subFrameHost), WTFMove(topFrameHost), frameID, pageID, userWasPrompted, WTFMove(completionHandler));
}
#endif // ENABLE(RESOURCE_LOAD_STATISTICS)

void WebsiteDataStore::setCacheMaxAgeCapForPrevalentResources(Seconds seconds, CompletionHandler<void()>&& completionHandler)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));
    
    for (auto& processPool : processPools()) {
        if (auto* networkProcess = processPool->networkProcess())
            networkProcess->setCacheMaxAgeCapForPrevalentResources(m_sessionID, seconds, [callbackAggregator = callbackAggregator.copyRef()] { });
    }
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

    if (m_resourceLoadStatistics)
        webProcessProxy.addMessageReceiver(Messages::WebResourceLoadStatisticsStore::messageReceiverName(), *m_resourceLoadStatistics);
}

void WebsiteDataStore::webPageWillOpenConnection(WebPageProxy& webPageProxy, IPC::Connection& connection)
{
    if (m_storageManager)
        m_storageManager->setAllowedSessionStorageNamespaceConnection(webPageProxy.pageID(), &connection);
}

void WebsiteDataStore::webPageDidCloseConnection(WebPageProxy& webPageProxy, IPC::Connection&)
{
    if (m_storageManager)
        m_storageManager->setAllowedSessionStorageNamespaceConnection(webPageProxy.pageID(), nullptr);
}

void WebsiteDataStore::webProcessDidCloseConnection(WebProcessProxy& webProcessProxy, IPC::Connection& connection)
{
    if (m_resourceLoadStatistics)
        webProcessProxy.removeMessageReceiver(Messages::WebResourceLoadStatisticsStore::messageReceiverName());

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
    return WebCore::FileSystem::pathByAppendingComponent(mediaKeyDirectory, "SecureStop.plist");
}

Vector<WebCore::SecurityOriginData> WebsiteDataStore::mediaKeyOrigins(const String& mediaKeysStorageDirectory)
{
    ASSERT(!mediaKeysStorageDirectory.isEmpty());

    Vector<WebCore::SecurityOriginData> origins;

    for (const auto& originPath : WebCore::FileSystem::listDirectory(mediaKeysStorageDirectory, "*")) {
        auto mediaKeyFile = computeMediaKeyFile(originPath);
        if (!WebCore::FileSystem::fileExists(mediaKeyFile))
            continue;

        auto mediaKeyIdentifier = WebCore::FileSystem::pathGetFileName(originPath);

        if (auto securityOrigin = WebCore::SecurityOriginData::fromDatabaseIdentifier(mediaKeyIdentifier))
            origins.append(*securityOrigin);
    }

    return origins;
}

void WebsiteDataStore::removeMediaKeys(const String& mediaKeysStorageDirectory, WallTime modifiedSince)
{
    ASSERT(!mediaKeysStorageDirectory.isEmpty());

    for (const auto& mediaKeyDirectory : WebCore::FileSystem::listDirectory(mediaKeysStorageDirectory, "*")) {
        auto mediaKeyFile = computeMediaKeyFile(mediaKeyDirectory);

        auto modificationTime = WebCore::FileSystem::getFileModificationTime(mediaKeyFile);
        if (!modificationTime)
            continue;

        if (modificationTime.value() < modifiedSince)
            continue;

        WebCore::FileSystem::deleteFile(mediaKeyFile);
        WebCore::FileSystem::deleteEmptyDirectory(mediaKeyDirectory);
    }
}

void WebsiteDataStore::removeMediaKeys(const String& mediaKeysStorageDirectory, const HashSet<WebCore::SecurityOriginData>& origins)
{
    ASSERT(!mediaKeysStorageDirectory.isEmpty());

    for (const auto& origin : origins) {
        auto mediaKeyDirectory = WebCore::FileSystem::pathByAppendingComponent(mediaKeysStorageDirectory, origin.databaseIdentifier());
        auto mediaKeyFile = computeMediaKeyFile(mediaKeyDirectory);

        WebCore::FileSystem::deleteFile(mediaKeyFile);
        WebCore::FileSystem::deleteEmptyDirectory(mediaKeyDirectory);
    }
}

bool WebsiteDataStore::resourceLoadStatisticsEnabled() const
{
    return !!m_resourceLoadStatistics;
}

void WebsiteDataStore::setResourceLoadStatisticsEnabled(bool enabled)
{
    if (m_sessionID.isEphemeral() || enabled == resourceLoadStatisticsEnabled())
        return;

    if (enabled) {
        ASSERT(!m_resourceLoadStatistics);
        enableResourceLoadStatisticsAndSetTestingCallback(nullptr);
        return;
    }


    unregisterWebResourceLoadStatisticsStoreAsMessageReceiver();
    m_resourceLoadStatistics = nullptr;

    auto existingProcessPools = processPools(std::numeric_limits<size_t>::max(), false);
    for (auto& processPool : existingProcessPools)
        processPool->setResourceLoadStatisticsEnabled(false);
}

void WebsiteDataStore::unregisterWebResourceLoadStatisticsStoreAsMessageReceiver()
{
    if (!m_resourceLoadStatistics)
        return;

    for (auto* webProcessProxy : processes())
        webProcessProxy->removeMessageReceiver(Messages::WebResourceLoadStatisticsStore::messageReceiverName());
}

void WebsiteDataStore::registerWebResourceLoadStatisticsStoreAsMessageReceiver()
{
    ASSERT(m_resourceLoadStatistics);
    if (!m_resourceLoadStatistics)
        return;

    for (auto* webProcessProxy : processes())
        webProcessProxy->addMessageReceiver(Messages::WebResourceLoadStatisticsStore::messageReceiverName(), *m_resourceLoadStatistics);
}

bool WebsiteDataStore::resourceLoadStatisticsDebugMode() const
{
    return m_resourceLoadStatisticsDebugMode;
}

void WebsiteDataStore::setResourceLoadStatisticsDebugMode(bool enabled)
{
    setResourceLoadStatisticsDebugMode(enabled, []() { });
}

void WebsiteDataStore::setResourceLoadStatisticsDebugMode(bool enabled, CompletionHandler<void()>&& completionHandler)
{
    m_resourceLoadStatisticsDebugMode = enabled;
    if (m_resourceLoadStatistics)
        m_resourceLoadStatistics->setResourceLoadStatisticsDebugMode(enabled, WTFMove(completionHandler));
    else
        completionHandler();
}

void WebsiteDataStore::enableResourceLoadStatisticsAndSetTestingCallback(Function<void (const String&)>&& callback)
{
    ASSERT(!m_sessionID.isEphemeral());

    if (m_resourceLoadStatistics) {
        m_resourceLoadStatistics->setStatisticsTestingCallback(WTFMove(callback));
        return;
    }

    resolveDirectoriesIfNecessary();
    m_resourceLoadStatistics = WebResourceLoadStatisticsStore::create(*this);
    m_resourceLoadStatistics->setStatisticsTestingCallback(WTFMove(callback));

    registerWebResourceLoadStatisticsStoreAsMessageReceiver();

    for (auto& processPool : processPools(std::numeric_limits<size_t>::max(), false))
        processPool->setResourceLoadStatisticsEnabled(true);
}

void WebsiteDataStore::clearResourceLoadStatisticsInWebProcesses(CompletionHandler<void()>&& callback)
{
    if (resourceLoadStatisticsEnabled()) {
        for (auto& processPool : processPools())
            processPool->clearResourceLoadStatistics();
    }
    callback();
}

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
    // FIXME: Implement cookies.
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

#if USE(CURL)
    platformSetParameters(parameters);
#endif

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

void WebsiteDataStore::didCreateNetworkProcess()
{
    if (m_resourceLoadStatistics)
        m_resourceLoadStatistics->didCreateNetworkProcess();
}

}

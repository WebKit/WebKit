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
#include "NetworkProcessMessages.h"
#include "StorageManager.h"
#include "StorageProcessCreationParameters.h"
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
#include <wtf/CompletionHandler.h>
#include <wtf/CrossThreadCopier.h>
#include <wtf/RunLoop.h>

#if ENABLE(NETSCAPE_PLUGIN_API)
#include "PluginProcessManager.h"
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

Ref<WebsiteDataStore> WebsiteDataStore::create(Configuration configuration, PAL::SessionID sessionID)
{
    return adoptRef(*new WebsiteDataStore(WTFMove(configuration), sessionID));
}

WebsiteDataStore::Configuration::Configuration()
    : resourceLoadStatisticsDirectory(API::WebsiteDataStore::defaultResourceLoadStatisticsDirectory())
{
}

WebsiteDataStore::WebsiteDataStore(Configuration configuration, PAL::SessionID sessionID)
    : m_sessionID(sessionID)
    , m_configuration(WTFMove(configuration))
    , m_storageManager(StorageManager::create(m_configuration.localStorageDirectory))
    , m_queue(WorkQueue::create("com.apple.WebKit.WebsiteDataStore"))
{
    maybeRegisterWithSessionIDMap();
    platformInitialize();
}

WebsiteDataStore::WebsiteDataStore(PAL::SessionID sessionID)
    : m_sessionID(sessionID)
    , m_configuration()
    , m_queue(WorkQueue::create("com.apple.WebKit.WebsiteDataStore"))
{
    maybeRegisterWithSessionIDMap();
    platformInitialize();
}

WebsiteDataStore::~WebsiteDataStore()
{
    platformDestroy();

    if (m_sessionID.isValid() && m_sessionID != PAL::SessionID::defaultSessionID()) {
        ASSERT(nonDefaultDataStores().get(m_sessionID) == this);
        nonDefaultDataStores().remove(m_sessionID);
        for (auto& processPool : WebProcessPool::allProcessPools())
            processPool->sendToNetworkingProcess(Messages::NetworkProcess::DestroySession(m_sessionID));
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
    if (!m_configuration.applicationCacheDirectory.isEmpty())
        m_resolvedConfiguration.applicationCacheDirectory = resolveAndCreateReadWriteDirectoryForSandboxExtension(m_configuration.applicationCacheDirectory);
    if (!m_configuration.mediaCacheDirectory.isEmpty())
        m_resolvedConfiguration.mediaCacheDirectory = resolveAndCreateReadWriteDirectoryForSandboxExtension(m_configuration.mediaCacheDirectory);
    if (!m_configuration.mediaKeysStorageDirectory.isEmpty())
        m_resolvedConfiguration.mediaKeysStorageDirectory = resolveAndCreateReadWriteDirectoryForSandboxExtension(m_configuration.mediaKeysStorageDirectory);
    if (!m_configuration.webSQLDatabaseDirectory.isEmpty())
        m_resolvedConfiguration.webSQLDatabaseDirectory = resolveAndCreateReadWriteDirectoryForSandboxExtension(m_configuration.webSQLDatabaseDirectory);
    if (!m_configuration.indexedDBDatabaseDirectory.isEmpty())
        m_resolvedConfiguration.indexedDBDatabaseDirectory = resolveAndCreateReadWriteDirectoryForSandboxExtension(m_configuration.indexedDBDatabaseDirectory);
    if (!m_configuration.serviceWorkerRegistrationDirectory.isEmpty() && m_resolvedConfiguration.serviceWorkerRegistrationDirectory.isEmpty())
        m_resolvedConfiguration.serviceWorkerRegistrationDirectory = resolveAndCreateReadWriteDirectoryForSandboxExtension(m_configuration.serviceWorkerRegistrationDirectory);
    if (!m_configuration.javaScriptConfigurationDirectory.isEmpty())
        m_resolvedConfiguration.javaScriptConfigurationDirectory = resolvePathForSandboxExtension(m_configuration.javaScriptConfigurationDirectory);
    if (!m_configuration.cacheStorageDirectory.isEmpty() && m_resolvedConfiguration.cacheStorageDirectory.isEmpty())
        m_resolvedConfiguration.cacheStorageDirectory = resolvePathForSandboxExtension(m_configuration.cacheStorageDirectory);

    // Resolve directories for file paths.
    if (!m_configuration.cookieStorageFile.isEmpty()) {
        m_resolvedConfiguration.cookieStorageFile = resolveAndCreateReadWriteDirectoryForSandboxExtension(WebCore::FileSystem::directoryName(m_configuration.cookieStorageFile));
        m_resolvedConfiguration.cookieStorageFile = WebCore::FileSystem::pathByAppendingComponent(m_resolvedConfiguration.cookieStorageFile, WebCore::FileSystem::pathGetFileName(m_configuration.cookieStorageFile));
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
        explicit CallbackAggregator(OptionSet<WebsiteDataFetchOption> fetchOptions, RefPtr<WorkQueue>&& queue, Function<void(Vector<WebsiteDataRecord>)>&& apply)
            : fetchOptions(fetchOptions)
            , queue(WTFMove(queue))
            , apply(WTFMove(apply))
        {
        }

        ~CallbackAggregator()
        {
            ASSERT(!pendingCallbacks);
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
                auto displayName = WebsiteDataRecord::displayNameForPluginDataHostName(hostName);
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
    };

    RefPtr<CallbackAggregator> callbackAggregator = adoptRef(new CallbackAggregator(fetchOptions, WTFMove(queue), WTFMove(apply)));

#if ENABLE(VIDEO)
    if (dataTypes.contains(WebsiteDataType::DiskCache)) {
        callbackAggregator->addPendingCallback();
        m_queue->dispatch([mediaCacheDirectory = m_configuration.mediaCacheDirectory.isolatedCopy(), callbackAggregator] {
            // FIXME: Make HTMLMediaElement::originsInMediaCache return a collection of SecurityOriginDatas.
            HashSet<RefPtr<WebCore::SecurityOrigin>> origins = WebCore::HTMLMediaElement::originsInMediaCache(mediaCacheDirectory);
            WebsiteData websiteData;
            
            for (auto& origin : origins) {
                WebsiteData::Entry entry { WebCore::SecurityOriginData::fromSecurityOrigin(*origin), WebsiteDataType::DiskCache, 0 };
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

    if (dataTypes.contains(WebsiteDataType::OfflineWebApplicationCache) && isPersistent()) {
        callbackAggregator->addPendingCallback();

        m_queue->dispatch([fetchOptions, applicationCacheDirectory = m_configuration.applicationCacheDirectory.isolatedCopy(), applicationCacheFlatFileSubdirectoryName = m_configuration.applicationCacheFlatFileSubdirectoryName.isolatedCopy(), callbackAggregator] {
            auto storage = WebCore::ApplicationCacheStorage::create(applicationCacheDirectory, applicationCacheFlatFileSubdirectoryName);

            WebsiteData websiteData;

            HashSet<RefPtr<WebCore::SecurityOrigin>> origins;
            // FIXME: getOriginsWithCache should return a collection of SecurityOriginDatas.
            storage->getOriginsWithCache(origins);

            for (auto& origin : origins) {
                uint64_t size = fetchOptions.contains(WebsiteDataFetchOption::ComputeSizes) ? storage->diskUsageForOrigin(*origin) : 0;
                WebsiteData::Entry entry { WebCore::SecurityOriginData::fromSecurityOrigin(*origin), WebsiteDataType::OfflineWebApplicationCache, size };

                websiteData.entries.append(WTFMove(entry));
            }

            RunLoop::main().dispatch([callbackAggregator, origins = WTFMove(origins), websiteData = WTFMove(websiteData)]() mutable {
                callbackAggregator->removePendingCallback(WTFMove(websiteData));
            });
        });
    }

    if (dataTypes.contains(WebsiteDataType::WebSQLDatabases) && isPersistent()) {
        callbackAggregator->addPendingCallback();

        m_queue->dispatch([webSQLDatabaseDirectory = m_configuration.webSQLDatabaseDirectory.isolatedCopy(), callbackAggregator] {
            auto origins = WebCore::DatabaseTracker::trackerWithDatabasePath(webSQLDatabaseDirectory)->origins();
            RunLoop::main().dispatch([callbackAggregator, origins = WTFMove(origins)]() mutable {
                WebsiteData websiteData;
                for (auto& origin : origins)
                    websiteData.entries.append(WebsiteData::Entry { WTFMove(origin), WebsiteDataType::WebSQLDatabases, 0 });
                callbackAggregator->removePendingCallback(WTFMove(websiteData));
            });
        });
    }

    if ((dataTypes.contains(WebsiteDataType::IndexedDBDatabases)
#if ENABLE(SERVICE_WORKER)
        || dataTypes.contains(WebsiteDataType::ServiceWorkerRegistrations)
#endif
        ) && isPersistent()) {
        for (auto& processPool : processPools()) {
            processPool->ensureStorageProcessAndWebsiteDataStore(this);

            callbackAggregator->addPendingCallback();
            processPool->storageProcess()->fetchWebsiteData(m_sessionID, dataTypes, [callbackAggregator, processPool](WebsiteData websiteData) {
                callbackAggregator->removePendingCallback(WTFMove(websiteData));
            });
        }
    }

    if (dataTypes.contains(WebsiteDataType::MediaKeys) && isPersistent()) {
        callbackAggregator->addPendingCallback();

        m_queue->dispatch([mediaKeysStorageDirectory = m_configuration.mediaKeysStorageDirectory.isolatedCopy(), callbackAggregator] {
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
        explicit CallbackAggregator(Function<void()>&& completionHandler)
            : completionHandler(WTFMove(completionHandler))
        {
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
    };

    RefPtr<CallbackAggregator> callbackAggregator = adoptRef(new CallbackAggregator(WTFMove(completionHandler)));

#if ENABLE(VIDEO)
    if (dataTypes.contains(WebsiteDataType::DiskCache)) {
        callbackAggregator->addPendingCallback();
        m_queue->dispatch([modifiedSince, mediaCacheDirectory = m_configuration.mediaCacheDirectory.isolatedCopy(), callbackAggregator] {
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

    if (dataTypes.contains(WebsiteDataType::OfflineWebApplicationCache) && isPersistent()) {
        callbackAggregator->addPendingCallback();

        m_queue->dispatch([applicationCacheDirectory = m_configuration.applicationCacheDirectory.isolatedCopy(), applicationCacheFlatFileSubdirectoryName = m_configuration.applicationCacheFlatFileSubdirectoryName.isolatedCopy(), callbackAggregator] {
            auto storage = WebCore::ApplicationCacheStorage::create(applicationCacheDirectory, applicationCacheFlatFileSubdirectoryName);

            storage->deleteAllCaches();

            WTF::RunLoop::main().dispatch([callbackAggregator] {
                callbackAggregator->removePendingCallback();
            });
        });
    }

    if (dataTypes.contains(WebsiteDataType::WebSQLDatabases) && isPersistent()) {
        callbackAggregator->addPendingCallback();

        m_queue->dispatch([webSQLDatabaseDirectory = m_configuration.webSQLDatabaseDirectory.isolatedCopy(), callbackAggregator, modifiedSince] {
            WebCore::DatabaseTracker::trackerWithDatabasePath(webSQLDatabaseDirectory)->deleteDatabasesModifiedSince(modifiedSince);

            RunLoop::main().dispatch([callbackAggregator] {
                callbackAggregator->removePendingCallback();
            });
        });
    }

    if ((dataTypes.contains(WebsiteDataType::IndexedDBDatabases)
#if ENABLE(SERVICE_WORKER)
        || dataTypes.contains(WebsiteDataType::ServiceWorkerRegistrations)
#endif
        ) && isPersistent()) {
        for (auto& processPool : processPools()) {
            processPool->ensureStorageProcessAndWebsiteDataStore(this);

            callbackAggregator->addPendingCallback();
            processPool->storageProcess()->deleteWebsiteData(m_sessionID, dataTypes, modifiedSince, [callbackAggregator, processPool] {
                callbackAggregator->removePendingCallback();
            });
        }
    }

    if (dataTypes.contains(WebsiteDataType::MediaKeys) && isPersistent()) {
        callbackAggregator->addPendingCallback();

        m_queue->dispatch([mediaKeysStorageDirectory = m_configuration.mediaKeysStorageDirectory.isolatedCopy(), callbackAggregator, modifiedSince] {
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
        explicit CallbackAggregator(Function<void()>&& completionHandler)
            : completionHandler(WTFMove(completionHandler))
        {
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
    };

    RefPtr<CallbackAggregator> callbackAggregator = adoptRef(new CallbackAggregator(WTFMove(completionHandler)));
    
    if (dataTypes.contains(WebsiteDataType::DiskCache)) {
        HashSet<WebCore::SecurityOriginData> origins;
        for (const auto& dataRecord : dataRecords) {
            for (const auto& origin : dataRecord.origins)
                origins.add(origin);
        }
        
#if ENABLE(VIDEO)
        callbackAggregator->addPendingCallback();
        m_queue->dispatch([origins = WTFMove(origins), mediaCacheDirectory = m_configuration.mediaCacheDirectory.isolatedCopy(), callbackAggregator] {

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
            for (const auto& dataRecord : dataRecords) {
                for (auto& hostName : dataRecord.cookieHostNames)
                    cookieHostNames.append(hostName);
            }

            callbackAggregator->addPendingCallback();
            processPool->networkProcess()->deleteWebsiteDataForOrigins(m_sessionID, dataTypes, origins, cookieHostNames, [callbackAggregator, processPool] {
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

    if (dataTypes.contains(WebsiteDataType::OfflineWebApplicationCache) && isPersistent()) {
        HashSet<WebCore::SecurityOriginData> origins;
        for (const auto& dataRecord : dataRecords) {
            for (const auto& origin : dataRecord.origins)
                origins.add(origin);
        }

        callbackAggregator->addPendingCallback();
        m_queue->dispatch([origins = WTFMove(origins), applicationCacheDirectory = m_configuration.applicationCacheDirectory.isolatedCopy(), applicationCacheFlatFileSubdirectoryName = m_configuration.applicationCacheFlatFileSubdirectoryName.isolatedCopy(), callbackAggregator] {
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
        m_queue->dispatch([origins = WTFMove(origins), callbackAggregator, webSQLDatabaseDirectory = m_configuration.webSQLDatabaseDirectory.isolatedCopy()] {
            auto databaseTracker = WebCore::DatabaseTracker::trackerWithDatabasePath(webSQLDatabaseDirectory);
            for (auto& origin : origins)
                databaseTracker->deleteOrigin(origin);
            RunLoop::main().dispatch([callbackAggregator] {
                callbackAggregator->removePendingCallback();
            });
        });
    }

    if ((dataTypes.contains(WebsiteDataType::IndexedDBDatabases)
#if ENABLE(SERVICE_WORKER)
        || dataTypes.contains(WebsiteDataType::ServiceWorkerRegistrations)
#endif
        ) && isPersistent()) {
        for (auto& processPool : processPools()) {
            processPool->ensureStorageProcessAndWebsiteDataStore(this);

            callbackAggregator->addPendingCallback();
            processPool->storageProcess()->deleteWebsiteDataForOrigins(m_sessionID, dataTypes, origins, [callbackAggregator, processPool] {
                callbackAggregator->removePendingCallback();
            });
        }
    }

    if (dataTypes.contains(WebsiteDataType::MediaKeys) && isPersistent()) {
        HashSet<WebCore::SecurityOriginData> origins;
        for (const auto& dataRecord : dataRecords) {
            for (const auto& origin : dataRecord.origins)
                origins.add(origin);
        }

        callbackAggregator->addPendingCallback();
        m_queue->dispatch([mediaKeysStorageDirectory = m_configuration.mediaKeysStorageDirectory.isolatedCopy(), callbackAggregator, origins = WTFMove(origins)] {

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

#if HAVE(CFNETWORK_STORAGE_PARTITIONING)
void WebsiteDataStore::updatePrevalentDomainsToPartitionOrBlockCookies(const Vector<String>& domainsToPartition, const Vector<String>& domainsToBlock, const Vector<String>& domainsToNeitherPartitionNorBlock, ShouldClearFirst shouldClearFirst)
{
    for (auto& processPool : processPools())
        processPool->sendToNetworkingProcess(Messages::NetworkProcess::UpdatePrevalentDomainsToPartitionOrBlockCookies(m_sessionID, domainsToPartition, domainsToBlock, domainsToNeitherPartitionNorBlock, shouldClearFirst == ShouldClearFirst::Yes));
}

void WebsiteDataStore::hasStorageAccessForFrameHandler(const String& resourceDomain, const String& firstPartyDomain, uint64_t frameID, uint64_t pageID, WTF::CompletionHandler<void(bool hasAccess)>&& callback)
{
    for (auto& processPool : processPools())
        processPool->networkProcess()->hasStorageAccessForFrame(m_sessionID, resourceDomain, firstPartyDomain, frameID, pageID, WTFMove(callback));
}

void WebsiteDataStore::getAllStorageAccessEntries(CompletionHandler<void(Vector<String>&& domains)>&& callback)
{
    for (auto& processPool : processPools())
        processPool->networkProcess()->getAllStorageAccessEntries(m_sessionID, WTFMove(callback));
}

void WebsiteDataStore::grantStorageAccessForFrameHandler(const String& resourceDomain, const String& firstPartyDomain, uint64_t frameID, uint64_t pageID, WTF::CompletionHandler<void(bool wasGranted)>&& callback)
{
    for (auto& processPool : processPools())
        processPool->networkProcess()->grantStorageAccessForFrame(m_sessionID, resourceDomain, firstPartyDomain, frameID, pageID, WTFMove(callback));
}

void WebsiteDataStore::removePrevalentDomains(const Vector<String>& domains)
{
    for (auto& processPool : processPools())
        processPool->sendToNetworkingProcess(Messages::NetworkProcess::RemovePrevalentDomains(m_sessionID, domains));
}

void WebsiteDataStore::hasStorageAccess(String&& subFrameHost, String&& topFrameHost, uint64_t frameID, uint64_t pageID, WTF::CompletionHandler<void (bool)>&& callback)
{
    if (!resourceLoadStatisticsEnabled()) {
        callback(false);
        return;
    }
    
    m_resourceLoadStatistics->hasStorageAccess(WTFMove(subFrameHost), WTFMove(topFrameHost), frameID, pageID, WTFMove(callback));
}

void WebsiteDataStore::requestStorageAccess(String&& subFrameHost, String&& topFrameHost, uint64_t frameID, uint64_t pageID, WTF::CompletionHandler<void (bool)>&& callback)
{
    if (!resourceLoadStatisticsEnabled()) {
        callback(false);
        return;
    }
    
    m_resourceLoadStatistics->requestStorageAccess(WTFMove(subFrameHost), WTFMove(topFrameHost), frameID, pageID, WTFMove(callback));
}
#endif

void WebsiteDataStore::networkProcessDidCrash()
{
#if HAVE(CFNETWORK_STORAGE_PARTITIONING)
    if (m_resourceLoadStatistics)
        m_resourceLoadStatistics->scheduleCookiePartitioningStateReset();
#endif
}

void WebsiteDataStore::webPageWasAdded(WebPageProxy& webPageProxy)
{
    if (m_storageManager)
        m_storageManager->createSessionStorageNamespace(webPageProxy.pageID(), std::numeric_limits<unsigned>::max());
}

void WebsiteDataStore::webPageWasRemoved(WebPageProxy& webPageProxy)
{
    if (m_storageManager)
        m_storageManager->destroySessionStorageNamespace(webPageProxy.pageID());
}

void WebsiteDataStore::webProcessWillOpenConnection(WebProcessProxy& webProcessProxy, IPC::Connection& connection)
{
    if (m_storageManager)
        m_storageManager->processWillOpenConnection(webProcessProxy, connection);

    if (m_resourceLoadStatistics)
        m_resourceLoadStatistics->processWillOpenConnection(webProcessProxy, connection);
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
        m_resourceLoadStatistics->processDidCloseConnection(webProcessProxy, connection);

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
    if (enabled == resourceLoadStatisticsEnabled())
        return;

    if (enabled) {
        ASSERT(!m_resourceLoadStatistics);
        enableResourceLoadStatisticsAndSetTestingCallback(nullptr);
        return;
    }

    m_resourceLoadStatistics = nullptr;

    auto existingProcessPools = processPools(std::numeric_limits<size_t>::max(), false);
    for (auto& processPool : existingProcessPools)
        processPool->setResourceLoadStatisticsEnabled(false);
}

bool WebsiteDataStore::resourceLoadStatisticsDebugMode() const
{
    return m_resourceLoadStatisticsDebugMode;
}

void WebsiteDataStore::setResourceLoadStatisticsDebugMode(bool enabled)
{
    m_resourceLoadStatisticsDebugMode = enabled;
    m_resourceLoadStatistics->setResourceLoadStatisticsDebugMode(enabled);
}

void WebsiteDataStore::enableResourceLoadStatisticsAndSetTestingCallback(Function<void (const String&)>&& callback)
{
    if (m_resourceLoadStatistics) {
        m_resourceLoadStatistics->setStatisticsTestingCallback(WTFMove(callback));
        return;
    }

#if HAVE(CFNETWORK_STORAGE_PARTITIONING)
    m_resourceLoadStatistics = WebResourceLoadStatisticsStore::create(m_configuration.resourceLoadStatisticsDirectory, WTFMove(callback), m_sessionID.isEphemeral(), [this, protectedThis = makeRef(*this)] (const Vector<String>& domainsToPartition, const Vector<String>& domainsToBlock, const Vector<String>& domainsToNeitherPartitionNorBlock, ShouldClearFirst shouldClearFirst) {
        updatePrevalentDomainsToPartitionOrBlockCookies(domainsToPartition, domainsToBlock, domainsToNeitherPartitionNorBlock, shouldClearFirst);
    }, [this, protectedThis = makeRef(*this)] (const String& resourceDomain, const String& firstPartyDomain, uint64_t frameID, uint64_t pageID, WTF::CompletionHandler<void(bool hasAccess)>&& callback) {
        hasStorageAccessForFrameHandler(resourceDomain, firstPartyDomain, frameID, pageID, WTFMove(callback));
    }, [this, protectedThis = makeRef(*this)] (const String& resourceDomain, const String& firstPartyDomain, uint64_t frameID, uint64_t pageID, WTF::CompletionHandler<void(bool wasGranted)>&& callback) {
        grantStorageAccessForFrameHandler(resourceDomain, firstPartyDomain, frameID, pageID, WTFMove(callback));
    }, [this, protectedThis = makeRef(*this)] (const Vector<String>& domainsToRemove) {
        removePrevalentDomains(domainsToRemove);
    });
#else
    m_resourceLoadStatistics = WebResourceLoadStatisticsStore::create(m_configuration.resourceLoadStatisticsDirectory, WTFMove(callback), m_sessionID.isEphemeral());
#endif

    for (auto& processPool : processPools())
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

StorageProcessCreationParameters WebsiteDataStore::storageProcessParameters()
{
    resolveDirectoriesIfNecessary();

    StorageProcessCreationParameters parameters;

    parameters.sessionID = m_sessionID;

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

    return parameters;
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

#if !PLATFORM(COCOA)
WebsiteDataStoreParameters WebsiteDataStore::parameters()
{
    // FIXME: Implement cookies.
    WebsiteDataStoreParameters parameters;
    parameters.networkSessionParameters.sessionID = m_sessionID;
    return parameters;
}
#endif

}

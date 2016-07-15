/*
 * Copyright (C) 2014-2016 Apple Inc. All rights reserved.
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
#include "NetworkProcessMessages.h"
#include "StorageManager.h"
#include "WebProcessMessages.h"
#include "WebProcessPool.h"
#include "WebResourceLoadStatisticsStore.h"
#include "WebResourceLoadStatisticsStoreMessages.h"
#include "WebsiteData.h"
#include <WebCore/ApplicationCacheStorage.h>
#include <WebCore/DatabaseTracker.h>
#include <WebCore/HTMLMediaElement.h>
#include <WebCore/OriginLock.h>
#include <WebCore/SecurityOrigin.h>
#include <wtf/RunLoop.h>

#if ENABLE(NETSCAPE_PLUGIN_API)
#include "PluginProcessManager.h"
#endif

namespace WebKit {

static WebCore::SessionID generateNonPersistentSessionID()
{
    // FIXME: We count backwards here to not conflict with API::Session.
    static uint64_t sessionID = std::numeric_limits<uint64_t>::max();

    return WebCore::SessionID(--sessionID);
}

static uint64_t generateIdentifier()
{
    static uint64_t identifier;

    return ++identifier;
}

Ref<WebsiteDataStore> WebsiteDataStore::createNonPersistent()
{
    return adoptRef(*new WebsiteDataStore(generateNonPersistentSessionID()));
}

Ref<WebsiteDataStore> WebsiteDataStore::create(Configuration configuration)
{
    return adoptRef(*new WebsiteDataStore(WTFMove(configuration)));
}

WebsiteDataStore::WebsiteDataStore(Configuration configuration)
    : m_identifier(generateIdentifier())
    , m_sessionID(WebCore::SessionID::defaultSessionID())
    , m_configuration(WTFMove(configuration))
    , m_storageManager(StorageManager::create(m_configuration.localStorageDirectory))
    , m_resourceLoadStatistics(WebResourceLoadStatisticsStore::create(m_configuration.resourceLoadStatisticsDirectory))
    , m_queue(WorkQueue::create("com.apple.WebKit.WebsiteDataStore"))
{
    platformInitialize();
}

WebsiteDataStore::WebsiteDataStore(WebCore::SessionID sessionID)
    : m_identifier(generateIdentifier())
    , m_sessionID(sessionID)
    , m_configuration()
    , m_queue(WorkQueue::create("com.apple.WebKit.WebsiteDataStore"))
{
    platformInitialize();
}

WebsiteDataStore::~WebsiteDataStore()
{
    platformDestroy();

    if (m_sessionID.isEphemeral()) {
        for (auto& processPool : WebProcessPool::allProcessPools())
            processPool->sendToNetworkingProcess(Messages::NetworkProcess::DestroyPrivateBrowsingSession(m_sessionID));
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

void WebsiteDataStore::fetchData(OptionSet<WebsiteDataType> dataTypes, OptionSet<WebsiteDataFetchOption> fetchOptions, std::function<void (Vector<WebsiteDataRecord>)> completionHandler)
{
    struct CallbackAggregator final : ThreadSafeRefCounted<CallbackAggregator> {
        explicit CallbackAggregator(OptionSet<WebsiteDataFetchOption> fetchOptions, std::function<void (Vector<WebsiteDataRecord>)> completionHandler)
            : fetchOptions(fetchOptions)
            , completionHandler(WTFMove(completionHandler))
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
                auto displayName = WebsiteDataRecord::displayNameForOrigin(*entry.origin);
                if (!displayName)
                    continue;

                auto& record = m_websiteDataRecords.add(displayName, WebsiteDataRecord { }).iterator->value;
                if (!record.displayName)
                    record.displayName = WTFMove(displayName);

                record.add(entry.type, WTFMove(entry.origin));

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

            callIfNeeded();
        }

        void callIfNeeded()
        {
            if (pendingCallbacks)
                return;

            RunLoop::main().dispatch([callbackAggregator = makeRef(*this)]() mutable {

                WTF::Vector<WebsiteDataRecord> records;
                records.reserveInitialCapacity(callbackAggregator->m_websiteDataRecords.size());

                for (auto& record : callbackAggregator->m_websiteDataRecords.values())
                    records.uncheckedAppend(WTFMove(record));

                callbackAggregator->completionHandler(WTFMove(records));
            });
        }

        const OptionSet<WebsiteDataFetchOption> fetchOptions;

        unsigned pendingCallbacks = 0;
        std::function<void (Vector<WebsiteDataRecord>)> completionHandler;

        HashMap<String, WebsiteDataRecord> m_websiteDataRecords;
    };

    RefPtr<CallbackAggregator> callbackAggregator = adoptRef(new CallbackAggregator(fetchOptions, WTFMove(completionHandler)));

#if ENABLE(VIDEO)
    if (dataTypes.contains(WebsiteDataType::DiskCache)) {
        callbackAggregator->addPendingCallback();
        m_queue->dispatch([fetchOptions, mediaCacheDirectory = m_configuration.mediaCacheDirectory.isolatedCopy(), callbackAggregator] {
            HashSet<RefPtr<WebCore::SecurityOrigin>> origins = WebCore::HTMLMediaElement::originsInMediaCache(mediaCacheDirectory);
            WebsiteData websiteData;
            
            for (auto& origin : origins) {
                WebsiteData::Entry entry { origin, WebsiteDataType::DiskCache, 0 };
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
                processPool->ensureNetworkProcess();
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

        m_storageManager->getSessionStorageOrigins([callbackAggregator](HashSet<RefPtr<WebCore::SecurityOrigin>>&& origins) {
            WebsiteData websiteData;

            while (!origins.isEmpty())
                websiteData.entries.append(WebsiteData::Entry { origins.takeAny(), WebsiteDataType::SessionStorage, 0 });

            callbackAggregator->removePendingCallback(WTFMove(websiteData));
        });
    }

    if (dataTypes.contains(WebsiteDataType::LocalStorage) && m_storageManager) {
        callbackAggregator->addPendingCallback();

        m_storageManager->getLocalStorageOrigins([callbackAggregator](HashSet<RefPtr<WebCore::SecurityOrigin>>&& origins) {
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
            storage->getOriginsWithCache(origins);

            for (auto& origin : origins) {
                uint64_t size = fetchOptions.contains(WebsiteDataFetchOption::ComputeSizes) ? storage->diskUsageForOrigin(*origin) : 0;
                WebsiteData::Entry entry { origin, WebsiteDataType::OfflineWebApplicationCache, size };

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
            Vector<RefPtr<WebCore::SecurityOrigin>> origins;
            WebCore::DatabaseTracker::trackerWithDatabasePath(webSQLDatabaseDirectory)->origins(origins);

            RunLoop::main().dispatch([callbackAggregator, origins = WTFMove(origins)]() mutable {
                WebsiteData websiteData;
                for (auto& origin : origins)
                    websiteData.entries.append(WebsiteData::Entry { WTFMove(origin), WebsiteDataType::WebSQLDatabases, 0 });

                callbackAggregator->removePendingCallback(WTFMove(websiteData));
            });
        });
    }

#if ENABLE(DATABASE_PROCESS)
    if (dataTypes.contains(WebsiteDataType::IndexedDBDatabases) && isPersistent()) {
        for (auto& processPool : processPools()) {
            processPool->ensureDatabaseProcess();

            callbackAggregator->addPendingCallback();
            processPool->databaseProcess()->fetchWebsiteData(m_sessionID, dataTypes, [callbackAggregator, processPool](WebsiteData websiteData) {
                callbackAggregator->removePendingCallback(WTFMove(websiteData));
            });
        }
    }
#endif

    if (dataTypes.contains(WebsiteDataType::MediaKeys) && isPersistent()) {
        callbackAggregator->addPendingCallback();

        m_queue->dispatch([mediaKeysStorageDirectory = m_configuration.mediaKeysStorageDirectory.isolatedCopy(), callbackAggregator] {
            auto origins = mediaKeyOrigins(mediaKeysStorageDirectory);

            RunLoop::main().dispatch([callbackAggregator, origins = WTFMove(origins)]() mutable {
                WebsiteData websiteData;
                for (auto& origin : origins)
                    websiteData.entries.append(WebsiteData::Entry { WTFMove(origin), WebsiteDataType::MediaKeys, 0 });

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
                PluginProcessManager::singleton().fetchWebsiteData(plugin, [this](Vector<String> hostNames) {
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

    return processAccessType;
}

static ProcessAccessType computeWebProcessAccessTypeForDataRemoval(OptionSet<WebsiteDataType> dataTypes, bool isNonPersistentStore)
{
    UNUSED_PARAM(isNonPersistentStore);

    ProcessAccessType processAccessType = ProcessAccessType::None;

    if (dataTypes.contains(WebsiteDataType::MemoryCache))
        processAccessType = std::max(processAccessType, ProcessAccessType::OnlyIfLaunched);

    return processAccessType;
}

void WebsiteDataStore::removeData(OptionSet<WebsiteDataType> dataTypes, std::chrono::system_clock::time_point modifiedSince, std::function<void ()> completionHandler)
{
    struct CallbackAggregator : ThreadSafeRefCounted<CallbackAggregator> {
        explicit CallbackAggregator (std::function<void ()> completionHandler)
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
        std::function<void ()> completionHandler;
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
                processPool->ensureNetworkProcess();
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

#if ENABLE(DATABASE_PROCESS)
    if (dataTypes.contains(WebsiteDataType::IndexedDBDatabases) && isPersistent()) {
        for (auto& processPool : processPools()) {
            processPool->ensureDatabaseProcess();

            callbackAggregator->addPendingCallback();
            processPool->databaseProcess()->deleteWebsiteData(m_sessionID, dataTypes, modifiedSince, [callbackAggregator, processPool] {
                callbackAggregator->removePendingCallback();
            });
        }
    }
#endif

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
            static void deleteData(Ref<CallbackAggregator>&& callbackAggregator, Vector<PluginModuleInfo>&& plugins, std::chrono::system_clock::time_point modifiedSince)
            {
                new State(WTFMove(callbackAggregator), WTFMove(plugins), modifiedSince);
            }

        private:
            State(Ref<CallbackAggregator>&& callbackAggregator, Vector<PluginModuleInfo>&& plugins, std::chrono::system_clock::time_point modifiedSince)
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
            std::chrono::system_clock::time_point m_modifiedSince;
        };

        State::deleteData(*callbackAggregator, plugins(), modifiedSince);
    }
#endif

    // There's a chance that we don't have any pending callbacks. If so, we want to dispatch the completion handler right away.
    callbackAggregator->callIfNeeded();
}

void WebsiteDataStore::removeData(OptionSet<WebsiteDataType> dataTypes, const Vector<WebsiteDataRecord>& dataRecords, std::function<void ()> completionHandler)
{
    Vector<RefPtr<WebCore::SecurityOrigin>> origins;

    for (const auto& dataRecord : dataRecords) {
        for (auto& origin : dataRecord.origins)
            origins.append(origin);
    }

    struct CallbackAggregator : ThreadSafeRefCounted<CallbackAggregator> {
        explicit CallbackAggregator (std::function<void ()> completionHandler)
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
        std::function<void ()> completionHandler;
    };

    RefPtr<CallbackAggregator> callbackAggregator = adoptRef(new CallbackAggregator(WTFMove(completionHandler)));
    
    if (dataTypes.contains(WebsiteDataType::DiskCache)) {
        HashSet<RefPtr<WebCore::SecurityOrigin>> origins;
        for (const auto& dataRecord : dataRecords) {
            for (const auto& origin : dataRecord.origins)
                origins.add(origin);
        }
        
#if ENABLE(VIDEO)
        callbackAggregator->addPendingCallback();
        m_queue->dispatch([origins = WTFMove(origins), mediaCacheDirectory = m_configuration.mediaCacheDirectory.isolatedCopy(), callbackAggregator] {
            WebCore::HTMLMediaElement::clearMediaCacheForOrigins(mediaCacheDirectory, origins);
            
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
                processPool->ensureNetworkProcess();
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
        HashSet<RefPtr<WebCore::SecurityOrigin>> origins;
        for (const auto& dataRecord : dataRecords) {
            for (const auto& origin : dataRecord.origins)
                origins.add(origin);
        }

        callbackAggregator->addPendingCallback();
        m_queue->dispatch([origins = WTFMove(origins), applicationCacheDirectory = m_configuration.applicationCacheDirectory.isolatedCopy(), applicationCacheFlatFileSubdirectoryName = m_configuration.applicationCacheFlatFileSubdirectoryName.isolatedCopy(), callbackAggregator] {
            auto storage = WebCore::ApplicationCacheStorage::create(applicationCacheDirectory, applicationCacheFlatFileSubdirectoryName);

            for (const auto& origin : origins)
                storage->deleteCacheForOrigin(*origin);

            WTF::RunLoop::main().dispatch([callbackAggregator] {
                callbackAggregator->removePendingCallback();
            });
        });
    }

    if (dataTypes.contains(WebsiteDataType::WebSQLDatabases) && isPersistent()) {
        HashSet<RefPtr<WebCore::SecurityOrigin>> origins;
        for (const auto& dataRecord : dataRecords) {
            for (const auto& origin : dataRecord.origins)
                origins.add(origin);
        }

        callbackAggregator->addPendingCallback();
        m_queue->dispatch([origins = WTFMove(origins), callbackAggregator, webSQLDatabaseDirectory = m_configuration.webSQLDatabaseDirectory.isolatedCopy()] {
            auto databaseTracker = WebCore::DatabaseTracker::trackerWithDatabasePath(webSQLDatabaseDirectory);

            for (const auto& origin : origins)
                databaseTracker->deleteOrigin(origin.get());

            RunLoop::main().dispatch([callbackAggregator] {
                callbackAggregator->removePendingCallback();
            });
        });
    }

#if ENABLE(DATABASE_PROCESS)
    if (dataTypes.contains(WebsiteDataType::IndexedDBDatabases) && isPersistent()) {
        for (auto& processPool : processPools()) {
            processPool->ensureDatabaseProcess();

            callbackAggregator->addPendingCallback();
            processPool->databaseProcess()->deleteWebsiteDataForOrigins(m_sessionID, dataTypes, origins, [callbackAggregator, processPool] {
                callbackAggregator->removePendingCallback();
            });
        }
    }
#endif

    if (dataTypes.contains(WebsiteDataType::MediaKeys) && isPersistent()) {
        HashSet<RefPtr<WebCore::SecurityOrigin>> origins;
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

        State::deleteData(*callbackAggregator, plugins(), WTFMove(hostNames));
    }
#endif

    // There's a chance that we don't have any pending callbacks. If so, we want to dispatch the completion handler right away.
    callbackAggregator->callIfNeeded();
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

HashSet<RefPtr<WebProcessPool>> WebsiteDataStore::processPools() const
{
    HashSet<RefPtr<WebProcessPool>> processPools;
    for (auto& process : processes())
        processPools.add(&process->processPool());

    if (processPools.isEmpty()) {
        // Check if we're one of the legacy data stores.
        for (auto& processPool : WebProcessPool::allProcessPools()) {
            if (auto dataStore = processPool->websiteDataStore()) {
                if (&dataStore->websiteDataStore() == this) {
                    processPools.add(processPool);
                    break;
                }
            }
        }
    }

    if (processPools.isEmpty()) {
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
    return WebCore::pathByAppendingComponent(mediaKeyDirectory, "SecureStop.plist");
}

Vector<RefPtr<WebCore::SecurityOrigin>> WebsiteDataStore::mediaKeyOrigins(const String& mediaKeysStorageDirectory)
{
    ASSERT(!mediaKeysStorageDirectory.isEmpty());

    Vector<RefPtr<WebCore::SecurityOrigin>> origins;

    for (const auto& originPath : WebCore::listDirectory(mediaKeysStorageDirectory, "*")) {
        auto mediaKeyFile = computeMediaKeyFile(originPath);
        if (!WebCore::fileExists(mediaKeyFile))
            continue;

        auto mediaKeyIdentifier = WebCore::pathGetFileName(originPath);

        if (auto securityOrigin = WebCore::SecurityOrigin::maybeCreateFromDatabaseIdentifier(mediaKeyIdentifier))
            origins.append(WTFMove(securityOrigin));
    }

    return origins;
}

void WebsiteDataStore::removeMediaKeys(const String& mediaKeysStorageDirectory, std::chrono::system_clock::time_point modifiedSince)
{
    ASSERT(!mediaKeysStorageDirectory.isEmpty());

    for (const auto& mediaKeyDirectory : WebCore::listDirectory(mediaKeysStorageDirectory, "*")) {
        auto mediaKeyFile = computeMediaKeyFile(mediaKeyDirectory);

        time_t modificationTime;
        if (!WebCore::getFileModificationTime(mediaKeyFile, modificationTime))
            continue;

        if (std::chrono::system_clock::from_time_t(modificationTime) < modifiedSince)
            continue;

        WebCore::deleteFile(mediaKeyFile);
        WebCore::deleteEmptyDirectory(mediaKeyDirectory);
    }
}

void WebsiteDataStore::removeMediaKeys(const String& mediaKeysStorageDirectory, const HashSet<RefPtr<WebCore::SecurityOrigin>>& origins)
{
    ASSERT(!mediaKeysStorageDirectory.isEmpty());

    for (const auto& origin : origins) {
        auto mediaKeyDirectory = WebCore::pathByAppendingComponent(mediaKeysStorageDirectory, origin->databaseIdentifier());
        auto mediaKeyFile = computeMediaKeyFile(mediaKeyDirectory);

        WebCore::deleteFile(mediaKeyFile);
        WebCore::deleteEmptyDirectory(mediaKeyDirectory);
    }
}

bool WebsiteDataStore::resourceLoadStatisticsEnabled() const
{
    return m_resourceLoadStatistics ? m_resourceLoadStatistics->resourceLoadStatisticsEnabled() : false;
}

void WebsiteDataStore::setResourceLoadStatisticsEnabled(bool enabled)
{
    if (!m_resourceLoadStatistics)
        return;

    if (enabled == resourceLoadStatisticsEnabled())
        return;

    m_resourceLoadStatistics->setResourceLoadStatisticsEnabled(enabled);

    for (auto& processPool : WebProcessPool::allProcessPools()) {
        processPool->setResourceLoadStatisticsEnabled(enabled);
        processPool->sendToAllProcesses(Messages::WebProcess::SetResourceLoadStatisticsEnabled(enabled));
    }
}

}

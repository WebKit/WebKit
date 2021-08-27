/*
 * Copyright (C) 2014-2020 Apple Inc. All rights reserved.
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

#include "APIHTTPCookieStore.h"
#include "APIProcessPoolConfiguration.h"
#include "APIWebsiteDataRecord.h"
#include "AuthenticatorManager.h"
#include "DeviceIdHashSaltStorage.h"
#include "GPUProcessProxy.h"
#include "Logging.h"
#include "MockAuthenticatorManager.h"
#include "NetworkProcessConnectionInfo.h"
#include "NetworkProcessMessages.h"
#include "ShouldGrandfatherStatistics.h"
#include "StorageAccessStatus.h"
#include "WebBackForwardCache.h"
#include "WebCookieManagerProxy.h"
#include "WebKit2Initialize.h"
#include "WebPageProxy.h"
#include "WebProcessCache.h"
#include "WebProcessMessages.h"
#include "WebProcessPool.h"
#include "WebResourceLoadStatisticsStore.h"
#include "WebsiteData.h"
#include "WebsiteDataStoreClient.h"
#include "WebsiteDataStoreParameters.h"
#include <WebCore/ApplicationCacheStorage.h>
#include <WebCore/CredentialStorage.h>
#include <WebCore/DatabaseTracker.h>
#include <WebCore/HTMLMediaElement.h>
#include <WebCore/NetworkStorageSession.h>
#include <WebCore/OriginLock.h>
#include <WebCore/RegistrableDomain.h>
#include <WebCore/SecurityOrigin.h>
#include <WebCore/SecurityOriginData.h>
#include <WebCore/StorageQuotaManager.h>
#include <wtf/CallbackAggregator.h>
#include <wtf/CompletionHandler.h>
#include <wtf/CrossThreadCopier.h>
#include <wtf/FileSystem.h>
#include <wtf/ProcessPrivilege.h>
#include <wtf/RunLoop.h>

#if OS(DARWIN)
#include <wtf/spi/darwin/OSVariantSPI.h>
#endif

#if ENABLE(NETSCAPE_PLUGIN_API)
#include "PluginProcessManager.h"
#endif

#if HAVE(SEC_KEY_PROXY)
#include "SecKeyProxyStore.h"
#endif

#if HAVE(APP_SSO)
#include "SOAuthorizationCoordinator.h"
#endif

#if PLATFORM(COCOA)
#include "DefaultWebBrowserChecks.h"
#endif

namespace WebKit {

static bool allowsWebsiteDataRecordsForAllOrigins;
void WebsiteDataStore::allowWebsiteDataRecordsForAllOrigins()
{
    allowsWebsiteDataRecordsForAllOrigins = true;
}

static HashMap<PAL::SessionID, WebsiteDataStore*>& allDataStores()
{
    RELEASE_ASSERT(isUIThread());
    static NeverDestroyed<HashMap<PAL::SessionID, WebsiteDataStore*>> map;
    return map;
}

void WebsiteDataStore::forEachWebsiteDataStore(Function<void(WebsiteDataStore&)>&& function)
{
    for (auto* dataStore : allDataStores().values())
        function(*dataStore);
}

Ref<WebsiteDataStore> WebsiteDataStore::createNonPersistent()
{
    return adoptRef(*new WebsiteDataStore(WebsiteDataStoreConfiguration::create(IsPersistent::No), PAL::SessionID::generateEphemeralSessionID()));
}

Ref<WebsiteDataStore> WebsiteDataStore::create(Ref<WebsiteDataStoreConfiguration>&& configuration, PAL::SessionID sessionID)
{
    return adoptRef(*new WebsiteDataStore(WTFMove(configuration), sessionID));
}

WebsiteDataStore::WebsiteDataStore(Ref<WebsiteDataStoreConfiguration>&& configuration, PAL::SessionID sessionID)
    : m_sessionID(sessionID)
    , m_resolvedConfiguration(WTFMove(configuration))
    , m_configuration(m_resolvedConfiguration->copy())
    , m_deviceIdHashSaltStorage(DeviceIdHashSaltStorage::create(isPersistent() ? m_configuration->deviceIdHashSaltsStorageDirectory() : String()))
    , m_queue(WorkQueue::create("com.apple.WebKit.WebsiteDataStore"))
#if ENABLE(WEB_AUTHN)
    , m_authenticatorManager(makeUniqueRef<AuthenticatorManager>())
#endif
    , m_client(makeUniqueRef<WebsiteDataStoreClient>())
#if HAVE(APP_SSO)
    , m_soAuthorizationCoordinator(makeUniqueRef<SOAuthorizationCoordinator>())
#endif
{
    WTF::setProcessPrivileges(allPrivileges());
    registerWithSessionIDMap();
    platformInitialize();

    ASSERT(RunLoop::isMain());
}

WebsiteDataStore::~WebsiteDataStore()
{
    ASSERT(RunLoop::isMain());
    RELEASE_ASSERT(m_sessionID.isValid());

    platformDestroy();

    ASSERT(allDataStores().get(m_sessionID) == this);
    allDataStores().remove(m_sessionID);
    networkProcess().removeSession(*this);
#if ENABLE(GPU_PROCESS)
    if (auto* gpuProcessProxy = GPUProcessProxy::singletonIfCreated())
        gpuProcessProxy->removeSession(m_sessionID);
#endif
}

static RefPtr<WebsiteDataStore>& globalDefaultDataStore()
{
    static NeverDestroyed<RefPtr<WebsiteDataStore>> globalDefaultDataStore;
    return globalDefaultDataStore.get();
}

Ref<WebsiteDataStore> WebsiteDataStore::defaultDataStore()
{
    InitializeWebKit2();
    auto& store = globalDefaultDataStore();
    if (!store)
        store = adoptRef(new WebsiteDataStore(WebsiteDataStoreConfiguration::create(IsPersistent::Yes), PAL::SessionID::defaultSessionID()));

    return *store;
}

void WebsiteDataStore::deleteDefaultDataStoreForTesting()
{
    globalDefaultDataStore() = nullptr;
}

bool WebsiteDataStore::defaultDataStoreExists()
{
    return !!globalDefaultDataStore();
}

void WebsiteDataStore::registerWithSessionIDMap()
{
    auto result = allDataStores().add(m_sessionID, this);
    ASSERT_UNUSED(result, result.isNewEntry);
}

WebsiteDataStore* WebsiteDataStore::existingDataStoreForSessionID(PAL::SessionID sessionID)
{
    return allDataStores().get(sessionID);
}

static Ref<NetworkProcessProxy> networkProcessForSession(PAL::SessionID sessionID)
{
#if PLATFORM(GTK) || PLATFORM(WPE)
    if (sessionID.isEphemeral()) {
        // Reuse a previous persistent session network process for ephemeral sessions.
        for (auto* dataStore : allDataStores().values()) {
            if (dataStore->isPersistent())
                return makeRef(dataStore->networkProcess());
        }
    }
    return NetworkProcessProxy::create();
#else
    UNUSED_PARAM(sessionID);
    return NetworkProcessProxy::ensureDefaultNetworkProcess();
#endif
}

NetworkProcessProxy& WebsiteDataStore::networkProcess()
{
    if (!m_networkProcess) {
        m_networkProcess = networkProcessForSession(m_sessionID);
        m_networkProcess->addSession(*this, NetworkProcessProxy::SendParametersToNetworkProcess::Yes);
    }

    return *m_networkProcess;
}

NetworkProcessProxy& WebsiteDataStore::networkProcess() const
{
    return const_cast<WebsiteDataStore&>(*this).networkProcess();
}

void WebsiteDataStore::registerProcess(WebProcessProxy& process)
{
    ASSERT(process.pageCount() || process.provisionalPageCount());
    m_processes.add(process);
}

void WebsiteDataStore::unregisterProcess(WebProcessProxy& process)
{
    ASSERT(!process.pageCount());
    ASSERT(!process.provisionalPageCount());
    m_processes.remove(process);
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
    if (!m_configuration->alternativeServicesDirectory().isEmpty() && WebsiteDataStore::http3Enabled())
        m_resolvedConfiguration->setAlternativeServicesDirectory(resolveAndCreateReadWriteDirectoryForSandboxExtension(m_configuration->alternativeServicesDirectory()));
    if (!m_configuration->deviceIdHashSaltsStorageDirectory().isEmpty())
        m_resolvedConfiguration->setDeviceIdHashSaltsStorageDirectory(resolveAndCreateReadWriteDirectoryForSandboxExtension(m_configuration->deviceIdHashSaltsStorageDirectory()));
    if (!m_configuration->networkCacheDirectory().isEmpty())
        m_resolvedConfiguration->setNetworkCacheDirectory(resolveAndCreateReadWriteDirectoryForSandboxExtension(m_configuration->networkCacheDirectory()));
    if (!m_configuration->resourceLoadStatisticsDirectory().isEmpty())
        m_resolvedConfiguration->setResourceLoadStatisticsDirectory(resolveAndCreateReadWriteDirectoryForSandboxExtension(m_configuration->resourceLoadStatisticsDirectory()));
    if (!m_configuration->privateClickMeasurementStorageDirectory().isEmpty())
        m_resolvedConfiguration->setPrivateClickMeasurementStorageDirectory(resolveAndCreateReadWriteDirectoryForSandboxExtension(m_configuration->privateClickMeasurementStorageDirectory()));
    if (!m_configuration->serviceWorkerRegistrationDirectory().isEmpty() && m_resolvedConfiguration->serviceWorkerRegistrationDirectory().isEmpty())
        m_resolvedConfiguration->setServiceWorkerRegistrationDirectory(resolveAndCreateReadWriteDirectoryForSandboxExtension(m_configuration->serviceWorkerRegistrationDirectory()));
    if (!m_configuration->javaScriptConfigurationDirectory().isEmpty())
        m_resolvedConfiguration->setJavaScriptConfigurationDirectory(resolvePathForSandboxExtension(m_configuration->javaScriptConfigurationDirectory()));
    if (!m_configuration->cacheStorageDirectory().isEmpty() && m_resolvedConfiguration->cacheStorageDirectory().isEmpty())
        m_resolvedConfiguration->setCacheStorageDirectory(resolvePathForSandboxExtension(m_configuration->cacheStorageDirectory()));
    if (!m_configuration->hstsStorageDirectory().isEmpty() && m_resolvedConfiguration->hstsStorageDirectory().isEmpty())
        m_resolvedConfiguration->setHSTSStorageDirectory(resolvePathForSandboxExtension(m_configuration->hstsStorageDirectory()));
#if HAVE(ARKIT_INLINE_PREVIEW)
    if (!m_configuration->modelElementCacheDirectory().isEmpty())
        m_resolvedConfiguration->setModelElementCacheDirectory(resolveAndCreateReadWriteDirectoryForSandboxExtension(m_configuration->modelElementCacheDirectory()));
#endif

    // Resolve directories for file paths.
    if (!m_configuration->cookieStorageFile().isEmpty()) {
        m_resolvedConfiguration->setCookieStorageFile(resolveAndCreateReadWriteDirectoryForSandboxExtension(FileSystem::parentPath(m_configuration->cookieStorageFile())));
        m_resolvedConfiguration->setCookieStorageFile(FileSystem::pathByAppendingComponent(m_resolvedConfiguration->cookieStorageFile(), FileSystem::pathFileName(m_configuration->cookieStorageFile())));
    }
}

enum class ProcessAccessType {
    None,
    OnlyIfLaunched,
    Launch,
};

static ProcessAccessType computeNetworkProcessAccessTypeForDataFetch(OptionSet<WebsiteDataType> dataTypes, bool isNonPersistentStore)
{
    for (auto dataType : dataTypes) {
        if (WebsiteData::ownerProcess(dataType) == WebsiteDataProcessType::Network) {
            if (isNonPersistentStore)
                return ProcessAccessType::OnlyIfLaunched;
            return ProcessAccessType::Launch;
        }
    }
    return ProcessAccessType::None;
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
    fetchDataAndApply(dataTypes, fetchOptions, WorkQueue::main(), WTFMove(completionHandler));
}

void WebsiteDataStore::fetchDataAndApply(OptionSet<WebsiteDataType> dataTypes, OptionSet<WebsiteDataFetchOption> fetchOptions, Ref<WorkQueue>&& queue, Function<void(Vector<WebsiteDataRecord>)>&& apply)
{
    class CallbackAggregator final : public ThreadSafeRefCounted<CallbackAggregator, WTF::DestructionThread::MainRunLoop> {
    public:
        static Ref<CallbackAggregator> create(OptionSet<WebsiteDataFetchOption> fetchOptions, Ref<WorkQueue>&& queue, Function<void(Vector<WebsiteDataRecord>)>&& apply, WebsiteDataStore& dataStore)
        {
            return adoptRef(*new CallbackAggregator(fetchOptions, WTFMove(queue), WTFMove(apply), dataStore));
        }

        ~CallbackAggregator()
        {
            ASSERT(RunLoop::isMain());

            Vector<WebsiteDataRecord> records;
            records.reserveInitialCapacity(m_websiteDataRecords.size());
            for (auto& record : m_websiteDataRecords.values())
                records.uncheckedAppend(m_queue.ptr() != &WorkQueue::main() ? crossThreadCopy(record) : WTFMove(record));

            m_queue->dispatch([apply = WTFMove(m_apply), records = WTFMove(records)] () mutable {
                apply(WTFMove(records));
            });
        }

        const OptionSet<WebsiteDataFetchOption>& fetchOptions() const { return m_fetchOptions; }

        void addWebsiteData(WebsiteData&& websiteData)
        {
            if (!RunLoop::isMain()) {
                RunLoop::main().dispatch([protectedThis = makeRef(*this), websiteData = crossThreadCopy(websiteData)]() mutable {
                    protectedThis->addWebsiteData(WTFMove(websiteData));
                });
                return;
            }
            ASSERT(RunLoop::isMain());
            for (auto& entry : websiteData.entries) {
                auto displayName = WebsiteDataRecord::displayNameForOrigin(entry.origin);
                if (!displayName) {
                    if (!allowsWebsiteDataRecordsForAllOrigins)
                        continue;

                    String hostString = entry.origin.host.isEmpty() ? "" : makeString(" ", entry.origin.host);
                    displayName = makeString(entry.origin.protocol, hostString);
                }

                auto& record = m_websiteDataRecords.add(displayName, WebsiteDataRecord { }).iterator->value;
                if (!record.displayName)
                    record.displayName = WTFMove(displayName);

                record.add(entry.type, entry.origin);

                if (m_fetchOptions.contains(WebsiteDataFetchOption::ComputeSizes)) {
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

            for (auto& hostName : websiteData.hostNamesWithHSTSCache) {
                auto displayName = WebsiteDataRecord::displayNameForHostName(hostName);
                if (!displayName)
                    continue;
                
                auto& record = m_websiteDataRecords.add(displayName, WebsiteDataRecord { }).iterator->value;
                if (!record.displayName)
                    record.displayName = WTFMove(displayName);

                record.addHSTSCacheHostname(hostName);
            }

#if ENABLE(RESOURCE_LOAD_STATISTICS)
            for (const auto& domain : websiteData.registrableDomainsWithResourceLoadStatistics) {
                auto displayName = WebsiteDataRecord::displayNameForHostName(domain.string());
                if (!displayName)
                    continue;

                auto& record = m_websiteDataRecords.add(displayName, WebsiteDataRecord { }).iterator->value;
                if (!record.displayName)
                    record.displayName = WTFMove(displayName);

                record.addResourceLoadStatisticsRegistrableDomain(domain);
            }
#endif
        }

private:
        CallbackAggregator(OptionSet<WebsiteDataFetchOption> fetchOptions, Ref<WorkQueue>&& queue, Function<void(Vector<WebsiteDataRecord>)>&& apply, WebsiteDataStore& dataStore)
            : m_fetchOptions(fetchOptions)
            , m_queue(WTFMove(queue))
            , m_apply(WTFMove(apply))
            , m_protectedDataStore(dataStore)
        {
            ASSERT(RunLoop::isMain());
        }

        const OptionSet<WebsiteDataFetchOption> m_fetchOptions;
        Ref<WorkQueue> m_queue;
        Function<void(Vector<WebsiteDataRecord>)> m_apply;

        HashMap<String, WebsiteDataRecord> m_websiteDataRecords;
        Ref<WebsiteDataStore> m_protectedDataStore;
    };

    auto callbackAggregator = CallbackAggregator::create(fetchOptions, WTFMove(queue), WTFMove(apply), *this);

#if ENABLE(VIDEO)
    if (dataTypes.contains(WebsiteDataType::DiskCache)) {
        m_queue->dispatch([mediaCacheDirectory = m_configuration->mediaCacheDirectory().isolatedCopy(), callbackAggregator] {
            WebsiteData websiteData;
            for (auto& origin : WebCore::HTMLMediaElement::originsInMediaCache(mediaCacheDirectory))
                websiteData.entries.append(WebsiteData::Entry { origin, WebsiteDataType::DiskCache, 0 });
            callbackAggregator->addWebsiteData(WTFMove(websiteData));
        });
    }
#endif

    auto networkProcessAccessType = computeNetworkProcessAccessTypeForDataFetch(dataTypes, !isPersistent());
    switch (networkProcessAccessType) {
    case ProcessAccessType::Launch:
        networkProcess();
        ASSERT(m_networkProcess);
        FALLTHROUGH;
    case ProcessAccessType::OnlyIfLaunched:
        if (m_networkProcess) {
            m_networkProcess->fetchWebsiteData(m_sessionID, dataTypes, fetchOptions, [callbackAggregator](WebsiteData websiteData) {
                callbackAggregator->addWebsiteData(WTFMove(websiteData));
            });
        }
        break;
    case ProcessAccessType::None:
        break;
    }

    auto webProcessAccessType = computeWebProcessAccessTypeForDataFetch(dataTypes, !isPersistent());
    if (webProcessAccessType != ProcessAccessType::None) {
        for (auto& process : processes()) {
            switch (webProcessAccessType) {
            case ProcessAccessType::OnlyIfLaunched:
                if (process.state() != WebProcessProxy::State::Running)
                    continue;
                break;

            case ProcessAccessType::Launch:
                // FIXME: Handle this.
                ASSERT_NOT_REACHED();
                break;

            case ProcessAccessType::None:
                ASSERT_NOT_REACHED();
            }

            process.fetchWebsiteData(m_sessionID, dataTypes, [callbackAggregator](WebsiteData websiteData) {
                callbackAggregator->addWebsiteData(WTFMove(websiteData));
            });
        }
    }

    if (dataTypes.contains(WebsiteDataType::DeviceIdHashSalt)) {
        m_deviceIdHashSaltStorage->getDeviceIdHashSaltOrigins([callbackAggregator](auto&& origins) {
            WebsiteData websiteData;
            while (!origins.isEmpty())
                websiteData.entries.append(WebsiteData::Entry { origins.takeAny(), WebsiteDataType::DeviceIdHashSalt, 0 });
            callbackAggregator->addWebsiteData(WTFMove(websiteData));
        });
    }

    if (dataTypes.contains(WebsiteDataType::OfflineWebApplicationCache) && isPersistent()) {
        m_queue->dispatch([fetchOptions, applicationCacheDirectory = m_configuration->applicationCacheDirectory().isolatedCopy(), applicationCacheFlatFileSubdirectoryName = m_configuration->applicationCacheFlatFileSubdirectoryName().isolatedCopy(), callbackAggregator] {
            auto storage = WebCore::ApplicationCacheStorage::create(applicationCacheDirectory, applicationCacheFlatFileSubdirectoryName);
            WebsiteData websiteData;
            for (auto& origin : storage->originsWithCache()) {
                uint64_t size = fetchOptions.contains(WebsiteDataFetchOption::ComputeSizes) ? storage->diskUsageForOrigin(origin) : 0;
                websiteData.entries.append(WebsiteData::Entry { origin, WebsiteDataType::OfflineWebApplicationCache, size });
            }
            callbackAggregator->addWebsiteData(WTFMove(websiteData));
        });
    }

    if (dataTypes.contains(WebsiteDataType::WebSQLDatabases) && isPersistent()) {
        m_queue->dispatch([webSQLDatabaseDirectory = m_configuration->webSQLDatabaseDirectory().isolatedCopy(), callbackAggregator]() {
            WebsiteData websiteData;
            for (auto& origin : WebCore::DatabaseTracker::trackerWithDatabasePath(webSQLDatabaseDirectory)->origins())
                websiteData.entries.append(WebsiteData::Entry { origin, WebsiteDataType::WebSQLDatabases, 0 });
            callbackAggregator->addWebsiteData(WTFMove(websiteData));
        });
    }

    if (dataTypes.contains(WebsiteDataType::MediaKeys) && isPersistent()) {
        m_queue->dispatch([mediaKeysStorageDirectory = m_configuration->mediaKeysStorageDirectory().isolatedCopy(), callbackAggregator] {
            WebsiteData websiteData;
            for (auto& origin : mediaKeyOrigins(mediaKeysStorageDirectory))
                websiteData.entries.append(WebsiteData::Entry { origin, WebsiteDataType::MediaKeys, 0 });
            callbackAggregator->addWebsiteData(WTFMove(websiteData));
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

                    m_callbackAggregator->addWebsiteData(WTFMove(websiteData));

                    delete this;
                    return;
                }

                auto plugin = m_plugins.takeLast();
                PluginProcessManager::singleton().fetchWebsiteData(plugin, m_callbackAggregator->fetchOptions(), [this](Vector<String> hostNames) {
                    for (auto& hostName : hostNames)
                        m_hostNames.add(WTFMove(hostName));
                    fetchWebsiteDataForNextPlugin();
                });
            }

            Ref<CallbackAggregator> m_callbackAggregator;
            Vector<PluginModuleInfo> m_plugins;
            HashSet<String> m_hostNames;
        };

        State::fetchData(callbackAggregator.copyRef(), plugins());
    }
#endif
}

#if ENABLE(RESOURCE_LOAD_STATISTICS)
void WebsiteDataStore::fetchDataForRegistrableDomains(OptionSet<WebsiteDataType> dataTypes, OptionSet<WebsiteDataFetchOption> fetchOptions, Vector<WebCore::RegistrableDomain>&& domains, CompletionHandler<void(Vector<WebsiteDataRecord>&&, HashSet<WebCore::RegistrableDomain>&&)>&& completionHandler)
{
    fetchDataAndApply(dataTypes, fetchOptions, m_queue.copyRef(), [domains = crossThreadCopy(domains), completionHandler = WTFMove(completionHandler)] (auto&& existingDataRecords) mutable {
        ASSERT(!RunLoop::isMain());
        
        Vector<WebsiteDataRecord> matchingDataRecords;
        HashSet<WebCore::RegistrableDomain> domainsWithMatchingDataRecords;
        for (auto&& dataRecord : existingDataRecords) {
            for (auto& domain : domains) {
                if (dataRecord.matches(domain)) {
                    matchingDataRecords.append(WTFMove(dataRecord));
                    domainsWithMatchingDataRecords.add(domain.isolatedCopy());
                    break;
                }
            }
        }
        RunLoop::main().dispatch([completionHandler = WTFMove(completionHandler), matchingDataRecords = WTFMove(matchingDataRecords), domainsWithMatchingDataRecords = WTFMove(domainsWithMatchingDataRecords)] () mutable {
            completionHandler(WTFMove(matchingDataRecords), WTFMove(domainsWithMatchingDataRecords));
        });
    });
}
#endif

static ProcessAccessType computeNetworkProcessAccessTypeForDataRemoval(OptionSet<WebsiteDataType> dataTypes, bool isNonPersistentStore)
{
    ProcessAccessType processAccessType = ProcessAccessType::None;

    for (auto dataType : dataTypes) {
        if (dataType == WebsiteDataType::Cookies) {
            if (isNonPersistentStore)
                processAccessType = std::max(processAccessType, ProcessAccessType::OnlyIfLaunched);
            else
                processAccessType = std::max(processAccessType, ProcessAccessType::Launch);
        } else if (WebsiteData::ownerProcess(dataType) == WebsiteDataProcessType::Network)
            return ProcessAccessType::Launch;
    }
    
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

class RemovalCallbackAggregator : public ThreadSafeRefCounted<RemovalCallbackAggregator, WTF::DestructionThread::MainRunLoop> {
public:
    static Ref<RemovalCallbackAggregator> create(WebsiteDataStore& dataStore, CompletionHandler<void()>&& completionHandler)
    {
        return adoptRef(*new RemovalCallbackAggregator(dataStore, WTFMove(completionHandler)));
    }

    ~RemovalCallbackAggregator()
    {
        ASSERT(RunLoop::isMain());
        RunLoop::main().dispatch(WTFMove(m_completionHandler));
    }

private:
    RemovalCallbackAggregator(WebsiteDataStore& dataStore, CompletionHandler<void()>&& completionHandler)
        : m_protectedDataStore(dataStore)
        , m_completionHandler(WTFMove(completionHandler))
    {
        ASSERT(RunLoop::isMain());
    }

    Ref<WebsiteDataStore> m_protectedDataStore;
    CompletionHandler<void()> m_completionHandler;
};

void WebsiteDataStore::removeData(OptionSet<WebsiteDataType> dataTypes, WallTime modifiedSince, Function<void()>&& completionHandler)
{
    auto callbackAggregator = RemovalCallbackAggregator::create(*this, WTFMove(completionHandler));

#if ENABLE(VIDEO)
    if (dataTypes.contains(WebsiteDataType::DiskCache)) {
        m_queue->dispatch([modifiedSince, mediaCacheDirectory = m_configuration->mediaCacheDirectory().isolatedCopy(), callbackAggregator] {
            WebCore::HTMLMediaElement::clearMediaCache(mediaCacheDirectory, modifiedSince);
        });
    }
#endif

#if ENABLE(RESOURCE_LOAD_STATISTICS)
    bool didNotifyNetworkProcessToDeleteWebsiteData = false;
#endif
    auto networkProcessAccessType = computeNetworkProcessAccessTypeForDataRemoval(dataTypes, !isPersistent());
    switch (networkProcessAccessType) {
    case ProcessAccessType::Launch:
        networkProcess();
        ASSERT(m_networkProcess);
        FALLTHROUGH;
    case ProcessAccessType::OnlyIfLaunched:
        if (m_networkProcess) {
            m_networkProcess->deleteWebsiteData(m_sessionID, dataTypes, modifiedSince, [callbackAggregator] { });
#if ENABLE(RESOURCE_LOAD_STATISTICS)
            didNotifyNetworkProcessToDeleteWebsiteData = true;
#endif
        }
        break;
    case ProcessAccessType::None:
        break;
    }

    auto webProcessAccessType = computeWebProcessAccessTypeForDataRemoval(dataTypes, !isPersistent());
    if (webProcessAccessType != ProcessAccessType::None) {
        for (auto& processPool : processPools()) {
            // Clear back/forward cache first as processes removed from the back/forward cache will likely
            // be added to the WebProcess cache.
            processPool->backForwardCache().removeEntriesForSession(sessionID());
            processPool->webProcessCache().clearAllProcessesForSession(sessionID());
        }

        for (auto& process : processes()) {
            switch (webProcessAccessType) {
            case ProcessAccessType::OnlyIfLaunched:
                if (process.state() != WebProcessProxy::State::Running)
                    continue;
                break;

            case ProcessAccessType::Launch:
                // FIXME: Handle this.
                ASSERT_NOT_REACHED();
                break;

            case ProcessAccessType::None:
                ASSERT_NOT_REACHED();
            }

            process.deleteWebsiteData(m_sessionID, dataTypes, modifiedSince, [callbackAggregator] { });
        }
    }

    if (dataTypes.contains(WebsiteDataType::DeviceIdHashSalt) || (dataTypes.contains(WebsiteDataType::Cookies)))
        m_deviceIdHashSaltStorage->deleteDeviceIdHashSaltOriginsModifiedSince(modifiedSince, [callbackAggregator] { });

    if (dataTypes.contains(WebsiteDataType::OfflineWebApplicationCache) && isPersistent()) {
        m_queue->dispatch([applicationCacheDirectory = m_configuration->applicationCacheDirectory().isolatedCopy(), applicationCacheFlatFileSubdirectoryName = m_configuration->applicationCacheFlatFileSubdirectoryName().isolatedCopy(), callbackAggregator] {
            auto storage = WebCore::ApplicationCacheStorage::create(applicationCacheDirectory, applicationCacheFlatFileSubdirectoryName);
            storage->deleteAllCaches();
        });
    }

    if (dataTypes.contains(WebsiteDataType::WebSQLDatabases) && isPersistent()) {
        m_queue->dispatch([webSQLDatabaseDirectory = m_configuration->webSQLDatabaseDirectory().isolatedCopy(), callbackAggregator, modifiedSince] {
            WebCore::DatabaseTracker::trackerWithDatabasePath(webSQLDatabaseDirectory)->deleteDatabasesModifiedSince(modifiedSince);
        });
    }

    if (dataTypes.contains(WebsiteDataType::MediaKeys) && isPersistent()) {
        m_queue->dispatch([mediaKeysStorageDirectory = m_configuration->mediaKeysStorageDirectory().isolatedCopy(), callbackAggregator, modifiedSince] {
            removeMediaKeys(mediaKeysStorageDirectory, modifiedSince);
        });
    }

    if (dataTypes.contains(WebsiteDataType::SearchFieldRecentSearches) && isPersistent()) {
        m_queue->dispatch([modifiedSince, callbackAggregator] {
            platformRemoveRecentSearches(modifiedSince);
        });
    }

#if ENABLE(NETSCAPE_PLUGIN_API)
    if (dataTypes.contains(WebsiteDataType::PlugInData) && isPersistent()) {
        class State {
        public:
            static void deleteData(Ref<RemovalCallbackAggregator>&& callbackAggregator, Vector<PluginModuleInfo>&& plugins, WallTime modifiedSince)
            {
                new State(WTFMove(callbackAggregator), WTFMove(plugins), modifiedSince);
            }

        private:
            State(Ref<RemovalCallbackAggregator>&& callbackAggregator, Vector<PluginModuleInfo>&& plugins, WallTime modifiedSince)
                : m_callbackAggregator(WTFMove(callbackAggregator))
                , m_plugins(WTFMove(plugins))
                , m_modifiedSince(modifiedSince)
            {
                deleteWebsiteDataForNextPlugin();
            }

            ~State()
            {
                ASSERT(m_plugins.isEmpty());
            }

            void deleteWebsiteDataForNextPlugin()
            {
                if (m_plugins.isEmpty()) {
                    delete this;
                    return;
                }

                auto plugin = m_plugins.takeLast();
                PluginProcessManager::singleton().deleteWebsiteData(plugin, m_modifiedSince, [this] {
                    deleteWebsiteDataForNextPlugin();
                });
            }

            Ref<RemovalCallbackAggregator> m_callbackAggregator;
            Vector<PluginModuleInfo> m_plugins;
            WallTime m_modifiedSince;
        };

        State::deleteData(callbackAggregator.copyRef(), plugins(), modifiedSince);
    }
#endif

#if ENABLE(RESOURCE_LOAD_STATISTICS)
    if (dataTypes.contains(WebsiteDataType::ResourceLoadStatistics)) {
        if (!didNotifyNetworkProcessToDeleteWebsiteData)
            networkProcess().deleteWebsiteData(m_sessionID, dataTypes, modifiedSince, [callbackAggregator] { });

        clearResourceLoadStatisticsInWebProcesses([callbackAggregator] { });
    }
#endif
}

void WebsiteDataStore::removeData(OptionSet<WebsiteDataType> dataTypes, const Vector<WebsiteDataRecord>& dataRecords, Function<void()>&& completionHandler)
{
    Vector<WebCore::SecurityOriginData> origins;

    for (const auto& dataRecord : dataRecords) {
        for (auto& origin : dataRecord.origins)
            origins.append(origin);
    }

    auto callbackAggregator = RemovalCallbackAggregator::create(*this, WTFMove(completionHandler));
    
    if (dataTypes.contains(WebsiteDataType::DiskCache)) {
        HashSet<WebCore::SecurityOriginData> origins;
        for (const auto& dataRecord : dataRecords) {
            for (const auto& origin : dataRecord.origins)
                origins.add(crossThreadCopy(origin));
        }
        
#if ENABLE(VIDEO)
        m_queue->dispatch([origins = WTFMove(origins), mediaCacheDirectory = m_configuration->mediaCacheDirectory().isolatedCopy(), callbackAggregator] {
            WebCore::HTMLMediaElement::clearMediaCacheForOrigins(mediaCacheDirectory, origins);
        });
#endif
    }
    
    auto networkProcessAccessType = computeNetworkProcessAccessTypeForDataRemoval(dataTypes, !isPersistent());
    if (networkProcessAccessType != ProcessAccessType::None) {
        auto pools = networkProcessAccessType == ProcessAccessType::Launch ? ensureProcessPools() : processPools();
        for (auto& processPool : pools) {
            switch (networkProcessAccessType) {
            case ProcessAccessType::OnlyIfLaunched:
                break;

            case ProcessAccessType::Launch:
                networkProcess();
                break;

            case ProcessAccessType::None:
                ASSERT_NOT_REACHED();
            }

            Vector<String> cookieHostNames;
            Vector<String> HSTSCacheHostNames;
            Vector<WebCore::RegistrableDomain> registrableDomains;
            for (const auto& dataRecord : dataRecords) {
                for (auto& hostName : dataRecord.cookieHostNames)
                    cookieHostNames.append(hostName);
                for (auto& hostName : dataRecord.HSTSCacheHostNames)
                    HSTSCacheHostNames.append(hostName);
#if ENABLE(RESOURCE_LOAD_STATISTICS)
                for (auto& registrableDomain : dataRecord.resourceLoadStatisticsRegistrableDomains)
                    registrableDomains.append(registrableDomain);
#endif
            }

            networkProcess().deleteWebsiteDataForOrigins(m_sessionID, dataTypes, origins, cookieHostNames, HSTSCacheHostNames, registrableDomains, [callbackAggregator, processPool] { });
        }
    }

    auto webProcessAccessType = computeWebProcessAccessTypeForDataRemoval(dataTypes, !isPersistent());
    if (webProcessAccessType != ProcessAccessType::None) {
        for (auto& process : processes()) {
            switch (webProcessAccessType) {
            case ProcessAccessType::OnlyIfLaunched:
                if (process.state() != WebProcessProxy::State::Running)
                    continue;
                break;

            case ProcessAccessType::Launch:
                // FIXME: Handle this.
                ASSERT_NOT_REACHED();
                break;

            case ProcessAccessType::None:
                ASSERT_NOT_REACHED();
            }

            process.deleteWebsiteDataForOrigins(m_sessionID, dataTypes, origins, [callbackAggregator] { });
        }
    }

    if (dataTypes.contains(WebsiteDataType::DeviceIdHashSalt) || (dataTypes.contains(WebsiteDataType::Cookies))) {
        m_deviceIdHashSaltStorage->deleteDeviceIdHashSaltForOrigins(origins, [callbackAggregator] { });
    }

    if (dataTypes.contains(WebsiteDataType::OfflineWebApplicationCache) && isPersistent()) {
        HashSet<WebCore::SecurityOriginData> origins;
        for (const auto& dataRecord : dataRecords) {
            for (const auto& origin : dataRecord.origins)
                origins.add(crossThreadCopy(origin));
        }

        m_queue->dispatch([origins = WTFMove(origins), applicationCacheDirectory = m_configuration->applicationCacheDirectory().isolatedCopy(), applicationCacheFlatFileSubdirectoryName = m_configuration->applicationCacheFlatFileSubdirectoryName().isolatedCopy(), callbackAggregator] {
            auto storage = WebCore::ApplicationCacheStorage::create(applicationCacheDirectory, applicationCacheFlatFileSubdirectoryName);
            for (const auto& origin : origins)
                storage->deleteCacheForOrigin(origin);
        });
    }

    if (dataTypes.contains(WebsiteDataType::WebSQLDatabases) && isPersistent()) {
        HashSet<WebCore::SecurityOriginData> origins;
        for (const auto& dataRecord : dataRecords) {
            for (const auto& origin : dataRecord.origins)
                origins.add(crossThreadCopy(origin));
        }

        m_queue->dispatch([origins = WTFMove(origins), callbackAggregator, webSQLDatabaseDirectory = m_configuration->webSQLDatabaseDirectory().isolatedCopy()] {
            auto databaseTracker = WebCore::DatabaseTracker::trackerWithDatabasePath(webSQLDatabaseDirectory);
            for (auto& origin : origins)
                databaseTracker->deleteOrigin(origin);
        });
    }

    if (dataTypes.contains(WebsiteDataType::MediaKeys) && isPersistent()) {
        HashSet<WebCore::SecurityOriginData> origins;
        for (const auto& dataRecord : dataRecords) {
            for (const auto& origin : dataRecord.origins)
                origins.add(crossThreadCopy(origin));
        }

        m_queue->dispatch([mediaKeysStorageDirectory = m_configuration->mediaKeysStorageDirectory().isolatedCopy(), callbackAggregator, origins = WTFMove(origins)] {
            removeMediaKeys(mediaKeysStorageDirectory, origins);
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
            static void deleteData(Ref<RemovalCallbackAggregator>&& callbackAggregator, Vector<PluginModuleInfo>&& plugins, Vector<String>&& hostNames)
            {
                new State(WTFMove(callbackAggregator), WTFMove(plugins), WTFMove(hostNames));
            }

        private:
            State(Ref<RemovalCallbackAggregator>&& callbackAggregator, Vector<PluginModuleInfo>&& plugins, Vector<String>&& hostNames)
                : m_callbackAggregator(WTFMove(callbackAggregator))
                , m_plugins(WTFMove(plugins))
                , m_hostNames(WTFMove(hostNames))
            {
                deleteWebsiteDataForNextPlugin();
            }

            ~State()
            {
                ASSERT(m_plugins.isEmpty());
            }

            void deleteWebsiteDataForNextPlugin()
            {
                if (m_plugins.isEmpty()) {
                    delete this;
                    return;
                }

                auto plugin = m_plugins.takeLast();
                PluginProcessManager::singleton().deleteWebsiteDataForHostNames(plugin, m_hostNames, [this] {
                    deleteWebsiteDataForNextPlugin();
                });
            }

            Ref<RemovalCallbackAggregator> m_callbackAggregator;
            Vector<PluginModuleInfo> m_plugins;
            Vector<String> m_hostNames;
        };

        if (!hostNames.isEmpty())
            State::deleteData(callbackAggregator.copyRef(), plugins(), WTFMove(hostNames));
    }
#endif
}

void WebsiteDataStore::setServiceWorkerTimeoutForTesting(Seconds seconds)
{
    networkProcess().sendSync(Messages::NetworkProcess::SetServiceWorkerFetchTimeoutForTesting(seconds), Messages::NetworkProcess::SetServiceWorkerFetchTimeoutForTesting::Reply(), 0);
}

void WebsiteDataStore::resetServiceWorkerTimeoutForTesting()
{
    networkProcess().sendSync(Messages::NetworkProcess::ResetServiceWorkerFetchTimeoutForTesting(), Messages::NetworkProcess::ResetServiceWorkerFetchTimeoutForTesting::Reply(), 0);
}

#if ENABLE(RESOURCE_LOAD_STATISTICS)
void WebsiteDataStore::setMaxStatisticsEntries(size_t maximumEntryCount, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));

    networkProcess().setMaxStatisticsEntries(m_sessionID, maximumEntryCount, [callbackAggregator] { });
}

void WebsiteDataStore::setPruneEntriesDownTo(size_t pruneTargetCount, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));

    networkProcess().setPruneEntriesDownTo(m_sessionID, pruneTargetCount, [callbackAggregator] { });
}

void WebsiteDataStore::setGrandfatheringTime(Seconds seconds, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));

    networkProcess().setGrandfatheringTime(m_sessionID, seconds, [callbackAggregator] { });
}

void WebsiteDataStore::setMinimumTimeBetweenDataRecordsRemoval(Seconds seconds, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));

    networkProcess().setMinimumTimeBetweenDataRecordsRemoval(m_sessionID, seconds, [callbackAggregator] { });
}
    
void WebsiteDataStore::dumpResourceLoadStatistics(CompletionHandler<void(const String&)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    networkProcess().dumpResourceLoadStatistics(m_sessionID, WTFMove(completionHandler));
}

void WebsiteDataStore::isPrevalentResource(const URL& url, CompletionHandler<void(bool isPrevalent)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    if (url.protocolIsAbout() || url.isEmpty()) {
        completionHandler(false);
        return;
    }

    networkProcess().isPrevalentResource(m_sessionID, WebCore::RegistrableDomain { url }, WTFMove(completionHandler));
}

void WebsiteDataStore::isGrandfathered(const URL& url, CompletionHandler<void(bool isPrevalent)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    if (url.protocolIsAbout() || url.isEmpty()) {
        completionHandler(false);
        return;
    }

    networkProcess().isGrandfathered(m_sessionID, WebCore::RegistrableDomain { url }, WTFMove(completionHandler));
}

void WebsiteDataStore::setPrevalentResource(const URL& url, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    if (url.protocolIsAbout() || url.isEmpty()) {
        completionHandler();
        return;
    }
    
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));

    networkProcess().setPrevalentResource(m_sessionID, WebCore::RegistrableDomain { url }, [callbackAggregator] { });
}

void WebsiteDataStore::setPrevalentResourceForDebugMode(const URL& url, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    if (url.protocolIsAbout() || url.isEmpty()) {
        completionHandler();
        return;
    }
    
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));
    
    networkProcess().setPrevalentResourceForDebugMode(m_sessionID, WebCore::RegistrableDomain { url }, [callbackAggregator] { });
}

void WebsiteDataStore::isVeryPrevalentResource(const URL& url, CompletionHandler<void(bool isVeryPrevalent)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    if (url.protocolIsAbout() || url.isEmpty()) {
        completionHandler(false);
        return;
    }
    
    networkProcess().isVeryPrevalentResource(m_sessionID, WebCore::RegistrableDomain { url }, WTFMove(completionHandler));
}

void WebsiteDataStore::setVeryPrevalentResource(const URL& url, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    if (url.protocolIsAbout() || url.isEmpty()) {
        completionHandler();
        return;
    }
    
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));
    
    networkProcess().setVeryPrevalentResource(m_sessionID, WebCore::RegistrableDomain { url }, [callbackAggregator] { });
}

void WebsiteDataStore::setShouldClassifyResourcesBeforeDataRecordsRemoval(bool value, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));

    networkProcess().setShouldClassifyResourcesBeforeDataRecordsRemoval(m_sessionID, value, [callbackAggregator] { });
}

void WebsiteDataStore::setSubframeUnderTopFrameDomain(const URL& subFrameURL, const URL& topFrameURL, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    if (subFrameURL.protocolIsAbout() || subFrameURL.isEmpty() || topFrameURL.protocolIsAbout() || topFrameURL.isEmpty()) {
        completionHandler();
        return;
    }

    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));
    
    networkProcess().setSubframeUnderTopFrameDomain(m_sessionID, WebCore::RegistrableDomain { subFrameURL }, WebCore::RegistrableDomain { topFrameURL }, [callbackAggregator] { });
}

void WebsiteDataStore::isRegisteredAsSubFrameUnder(const URL& subFrameURL, const URL& topFrameURL, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    networkProcess().isRegisteredAsSubFrameUnder(m_sessionID, WebCore::RegistrableDomain { subFrameURL }, WebCore::RegistrableDomain { topFrameURL }, WTFMove(completionHandler));
}

void WebsiteDataStore::setSubresourceUnderTopFrameDomain(const URL& subresourceURL, const URL& topFrameURL, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    if (subresourceURL.protocolIsAbout() || subresourceURL.isEmpty() || topFrameURL.protocolIsAbout() || topFrameURL.isEmpty()) {
        completionHandler();
        return;
    }

    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));
    
    networkProcess().setSubresourceUnderTopFrameDomain(m_sessionID, WebCore::RegistrableDomain { subresourceURL }, WebCore::RegistrableDomain { topFrameURL }, [callbackAggregator] { });
}

void WebsiteDataStore::isRegisteredAsSubresourceUnder(const URL& subresourceURL, const URL& topFrameURL, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    networkProcess().isRegisteredAsSubresourceUnder(m_sessionID, WebCore::RegistrableDomain { subresourceURL }, WebCore::RegistrableDomain { topFrameURL }, WTFMove(completionHandler));
}

void WebsiteDataStore::setSubresourceUniqueRedirectTo(const URL& subresourceURL, const URL& urlRedirectedTo, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    if (subresourceURL.protocolIsAbout() || subresourceURL.isEmpty() || urlRedirectedTo.protocolIsAbout() || urlRedirectedTo.isEmpty()) {
        completionHandler();
        return;
    }

    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));
    
    networkProcess().setSubresourceUniqueRedirectTo(m_sessionID, WebCore::RegistrableDomain { subresourceURL }, WebCore::RegistrableDomain { urlRedirectedTo }, [callbackAggregator] { });
}

void WebsiteDataStore::setSubresourceUniqueRedirectFrom(const URL& subresourceURL, const URL& urlRedirectedFrom, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    if (subresourceURL.protocolIsAbout() || subresourceURL.isEmpty() || urlRedirectedFrom.protocolIsAbout() || urlRedirectedFrom.isEmpty()) {
        completionHandler();
        return;
    }

    networkProcess().setSubresourceUniqueRedirectFrom(m_sessionID, WebCore::RegistrableDomain { subresourceURL }, WebCore::RegistrableDomain { urlRedirectedFrom }, WTFMove(completionHandler));
}

void WebsiteDataStore::setTopFrameUniqueRedirectTo(const URL& topFrameURL, const URL& urlRedirectedTo, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    if (topFrameURL.protocolIsAbout() || topFrameURL.isEmpty() || urlRedirectedTo.protocolIsAbout() || urlRedirectedTo.isEmpty()) {
        completionHandler();
        return;
    }

    networkProcess().setTopFrameUniqueRedirectTo(m_sessionID, WebCore::RegistrableDomain { topFrameURL }, WebCore::RegistrableDomain { urlRedirectedTo }, WTFMove(completionHandler));
}

void WebsiteDataStore::setTopFrameUniqueRedirectFrom(const URL& topFrameURL, const URL& urlRedirectedFrom, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    if (topFrameURL.protocolIsAbout() || topFrameURL.isEmpty() || urlRedirectedFrom.protocolIsAbout() || urlRedirectedFrom.isEmpty()) {
        completionHandler();
        return;
    }

    networkProcess().setTopFrameUniqueRedirectFrom(m_sessionID, WebCore::RegistrableDomain { topFrameURL }, WebCore::RegistrableDomain { urlRedirectedFrom }, WTFMove(completionHandler));
}

void WebsiteDataStore::isRegisteredAsRedirectingTo(const URL& urlRedirectedFrom, const URL& urlRedirectedTo, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    networkProcess().isRegisteredAsRedirectingTo(m_sessionID, WebCore::RegistrableDomain { urlRedirectedFrom }, WebCore::RegistrableDomain { urlRedirectedTo }, WTFMove(completionHandler));
}

void WebsiteDataStore::clearPrevalentResource(const URL& url, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
        
    if (url.protocolIsAbout() || url.isEmpty()) {
        completionHandler();
        return;
    }

    networkProcess().clearPrevalentResource(m_sessionID, WebCore::RegistrableDomain { url }, WTFMove(completionHandler));
}

void WebsiteDataStore::resetParametersToDefaultValues(CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    networkProcess().resetParametersToDefaultValues(m_sessionID, WTFMove(completionHandler));
}

void WebsiteDataStore::scheduleClearInMemoryAndPersistent(WallTime modifiedSince, ShouldGrandfatherStatistics shouldGrandfather, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    networkProcess().scheduleClearInMemoryAndPersistent(m_sessionID, modifiedSince, shouldGrandfather, WTFMove(completionHandler));
}

void WebsiteDataStore::getResourceLoadStatisticsDataSummary(CompletionHandler<void(Vector<WebResourceLoadStatisticsStore::ThirdPartyData>&&)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    struct LocalCallbackAggregator : RefCounted<LocalCallbackAggregator> {
        LocalCallbackAggregator(CompletionHandler<void(Vector<WebResourceLoadStatisticsStore::ThirdPartyData>&&)>&& completionHandler)
            : m_completionHandler(WTFMove(completionHandler))
        {
            ASSERT(RunLoop::isMain());
        };

        ~LocalCallbackAggregator()
        {
            ASSERT(RunLoop::isMain());

            m_completionHandler(WTFMove(m_results));
        }

        void addResult(Vector<WebResourceLoadStatisticsStore::ThirdPartyData>&& results)
        {
            m_results.appendVector(WTFMove(results));
        }

        CompletionHandler<void(Vector<WebResourceLoadStatisticsStore::ThirdPartyData>&&)> m_completionHandler;
        Vector<WebResourceLoadStatisticsStore::ThirdPartyData> m_results;
    };
    
    auto localCallbackAggregator = adoptRef(new LocalCallbackAggregator(WTFMove(completionHandler)));
    
    auto wtfCallbackAggregator = WTF::CallbackAggregator::create([this, protectedThis = makeRef(*this), localCallbackAggregator] {
        networkProcess().getResourceLoadStatisticsDataSummary(m_sessionID, [localCallbackAggregator](Vector<WebResourceLoadStatisticsStore::ThirdPartyData>&& data) {
            localCallbackAggregator->addResult(WTFMove(data));
        });
    });
    
    for (auto& processPool : WebProcessPool::allProcessPools())
        processPool->sendResourceLoadStatisticsDataImmediately([wtfCallbackAggregator] { });
}

void WebsiteDataStore::scheduleClearInMemoryAndPersistent(ShouldGrandfatherStatistics shouldGrandfather, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    networkProcess().scheduleClearInMemoryAndPersistent(m_sessionID, { }, shouldGrandfather, WTFMove(completionHandler));
}

void WebsiteDataStore::scheduleCookieBlockingUpdate(CompletionHandler<void()>&& completionHandler)
{
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));

    networkProcess().scheduleCookieBlockingUpdate(m_sessionID, [callbackAggregator] { });
}

void WebsiteDataStore::scheduleStatisticsAndDataRecordsProcessing(CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));
    
    networkProcess().scheduleStatisticsAndDataRecordsProcessing(m_sessionID, [callbackAggregator] { });
}

void WebsiteDataStore::statisticsDatabaseHasAllTables(CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    networkProcess().statisticsDatabaseHasAllTables(m_sessionID, WTFMove(completionHandler));
}

void WebsiteDataStore::statisticsDatabaseColumnsForTable(const String& tableName, CompletionHandler<void(Vector<String>&&)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    networkProcess().statisticsDatabaseColumnsForTable(m_sessionID, tableName, WTFMove(completionHandler));
}

void WebsiteDataStore::setLastSeen(const URL& url, Seconds seconds, CompletionHandler<void()>&& completionHandler)
{
    if (url.protocolIsAbout() || url.isEmpty()) {
        completionHandler();
        return;
    }

    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));

    networkProcess().setLastSeen(m_sessionID, WebCore::RegistrableDomain { url }, seconds, [callbackAggregator] { });
}

void WebsiteDataStore::domainIDExistsInDatabase(int domainID, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    networkProcess().domainIDExistsInDatabase(m_sessionID, domainID, WTFMove(completionHandler));
}

void WebsiteDataStore::mergeStatisticForTesting(const URL& url, const URL& topFrameUrl1, const URL& topFrameUrl2, Seconds lastSeen, bool hadUserInteraction, Seconds mostRecentUserInteraction, bool isGrandfathered, bool isPrevalent, bool isVeryPrevalent, unsigned dataRecordsRemoved, CompletionHandler<void()>&& completionHandler)
{
    if (url.protocolIsAbout() || url.isEmpty()) {
        completionHandler();
        return;
    }

    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));

    networkProcess().mergeStatisticForTesting(m_sessionID, WebCore::RegistrableDomain { url }, WebCore::RegistrableDomain { topFrameUrl1 }, WebCore::RegistrableDomain { topFrameUrl2 }, lastSeen, hadUserInteraction, mostRecentUserInteraction, isGrandfathered, isPrevalent, isVeryPrevalent, dataRecordsRemoved, [callbackAggregator] { });
}

void WebsiteDataStore::insertExpiredStatisticForTesting(const URL& url, unsigned numberOfOperatingDaysPassed, bool hadUserInteraction, bool isScheduledForAllButCookieDataRemoval, bool isPrevalent, CompletionHandler<void()>&& completionHandler)
{
    if (url.protocolIsAbout() || url.isEmpty()) {
        completionHandler();
        return;
    }

    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));

    networkProcess().insertExpiredStatisticForTesting(m_sessionID, WebCore::RegistrableDomain { url }, numberOfOperatingDaysPassed, hadUserInteraction, isScheduledForAllButCookieDataRemoval, isPrevalent, [callbackAggregator] { });
}

void WebsiteDataStore::setNotifyPagesWhenDataRecordsWereScanned(bool value, CompletionHandler<void()>&& completionHandler)
{
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));

    networkProcess().setNotifyPagesWhenDataRecordsWereScanned(m_sessionID, value, [callbackAggregator] { });
}

void WebsiteDataStore::setIsRunningResourceLoadStatisticsTest(bool value, CompletionHandler<void()>&& completionHandler)
{
    useExplicitITPState();
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));
    
    networkProcess().setIsRunningResourceLoadStatisticsTest(m_sessionID, value, [callbackAggregator] { });
}

void WebsiteDataStore::getAllStorageAccessEntries(WebPageProxyIdentifier pageID, CompletionHandler<void(Vector<String>&& domains)>&& completionHandler)
{
    auto* webPage = WebProcessProxy::webPage(pageID);
    if (!webPage) {
        completionHandler({ });
        return;
    }

    networkProcess().getAllStorageAccessEntries(m_sessionID, WTFMove(completionHandler));
}


void WebsiteDataStore::setTimeToLiveUserInteraction(Seconds seconds, CompletionHandler<void()>&& completionHandler)
{
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));
    
    networkProcess().setTimeToLiveUserInteraction(m_sessionID, seconds, [callbackAggregator] { });
}

void WebsiteDataStore::logUserInteraction(const URL& url, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    if (url.protocolIsAbout() || url.isEmpty()) {
        completionHandler();
        return;
    }
    
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));

    networkProcess().logUserInteraction(m_sessionID, WebCore::RegistrableDomain { url }, [callbackAggregator] { });
}

void WebsiteDataStore::hasHadUserInteraction(const URL& url, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    if (url.protocolIsAbout() || url.isEmpty()) {
        completionHandler(false);
        return;
    }
    
    networkProcess().hasHadUserInteraction(m_sessionID, WebCore::RegistrableDomain { url }, WTFMove(completionHandler));
}

void WebsiteDataStore::isRelationshipOnlyInDatabaseOnce(const URL& subUrl, const URL& topUrl, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    if (subUrl.protocolIsAbout() || subUrl.isEmpty() || topUrl.protocolIsAbout() || topUrl.isEmpty()) {
        completionHandler(false);
        return;
    }
    
    networkProcess().isRelationshipOnlyInDatabaseOnce(m_sessionID, WebCore::RegistrableDomain { subUrl }, WebCore::RegistrableDomain { topUrl }, WTFMove(completionHandler));
}

void WebsiteDataStore::clearUserInteraction(const URL& url, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    if (url.protocolIsAbout() || url.isEmpty()) {
        completionHandler();
        return;
    }
    
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));

    networkProcess().clearUserInteraction(m_sessionID, WebCore::RegistrableDomain { url }, [callbackAggregator] { });
}

void WebsiteDataStore::setGrandfathered(const URL& url, bool isGrandfathered, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    if (url.protocolIsAbout() || url.isEmpty()) {
        completionHandler();
        return;
    }
    
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));
    
    networkProcess().setGrandfathered(m_sessionID, WebCore::RegistrableDomain { url }, isGrandfathered, [callbackAggregator] { });
}

void WebsiteDataStore::setCrossSiteLoadWithLinkDecorationForTesting(const URL& fromURL, const URL& toURL, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));
    
    networkProcess().setCrossSiteLoadWithLinkDecorationForTesting(m_sessionID, WebCore::RegistrableDomain { fromURL }, WebCore::RegistrableDomain { toURL }, [callbackAggregator] { });
}

void WebsiteDataStore::resetCrossSiteLoadsWithLinkDecorationForTesting(CompletionHandler<void()>&& completionHandler)
{
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));
    
    networkProcess().resetCrossSiteLoadsWithLinkDecorationForTesting(m_sessionID, [callbackAggregator] { });
}

void WebsiteDataStore::deleteCookiesForTesting(const URL& url, bool includeHttpOnlyCookies, CompletionHandler<void()>&& completionHandler)
{
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));
    
    networkProcess().deleteCookiesForTesting(m_sessionID, WebCore::RegistrableDomain { url }, includeHttpOnlyCookies, [callbackAggregator] { });
}

void WebsiteDataStore::hasLocalStorageForTesting(const URL& url, CompletionHandler<void(bool)>&& completionHandler) const
{
    networkProcess().hasLocalStorage(m_sessionID, WebCore::RegistrableDomain { url }, WTFMove(completionHandler));
}

void WebsiteDataStore::hasIsolatedSessionForTesting(const URL& url, CompletionHandler<void(bool)>&& completionHandler) const
{
    networkProcess().hasIsolatedSession(m_sessionID, WebCore::RegistrableDomain { url }, WTFMove(completionHandler));
}

void WebsiteDataStore::setResourceLoadStatisticsShouldDowngradeReferrerForTesting(bool enabled, CompletionHandler<void()>&& completionHandler)
{
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));
    
    networkProcess().setShouldDowngradeReferrerForTesting(enabled, [callbackAggregator] { });
}

#if !PLATFORM(COCOA)
WebCore::ThirdPartyCookieBlockingMode WebsiteDataStore::thirdPartyCookieBlockingMode() const
{
    if (!m_thirdPartyCookieBlockingMode)
        m_thirdPartyCookieBlockingMode = WebCore::ThirdPartyCookieBlockingMode::All;
    return *m_thirdPartyCookieBlockingMode;
}
#endif

void WebsiteDataStore::setResourceLoadStatisticsShouldBlockThirdPartyCookiesForTesting(bool enabled, bool onlyOnSitesWithoutUserInteraction, CompletionHandler<void()>&& completionHandler)
{
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));
    WebCore::ThirdPartyCookieBlockingMode blockingMode = WebCore::ThirdPartyCookieBlockingMode::OnlyAccordingToPerDomainPolicy;
    if (enabled)
        blockingMode = onlyOnSitesWithoutUserInteraction ? WebCore::ThirdPartyCookieBlockingMode::AllOnSitesWithoutUserInteraction : WebCore::ThirdPartyCookieBlockingMode::All;
    setThirdPartyCookieBlockingMode(blockingMode, WTFMove(completionHandler));
}

void WebsiteDataStore::setThirdPartyCookieBlockingMode(WebCore::ThirdPartyCookieBlockingMode blockingMode, CompletionHandler<void()>&& completionHandler)
{
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));

    if (thirdPartyCookieBlockingMode() != blockingMode) {
        m_thirdPartyCookieBlockingMode = blockingMode;
        for (auto& webProcess : processes())
            webProcess.setThirdPartyCookieBlockingMode(blockingMode, [callbackAggregator] { });
    }

    networkProcess().setThirdPartyCookieBlockingMode(m_sessionID, blockingMode, [callbackAggregator] { });
}

void WebsiteDataStore::setResourceLoadStatisticsShouldEnbleSameSiteStrictEnforcementForTesting(bool enabled, CompletionHandler<void()>&& completionHandler)
{
    auto flag = enabled ? WebCore::SameSiteStrictEnforcementEnabled::Yes : WebCore::SameSiteStrictEnforcementEnabled::No;

    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));
    networkProcess().setShouldEnbleSameSiteStrictEnforcementForTesting(m_sessionID, flag, [callbackAggregator] { });
}

void WebsiteDataStore::setResourceLoadStatisticsFirstPartyWebsiteDataRemovalModeForTesting(bool enabled, CompletionHandler<void()>&& completionHandler)
{
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));
    auto mode = enabled ? WebCore::FirstPartyWebsiteDataRemovalMode::AllButCookies : WebCore::FirstPartyWebsiteDataRemovalMode::None;

    networkProcess().setFirstPartyWebsiteDataRemovalModeForTesting(m_sessionID, mode, [callbackAggregator] { });
}

void WebsiteDataStore::setResourceLoadStatisticsToSameSiteStrictCookiesForTesting(const URL& url, CompletionHandler<void()>&& completionHandler)
{
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));

    networkProcess().setToSameSiteStrictCookiesForTesting(m_sessionID, WebCore::RegistrableDomain { url }, [callbackAggregator] { });
}

void WebsiteDataStore::setResourceLoadStatisticsFirstPartyHostCNAMEDomainForTesting(const URL& firstPartyURL, const URL& cnameURL, CompletionHandler<void()>&& completionHandler)
{
    if (cnameURL.host() != "testwebkit.org" && cnameURL.host() != "3rdpartytestwebkit.org") {
        completionHandler();
        return;
    }

    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));

    networkProcess().setFirstPartyHostCNAMEDomainForTesting(m_sessionID, firstPartyURL.host().toString(), WebCore::RegistrableDomain { cnameURL }, [callbackAggregator] { });
}

void WebsiteDataStore::setResourceLoadStatisticsThirdPartyCNAMEDomainForTesting(const URL& cnameURL, CompletionHandler<void()>&& completionHandler)
{
    if (cnameURL.host() != "testwebkit.org" && cnameURL.host() != "3rdpartytestwebkit.org") {
        completionHandler();
        return;
    }

    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));

    networkProcess().setThirdPartyCNAMEDomainForTesting(m_sessionID, WebCore::RegistrableDomain { cnameURL }, [callbackAggregator] { });
}
#endif // ENABLE(RESOURCE_LOAD_STATISTICS)

void WebsiteDataStore::syncLocalStorage(CompletionHandler<void()>&& completionHandler)
{
    networkProcess().sendWithAsyncReply(Messages::NetworkProcess::SyncLocalStorage(), WTFMove(completionHandler));
}

void WebsiteDataStore::setCacheMaxAgeCapForPrevalentResources(Seconds seconds, CompletionHandler<void()>&& completionHandler)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));
    
    networkProcess().setCacheMaxAgeCapForPrevalentResources(m_sessionID, seconds, [callbackAggregator] { });
#else
    UNUSED_PARAM(seconds);
    completionHandler();
#endif
}

void WebsiteDataStore::resetCacheMaxAgeCapForPrevalentResources(CompletionHandler<void()>&& completionHandler)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    networkProcess().resetCacheMaxAgeCapForPrevalentResources(m_sessionID, WTFMove(completionHandler));
#else
    completionHandler();
#endif
}

HashSet<RefPtr<WebProcessPool>> WebsiteDataStore::processPools(size_t limit) const
{
    HashSet<RefPtr<WebProcessPool>> processPools;
    for (auto& process : processes()) {
        if (auto* processPool = process.processPoolIfExists()) {
            processPools.add(processPool);
            if (processPools.size() == limit)
                break;
        }
    }

    if (processPools.isEmpty()) {
        // Check if we're one of the legacy data stores.
        for (auto& processPool : WebProcessPool::allProcessPools()) {
            processPools.add(processPool.ptr());
            if (processPools.size() == limit)
                break;
        }
    }

    return processPools;
}

HashSet<RefPtr<WebProcessPool>> WebsiteDataStore::ensureProcessPools() const
{
    auto processPools = this->processPools();
    if (processPools.isEmpty())
        processPools.add(WebProcessPool::create(API::ProcessPoolConfiguration::create()));
    return processPools;
}

#if ENABLE(NETSCAPE_PLUGIN_API)
Vector<PluginModuleInfo> WebsiteDataStore::plugins() const
{
    Vector<PluginModuleInfo> plugins;

    for (auto& processPool : ensureProcessPools()) {
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

void WebsiteDataStore::allowSpecificHTTPSCertificateForHost(const WebCertificateInfo* certificate, const String& host)
{
    networkProcess().send(Messages::NetworkProcess::AllowSpecificHTTPSCertificateForHost(certificate->certificateInfo(), host), 0);
}

Vector<WebCore::SecurityOriginData> WebsiteDataStore::mediaKeyOrigins(const String& mediaKeysStorageDirectory)
{
    ASSERT(!mediaKeysStorageDirectory.isEmpty());

    Vector<WebCore::SecurityOriginData> origins;

    for (const auto& mediaKeyIdentifier : FileSystem::listDirectory(mediaKeysStorageDirectory)) {
        auto originPath = FileSystem::pathByAppendingComponent(mediaKeysStorageDirectory, mediaKeyIdentifier);
        auto mediaKeyFile = computeMediaKeyFile(originPath);
        if (!FileSystem::fileExists(mediaKeyFile))
            continue;

        if (auto securityOrigin = WebCore::SecurityOriginData::fromDatabaseIdentifier(mediaKeyIdentifier))
            origins.append(*securityOrigin);
    }

    return origins;
}

void WebsiteDataStore::removeMediaKeys(const String& mediaKeysStorageDirectory, WallTime modifiedSince)
{
    ASSERT(!mediaKeysStorageDirectory.isEmpty());

    for (const auto& directoryName : FileSystem::listDirectory(mediaKeysStorageDirectory)) {
        auto mediaKeyDirectory = FileSystem::pathByAppendingComponent(mediaKeysStorageDirectory, directoryName);
        auto mediaKeyFile = computeMediaKeyFile(mediaKeyDirectory);

        auto modificationTime = FileSystem::fileModificationTime(mediaKeyFile);
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

void WebsiteDataStore::getNetworkProcessConnection(WebProcessProxy& webProcessProxy, CompletionHandler<void(const NetworkProcessConnectionInfo&)>&& reply, ShouldRetryOnFailure shouldRetryOnFailure)
{
    auto& networkProcessProxy = networkProcess();
    networkProcessProxy.getNetworkProcessConnection(webProcessProxy, [weakThis = makeWeakPtr(*this), networkProcessProxy = makeWeakPtr(networkProcessProxy), webProcessProxy = makeWeakPtr(webProcessProxy), reply = WTFMove(reply), shouldRetryOnFailure] (auto& connectionInfo) mutable {
        if (UNLIKELY(!IPC::Connection::identifierIsValid(connectionInfo.identifier()))) {
            auto logError = [networkProcessProxy, webProcessProxy]() {
#if OS(DARWIN)
                if (!os_variant_allows_internal_security_policies("com.apple.WebKit"))
                    return;

                if (!webProcessProxy)
                    return;

                int networkProcessIdentifier = 0;
                String networkProcessState = "Unknown"_s;
                if (networkProcessProxy) {
                    networkProcessIdentifier = networkProcessProxy->processIdentifier();
                    networkProcessState = networkProcessProxy->stateString();
                }
                RELEASE_ASSERT_NOT_REACHED_WITH_MESSAGE("WebsiteDataStore::getNetworkProcessConnection: Failed to get connection - networkProcessProxy=%p, networkProcessIdentifier=%d, processState=%s, webProcessProxy=%p, webProcessIdentifier=%d", networkProcessProxy.get(), networkProcessIdentifier, networkProcessState.utf8().data(), webProcessProxy.get(), webProcessProxy->processIdentifier());
#endif
            };

            if (shouldRetryOnFailure == ShouldRetryOnFailure::No || !webProcessProxy) {
                logError();
                reply({ });
                return;
            }

            // Retry on the next RunLoop iteration because we may be inside the WebsiteDataStore destructor.
            RunLoop::main().dispatch([weakThis = WTFMove(weakThis), networkProcessProxy = WTFMove(networkProcessProxy), webProcessProxy = WTFMove(webProcessProxy), reply = WTFMove(reply), logError = WTFMove(logError)] () mutable {
                if (RefPtr<WebsiteDataStore> strongThis = weakThis.get(); strongThis && webProcessProxy) {
                    // Terminate if it is the same network process.
                    if (networkProcessProxy && strongThis->m_networkProcess == networkProcessProxy.get())
                        strongThis->terminateNetworkProcess();
                    RELEASE_LOG_ERROR(Process, "getNetworkProcessConnection: Failed to get connection to network process, will retry ...");
                    strongThis->getNetworkProcessConnection(*webProcessProxy, WTFMove(reply), ShouldRetryOnFailure::No);
                } else {
                    logError();
                    reply({ });
                }
            });
            return;
        }

        reply(connectionInfo);
    });
}

void WebsiteDataStore::networkProcessDidTerminate(NetworkProcessProxy& networkProcess)
{
    ASSERT(!m_networkProcess || m_networkProcess == &networkProcess);
    m_networkProcess = nullptr;
}

void WebsiteDataStore::terminateNetworkProcess()
{
    if (auto networkProcess = std::exchange(m_networkProcess, nullptr))
        networkProcess->terminate();
}

void WebsiteDataStore::sendNetworkProcessPrepareToSuspendForTesting(CompletionHandler<void()>&& completionHandler)
{
    networkProcess().sendPrepareToSuspend(IsSuspensionImminent::No, WTFMove(completionHandler));
}

void WebsiteDataStore::sendNetworkProcessWillSuspendImminentlyForTesting()
{
    networkProcess().sendProcessWillSuspendImminentlyForTesting();
}

void WebsiteDataStore::sendNetworkProcessDidResume()
{
    networkProcess().sendProcessDidResume();
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
    if (enabled == resourceLoadStatisticsEnabled())
        return;

    if (enabled) {
        m_resourceLoadStatisticsEnabled = true;
        
        resolveDirectoriesIfNecessary();
        
        if (m_networkProcess)
            m_networkProcess->send(Messages::NetworkProcess::SetResourceLoadStatisticsEnabled(m_sessionID, true), 0);
        for (auto& processPool : processPools())
            processPool->sendToAllProcessesForSession(Messages::WebProcess::SetResourceLoadStatisticsEnabled(true), m_sessionID);
        return;
    }

    if (m_networkProcess)
        m_networkProcess->send(Messages::NetworkProcess::SetResourceLoadStatisticsEnabled(m_sessionID, false), 0);
    for (auto& processPool : processPools())
        processPool->sendToAllProcessesForSession(Messages::WebProcess::SetResourceLoadStatisticsEnabled(false), m_sessionID);

    m_resourceLoadStatisticsEnabled = false;
#else
    UNUSED_PARAM(enabled);
#endif
}

#if ENABLE(RESOURCE_LOAD_STATISTICS)
void WebsiteDataStore::setStatisticsTestingCallback(Function<void(const String&)>&& callback)
{
    if (callback) {
        networkProcess().send(Messages::NetworkProcess::SetResourceLoadStatisticsLogTestingEvent(true), 0);
    }
    
    m_statisticsTestingCallback = WTFMove(callback);
}
#endif

void WebsiteDataStore::setResourceLoadStatisticsDebugMode(bool enabled)
{
    setResourceLoadStatisticsDebugMode(enabled, []() { });
}

void WebsiteDataStore::setResourceLoadStatisticsDebugMode(bool enabled, CompletionHandler<void()>&& completionHandler)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    m_resourceLoadStatisticsDebugMode = enabled;

    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));
    
    networkProcess().setResourceLoadStatisticsDebugMode(m_sessionID, enabled, [callbackAggregator] { });
#else
    UNUSED_PARAM(enabled);
    UNUSED_PARAM(completionHandler);
#endif
}

void WebsiteDataStore::isResourceLoadStatisticsEphemeral(CompletionHandler<void(bool)>&& completionHandler) const
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    if (!resourceLoadStatisticsEnabled() || !m_sessionID.isEphemeral()) {
        completionHandler(false);
        return;
    }

    networkProcess().isResourceLoadStatisticsEphemeral(m_sessionID, WTFMove(completionHandler));
#else
    completionHandler(false);
#endif
}

void WebsiteDataStore::setPrivateClickMeasurementDebugMode(bool enabled)
{
    networkProcess().setPrivateClickMeasurementDebugMode(enabled);
}

#if ENABLE(RESOURCE_LOAD_STATISTICS)
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
            processPool->sendToAllProcessesForSession(Messages::WebProcess::ClearResourceLoadStatistics(), m_sessionID);
    }
    callback();
}
#endif

void WebsiteDataStore::flushCookies(CompletionHandler<void()>&& completionHandler)
{
    networkProcess().flushCookies(sessionID(), WTFMove(completionHandler));
}

void WebsiteDataStore::setAllowsAnySSLCertificateForWebSocket(bool allows)
{
    networkProcess().sendSync(Messages::NetworkProcess::SetAllowsAnySSLCertificateForWebSocket(allows), Messages::NetworkProcess::SetAllowsAnySSLCertificateForWebSocket::Reply(), 0);
}

void WebsiteDataStore::clearCachedCredentials()
{
    networkProcess().send(Messages::NetworkProcess::ClearCachedCredentials(sessionID()), 0);
}

void WebsiteDataStore::dispatchOnQueue(Function<void()>&& function)
{
    m_queue->dispatch(WTFMove(function));
}

uint64_t WebsiteDataStore::perThirdPartyOriginStorageQuota() const
{
    // FIXME: Consider whether allowing to set a perThirdPartyOriginStorageQuota from a WebsiteDataStore.
    return WebCore::StorageQuotaManager::defaultThirdPartyQuotaFromPerOriginQuota(perOriginStorageQuota());
}

void WebsiteDataStore::setCacheModelSynchronouslyForTesting(CacheModel cacheModel)
{
    for (auto& processPool : WebProcessPool::allProcessPools())
        processPool->setCacheModelSynchronouslyForTesting(cacheModel);
}

Vector<WebsiteDataStoreParameters> WebsiteDataStore::parametersFromEachWebsiteDataStore()
{
    Vector<WebsiteDataStoreParameters> parameters;
    parameters.reserveInitialCapacity(allDataStores().size());
    for (auto* dataStore : allDataStores().values())
        parameters.uncheckedAppend(dataStore->parameters());
    return parameters;
}

WebsiteDataStoreParameters WebsiteDataStore::parameters()
{
    WebsiteDataStoreParameters parameters;

    resolveDirectoriesIfNecessary();

    auto resourceLoadStatisticsDirectory = m_resolvedConfiguration->resourceLoadStatisticsDirectory();
    SandboxExtension::Handle resourceLoadStatisticsDirectoryHandle;
    if (!resourceLoadStatisticsDirectory.isEmpty()) {
        if (auto handle = SandboxExtension::createHandleForReadWriteDirectory(resourceLoadStatisticsDirectory))
            resourceLoadStatisticsDirectoryHandle = WTFMove(*handle);
    }

    auto privateClickMeasurementStorageDirectory = m_resolvedConfiguration->privateClickMeasurementStorageDirectory();
    SandboxExtension::Handle privateClickMeasurementStorageDirectoryHandle;
    if (!privateClickMeasurementStorageDirectory.isEmpty()) {
        if (auto handle = SandboxExtension::createHandleForReadWriteDirectory(privateClickMeasurementStorageDirectory))
            privateClickMeasurementStorageDirectoryHandle = WTFMove(*handle);
    }

    auto networkCacheDirectory = resolvedNetworkCacheDirectory();
    SandboxExtension::Handle networkCacheDirectoryExtensionHandle;
    if (!networkCacheDirectory.isEmpty()) {
        // FIXME: SandboxExtension::createHandleForReadWriteDirectory resolves the directory, but that has already been done. Remove this duplicate work.
        if (auto handle = SandboxExtension::createHandleForReadWriteDirectory(networkCacheDirectory))
            networkCacheDirectoryExtensionHandle = WTFMove(*handle);
    }

    auto hstsStorageDirectory = resolvedHSTSStorageDirectory();
    SandboxExtension::Handle hstsStorageDirectoryExtensionHandle;
    if (!hstsStorageDirectory.isEmpty()) {
        // FIXME: SandboxExtension::createHandleForReadWriteDirectory resolves the directory, but that has already been done. Remove this duplicate work.
        if (auto handle = SandboxExtension::createHandleForReadWriteDirectory(hstsStorageDirectory))
            hstsStorageDirectoryExtensionHandle = WTFMove(*handle);
    }

    bool shouldIncludeLocalhostInResourceLoadStatistics = false;
    bool enableResourceLoadStatisticsDebugMode = false;
    auto firstPartyWebsiteDataRemovalMode = WebCore::FirstPartyWebsiteDataRemovalMode::AllButCookies;
    WebCore::RegistrableDomain standaloneApplicationDomain;
    HashSet<WebCore::RegistrableDomain> appBoundDomains;
#if ENABLE(APP_BOUND_DOMAINS)
    if (isAppBoundITPRelaxationEnabled)
        appBoundDomains = appBoundDomainsIfInitialized().value_or(HashSet<WebCore::RegistrableDomain> { });
#endif
    WebCore::RegistrableDomain resourceLoadStatisticsManualPrevalentResource;
    ResourceLoadStatisticsParameters resourceLoadStatisticsParameters = {
        WTFMove(resourceLoadStatisticsDirectory),
        WTFMove(resourceLoadStatisticsDirectoryHandle),
        WTFMove(privateClickMeasurementStorageDirectory),
        WTFMove(privateClickMeasurementStorageDirectoryHandle),
        resourceLoadStatisticsEnabled(),
#if ENABLE(RESOURCE_LOAD_STATISTICS)
        isItpStateExplicitlySet(),
        hasStatisticsTestingCallback(),
#else
        false,
        false,
#endif
        shouldIncludeLocalhostInResourceLoadStatistics,
        enableResourceLoadStatisticsDebugMode,
#if ENABLE(RESOURCE_LOAD_STATISTICS)
        thirdPartyCookieBlockingMode(),
        WebCore::SameSiteStrictEnforcementEnabled::No,
#endif
        firstPartyWebsiteDataRemovalMode,
        WTFMove(standaloneApplicationDomain),
        WTFMove(appBoundDomains),
        WTFMove(resourceLoadStatisticsManualPrevalentResource),
    };

    NetworkSessionCreationParameters networkSessionParameters;
    networkSessionParameters.sessionID = m_sessionID;
    networkSessionParameters.boundInterfaceIdentifier = configuration().boundInterfaceIdentifier();
    networkSessionParameters.allowsCellularAccess = configuration().allowsCellularAccess() ? AllowsCellularAccess::Yes : AllowsCellularAccess::No;
    networkSessionParameters.deviceManagementRestrictionsEnabled = m_configuration->deviceManagementRestrictionsEnabled();
    networkSessionParameters.allLoadsBlockedByDeviceManagementRestrictionsForTesting = m_configuration->allLoadsBlockedByDeviceManagementRestrictionsForTesting();
    networkSessionParameters.networkCacheDirectory = WTFMove(networkCacheDirectory);
    networkSessionParameters.networkCacheDirectoryExtensionHandle = WTFMove(networkCacheDirectoryExtensionHandle);
    networkSessionParameters.hstsStorageDirectory = WTFMove(hstsStorageDirectory);
    networkSessionParameters.hstsStorageDirectoryExtensionHandle = WTFMove(hstsStorageDirectoryExtensionHandle);
    networkSessionParameters.dataConnectionServiceType = m_configuration->dataConnectionServiceType();
    networkSessionParameters.fastServerTrustEvaluationEnabled = m_configuration->fastServerTrustEvaluationEnabled();
    networkSessionParameters.networkCacheSpeculativeValidationEnabled = m_configuration->networkCacheSpeculativeValidationEnabled();
    networkSessionParameters.shouldUseTestingNetworkSession = m_configuration->testingSessionEnabled();
    networkSessionParameters.staleWhileRevalidateEnabled = m_configuration->staleWhileRevalidateEnabled();
    networkSessionParameters.testSpeedMultiplier = m_configuration->testSpeedMultiplier();
    networkSessionParameters.suppressesConnectionTerminationOnSystemChange = m_configuration->suppressesConnectionTerminationOnSystemChange();
    networkSessionParameters.allowsServerPreconnect = m_configuration->allowsServerPreconnect();
    networkSessionParameters.resourceLoadStatisticsParameters = WTFMove(resourceLoadStatisticsParameters);
    networkSessionParameters.requiresSecureHTTPSProxyConnection = m_configuration->requiresSecureHTTPSProxyConnection();
    networkSessionParameters.preventsSystemHTTPProxyAuthentication = m_configuration->preventsSystemHTTPProxyAuthentication();
    networkSessionParameters.allowsHSTSWithUntrustedRootCertificate = m_configuration->allowsHSTSWithUntrustedRootCertificate();

    parameters.networkSessionParameters = WTFMove(networkSessionParameters);

    parameters.indexedDatabaseDirectory = resolvedIndexedDatabaseDirectory();
    if (!parameters.indexedDatabaseDirectory.isEmpty()) {
        // FIXME: SandboxExtension::createHandleForReadWriteDirectory resolves the directory, but that has already been done. Remove this duplicate work.
        if (auto handle =  SandboxExtension::createHandleForReadWriteDirectory(parameters.indexedDatabaseDirectory))
            parameters.indexedDatabaseDirectoryExtensionHandle = WTFMove(*handle);
    }

#if ENABLE(SERVICE_WORKER)
    parameters.serviceWorkerRegistrationDirectory = resolvedServiceWorkerRegistrationDirectory();
    if (!parameters.serviceWorkerRegistrationDirectory.isEmpty()) {
        // FIXME: SandboxExtension::createHandleForReadWriteDirectory resolves the directory, but that has already been done. Remove this duplicate work.
        if (auto handle = SandboxExtension::createHandleForReadWriteDirectory(parameters.serviceWorkerRegistrationDirectory))
            parameters.serviceWorkerRegistrationDirectoryExtensionHandle = WTFMove(*handle);
    }
    parameters.serviceWorkerProcessTerminationDelayEnabled = m_configuration->serviceWorkerProcessTerminationDelayEnabled();
#endif

    auto localStorageDirectory = resolvedLocalStorageDirectory();
    if (!localStorageDirectory.isEmpty()) {
        parameters.localStorageDirectory = localStorageDirectory;
        // FIXME: SandboxExtension::createHandleForReadWriteDirectory resolves the directory, but that has already been done. Remove this duplicate work.
        if (auto handle = SandboxExtension::createHandleForReadWriteDirectory(localStorageDirectory))
            parameters.localStorageDirectoryExtensionHandle = WTFMove(*handle);
    }

    auto cacheStorageDirectory = this->cacheStorageDirectory();
    if (!cacheStorageDirectory.isEmpty()) {
        parameters.cacheStorageDirectory = cacheStorageDirectory;
        if (auto handle = SandboxExtension::createHandleForReadWriteDirectory(cacheStorageDirectory))
            parameters.cacheStorageDirectoryExtensionHandle = WTFMove(*handle);
    }

    parameters.perOriginStorageQuota = perOriginStorageQuota();
    parameters.perThirdPartyOriginStorageQuota = perThirdPartyOriginStorageQuota();

#if ENABLE(RESOURCE_LOAD_STATISTICS)
    parameters.networkSessionParameters.resourceLoadStatisticsParameters.enabled = m_resourceLoadStatisticsEnabled;
#endif
    platformSetNetworkParameters(parameters);
#if PLATFORM(COCOA)
    parameters.networkSessionParameters.appHasRequestedCrossWebsiteTrackingPermission = hasRequestedCrossWebsiteTrackingPermission();
    parameters.networkSessionParameters.useNetworkLoader = useNetworkLoader();
#endif
    
    return parameters;
}

#if HAVE(SEC_KEY_PROXY)
void WebsiteDataStore::addSecKeyProxyStore(Ref<SecKeyProxyStore>&& store)
{
    m_secKeyProxyStores.append(WTFMove(store));
}
#endif

#if ENABLE(WEB_AUTHN)
void WebsiteDataStore::setMockWebAuthenticationConfiguration(WebCore::MockWebAuthenticationConfiguration&& configuration)
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

void WebsiteDataStore::getLocalStorageDetails(Function<void(Vector<LocalStorageDatabaseTracker::OriginDetails>&&)>&& completionHandler)
{
    if (!isPersistent()) {
        completionHandler({ });
        return;
    }

    networkProcess().getLocalStorageDetails(m_sessionID, [completionHandler = WTFMove(completionHandler)](auto&& details) {
        completionHandler(WTFMove(details));
    });
}

void WebsiteDataStore::resetQuota(CompletionHandler<void()>&& completionHandler)
{
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));
    networkProcess().resetQuota(m_sessionID, [callbackAggregator] { });
}

#if !PLATFORM(COCOA)
WTF::String WebsiteDataStore::defaultMediaCacheDirectory()
{
    // FIXME: Implement. https://bugs.webkit.org/show_bug.cgi?id=156369 and https://bugs.webkit.org/show_bug.cgi?id=156370
    return WTF::String();
}

WTF::String WebsiteDataStore::defaultAlternativeServicesDirectory()
{
    // FIXME: Implement.
    return WTF::String();
}

WTF::String WebsiteDataStore::defaultJavaScriptConfigurationDirectory()
{
    // FIXME: Implement.
    return WTF::String();
}

bool WebsiteDataStore::http3Enabled()
{
    return false;
}

bool WebsiteDataStore::networkProcessHasEntitlementForTesting(const String&)
{
    return false;
}
#endif // !PLATFORM(COCOA)

#if !USE(GLIB) && !PLATFORM(COCOA)
WTF::String WebsiteDataStore::defaultDeviceIdHashSaltsStorageDirectory()
{
    // Not implemented.
    return WTF::String();
}
#endif

void WebsiteDataStore::renameOriginInWebsiteData(URL&& oldName, URL&& newName, OptionSet<WebsiteDataType> dataTypes, CompletionHandler<void()>&& completionHandler)
{
    networkProcess().renameOriginInWebsiteData(m_sessionID, oldName, newName, dataTypes, WTFMove(completionHandler));
}

#if ENABLE(APP_BOUND_DOMAINS)
void WebsiteDataStore::hasAppBoundSession(CompletionHandler<void(bool)>&& completionHandler) const
{
    networkProcess().hasAppBoundSession(m_sessionID, WTFMove(completionHandler));
}

void WebsiteDataStore::clearAppBoundSession(CompletionHandler<void()>&& completionHandler)
{
    networkProcess().clearAppBoundSession(m_sessionID, WTFMove(completionHandler));
}

void WebsiteDataStore::forwardAppBoundDomainsToITPIfInitialized(CompletionHandler<void()>&& completionHandler)
{
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));
    auto appBoundDomains = appBoundDomainsIfInitialized();
    if (!appBoundDomains)
        return;

    auto propagateAppBoundDomains = [callbackAggregator] (WebsiteDataStore* store, const HashSet<WebCore::RegistrableDomain>& domains) {
        if (!store)
            return;

        if (store->thirdPartyCookieBlockingMode() != WebCore::ThirdPartyCookieBlockingMode::AllExceptBetweenAppBoundDomains)
            store->setThirdPartyCookieBlockingMode(WebCore::ThirdPartyCookieBlockingMode::AllExceptBetweenAppBoundDomains, [callbackAggregator] { });

        store->setAppBoundDomainsForITP(domains, [callbackAggregator] { });
    };

    propagateAppBoundDomains(globalDefaultDataStore().get(), *appBoundDomains);

    for (auto* store : allDataStores().values())
        propagateAppBoundDomains(store, *appBoundDomains);
}

void WebsiteDataStore::setAppBoundDomainsForITP(const HashSet<WebCore::RegistrableDomain>& domains, CompletionHandler<void()>&& completionHandler)
{
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));

    networkProcess().setAppBoundDomainsForResourceLoadStatistics(m_sessionID, domains, [callbackAggregator] { });
}
#endif

void WebsiteDataStore::updateBundleIdentifierInNetworkProcess(const String& bundleIdentifier, CompletionHandler<void()>&& completionHandler)
{
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));

    networkProcess().updateBundleIdentifier(bundleIdentifier, [callbackAggregator] { });
}

void WebsiteDataStore::clearBundleIdentifierInNetworkProcess(CompletionHandler<void()>&& completionHandler)
{
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));

    networkProcess().clearBundleIdentifier([callbackAggregator] { });
}

void WebsiteDataStore::countNonDefaultSessionSets(CompletionHandler<void(size_t)>&& completionHandler)
{
    networkProcess().sendWithAsyncReply(Messages::NetworkProcess::CountNonDefaultSessionSets(m_sessionID), WTFMove(completionHandler));
}

static bool nextNetworkProcessLaunchShouldFailForTesting { false };

void WebsiteDataStore::makeNextNetworkProcessLaunchFailForTesting()
{
    nextNetworkProcessLaunchShouldFailForTesting = true;
}

bool WebsiteDataStore::shouldMakeNextNetworkProcessLaunchFailForTesting()
{
    return std::exchange(nextNetworkProcessLaunchShouldFailForTesting, false);
}

}

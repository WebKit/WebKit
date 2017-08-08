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

#pragma once

#include "WebProcessLifetimeObserver.h"
#include <WebCore/Cookie.h>
#include <WebCore/SecurityOriginData.h>
#include <WebCore/SecurityOriginHash.h>
#include <WebCore/SessionID.h>
#include <wtf/Function.h>
#include <wtf/HashSet.h>
#include <wtf/OptionSet.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/WorkQueue.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(COCOA)
#include <pal/spi/cf/CFNetworkSPI.h>
#endif

namespace WebCore {
class SecurityOrigin;
}

namespace WebKit {

class StorageManager;
class WebPageProxy;
class WebProcessPool;
class WebResourceLoadStatisticsStore;
enum class WebsiteDataFetchOption;
enum class WebsiteDataType;
struct StorageProcessCreationParameters;
struct WebsiteDataRecord;
struct WebsiteDataStoreParameters;

#if ENABLE(NETSCAPE_PLUGIN_API)
struct PluginModuleInfo;
#endif

enum class ShouldClearFirst { No, Yes };

class WebsiteDataStore : public RefCounted<WebsiteDataStore>, public WebProcessLifetimeObserver {
public:
    struct Configuration {
        String networkCacheDirectory;
        String applicationCacheDirectory;
        String applicationCacheFlatFileSubdirectoryName;

        String mediaCacheDirectory;
        String indexedDBDatabaseDirectory;
        String webSQLDatabaseDirectory;
        String localStorageDirectory;
        String mediaKeysStorageDirectory;
        String resourceLoadStatisticsDirectory;
        String javaScriptConfigurationDirectory;
        String cookieStorageFile;
    };
    static Ref<WebsiteDataStore> createNonPersistent();
    static Ref<WebsiteDataStore> create(Configuration, WebCore::SessionID);
    virtual ~WebsiteDataStore();

    uint64_t identifier() const { return m_identifier; }

    bool isPersistent() const { return !m_sessionID.isEphemeral(); }
    WebCore::SessionID sessionID() const { return m_sessionID; }

    bool resourceLoadStatisticsEnabled() const;
    void setResourceLoadStatisticsEnabled(bool);
    WebResourceLoadStatisticsStore* resourceLoadStatistics() const { return m_resourceLoadStatistics.get(); }
    void clearResourceLoadStatisticsInWebProcesses();

    static void cloneSessionData(WebPageProxy& sourcePage, WebPageProxy& newPage);

    void fetchData(OptionSet<WebsiteDataType>, OptionSet<WebsiteDataFetchOption>, Function<void(Vector<WebsiteDataRecord>)>&& completionHandler);
    void fetchDataForTopPrivatelyControlledDomains(OptionSet<WebsiteDataType>, OptionSet<WebsiteDataFetchOption>, const Vector<String>& topPrivatelyControlledDomains, Function<void(Vector<WebsiteDataRecord>&&, HashSet<String>&&)>&& completionHandler);
    void topPrivatelyControlledDomainsWithWebsiteData(OptionSet<WebsiteDataType> dataTypes, OptionSet<WebsiteDataFetchOption> fetchOptions, Function<void(HashSet<String>&&)>&& completionHandler);
    void removeData(OptionSet<WebsiteDataType>, std::chrono::system_clock::time_point modifiedSince, Function<void()>&& completionHandler);
    void removeData(OptionSet<WebsiteDataType>, const Vector<WebsiteDataRecord>&, Function<void()>&& completionHandler);
    void removeDataForTopPrivatelyControlledDomains(OptionSet<WebsiteDataType>, OptionSet<WebsiteDataFetchOption>, const Vector<String>& topPrivatelyControlledDomains, Function<void(HashSet<String>&&)>&& completionHandler);

#if HAVE(CFNETWORK_STORAGE_PARTITIONING)
    void updateCookiePartitioningForTopPrivatelyOwnedDomains(const Vector<String>& domainsToRemove, const Vector<String>& domainsToAdd, ShouldClearFirst);
#endif
    void networkProcessDidCrash();
    void resolveDirectoriesIfNecessary();
    const String& resolvedApplicationCacheDirectory() const { return m_resolvedConfiguration.applicationCacheDirectory; }
    const String& resolvedMediaCacheDirectory() const { return m_resolvedConfiguration.mediaCacheDirectory; }
    const String& resolvedMediaKeysDirectory() const { return m_resolvedConfiguration.mediaKeysStorageDirectory; }
    const String& resolvedDatabaseDirectory() const { return m_resolvedConfiguration.webSQLDatabaseDirectory; }
    const String& resolvedJavaScriptConfigurationDirectory() const { return m_resolvedConfiguration.javaScriptConfigurationDirectory; }
    const String& resolvedCookieStorageFile() const { return m_resolvedConfiguration.cookieStorageFile; }
    const String& resolvedIndexedDatabaseDirectory() const { return m_resolvedConfiguration.indexedDBDatabaseDirectory; }

    StorageManager* storageManager() { return m_storageManager.get(); }

    WebProcessPool* processPoolForCookieStorageOperations();
    bool isAssociatedProcessPool(WebProcessPool&) const;

    WebsiteDataStoreParameters parameters();
    StorageProcessCreationParameters storageProcessParameters();

    Vector<WebCore::Cookie> pendingCookies() const;
    void addPendingCookie(const WebCore::Cookie&);
    void removePendingCookie(const WebCore::Cookie&);

    void enableResourceLoadStatisticsAndSetTestingCallback(Function<void (const String&)>&& callback);

private:
    explicit WebsiteDataStore(WebCore::SessionID);
    explicit WebsiteDataStore(Configuration, WebCore::SessionID);

    void fetchDataAndApply(OptionSet<WebsiteDataType>, OptionSet<WebsiteDataFetchOption>, RefPtr<WorkQueue>&&, Function<void(Vector<WebsiteDataRecord>)>&& apply);

    // WebProcessLifetimeObserver.
    void webPageWasAdded(WebPageProxy&) override;
    void webPageWasRemoved(WebPageProxy&) override;
    void webProcessWillOpenConnection(WebProcessProxy&, IPC::Connection&) override;
    void webPageWillOpenConnection(WebPageProxy&, IPC::Connection&) override;
    void webPageDidCloseConnection(WebPageProxy&, IPC::Connection&) override;
    void webProcessDidCloseConnection(WebProcessProxy&, IPC::Connection&) override;

    void platformInitialize();
    void platformDestroy();
    static void platformRemoveRecentSearches(std::chrono::system_clock::time_point);

    HashSet<RefPtr<WebProcessPool>> processPools(size_t count = std::numeric_limits<size_t>::max(), bool ensureAPoolExists = true) const;

#if ENABLE(NETSCAPE_PLUGIN_API)
    Vector<PluginModuleInfo> plugins() const;
#endif

    static Vector<WebCore::SecurityOriginData> mediaKeyOrigins(const String& mediaKeysStorageDirectory);
    static void removeMediaKeys(const String& mediaKeysStorageDirectory, std::chrono::system_clock::time_point modifiedSince);
    static void removeMediaKeys(const String& mediaKeysStorageDirectory, const HashSet<WebCore::SecurityOriginData>&);

    const uint64_t m_identifier;
    const WebCore::SessionID m_sessionID;

    const Configuration m_configuration;
    Configuration m_resolvedConfiguration;
    bool m_hasResolvedDirectories { false };

    const RefPtr<StorageManager> m_storageManager;
    RefPtr<WebResourceLoadStatisticsStore> m_resourceLoadStatistics;

    Ref<WorkQueue> m_queue;

#if PLATFORM(COCOA)
    Vector<uint8_t> m_uiProcessCookieStorageIdentifier;
    RetainPtr<CFHTTPCookieStorageRef> m_cfCookieStorage;
#endif
    HashSet<WebCore::Cookie> m_pendingCookies;
};

}

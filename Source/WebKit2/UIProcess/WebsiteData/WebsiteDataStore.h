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
#include <WebCore/SecurityOriginData.h>
#include <WebCore/SecurityOriginHash.h>
#include <WebCore/SessionID.h>
#include <functional>
#include <wtf/HashSet.h>
#include <wtf/OptionSet.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/WorkQueue.h>
#include <wtf/text/WTFString.h>

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
struct WebsiteDataRecord;

#if ENABLE(NETSCAPE_PLUGIN_API)
struct PluginModuleInfo;
#endif

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
    };
    static Ref<WebsiteDataStore> createNonPersistent();
    static Ref<WebsiteDataStore> create(Configuration);
    virtual ~WebsiteDataStore();

    uint64_t identifier() const { return m_identifier; }

    bool isPersistent() const { return !m_sessionID.isEphemeral(); }
    WebCore::SessionID sessionID() const { return m_sessionID; }

    bool resourceLoadStatisticsEnabled() const;
    void setResourceLoadStatisticsEnabled(bool);
    void registerSharedResourceLoadObserver();

    static void cloneSessionData(WebPageProxy& sourcePage, WebPageProxy& newPage);

    void fetchData(OptionSet<WebsiteDataType>, OptionSet<WebsiteDataFetchOption>, std::function<void (Vector<WebsiteDataRecord>)> completionHandler);
    void fetchDataForTopPrivatelyOwnedDomains(OptionSet<WebsiteDataType>, OptionSet<WebsiteDataFetchOption>, const Vector<String>& topPrivatelyOwnedDomains, std::function<void(Vector<WebsiteDataRecord>&&, Vector<String>&&)> completionHandler);
    void removeData(OptionSet<WebsiteDataType>, std::chrono::system_clock::time_point modifiedSince, std::function<void ()> completionHandler);
    void removeData(OptionSet<WebsiteDataType>, const Vector<WebsiteDataRecord>&, std::function<void ()> completionHandler);
    void removeDataForTopPrivatelyOwnedDomains(OptionSet<WebsiteDataType>, OptionSet<WebsiteDataFetchOption>, const Vector<String>& topPrivatelyOwnedDomains, std::function<void(Vector<String>)> completionHandler);

#if HAVE(CFNETWORK_STORAGE_PARTITIONING)
    void shouldPartitionCookiesForTopPrivatelyOwnedDomains(const Vector<String>& domainsToRemove, const Vector<String>& domainsToAdd);
#endif
    void resolveDirectoriesIfNecessary();
    const String& resolvedApplicationCacheDirectory() const { return m_resolvedConfiguration.applicationCacheDirectory; }
    const String& resolvedMediaCacheDirectory() const { return m_resolvedConfiguration.mediaCacheDirectory; }
    const String& resolvedMediaKeysDirectory() const { return m_resolvedConfiguration.mediaKeysStorageDirectory; }
    const String& resolvedDatabaseDirectory() const { return m_resolvedConfiguration.webSQLDatabaseDirectory; }
    const String& resolvedJavaScriptConfigurationDirectory() const { return m_resolvedConfiguration.javaScriptConfigurationDirectory; }

    StorageManager* storageManager() { return m_storageManager.get(); }

    WebProcessPool* processPoolForCookieStorageOperations();
    bool isAssociatedProcessPool(WebProcessPool&) const;

private:
    explicit WebsiteDataStore(WebCore::SessionID);
    explicit WebsiteDataStore(Configuration);

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
    const RefPtr<WebResourceLoadStatisticsStore> m_resourceLoadStatistics;

    Ref<WorkQueue> m_queue;
};

}

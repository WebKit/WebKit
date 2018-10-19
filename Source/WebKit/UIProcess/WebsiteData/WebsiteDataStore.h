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

#include "NetworkSessionCreationParameters.h"
#include "WebProcessLifetimeObserver.h"
#include <WebCore/Cookie.h>
#include <WebCore/SecurityOriginData.h>
#include <WebCore/SecurityOriginHash.h>
#include <pal/SessionID.h>
#include <wtf/Function.h>
#include <wtf/HashSet.h>
#include <wtf/Identified.h>
#include <wtf/OptionSet.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/UniqueRef.h>
#include <wtf/WeakPtr.h>
#include <wtf/WorkQueue.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(COCOA)
#include <pal/spi/cf/CFNetworkSPI.h>
#endif

namespace WebCore {
class SecurityOrigin;
}

namespace WebKit {

class AuthenticatorManager;
class SecKeyProxyStore;
class StorageManager;
class DeviceIdHashSaltStorage;
class WebPageProxy;
class WebProcessPool;
class WebResourceLoadStatisticsStore;
enum class WebsiteDataFetchOption;
enum class WebsiteDataType;
struct MockWebAuthenticationConfiguration;
struct WebsiteDataRecord;
struct WebsiteDataStoreParameters;

#if ENABLE(RESOURCE_LOAD_STATISTICS)
enum class StorageAccessStatus;
enum class StorageAccessPromptStatus;
#endif

#if ENABLE(NETSCAPE_PLUGIN_API)
struct PluginModuleInfo;
#endif

enum class ShouldCapLifetimeForClientSideCookies { No, Yes };

class WebsiteDataStore : public RefCounted<WebsiteDataStore>, public WebProcessLifetimeObserver, public Identified<WebsiteDataStore>, public CanMakeWeakPtr<WebsiteDataStore>  {
public:
    constexpr static uint64_t defaultCacheStoragePerOriginQuota = 50 * 1024 * 1024;

    struct Configuration {
        String cacheStorageDirectory;
        uint64_t cacheStoragePerOriginQuota { defaultCacheStoragePerOriginQuota };
        String networkCacheDirectory;
        String applicationCacheDirectory;
        String applicationCacheFlatFileSubdirectoryName;

        String mediaCacheDirectory;
        String indexedDBDatabaseDirectory;
        String serviceWorkerRegistrationDirectory;
        String webSQLDatabaseDirectory;
        String localStorageDirectory;
        String mediaKeysStorageDirectory;
        String deviceIdHashSaltsStorageDirectory;
        String resourceLoadStatisticsDirectory;
        String javaScriptConfigurationDirectory;
        String cookieStorageFile;
        String sourceApplicationBundleIdentifier;
        String sourceApplicationSecondaryIdentifier;

        explicit Configuration();
    };
    static Ref<WebsiteDataStore> createNonPersistent();
    static Ref<WebsiteDataStore> create(Configuration, PAL::SessionID);
    virtual ~WebsiteDataStore();

    static WebsiteDataStore* existingNonDefaultDataStoreForSessionID(PAL::SessionID);

    bool isPersistent() const { return !m_sessionID.isEphemeral(); }
    PAL::SessionID sessionID() const { return m_sessionID; }

    bool resourceLoadStatisticsEnabled() const;
    void setResourceLoadStatisticsEnabled(bool);
    bool resourceLoadStatisticsDebugMode() const;
    void setResourceLoadStatisticsDebugMode(bool);
    void setResourceLoadStatisticsDebugMode(bool, CompletionHandler<void()>&&);

    uint64_t cacheStoragePerOriginQuota() const { return m_resolvedConfiguration.cacheStoragePerOriginQuota; }
    void setCacheStoragePerOriginQuota(uint64_t quota) { m_resolvedConfiguration.cacheStoragePerOriginQuota = quota; }
    const String& cacheStorageDirectory() const { return m_resolvedConfiguration.cacheStorageDirectory; }
    void setCacheStorageDirectory(String&& directory) { m_resolvedConfiguration.cacheStorageDirectory = WTFMove(directory); }
    const String& serviceWorkerRegistrationDirectory() const { return m_resolvedConfiguration.serviceWorkerRegistrationDirectory; }
    void setServiceWorkerRegistrationDirectory(String&& directory) { m_resolvedConfiguration.serviceWorkerRegistrationDirectory = WTFMove(directory); }

    WebResourceLoadStatisticsStore* resourceLoadStatistics() const { return m_resourceLoadStatistics.get(); }
    void clearResourceLoadStatisticsInWebProcesses(CompletionHandler<void()>&&);

    static void cloneSessionData(WebPageProxy& sourcePage, WebPageProxy& newPage);

    void fetchData(OptionSet<WebsiteDataType>, OptionSet<WebsiteDataFetchOption>, Function<void(Vector<WebsiteDataRecord>)>&& completionHandler);
    void fetchDataForTopPrivatelyControlledDomains(OptionSet<WebsiteDataType>, OptionSet<WebsiteDataFetchOption>, const Vector<String>& topPrivatelyControlledDomains, Function<void(Vector<WebsiteDataRecord>&&, HashSet<String>&&)>&& completionHandler);
    void topPrivatelyControlledDomainsWithWebsiteData(OptionSet<WebsiteDataType> dataTypes, OptionSet<WebsiteDataFetchOption> fetchOptions, Function<void(HashSet<String>&&)>&& completionHandler);
    void removeData(OptionSet<WebsiteDataType>, WallTime modifiedSince, Function<void()>&& completionHandler);
    void removeData(OptionSet<WebsiteDataType>, const Vector<WebsiteDataRecord>&, Function<void()>&& completionHandler);
    void removeDataForTopPrivatelyControlledDomains(OptionSet<WebsiteDataType>, OptionSet<WebsiteDataFetchOption>, const Vector<String>& topPrivatelyControlledDomains, Function<void(HashSet<String>&&)>&& completionHandler);

#if ENABLE(RESOURCE_LOAD_STATISTICS)
    void updatePrevalentDomainsToBlockCookiesFor(const Vector<String>& domainsToBlock, CompletionHandler<void()>&&);
    void setShouldCapLifetimeForClientSideCookies(ShouldCapLifetimeForClientSideCookies, CompletionHandler<void()>&&);
    void hasStorageAccessForFrameHandler(const String& resourceDomain, const String& firstPartyDomain, uint64_t frameID, uint64_t pageID, CompletionHandler<void(bool hasAccess)>&&);
    void getAllStorageAccessEntries(uint64_t pageID, CompletionHandler<void(Vector<String>&& domains)>&&);
    void grantStorageAccessHandler(const String& resourceDomain, const String& firstPartyDomain, std::optional<uint64_t> frameID, uint64_t pageID, CompletionHandler<void(bool wasGranted)>&&);
    void removeAllStorageAccessHandler(CompletionHandler<void()>&&);
    void removePrevalentDomains(const Vector<String>& domains);
    void hasStorageAccess(String&& subFrameHost, String&& topFrameHost, uint64_t frameID, uint64_t pageID, CompletionHandler<void(bool)>&&);
    void requestStorageAccess(String&& subFrameHost, String&& topFrameHost, uint64_t frameID, uint64_t pageID, bool promptEnabled, CompletionHandler<void(StorageAccessStatus)>&&);
    void grantStorageAccess(String&& subFrameHost, String&& topFrameHost, uint64_t frameID, uint64_t pageID, bool userWasPrompted, CompletionHandler<void(bool)>&&);
#endif
    void setCacheMaxAgeCapForPrevalentResources(Seconds, CompletionHandler<void()>&&);
    void resetCacheMaxAgeCapForPrevalentResources(CompletionHandler<void()>&&);
    void resolveDirectoriesIfNecessary();
    const String& resolvedApplicationCacheDirectory() const { return m_resolvedConfiguration.applicationCacheDirectory; }
    const String& resolvedMediaCacheDirectory() const { return m_resolvedConfiguration.mediaCacheDirectory; }
    const String& resolvedMediaKeysDirectory() const { return m_resolvedConfiguration.mediaKeysStorageDirectory; }
    const String& resolvedDatabaseDirectory() const { return m_resolvedConfiguration.webSQLDatabaseDirectory; }
    const String& resolvedJavaScriptConfigurationDirectory() const { return m_resolvedConfiguration.javaScriptConfigurationDirectory; }
    const String& resolvedCookieStorageFile() const { return m_resolvedConfiguration.cookieStorageFile; }
    const String& resolvedIndexedDatabaseDirectory() const { return m_resolvedConfiguration.indexedDBDatabaseDirectory; }
    const String& resolvedServiceWorkerRegistrationDirectory() const { return m_resolvedConfiguration.serviceWorkerRegistrationDirectory; }
    const String& resolvedResourceLoadStatisticsDirectory() const { return m_resolvedConfiguration.resourceLoadStatisticsDirectory; }

    StorageManager* storageManager() { return m_storageManager.get(); }

    DeviceIdHashSaltStorage* deviceIdHashSaltStorage() { return m_deviceIdHashSaltStorage.get(); }

    WebProcessPool* processPoolForCookieStorageOperations();
    bool isAssociatedProcessPool(WebProcessPool&) const;

    WebsiteDataStoreParameters parameters();

    Vector<WebCore::Cookie> pendingCookies() const;
    void addPendingCookie(const WebCore::Cookie&);
    void removePendingCookie(const WebCore::Cookie&);
    void clearPendingCookies();

    void enableResourceLoadStatisticsAndSetTestingCallback(Function<void (const String&)>&& callback);

    void setBoundInterfaceIdentifier(String&& identifier) { m_boundInterfaceIdentifier = WTFMove(identifier); }
    const String& boundInterfaceIdentifier() { return m_boundInterfaceIdentifier; }
    
    void setAllowsCellularAccess(AllowsCellularAccess allows) { m_allowsCellularAccess = allows; }
    AllowsCellularAccess allowsCellularAccess() { return m_allowsCellularAccess; }

#if PLATFORM(COCOA)
    void setProxyConfiguration(CFDictionaryRef configuration) { m_proxyConfiguration = configuration; }
    CFDictionaryRef proxyConfiguration() { return m_proxyConfiguration.get(); }
#endif
    
    static void allowWebsiteDataRecordsForAllOrigins();

#if HAVE(SEC_KEY_PROXY)
    void addSecKeyProxyStore(Ref<SecKeyProxyStore>&&);
#endif

#if ENABLE(WEB_AUTHN)
    AuthenticatorManager& authenticatorManager() { return m_authenticatorManager.get(); }
    void setMockWebAuthenticationConfiguration(MockWebAuthenticationConfiguration&&);
#endif

    void didCreateNetworkProcess();

private:
    explicit WebsiteDataStore(PAL::SessionID);
    explicit WebsiteDataStore(Configuration, PAL::SessionID);

    void fetchDataAndApply(OptionSet<WebsiteDataType>, OptionSet<WebsiteDataFetchOption>, RefPtr<WorkQueue>&&, Function<void(Vector<WebsiteDataRecord>)>&& apply);

    // WebProcessLifetimeObserver.
    void webPageWasAdded(WebPageProxy&) override;
    void webPageWasInvalidated(WebPageProxy&) override;
    void webProcessWillOpenConnection(WebProcessProxy&, IPC::Connection&) override;
    void webPageWillOpenConnection(WebPageProxy&, IPC::Connection&) override;
    void webPageDidCloseConnection(WebPageProxy&, IPC::Connection&) override;
    void webProcessDidCloseConnection(WebProcessProxy&, IPC::Connection&) override;

    void platformInitialize();
    void platformDestroy();
    static void platformRemoveRecentSearches(WallTime);

    void registerWebResourceLoadStatisticsStoreAsMessageReceiver();
    void unregisterWebResourceLoadStatisticsStoreAsMessageReceiver();

    HashSet<RefPtr<WebProcessPool>> processPools(size_t count = std::numeric_limits<size_t>::max(), bool ensureAPoolExists = true) const;

#if ENABLE(NETSCAPE_PLUGIN_API)
    Vector<PluginModuleInfo> plugins() const;
#endif

    static Vector<WebCore::SecurityOriginData> mediaKeyOrigins(const String& mediaKeysStorageDirectory);
    static void removeMediaKeys(const String& mediaKeysStorageDirectory, WallTime modifiedSince);
    static void removeMediaKeys(const String& mediaKeysStorageDirectory, const HashSet<WebCore::SecurityOriginData>&);

    void maybeRegisterWithSessionIDMap();

    const PAL::SessionID m_sessionID;

    const Configuration m_configuration;
    Configuration m_resolvedConfiguration;
    bool m_hasResolvedDirectories { false };

    const RefPtr<StorageManager> m_storageManager;
    const RefPtr<DeviceIdHashSaltStorage> m_deviceIdHashSaltStorage;
    RefPtr<WebResourceLoadStatisticsStore> m_resourceLoadStatistics;
    bool m_resourceLoadStatisticsDebugMode { false };

    Ref<WorkQueue> m_queue;

#if PLATFORM(COCOA)
    Vector<uint8_t> m_uiProcessCookieStorageIdentifier;
    RetainPtr<CFHTTPCookieStorageRef> m_cfCookieStorage;
    RetainPtr<CFDictionaryRef> m_proxyConfiguration;
#endif
    HashSet<WebCore::Cookie> m_pendingCookies;
    
    String m_boundInterfaceIdentifier;
    AllowsCellularAccess m_allowsCellularAccess { AllowsCellularAccess::Yes };

#if HAVE(SEC_KEY_PROXY)
    Vector<Ref<SecKeyProxyStore>> m_secKeyProxyStores;
#endif

#if ENABLE(WEB_AUTHN)
    UniqueRef<AuthenticatorManager> m_authenticatorManager;
#endif
};

}

/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#ifndef WebsiteDataStore_h
#define WebsiteDataStore_h

#include "WebProcessLifetimeObserver.h"
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

        String webSQLDatabaseDirectory;
        String localStorageDirectory;
        String mediaKeysStorageDirectory;
    };
    static Ref<WebsiteDataStore> createNonPersistent();
    static Ref<WebsiteDataStore> create(Configuration);
    virtual ~WebsiteDataStore();

    uint64_t identifier() const { return m_identifier; }

    bool isPersistent() const { return !m_sessionID.isEphemeral(); }
    WebCore::SessionID sessionID() const { return m_sessionID; }

    static void cloneSessionData(WebPageProxy& sourcePage, WebPageProxy& newPage);

    void fetchData(OptionSet<WebsiteDataType>, OptionSet<WebsiteDataFetchOption>, std::function<void (Vector<WebsiteDataRecord>)> completionHandler);
    void removeData(OptionSet<WebsiteDataType>, std::chrono::system_clock::time_point modifiedSince, std::function<void ()> completionHandler);
    void removeData(OptionSet<WebsiteDataType>, const Vector<WebsiteDataRecord>&, std::function<void ()> completionHandler);

    StorageManager* storageManager() { return m_storageManager.get(); }

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

    HashSet<RefPtr<WebProcessPool>> processPools() const;

#if ENABLE(NETSCAPE_PLUGIN_API)
    Vector<PluginModuleInfo> plugins() const;
#endif

    static Vector<RefPtr<WebCore::SecurityOrigin>> mediaKeyOrigins(const String& mediaKeysStorageDirectory);
    static void removeMediaKeys(const String& mediaKeysStorageDirectory, std::chrono::system_clock::time_point modifiedSince);
    static void removeMediaKeys(const String& mediaKeysStorageDirectory, const HashSet<RefPtr<WebCore::SecurityOrigin>>&);

    const uint64_t m_identifier;
    const WebCore::SessionID m_sessionID;

    const String m_networkCacheDirectory;
    const String m_applicationCacheDirectory;

    const String m_webSQLDatabaseDirectory;
    const String m_mediaKeysStorageDirectory;
    const RefPtr<StorageManager> m_storageManager;

    Ref<WorkQueue> m_queue;
};

}

#endif // WebsiteDataStore_h

/*
 * Copyright (C) 2013-2019 Apple Inc. All rights reserved.
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

#include "Connection.h"
#include "LocalStorageDatabaseTracker.h"
#include <WebCore/SecurityOriginData.h>
#include <WebCore/StorageMap.h>
#include <wtf/Forward.h>
#include <wtf/Function.h>
#include <wtf/HashSet.h>
#include <wtf/text/StringHash.h>

namespace WebCore {
class SecurityOrigin;
}

namespace WebKit {

class LocalStorageDatabaseTracker;
class WebProcessProxy;

using GetValuesCallback = CompletionHandler<void(const HashMap<String, String>&)>;

class StorageManager : public IPC::Connection::WorkQueueMessageReceiver {
public:
    static Ref<StorageManager> create(String&& localStorageDirectory);
    ~StorageManager();

    void createSessionStorageNamespace(uint64_t storageNamespaceID, unsigned quotaInBytes);
    void destroySessionStorageNamespace(uint64_t storageNamespaceID);
    void addAllowedSessionStorageNamespaceConnection(uint64_t storageNamespaceID, IPC::Connection&);
    void removeAllowedSessionStorageNamespaceConnection(uint64_t storageNamespaceID, IPC::Connection&);
    void cloneSessionStorageNamespace(uint64_t storageNamespaceID, uint64_t newStorageNamespaceID);

    void processDidCloseConnection(IPC::Connection&);
    void waitUntilTasksFinished();
    void suspend(CompletionHandler<void()>&&);
    void resume();

    void getSessionStorageOrigins(Function<void(HashSet<WebCore::SecurityOriginData>&&)>&& completionHandler);
    void deleteSessionStorageOrigins(Function<void()>&& completionHandler);
    void deleteSessionStorageEntriesForOrigins(const Vector<WebCore::SecurityOriginData>&, Function<void()>&& completionHandler);

    void getLocalStorageOrigins(Function<void(HashSet<WebCore::SecurityOriginData>&&)>&& completionHandler);
    void deleteLocalStorageEntriesForOrigin(const WebCore::SecurityOriginData&);

    void deleteLocalStorageOriginsModifiedSince(WallTime, Function<void()>&& completionHandler);
    void deleteLocalStorageEntriesForOrigins(const Vector<WebCore::SecurityOriginData>&, Function<void()>&& completionHandler);

    void getLocalStorageOriginDetails(Function<void(Vector<LocalStorageDatabaseTracker::OriginDetails>&&)>&& completionHandler);

    void didReceiveMessage(IPC::Connection&, IPC::Decoder&);
    void didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, std::unique_ptr<IPC::Encoder>& replyEncoder);

private:
    explicit StorageManager(String&& localStorageDirectory);

    // Message handlers.
    void createLocalStorageMap(IPC::Connection&, uint64_t storageMapID, uint64_t storageNamespaceID, WebCore::SecurityOriginData&&);
    void createTransientLocalStorageMap(IPC::Connection&, uint64_t storageMapID, uint64_t storageNamespaceID, WebCore::SecurityOriginData&& topLevelOriginData, WebCore::SecurityOriginData&&);
    void createSessionStorageMap(IPC::Connection&, uint64_t storageMapID, uint64_t storageNamespaceID, WebCore::SecurityOriginData&&);
    void destroyStorageMap(IPC::Connection&, uint64_t storageMapID);

    void getValues(IPC::Connection&, uint64_t storageMapID, uint64_t storageMapSeed, GetValuesCallback&&);
    void prewarm(IPC::Connection&, uint64_t storageMapID);
    void setItem(IPC::Connection&, WebCore::SecurityOriginData&&, uint64_t storageMapID, uint64_t sourceStorageAreaID, uint64_t storageMapSeed, const String& key, const String& value, const String& urlString);
    void setItems(IPC::Connection&, uint64_t storageMapID, const HashMap<String, String>& items);
    void removeItem(IPC::Connection&, WebCore::SecurityOriginData&&, uint64_t storageMapID, uint64_t sourceStorageAreaID, uint64_t storageMapSeed, const String& key, const String& urlString);
    void clear(IPC::Connection&, WebCore::SecurityOriginData&&, uint64_t storageMapID, uint64_t sourceStorageAreaID, uint64_t storageMapSeed, const String& urlString);

    class StorageArea;
    StorageArea* findStorageArea(IPC::Connection&, uint64_t) const;

    class LocalStorageNamespace;
    LocalStorageNamespace* getOrCreateLocalStorageNamespace(uint64_t storageNamespaceID);

    class TransientLocalStorageNamespace;
    TransientLocalStorageNamespace* getOrCreateTransientLocalStorageNamespace(uint64_t storageNamespaceID, WebCore::SecurityOriginData&& topLevelOrigin);

    Ref<WorkQueue> m_queue;

    RefPtr<LocalStorageDatabaseTracker> m_localStorageDatabaseTracker;
    HashMap<uint64_t, RefPtr<LocalStorageNamespace>> m_localStorageNamespaces;

    HashMap<std::pair<uint64_t, WebCore::SecurityOriginData>, RefPtr<TransientLocalStorageNamespace>> m_transientLocalStorageNamespaces;

    class SessionStorageNamespace;
    HashMap<uint64_t, RefPtr<SessionStorageNamespace>> m_sessionStorageNamespaces;

    HashMap<std::pair<IPC::Connection::UniqueID, uint64_t>, RefPtr<StorageArea>> m_storageAreasByConnection;
    HashSet<IPC::Connection::UniqueID> m_connections;

    enum class State {
        Running,
        WillSuspend,
        Suspended
    };
    State m_state { State::Running };
    Lock m_stateLock;
    Condition m_stateChangeCondition;
};

} // namespace WebKit

/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "SandboxExtension.h"
#include "StorageAreaIdentifier.h"
#include "StorageAreaImplIdentifier.h"
#include "StorageManager.h"
#include <WebCore/SecurityOriginData.h>
#include <pal/SessionID.h>
#include <wtf/WeakPtr.h>

using WebCore::SecurityOriginData;

namespace WebKit {

class SandboxExtension;

using ConnectToStorageAreaCallback = CompletionHandler<void(const Optional<StorageAreaIdentifier>&)>;
using GetValuesCallback = CompletionHandler<void(const HashMap<String, String>&)>;
using GetOriginsCallback = CompletionHandler<void(HashSet<WebCore::SecurityOriginData>&&)>;
using GetOriginDetailsCallback = CompletionHandler<void(Vector<LocalStorageDatabaseTracker::OriginDetails>&&)>;
using DeleteCallback = CompletionHandler<void()>;

class StorageManagerSet : public IPC::Connection::WorkQueueMessageReceiver {
public:
    static Ref<StorageManagerSet> create();
    ~StorageManagerSet();

    void add(PAL::SessionID, const String& localStorageDirectory, SandboxExtension::Handle& localStorageDirectoryHandle);
    void remove(PAL::SessionID);
    bool contains(PAL::SessionID);

    void addConnection(IPC::Connection&);
    void removeConnection(IPC::Connection&);

    void waitUntilTasksFinished();
    void waitUntilSyncingLocalStorageFinished();
    void suspend(CompletionHandler<void()>&&);
    void resume();

    void getSessionStorageOrigins(PAL::SessionID, GetOriginsCallback&&);
    void deleteSessionStorage(PAL::SessionID, DeleteCallback&&);
    void deleteSessionStorageForOrigins(PAL::SessionID, const Vector<WebCore::SecurityOriginData>&, DeleteCallback&&);
    void getLocalStorageOrigins(PAL::SessionID, GetOriginsCallback&&);
    void deleteLocalStorageModifiedSince(PAL::SessionID, WallTime, DeleteCallback&&);
    void deleteLocalStorageForOrigins(PAL::SessionID, const Vector<WebCore::SecurityOriginData>&, DeleteCallback&&);
    void getLocalStorageOriginDetails(PAL::SessionID, GetOriginDetailsCallback&&);

    void didReceiveMessage(IPC::Connection&, IPC::Decoder&);
    void didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, std::unique_ptr<IPC::Encoder>& replyEncoder);

private:
    StorageManagerSet();

    // Message Handlers
    void connectToLocalStorageArea(IPC::Connection&, PAL::SessionID , StorageNamespaceIdentifier, SecurityOriginData&&, ConnectToStorageAreaCallback&&);
    void connectToTransientLocalStorageArea(IPC::Connection&, PAL::SessionID , StorageNamespaceIdentifier, SecurityOriginData&&, SecurityOriginData&&, ConnectToStorageAreaCallback&&);
    void connectToSessionStorageArea(IPC::Connection&, PAL::SessionID, StorageNamespaceIdentifier, SecurityOriginData&&, ConnectToStorageAreaCallback&&);
    void disconnectFromStorageArea(IPC::Connection&, StorageAreaIdentifier);
    void getValues(IPC::Connection&, StorageAreaIdentifier, GetValuesCallback&&);
    void setItem(IPC::Connection&, StorageAreaIdentifier, StorageAreaImplIdentifier, uint64_t storageMapSeed, const String& key, const String& value, const String& urlString);
    void removeItem(IPC::Connection&, StorageAreaIdentifier, StorageAreaImplIdentifier, uint64_t storageMapSeed, const String& key, const String& urlString);
    void clear(IPC::Connection&, StorageAreaIdentifier, StorageAreaImplIdentifier, uint64_t storageMapSeed, const String& urlString);
    void cloneSessionStorageNamespace(IPC::Connection&, PAL::SessionID, StorageNamespaceIdentifier fromStorageNamespaceID, StorageNamespaceIdentifier toStorageNamespaceID);

    HashMap<PAL::SessionID, std::unique_ptr<StorageManager>> m_storageManagers;
    HashMap<PAL::SessionID, String> m_storageManagerPaths;
    HashMap<StorageAreaIdentifier, WeakPtr<StorageArea>> m_storageAreas;

    HashSet<IPC::Connection::UniqueID> m_connections;
    Ref<WorkQueue> m_queue;

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

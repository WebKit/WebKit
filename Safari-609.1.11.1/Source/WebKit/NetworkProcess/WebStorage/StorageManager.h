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
#include "StorageAreaIdentifier.h"
#include "StorageNamespaceIdentifier.h"
#include <WebCore/SecurityOriginData.h>
#include <WebCore/StorageMap.h>
#include <wtf/Forward.h>
#include <wtf/HashSet.h>
#include <wtf/text/StringHash.h>

namespace WebCore {
class SecurityOrigin;
}

namespace WebKit {

class LocalStorageDatabaseTracker;
class LocalStorageNamespace;
class SessionStorageNamespace;
class StorageArea;
class TransientLocalStorageNamespace;
class WebProcessProxy;

using GetValuesCallback = CompletionHandler<void(const HashMap<String, String>&)>;

class StorageManager {
    WTF_MAKE_NONCOPYABLE(StorageManager);
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit StorageManager(String&& localStorageDirectory);
    ~StorageManager();

    void createSessionStorageNamespace(StorageNamespaceIdentifier, unsigned quotaInBytes);
    void destroySessionStorageNamespace(StorageNamespaceIdentifier);
    void cloneSessionStorageNamespace(StorageNamespaceIdentifier oldStorageNamespaceID, StorageNamespaceIdentifier newStorageNamespaceID);

    HashSet<WebCore::SecurityOriginData> getSessionStorageOriginsCrossThreadCopy() const;
    void deleteSessionStorageOrigins();
    void deleteSessionStorageEntriesForOrigins(const Vector<WebCore::SecurityOriginData>&);

    HashSet<WebCore::SecurityOriginData> getLocalStorageOriginsCrossThreadCopy() const;
    void deleteLocalStorageOriginsModifiedSince(WallTime);
    void deleteLocalStorageEntriesForOrigins(const Vector<WebCore::SecurityOriginData>&);
    Vector<LocalStorageDatabaseTracker::OriginDetails> getLocalStorageOriginDetailsCrossThreadCopy() const;

    void clearStorageNamespaces();

    void didReceiveMessage(IPC::Connection&, IPC::Decoder&);
    void didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, std::unique_ptr<IPC::Encoder>& replyEncoder);
    
    LocalStorageDatabaseTracker* localStorageDatabaseTracker() const { return m_localStorageDatabaseTracker.get(); }
    
    static const unsigned localStorageDatabaseQuotaInBytes;

    StorageArea* createLocalStorageArea(StorageNamespaceIdentifier, WebCore::SecurityOriginData&&, Ref<WorkQueue>&&);
    StorageArea* createTransientLocalStorageArea(StorageNamespaceIdentifier, WebCore::SecurityOriginData&&, WebCore::SecurityOriginData&&, Ref<WorkQueue>&&);
    StorageArea* createSessionStorageArea(StorageNamespaceIdentifier, WebCore::SecurityOriginData&&, Ref<WorkQueue>&&);

    Vector<StorageAreaIdentifier> allStorageAreaIdentifiers() const;

private:
    LocalStorageNamespace* getOrCreateLocalStorageNamespace(StorageNamespaceIdentifier);
    TransientLocalStorageNamespace* getOrCreateTransientLocalStorageNamespace(StorageNamespaceIdentifier, WebCore::SecurityOriginData&& topLevelOrigin);
    SessionStorageNamespace* getOrCreateSessionStorageNamespace(StorageNamespaceIdentifier);

    RefPtr<LocalStorageDatabaseTracker> m_localStorageDatabaseTracker;
    HashMap<StorageNamespaceIdentifier, std::unique_ptr<LocalStorageNamespace>> m_localStorageNamespaces;
    HashMap<std::pair<StorageNamespaceIdentifier, WebCore::SecurityOriginData>, std::unique_ptr<TransientLocalStorageNamespace>> m_transientLocalStorageNamespaces;
    HashMap<StorageNamespaceIdentifier, std::unique_ptr<SessionStorageNamespace>> m_sessionStorageNamespaces;
};

} // namespace WebKit

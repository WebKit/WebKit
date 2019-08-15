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
class LocalStorageNamespace;
class SessionStorageNamespace;
class StorageArea;
class TransientLocalStorageNamespace;
class WebProcessProxy;

using GetValuesCallback = CompletionHandler<void(const HashMap<String, String>&)>;

class StorageManager : public RefCounted<StorageManager> {
public:
    static Ref<StorageManager> create(String&& localStorageDirectory)
    {
        return adoptRef(*new StorageManager(WTFMove(localStorageDirectory)));
    }
    
    ~StorageManager();

    void createSessionStorageNamespace(uint64_t storageNamespaceID, unsigned quotaInBytes);
    void destroySessionStorageNamespace(uint64_t storageNamespaceID);
    void cloneSessionStorageNamespace(uint64_t storageNamespaceID, uint64_t newStorageNamespaceID);

    void getSessionStorageOrigins(Function<void(HashSet<WebCore::SecurityOriginData>&&)>&&);
    void deleteSessionStorageOrigins(Function<void()>&& completionHandler);
    void deleteSessionStorageEntriesForOrigins(const Vector<WebCore::SecurityOriginData>&, Function<void()>&&);

    void getLocalStorageOrigins(Function<void(HashSet<WebCore::SecurityOriginData>&&)>&&);
    void deleteLocalStorageOriginsModifiedSince(WallTime, Function<void()>&&);
    void deleteLocalStorageEntriesForOrigins(const Vector<WebCore::SecurityOriginData>&, Function<void()>&&);
    void getLocalStorageOriginDetails(Function<void(Vector<LocalStorageDatabaseTracker::OriginDetails>&&)>&&);

    void clearStorageNamespaces();

    void didReceiveMessage(IPC::Connection&, IPC::Decoder&);
    void didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, std::unique_ptr<IPC::Encoder>& replyEncoder);
    
    LocalStorageDatabaseTracker* localStorageDatabaseTracker() const { return m_localStorageDatabaseTracker.get(); }
    
    static const unsigned localStorageDatabaseQuotaInBytes;

    StorageArea* createLocalStorageArea(uint64_t storageNamespaceID, WebCore::SecurityOriginData&&);
    StorageArea* createTransientLocalStorageArea(uint64_t storageNamespaceID, WebCore::SecurityOriginData&&, WebCore::SecurityOriginData&&);
    StorageArea* createSessionStorageArea(uint64_t storageNamespaceID, WebCore::SecurityOriginData&&);

private:
    explicit StorageManager(String&& localStorageDirectory);

    LocalStorageNamespace* getOrCreateLocalStorageNamespace(uint64_t storageNamespaceID);
    TransientLocalStorageNamespace* getOrCreateTransientLocalStorageNamespace(uint64_t storageNamespaceID, WebCore::SecurityOriginData&& topLevelOrigin);
    SessionStorageNamespace* getOrCreateSessionStorageNamespace(uint64_t storageNamespaceID);

    RefPtr<LocalStorageDatabaseTracker> m_localStorageDatabaseTracker;
    HashMap<uint64_t, RefPtr<LocalStorageNamespace>> m_localStorageNamespaces;
    HashMap<std::pair<uint64_t, WebCore::SecurityOriginData>, RefPtr<TransientLocalStorageNamespace>> m_transientLocalStorageNamespaces;
    HashMap<uint64_t, RefPtr<SessionStorageNamespace>> m_sessionStorageNamespaces;
};

} // namespace WebKit

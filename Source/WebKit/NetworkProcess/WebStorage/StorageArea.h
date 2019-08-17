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
#include "StorageAreaIdentifier.h"
#include "StorageAreaImplIdentifier.h"
#include <WebCore/SecurityOriginData.h>
#include <wtf/Forward.h>

namespace WebCore {
class StorageMap;
}

namespace WebKit {

class LocalStorageDatabase;
class LocalStorageNamespace;

class StorageArea {
    WTF_MAKE_NONCOPYABLE(StorageArea);
    WTF_MAKE_FAST_ALLOCATED;
public:
    using Identifier = StorageAreaIdentifier;
    
    StorageArea(LocalStorageNamespace*, const WebCore::SecurityOriginData&, unsigned quotaInBytes, Ref<WorkQueue>&&);
    ~StorageArea();

    const WebCore::SecurityOriginData& securityOrigin() const { return m_securityOrigin; }
    Identifier identifier() { return m_identifier; }

    void addListener(IPC::Connection::UniqueID);
    void removeListener(IPC::Connection::UniqueID);
    bool hasListener(IPC::Connection::UniqueID connectionID) const;

    std::unique_ptr<StorageArea> clone() const;

    void setItem(IPC::Connection::UniqueID sourceConnection, StorageAreaImplIdentifier, const String& key, const String& value, const String& urlString, bool& quotaException);
    void removeItem(IPC::Connection::UniqueID sourceConnection, StorageAreaImplIdentifier, const String& key, const String& urlString);
    void clear(IPC::Connection::UniqueID sourceConnection, StorageAreaImplIdentifier, const String& urlString);

    const HashMap<String, String>& items() const;
    void clear();

    bool isEphemeral() const { return !m_localStorageNamespace; }

    void openDatabaseAndImportItemsIfNeeded() const;

    void syncToDatabase();

private:
    void dispatchEvents(IPC::Connection::UniqueID sourceConnection, StorageAreaImplIdentifier, const String& key, const String& oldValue, const String& newValue, const String& urlString) const;

    // Will be null if the storage area belongs to a session storage namespace or the storage area is in an ephemeral session.
    WeakPtr<LocalStorageNamespace> m_localStorageNamespace;
    mutable RefPtr<LocalStorageDatabase> m_localStorageDatabase;
    mutable bool m_didImportItemsFromDatabase { false };

    WebCore::SecurityOriginData m_securityOrigin;
    unsigned m_quotaInBytes { 0 };

    RefPtr<WebCore::StorageMap> m_storageMap;
    HashSet<IPC::Connection::UniqueID> m_eventListeners;

    Identifier m_identifier;
    Ref<WorkQueue> m_queue;
};

} // namespace WebKit

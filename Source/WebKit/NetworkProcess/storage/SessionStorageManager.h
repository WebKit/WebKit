/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "StorageAreaMapIdentifier.h"
#include "StorageNamespaceIdentifier.h"

namespace WebCore {
struct ClientOrigin;
} // namespace WebCore

namespace WebKit {

class MemoryStorageArea;
class StorageAreaRegistry;

class SessionStorageManager {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit SessionStorageManager(StorageAreaRegistry&);
    bool isActive() const;
    bool hasDataInMemory() const;
    void clearData();
    void connectionClosed(IPC::Connection::UniqueID);
    void removeNamespace(StorageNamespaceIdentifier);

    StorageAreaIdentifier connectToSessionStorageArea(IPC::Connection::UniqueID, StorageAreaMapIdentifier, const WebCore::ClientOrigin&, StorageNamespaceIdentifier);
    void cancelConnectToSessionStorageArea(IPC::Connection::UniqueID, StorageNamespaceIdentifier);
    void disconnectFromStorageArea(IPC::Connection::UniqueID, StorageAreaIdentifier);
    void cloneStorageArea(StorageNamespaceIdentifier, StorageNamespaceIdentifier);

private:
    StorageAreaIdentifier addStorageArea(std::unique_ptr<MemoryStorageArea>, StorageNamespaceIdentifier);

    StorageAreaRegistry& m_registry;
    HashMap<StorageAreaIdentifier, std::unique_ptr<MemoryStorageArea>> m_storageAreas;
    HashMap<StorageNamespaceIdentifier, StorageAreaIdentifier> m_storageAreasByNamespace;
};

} // namespace WebKit

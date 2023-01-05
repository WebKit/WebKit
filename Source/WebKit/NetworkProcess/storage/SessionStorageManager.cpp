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

#include "config.h"
#include "SessionStorageManager.h"

#include "MemoryStorageArea.h"
#include "StorageAreaRegistry.h"

namespace WebKit {

SessionStorageManager::SessionStorageManager(StorageAreaRegistry& registry)
    : m_registry(registry)
{
}

bool SessionStorageManager::isActive() const
{
    return WTF::anyOf(m_storageAreas.values(), [&] (auto& storageArea) {
        return storageArea->hasListeners();
    });
}

bool SessionStorageManager::hasDataInMemory() const
{
    return WTF::anyOf(m_storageAreas.values(), [&] (auto& storageArea) {
        return !storageArea->isEmpty();
    });
}

void SessionStorageManager::clearData()
{
    for (auto& storageArea : m_storageAreas.values())
        storageArea->clear();
}

void SessionStorageManager::connectionClosed(IPC::Connection::UniqueID connection)
{
    for (auto& storageArea : m_storageAreas.values())
        storageArea->removeListener(connection);
}

void SessionStorageManager::removeNamespace(StorageNamespaceIdentifier namespaceIdentifier)
{
    auto identifier = m_storageAreasByNamespace.take(namespaceIdentifier);
    if (!identifier.isValid())
        return;

    m_storageAreas.remove(identifier);
    m_registry.unregisterStorageArea(identifier);
}

StorageAreaIdentifier SessionStorageManager::addStorageArea(std::unique_ptr<MemoryStorageArea> storageArea, StorageNamespaceIdentifier namespaceIdentifier)
{
    auto identifier = storageArea->identifier();
    m_registry.registerStorageArea(identifier, *storageArea);
    m_storageAreasByNamespace.add(namespaceIdentifier, identifier);
    m_storageAreas.add(identifier, WTFMove(storageArea));

    return identifier;
}

StorageAreaIdentifier SessionStorageManager::connectToSessionStorageArea(IPC::Connection::UniqueID connection, StorageAreaMapIdentifier sourceIdentifier, const WebCore::ClientOrigin& origin, StorageNamespaceIdentifier namespaceIdentifier)
{
    auto identifier = m_storageAreasByNamespace.get(namespaceIdentifier);
    if (!identifier.isValid()) {
        auto newStorageArea = makeUnique<MemoryStorageArea>(origin);
        identifier = addStorageArea(WTFMove(newStorageArea), namespaceIdentifier);
    }

    auto storageArea = m_storageAreas.get(identifier);
    if (!storageArea)
        return StorageAreaIdentifier { };

    storageArea->addListener(connection, sourceIdentifier);

    return identifier;
}

void SessionStorageManager::cancelConnectToSessionStorageArea(IPC::Connection::UniqueID connection, StorageNamespaceIdentifier namespaceIdentifier)
{
    auto identifier = m_storageAreasByNamespace.get(namespaceIdentifier);
    if (!identifier.isValid())
        return;

    auto* storageArea = m_storageAreas.get(identifier);
    if (!storageArea)
        return;

    storageArea->removeListener(connection);
}

void SessionStorageManager::disconnectFromStorageArea(IPC::Connection::UniqueID connection, StorageAreaIdentifier identifier)
{
    if (auto* storageArea = m_storageAreas.get(identifier))
        storageArea->removeListener(connection);
}

void SessionStorageManager::cloneStorageArea(StorageNamespaceIdentifier sourceNamespaceIdentifier, StorageNamespaceIdentifier targetNamespaceIdentifier)
{
    auto identifier = m_storageAreasByNamespace.get(sourceNamespaceIdentifier);
    if (!identifier.isValid())
        return;

    if (auto storageArea = m_storageAreas.get(identifier))
        addStorageArea(storageArea->clone(), targetNamespaceIdentifier);
}

} // namespace WebKit

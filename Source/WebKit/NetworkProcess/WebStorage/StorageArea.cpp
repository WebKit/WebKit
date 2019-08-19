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

#include "config.h"
#include "StorageArea.h"

#include "LocalStorageDatabase.h"
#include "LocalStorageNamespace.h"
#include "StorageAreaMapMessages.h"
#include "StorageManager.h"
#include <WebCore/StorageMap.h>

namespace WebKit {

using namespace WebCore;

StorageArea::StorageArea(LocalStorageNamespace* localStorageNamespace, const SecurityOriginData& securityOrigin, unsigned quotaInBytes, Ref<WorkQueue>&& queue)
    : m_localStorageNamespace(makeWeakPtr(localStorageNamespace))
    , m_securityOrigin(securityOrigin)
    , m_quotaInBytes(quotaInBytes)
    , m_storageMap(StorageMap::create(m_quotaInBytes))
    , m_identifier(Identifier::generate())
    , m_queue(WTFMove(queue))
{
    ASSERT(!RunLoop::isMain());
}

StorageArea::~StorageArea()
{
    ASSERT(!RunLoop::isMain());

    if (m_localStorageDatabase)
        m_localStorageDatabase->close();
}

void StorageArea::addListener(IPC::Connection::UniqueID connectionID)
{
    ASSERT(!RunLoop::isMain());
    ASSERT(!m_eventListeners.contains(connectionID));

    if (m_eventListeners.isEmpty() && !isEphemeral())
        openDatabaseAndImportItemsIfNeeded();

    m_eventListeners.add(connectionID);
}

void StorageArea::removeListener(IPC::Connection::UniqueID connectionID)
{
    ASSERT(!RunLoop::isMain());
    m_eventListeners.remove(connectionID);
}

bool StorageArea::hasListener(IPC::Connection::UniqueID connectionID) const
{
    ASSERT(!RunLoop::isMain());
    return m_eventListeners.contains(connectionID);
}

std::unique_ptr<StorageArea> StorageArea::clone() const
{
    ASSERT(!RunLoop::isMain());
    ASSERT(!m_localStorageNamespace);

    auto storageArea = makeUnique<StorageArea>(nullptr, m_securityOrigin, m_quotaInBytes, m_queue.copyRef());
    storageArea->m_storageMap = m_storageMap;

    return storageArea;
}

void StorageArea::setItem(IPC::Connection::UniqueID sourceConnection, StorageAreaImplIdentifier storageAreaImplID, const String& key, const String& value, const String& urlString, bool& quotaException)
{
    ASSERT(!RunLoop::isMain());
    openDatabaseAndImportItemsIfNeeded();

    String oldValue;

    auto newStorageMap = m_storageMap->setItem(key, value, oldValue, quotaException);
    if (newStorageMap)
        m_storageMap = WTFMove(newStorageMap);

    if (quotaException)
        return;

    if (m_localStorageDatabase)
        m_localStorageDatabase->setItem(key, value);

    dispatchEvents(sourceConnection, storageAreaImplID, key, oldValue, value, urlString);
}

void StorageArea::removeItem(IPC::Connection::UniqueID sourceConnection, StorageAreaImplIdentifier storageAreaImplID, const String& key, const String& urlString)
{
    ASSERT(!RunLoop::isMain());
    openDatabaseAndImportItemsIfNeeded();

    String oldValue;
    auto newStorageMap = m_storageMap->removeItem(key, oldValue);
    if (newStorageMap)
        m_storageMap = WTFMove(newStorageMap);

    if (oldValue.isNull())
        return;

    if (m_localStorageDatabase)
        m_localStorageDatabase->removeItem(key);

    dispatchEvents(sourceConnection, storageAreaImplID, key, oldValue, String(), urlString);
}

void StorageArea::clear(IPC::Connection::UniqueID sourceConnection, StorageAreaImplIdentifier storageAreaImplID, const String& urlString)
{
    ASSERT(!RunLoop::isMain());
    openDatabaseAndImportItemsIfNeeded();

    if (!m_storageMap->length())
        return;

    m_storageMap = StorageMap::create(m_quotaInBytes);

    if (m_localStorageDatabase)
        m_localStorageDatabase->clear();

    dispatchEvents(sourceConnection, storageAreaImplID, String(), String(), String(), urlString);
}

const HashMap<String, String>& StorageArea::items() const
{
    ASSERT(!RunLoop::isMain());
    openDatabaseAndImportItemsIfNeeded();

    return m_storageMap->items();
}

void StorageArea::clear()
{
    ASSERT(!RunLoop::isMain());
    m_storageMap = StorageMap::create(m_quotaInBytes);

    if (m_localStorageDatabase) {
        m_localStorageDatabase->close();
        m_localStorageDatabase = nullptr;
    }

    for (auto it = m_eventListeners.begin(), end = m_eventListeners.end(); it != end; ++it) {
        RunLoop::main().dispatch([connectionID = *it, destinationStorageAreaID = m_identifier] {
            if (auto* connection = IPC::Connection::connection(connectionID))
                connection->send(Messages::StorageAreaMap::ClearCache(), destinationStorageAreaID);
        });
    }
}

void StorageArea::openDatabaseAndImportItemsIfNeeded() const
{
    ASSERT(!RunLoop::isMain());
    if (!m_localStorageNamespace)
        return;

    ASSERT(m_localStorageNamespace->storageManager()->localStorageDatabaseTracker());
    // We open the database here even if we've already imported our items to ensure that the database is open if we need to write to it.
    if (!m_localStorageDatabase)
        m_localStorageDatabase = LocalStorageDatabase::create(m_queue.copyRef(), *m_localStorageNamespace->storageManager()->localStorageDatabaseTracker(), m_securityOrigin);

    if (m_didImportItemsFromDatabase)
        return;

    m_localStorageDatabase->importItems(*m_storageMap);
    m_didImportItemsFromDatabase = true;
}

void StorageArea::dispatchEvents(IPC::Connection::UniqueID sourceConnection, StorageAreaImplIdentifier storageAreaImplID, const String& key, const String& oldValue, const String& newValue, const String& urlString) const
{
    ASSERT(!RunLoop::isMain());
    ASSERT(storageAreaImplID);
    for (auto it = m_eventListeners.begin(), end = m_eventListeners.end(); it != end; ++it) {
        Optional<StorageAreaImplIdentifier> optionalStorageAreaImplID = *it == sourceConnection ? makeOptional(storageAreaImplID) : WTF::nullopt;

        RunLoop::main().dispatch([connectionID = *it, destinationStorageAreaID = m_identifier, key = key.isolatedCopy(), oldValue = oldValue.isolatedCopy(), newValue = newValue.isolatedCopy(), urlString = urlString.isolatedCopy(), optionalStorageAreaImplID = WTFMove(optionalStorageAreaImplID)] {
            if (auto* connection = IPC::Connection::connection(connectionID))
                connection->send(Messages::StorageAreaMap::DispatchStorageEvent(optionalStorageAreaImplID, key, oldValue, newValue, urlString), destinationStorageAreaID);
        });
    }
}

void StorageArea::syncToDatabase()
{
    if (!m_localStorageDatabase)
        return;

    m_localStorageDatabase->updateDatabase();
}

} // namespace WebKit

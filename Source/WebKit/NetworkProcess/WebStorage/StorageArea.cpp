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
#include <wtf/SuspendableWorkQueue.h>

namespace WebKit {

using namespace WebCore;

StorageArea::StorageArea(LocalStorageNamespace* localStorageNamespace, const SecurityOriginData& securityOrigin, unsigned quotaInBytes, Ref<SuspendableWorkQueue>&& queue)
    : m_localStorageNamespace(localStorageNamespace)
    , m_securityOrigin(securityOrigin)
    , m_quotaInBytes(quotaInBytes)
    , m_identifier(Identifier::generate())
    , m_queue(WTFMove(queue))
{
    ASSERT(!RunLoop::isMain());
    if (isEphemeral())
        m_sessionStorageMap = makeUnique<StorageMap>(m_quotaInBytes);
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
        ensureDatabase();

    m_eventListeners.add(connectionID);
}

void StorageArea::removeListener(IPC::Connection::UniqueID connectionID)
{
    ASSERT(!RunLoop::isMain());
    m_eventListeners.remove(connectionID);

    if (!m_eventListeners.isEmpty())
        return;

    if (m_localStorageNamespace)
        m_localStorageNamespace->removeStorageArea(m_securityOrigin);
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
    *storageArea->m_sessionStorageMap = *m_sessionStorageMap;

    return storageArea;
}

void StorageArea::setItem(IPC::Connection::UniqueID sourceConnection, StorageAreaImplIdentifier storageAreaImplID, const String& key, const String& value, const String& urlString, bool& quotaException)
{
    ASSERT(!RunLoop::isMain());

    String oldValue;
    if (isEphemeral())
        m_sessionStorageMap->setItem(key, value, oldValue, quotaException);
    else
        ensureDatabase().setItem(key, value, oldValue, quotaException);

    if (quotaException)
        return;

    dispatchEvents(sourceConnection, storageAreaImplID, key, oldValue, value, urlString);
}

void StorageArea::removeItem(IPC::Connection::UniqueID sourceConnection, StorageAreaImplIdentifier storageAreaImplID, const String& key, const String& urlString)
{
    ASSERT(!RunLoop::isMain());

    String oldValue;
    if (isEphemeral())
        m_sessionStorageMap->removeItem(key, oldValue);
    else
        ensureDatabase().removeItem(key, oldValue);
    if (oldValue.isNull())
        return;

    dispatchEvents(sourceConnection, storageAreaImplID, key, oldValue, String(), urlString);
}

void StorageArea::clear(IPC::Connection::UniqueID sourceConnection, StorageAreaImplIdentifier storageAreaImplID, const String& urlString)
{
    ASSERT(!RunLoop::isMain());

    if (isEphemeral()) {
        if (!m_sessionStorageMap->length())
            return;
        m_sessionStorageMap->clear();
    } else {
        if (!ensureDatabase().clear())
            return;
    }

    dispatchEvents(sourceConnection, storageAreaImplID, String(), String(), String(), urlString);
}

HashMap<String, String> StorageArea::items() const
{
    ASSERT(!RunLoop::isMain());
    if (isEphemeral())
        return m_sessionStorageMap->items();

    return ensureDatabase().items();
}

void StorageArea::clear()
{
    ASSERT(!RunLoop::isMain());
    if (isEphemeral())
        m_sessionStorageMap->clear();
    else {
        if (m_localStorageDatabase) {
            m_localStorageDatabase->close();
            m_localStorageDatabase = nullptr;
        }
    }

    for (auto& listenerUniqueID : m_eventListeners)
        IPC::Connection::send(listenerUniqueID, Messages::StorageAreaMap::ClearCache(0), m_identifier.toUInt64());
}

LocalStorageDatabase& StorageArea::ensureDatabase() const
{
    ASSERT(!isEphemeral());

    ASSERT(m_localStorageNamespace->storageManager()->localStorageDatabaseTracker());
    // We open the database here even if we've already imported our items to ensure that the database is open if we need to write to it.
    if (!m_localStorageDatabase) {
        auto* localStorageDatabaseTracker = m_localStorageNamespace->storageManager()->localStorageDatabaseTracker();
        m_localStorageDatabase = LocalStorageDatabase::create(m_queue.copyRef(), localStorageDatabaseTracker->databasePath(m_securityOrigin), m_quotaInBytes);
        m_localStorageDatabase->openIfExisting();
    }
    return *m_localStorageDatabase;
}

void StorageArea::dispatchEvents(IPC::Connection::UniqueID sourceConnection, StorageAreaImplIdentifier storageAreaImplID, const String& key, const String& oldValue, const String& newValue, const String& urlString) const
{
    ASSERT(!RunLoop::isMain());
    ASSERT(storageAreaImplID);
    for (auto& listenerUniqueID : m_eventListeners) {
        auto optionalStorageAreaImplID = listenerUniqueID == sourceConnection ? std::make_optional(storageAreaImplID) : std::nullopt;
        IPC::Connection::send(listenerUniqueID, Messages::StorageAreaMap::DispatchStorageEvent(optionalStorageAreaImplID, key, oldValue, newValue, urlString, 0), m_identifier.toUInt64());
    }
}

void StorageArea::close()
{
    if (!m_localStorageDatabase)
        return;

    m_localStorageDatabase->close();
}

void StorageArea::syncToDatabase()
{
    if (m_localStorageDatabase)
        m_localStorageDatabase->flushToDisk();
}

void StorageArea::handleLowMemoryWarning()
{
    if (m_localStorageDatabase)
        m_localStorageDatabase->handleLowMemoryWarning();
}

} // namespace WebKit

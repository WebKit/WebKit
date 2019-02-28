/*
 * Copyright (C) 2013-2016 Apple Inc. All rights reserved.
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
#include "StorageManager.h"

#include "LocalStorageDatabase.h"
#include "LocalStorageDatabaseTracker.h"
#include "StorageAreaMapMessages.h"
#include "StorageManagerMessages.h"
#include "WebProcessProxy.h"
#include <WebCore/SecurityOriginData.h>
#include <WebCore/SecurityOriginHash.h>
#include <WebCore/StorageMap.h>
#include <WebCore/TextEncoding.h>
#include <memory>
#include <wtf/WorkQueue.h>
#include <wtf/threads/BinarySemaphore.h>

namespace WebKit {
using namespace WebCore;

class StorageManager::StorageArea : public ThreadSafeRefCounted<StorageManager::StorageArea> {
public:
    static Ref<StorageArea> create(LocalStorageNamespace*, const SecurityOriginData&, unsigned quotaInBytes);
    ~StorageArea();

    const WebCore::SecurityOriginData& securityOrigin() const { return m_securityOrigin; }

    void addListener(IPC::Connection::UniqueID, uint64_t storageMapID);
    void removeListener(IPC::Connection::UniqueID, uint64_t storageMapID);
    bool hasListener(IPC::Connection::UniqueID, uint64_t storageMapID) const;

    Ref<StorageArea> clone() const;

    void setItem(IPC::Connection::UniqueID sourceConnection, uint64_t sourceStorageAreaID, const String& key, const String& value, const String& urlString, bool& quotaException);
    void removeItem(IPC::Connection::UniqueID sourceConnection, uint64_t sourceStorageAreaID, const String& key, const String& urlString);
    void clear(IPC::Connection::UniqueID sourceConnection, uint64_t sourceStorageAreaID, const String& urlString);

    const HashMap<String, String>& items() const;
    void clear();

    bool isSessionStorage() const { return !m_localStorageNamespace; }

private:
    explicit StorageArea(LocalStorageNamespace*, const SecurityOriginData&, unsigned quotaInBytes);

    void openDatabaseAndImportItemsIfNeeded() const;

    void dispatchEvents(IPC::Connection::UniqueID sourceConnection, uint64_t sourceStorageAreaID, const String& key, const String& oldValue, const String& newValue, const String& urlString) const;

    // Will be null if the storage area belongs to a session storage namespace.
    LocalStorageNamespace* m_localStorageNamespace;
    mutable RefPtr<LocalStorageDatabase> m_localStorageDatabase;
    mutable bool m_didImportItemsFromDatabase { false };

    SecurityOriginData m_securityOrigin;
    unsigned m_quotaInBytes;

    RefPtr<StorageMap> m_storageMap;
    HashSet<std::pair<IPC::Connection::UniqueID, uint64_t>> m_eventListeners;
};

class StorageManager::LocalStorageNamespace : public ThreadSafeRefCounted<LocalStorageNamespace> {
public:
    static Ref<LocalStorageNamespace> create(StorageManager*, uint64_t storageManagerID);
    ~LocalStorageNamespace();

    StorageManager* storageManager() const { return m_storageManager; }

    Ref<StorageArea> getOrCreateStorageArea(SecurityOriginData&&);
    void didDestroyStorageArea(StorageArea*);

    void clearStorageAreasMatchingOrigin(const SecurityOriginData&);
    void clearAllStorageAreas();

private:
    explicit LocalStorageNamespace(StorageManager*, uint64_t storageManagerID);

    StorageManager* m_storageManager;
    uint64_t m_storageNamespaceID;
    unsigned m_quotaInBytes;

    // We don't hold an explicit reference to the StorageAreas; they are kept alive by the m_storageAreasByConnection map in StorageManager.
    HashMap<SecurityOriginData, StorageArea*> m_storageAreaMap;
};

// Suggested by https://www.w3.org/TR/webstorage/#disk-space
const unsigned localStorageDatabaseQuotaInBytes = 5 * 1024 * 1024;

class StorageManager::TransientLocalStorageNamespace : public ThreadSafeRefCounted<TransientLocalStorageNamespace> {
public:
    static Ref<TransientLocalStorageNamespace> create()
    {
        return adoptRef(*new TransientLocalStorageNamespace());
    }

    ~TransientLocalStorageNamespace()
    {
    }

    Ref<StorageArea> getOrCreateStorageArea(SecurityOriginData&& securityOrigin)
    {
        return *m_storageAreaMap.ensure(securityOrigin, [this, securityOrigin]() mutable {
            return StorageArea::create(nullptr, WTFMove(securityOrigin), m_quotaInBytes);
        }).iterator->value.copyRef();
    }

    Vector<SecurityOriginData> origins() const
    {
        Vector<SecurityOriginData> origins;

        for (const auto& storageArea : m_storageAreaMap.values()) {
            if (!storageArea->items().isEmpty())
                origins.append(storageArea->securityOrigin());
        }

        return origins;
    }

    void clearStorageAreasMatchingOrigin(const SecurityOriginData& securityOrigin)
    {
        for (auto& storageArea : m_storageAreaMap.values()) {
            if (storageArea->securityOrigin() == securityOrigin)
                storageArea->clear();
        }
    }

    void clearAllStorageAreas()
    {
        for (auto& storageArea : m_storageAreaMap.values())
            storageArea->clear();
    }

private:
    explicit TransientLocalStorageNamespace()
    {
    }

    const unsigned m_quotaInBytes = localStorageDatabaseQuotaInBytes;

    HashMap<SecurityOriginData, RefPtr<StorageArea>> m_storageAreaMap;
};

auto StorageManager::StorageArea::create(LocalStorageNamespace* localStorageNamespace, const SecurityOriginData& securityOrigin, unsigned quotaInBytes) -> Ref<StorageManager::StorageArea>
{
    return adoptRef(*new StorageArea(localStorageNamespace, securityOrigin, quotaInBytes));
}

StorageManager::StorageArea::StorageArea(LocalStorageNamespace* localStorageNamespace, const SecurityOriginData& securityOrigin, unsigned quotaInBytes)
    : m_localStorageNamespace(localStorageNamespace)
    , m_securityOrigin(securityOrigin)
    , m_quotaInBytes(quotaInBytes)
    , m_storageMap(StorageMap::create(m_quotaInBytes))
{
}

StorageManager::StorageArea::~StorageArea()
{
    ASSERT(m_eventListeners.isEmpty());

    if (m_localStorageDatabase)
        m_localStorageDatabase->close();

    if (m_localStorageNamespace)
        m_localStorageNamespace->didDestroyStorageArea(this);
}

void StorageManager::StorageArea::addListener(IPC::Connection::UniqueID connectionID, uint64_t storageMapID)
{
    ASSERT(!m_eventListeners.contains(std::make_pair(connectionID, storageMapID)));
    m_eventListeners.add(std::make_pair(connectionID, storageMapID));
}

void StorageManager::StorageArea::removeListener(IPC::Connection::UniqueID connectionID, uint64_t storageMapID)
{
    ASSERT(isSessionStorage() || m_eventListeners.contains(std::make_pair(connectionID, storageMapID)));
    m_eventListeners.remove(std::make_pair(connectionID, storageMapID));
}

bool StorageManager::StorageArea::hasListener(IPC::Connection::UniqueID connectionID, uint64_t storageMapID) const
{
    return m_eventListeners.contains(std::make_pair(connectionID, storageMapID));
}

Ref<StorageManager::StorageArea> StorageManager::StorageArea::clone() const
{
    ASSERT(!m_localStorageNamespace);

    auto storageArea = StorageArea::create(nullptr, m_securityOrigin, m_quotaInBytes);
    storageArea->m_storageMap = m_storageMap;

    return storageArea;
}

void StorageManager::StorageArea::setItem(IPC::Connection::UniqueID sourceConnection, uint64_t sourceStorageAreaID, const String& key, const String& value, const String& urlString, bool& quotaException)
{
    openDatabaseAndImportItemsIfNeeded();

    String oldValue;

    auto newStorageMap = m_storageMap->setItem(key, value, oldValue, quotaException);
    if (newStorageMap)
        m_storageMap = WTFMove(newStorageMap);

    if (quotaException)
        return;

    if (m_localStorageDatabase)
        m_localStorageDatabase->setItem(key, value);

    dispatchEvents(sourceConnection, sourceStorageAreaID, key, oldValue, value, urlString);
}

void StorageManager::StorageArea::removeItem(IPC::Connection::UniqueID sourceConnection, uint64_t sourceStorageAreaID, const String& key, const String& urlString)
{
    openDatabaseAndImportItemsIfNeeded();

    String oldValue;
    auto newStorageMap = m_storageMap->removeItem(key, oldValue);
    if (newStorageMap)
        m_storageMap = WTFMove(newStorageMap);

    if (oldValue.isNull())
        return;

    if (m_localStorageDatabase)
        m_localStorageDatabase->removeItem(key);

    dispatchEvents(sourceConnection, sourceStorageAreaID, key, oldValue, String(), urlString);
}

void StorageManager::StorageArea::clear(IPC::Connection::UniqueID sourceConnection, uint64_t sourceStorageAreaID, const String& urlString)
{
    openDatabaseAndImportItemsIfNeeded();

    if (!m_storageMap->length())
        return;

    m_storageMap = StorageMap::create(m_quotaInBytes);

    if (m_localStorageDatabase)
        m_localStorageDatabase->clear();

    dispatchEvents(sourceConnection, sourceStorageAreaID, String(), String(), String(), urlString);
}

const HashMap<String, String>& StorageManager::StorageArea::items() const
{
    openDatabaseAndImportItemsIfNeeded();

    return m_storageMap->items();
}

void StorageManager::StorageArea::clear()
{
    m_storageMap = StorageMap::create(m_quotaInBytes);

    if (m_localStorageDatabase) {
        m_localStorageDatabase->close();
        m_localStorageDatabase = nullptr;
    }

    for (auto it = m_eventListeners.begin(), end = m_eventListeners.end(); it != end; ++it) {
        RunLoop::main().dispatch([connectionID = it->first, destinationStorageAreaID = it->second] {
            if (auto* connection = IPC::Connection::connection(connectionID))
                connection->send(Messages::StorageAreaMap::ClearCache(), destinationStorageAreaID);
        });
    }
}

void StorageManager::StorageArea::openDatabaseAndImportItemsIfNeeded() const
{
    if (!m_localStorageNamespace)
        return;

    // We open the database here even if we've already imported our items to ensure that the database is open if we need to write to it.
    if (!m_localStorageDatabase)
        m_localStorageDatabase = LocalStorageDatabase::create(m_localStorageNamespace->storageManager()->m_queue.copyRef(), m_localStorageNamespace->storageManager()->m_localStorageDatabaseTracker.copyRef(), m_securityOrigin);

    if (m_didImportItemsFromDatabase)
        return;

    m_localStorageDatabase->importItems(*m_storageMap);
    m_didImportItemsFromDatabase = true;
}

void StorageManager::StorageArea::dispatchEvents(IPC::Connection::UniqueID sourceConnection, uint64_t sourceStorageAreaID, const String& key, const String& oldValue, const String& newValue, const String& urlString) const
{
    for (auto it = m_eventListeners.begin(), end = m_eventListeners.end(); it != end; ++it) {
        sourceStorageAreaID = it->first == sourceConnection ? sourceStorageAreaID : 0;

        RunLoop::main().dispatch([connectionID = it->first, sourceStorageAreaID, destinationStorageAreaID = it->second, key = key.isolatedCopy(), oldValue = oldValue.isolatedCopy(), newValue = newValue.isolatedCopy(), urlString = urlString.isolatedCopy()] {
            if (auto* connection = IPC::Connection::connection(connectionID))
                connection->send(Messages::StorageAreaMap::DispatchStorageEvent(sourceStorageAreaID, key, oldValue, newValue, urlString), destinationStorageAreaID);
        });
    }
}

Ref<StorageManager::LocalStorageNamespace> StorageManager::LocalStorageNamespace::create(StorageManager* storageManager, uint64_t storageNamespaceID)
{
    return adoptRef(*new LocalStorageNamespace(storageManager, storageNamespaceID));
}

// FIXME: The quota value is copied from GroupSettings.cpp.
// We should investigate a way to share it with WebCore.
StorageManager::LocalStorageNamespace::LocalStorageNamespace(StorageManager* storageManager, uint64_t storageNamespaceID)
    : m_storageManager(storageManager)
    , m_storageNamespaceID(storageNamespaceID)
    , m_quotaInBytes(localStorageDatabaseQuotaInBytes)
{
}

StorageManager::LocalStorageNamespace::~LocalStorageNamespace()
{
    ASSERT(m_storageAreaMap.isEmpty());
}

auto StorageManager::LocalStorageNamespace::getOrCreateStorageArea(SecurityOriginData&& securityOrigin) -> Ref<StorageArea>
{
    auto& slot = m_storageAreaMap.add(securityOrigin, nullptr).iterator->value;
    if (slot)
        return *slot;

    auto storageArea = StorageArea::create(this, WTFMove(securityOrigin), m_quotaInBytes);
    slot = &storageArea.get();

    return storageArea;
}

void StorageManager::LocalStorageNamespace::didDestroyStorageArea(StorageArea* storageArea)
{
    ASSERT(m_storageAreaMap.contains(storageArea->securityOrigin()));

    m_storageAreaMap.remove(storageArea->securityOrigin());
    if (!m_storageAreaMap.isEmpty())
        return;

    ASSERT(m_storageManager->m_localStorageNamespaces.contains(m_storageNamespaceID));
    m_storageManager->m_localStorageNamespaces.remove(m_storageNamespaceID);
}

void StorageManager::LocalStorageNamespace::clearStorageAreasMatchingOrigin(const SecurityOriginData& securityOrigin)
{
    for (const auto& originAndStorageArea : m_storageAreaMap) {
        if (originAndStorageArea.key == securityOrigin)
            originAndStorageArea.value->clear();
    }
}

void StorageManager::LocalStorageNamespace::clearAllStorageAreas()
{
    for (auto* storageArea : m_storageAreaMap.values())
        storageArea->clear();
}

class StorageManager::SessionStorageNamespace : public ThreadSafeRefCounted<SessionStorageNamespace> {
public:
    static Ref<SessionStorageNamespace> create(unsigned quotaInBytes);
    ~SessionStorageNamespace();

    bool isEmpty() const { return m_storageAreaMap.isEmpty(); }

    Vector<IPC::Connection::UniqueID> allowedConnections() const { return m_allowedConnections; }
    void addAllowedConnection(IPC::Connection::UniqueID);
    void removeAllowedConnection(IPC::Connection::UniqueID);

    Ref<StorageArea> getOrCreateStorageArea(SecurityOriginData&&);

    void cloneTo(SessionStorageNamespace& newSessionStorageNamespace);

    Vector<SecurityOriginData> origins() const
    {
        Vector<SecurityOriginData> origins;

        for (const auto& storageArea : m_storageAreaMap.values()) {
            if (!storageArea->items().isEmpty())
                origins.append(storageArea->securityOrigin());
        }

        return origins;
    }

    void clearStorageAreasMatchingOrigin(const SecurityOriginData& securityOrigin)
    {
        for (auto& storageArea : m_storageAreaMap.values()) {
            if (storageArea->securityOrigin() == securityOrigin)
                storageArea->clear();
        }
    }

    void clearAllStorageAreas()
    {
        for (auto& storageArea : m_storageAreaMap.values())
            storageArea->clear();
    }

private:
    explicit SessionStorageNamespace(unsigned quotaInBytes);

    Vector<IPC::Connection::UniqueID> m_allowedConnections;
    unsigned m_quotaInBytes;

    HashMap<SecurityOriginData, RefPtr<StorageArea>> m_storageAreaMap;
};

Ref<StorageManager::SessionStorageNamespace> StorageManager::SessionStorageNamespace::create(unsigned quotaInBytes)
{
    return adoptRef(*new SessionStorageNamespace(quotaInBytes));
}

StorageManager::SessionStorageNamespace::SessionStorageNamespace(unsigned quotaInBytes)
    : m_quotaInBytes(quotaInBytes)
{
}

StorageManager::SessionStorageNamespace::~SessionStorageNamespace()
{
}

void StorageManager::SessionStorageNamespace::addAllowedConnection(IPC::Connection::UniqueID allowedConnection)
{
    ASSERT(!m_allowedConnections.contains(allowedConnection));
    m_allowedConnections.append(allowedConnection);
}


void StorageManager::SessionStorageNamespace::removeAllowedConnection(IPC::Connection::UniqueID allowedConnection)
{
    ASSERT(m_allowedConnections.contains(allowedConnection));
    m_allowedConnections.removeAll(allowedConnection);
}
auto StorageManager::SessionStorageNamespace::getOrCreateStorageArea(SecurityOriginData&& securityOrigin) -> Ref<StorageArea>
{
    return *m_storageAreaMap.ensure(securityOrigin, [this, securityOrigin]() mutable {
        return StorageArea::create(nullptr, WTFMove(securityOrigin), m_quotaInBytes);
    }).iterator->value.copyRef();
}

void StorageManager::SessionStorageNamespace::cloneTo(SessionStorageNamespace& newSessionStorageNamespace)
{
    ASSERT_UNUSED(newSessionStorageNamespace, newSessionStorageNamespace.isEmpty());

    for (auto& pair : m_storageAreaMap)
        newSessionStorageNamespace.m_storageAreaMap.add(pair.key, pair.value->clone());
}

Ref<StorageManager> StorageManager::create(const String& localStorageDirectory)
{
    return adoptRef(*new StorageManager(localStorageDirectory));
}

StorageManager::StorageManager(const String& localStorageDirectory)
    : m_queue(WorkQueue::create("com.apple.WebKit.StorageManager"))
    , m_localStorageDatabaseTracker(LocalStorageDatabaseTracker::create(m_queue.copyRef(), localStorageDirectory))
    , m_isEphemeral(localStorageDirectory.isNull())
{
    // Make sure the encoding is initialized before we start dispatching things to the queue.
    UTF8Encoding();
}

StorageManager::~StorageManager()
{
}

void StorageManager::createSessionStorageNamespace(uint64_t storageNamespaceID, unsigned quotaInBytes)
{
    m_queue->dispatch([this, protectedThis = makeRef(*this), storageNamespaceID, quotaInBytes]() mutable {
        ASSERT(!m_sessionStorageNamespaces.contains(storageNamespaceID));

        m_sessionStorageNamespaces.set(storageNamespaceID, SessionStorageNamespace::create(quotaInBytes));
    });
}

void StorageManager::destroySessionStorageNamespace(uint64_t storageNamespaceID)
{
    m_queue->dispatch([this, protectedThis = makeRef(*this), storageNamespaceID] {
        ASSERT(m_sessionStorageNamespaces.contains(storageNamespaceID));
        m_sessionStorageNamespaces.remove(storageNamespaceID);
    });
}

void StorageManager::addAllowedSessionStorageNamespaceConnection(uint64_t storageNamespaceID, IPC::Connection& allowedConnection)
{
    auto allowedConnectionID = allowedConnection.uniqueID();
    m_queue->dispatch([this, protectedThis = makeRef(*this), allowedConnectionID, storageNamespaceID]() mutable {
        ASSERT(m_sessionStorageNamespaces.contains(storageNamespaceID));

        m_sessionStorageNamespaces.get(storageNamespaceID)->addAllowedConnection(allowedConnectionID);
    });
}

void StorageManager::removeAllowedSessionStorageNamespaceConnection(uint64_t storageNamespaceID, IPC::Connection& allowedConnection)
{
    auto allowedConnectionID = allowedConnection.uniqueID();
    m_queue->dispatch([this, protectedThis = makeRef(*this), allowedConnectionID, storageNamespaceID]() mutable {
        ASSERT(m_sessionStorageNamespaces.contains(storageNamespaceID));

        m_sessionStorageNamespaces.get(storageNamespaceID)->removeAllowedConnection(allowedConnectionID);
    });
}

void StorageManager::cloneSessionStorageNamespace(uint64_t storageNamespaceID, uint64_t newStorageNamespaceID)
{
    m_queue->dispatch([this, protectedThis = makeRef(*this), storageNamespaceID, newStorageNamespaceID] {
        SessionStorageNamespace* sessionStorageNamespace = m_sessionStorageNamespaces.get(storageNamespaceID);
        if (!sessionStorageNamespace) {
            // FIXME: We can get into this situation if someone closes the originating page from within a
            // createNewPage callback. We bail for now, but we should really find a way to keep the session storage alive
            // so we we'll clone the session storage correctly.
            return;
        }

        SessionStorageNamespace* newSessionStorageNamespace = m_sessionStorageNamespaces.get(newStorageNamespaceID);
        ASSERT(newSessionStorageNamespace);

        sessionStorageNamespace->cloneTo(*newSessionStorageNamespace);
    });
}

void StorageManager::processWillOpenConnection(WebProcessProxy& process, IPC::Connection& connection)
{
    connection.addWorkQueueMessageReceiver(Messages::StorageManager::messageReceiverName(), m_queue.get(), this);
}

void StorageManager::processDidCloseConnection(WebProcessProxy& process, IPC::Connection& connection)
{
    connection.removeWorkQueueMessageReceiver(Messages::StorageManager::messageReceiverName());

    m_queue->dispatch([this, protectedThis = makeRef(*this), connectionID = connection.uniqueID()]() mutable {
        Vector<std::pair<IPC::Connection::UniqueID, uint64_t>> connectionAndStorageMapIDPairsToRemove;
        for (auto& storageArea : m_storageAreasByConnection) {
            if (storageArea.key.first != connectionID)
                continue;

            storageArea.value->removeListener(storageArea.key.first, storageArea.key.second);
            connectionAndStorageMapIDPairsToRemove.append(storageArea.key);
        }

        for (auto& pair : connectionAndStorageMapIDPairsToRemove)
            m_storageAreasByConnection.remove(pair);
    });
}

void StorageManager::getSessionStorageOrigins(Function<void(HashSet<WebCore::SecurityOriginData>&&)>&& completionHandler)
{
    m_queue->dispatch([this, protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)]() mutable {
        HashSet<SecurityOriginData> origins;

        for (const auto& sessionStorageNamespace : m_sessionStorageNamespaces.values()) {
            for (auto& origin : sessionStorageNamespace->origins())
                origins.add(origin);
        }

        RunLoop::main().dispatch([origins = WTFMove(origins), completionHandler = WTFMove(completionHandler)]() mutable {
            completionHandler(WTFMove(origins));
        });
    });
}

void StorageManager::deleteSessionStorageOrigins(Function<void()>&& completionHandler)
{
    m_queue->dispatch([this, protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)]() mutable {
        for (auto& sessionStorageNamespace : m_sessionStorageNamespaces.values())
            sessionStorageNamespace->clearAllStorageAreas();

        RunLoop::main().dispatch(WTFMove(completionHandler));
    });
}

void StorageManager::deleteSessionStorageEntriesForOrigins(const Vector<WebCore::SecurityOriginData>& origins, Function<void()>&& completionHandler)
{
    Vector<WebCore::SecurityOriginData> copiedOrigins;
    copiedOrigins.reserveInitialCapacity(origins.size());

    for (auto& origin : origins)
        copiedOrigins.uncheckedAppend(origin.isolatedCopy());

    m_queue->dispatch([this, protectedThis = makeRef(*this), copiedOrigins = WTFMove(copiedOrigins), completionHandler = WTFMove(completionHandler)]() mutable {
        for (auto& origin : copiedOrigins) {
            for (auto& sessionStorageNamespace : m_sessionStorageNamespaces.values())
                sessionStorageNamespace->clearStorageAreasMatchingOrigin(origin);
        }

        RunLoop::main().dispatch(WTFMove(completionHandler));
    });
}

void StorageManager::getLocalStorageOrigins(Function<void(HashSet<WebCore::SecurityOriginData>&&)>&& completionHandler)
{
    m_queue->dispatch([this, protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)]() mutable {
        HashSet<SecurityOriginData> origins;

        for (auto& origin : m_localStorageDatabaseTracker->origins())
            origins.add(origin);

        for (auto& transientLocalStorageNamespace : m_transientLocalStorageNamespaces.values()) {
            for (auto& origin : transientLocalStorageNamespace->origins())
                origins.add(origin);
        }

        RunLoop::main().dispatch([origins = WTFMove(origins), completionHandler = WTFMove(completionHandler)]() mutable {
            completionHandler(WTFMove(origins));
        });
    });
}

void StorageManager::getLocalStorageOriginDetails(Function<void (Vector<LocalStorageDatabaseTracker::OriginDetails>)>&& completionHandler)
{
    m_queue->dispatch([this, protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)]() mutable {
        auto originDetails = m_localStorageDatabaseTracker->originDetails();

        RunLoop::main().dispatch([originDetails = WTFMove(originDetails), completionHandler = WTFMove(completionHandler)]() mutable {
            completionHandler(WTFMove(originDetails));
        });
    });
}

void StorageManager::deleteLocalStorageEntriesForOrigin(const SecurityOriginData& securityOrigin)
{
    m_queue->dispatch([this, protectedThis = makeRef(*this), copiedOrigin = securityOrigin.isolatedCopy()]() mutable {
        for (auto& localStorageNamespace : m_localStorageNamespaces.values())
            localStorageNamespace->clearStorageAreasMatchingOrigin(copiedOrigin);

        for (auto& transientLocalStorageNamespace : m_transientLocalStorageNamespaces.values())
            transientLocalStorageNamespace->clearStorageAreasMatchingOrigin(copiedOrigin);

        m_localStorageDatabaseTracker->deleteDatabaseWithOrigin(copiedOrigin);
    });
}

void StorageManager::deleteLocalStorageOriginsModifiedSince(WallTime time, Function<void()>&& completionHandler)
{
    m_queue->dispatch([this, protectedThis = makeRef(*this), time, completionHandler = WTFMove(completionHandler)]() mutable {
        auto originsToDelete = m_localStorageDatabaseTracker->databasesModifiedSince(time);
        
        for (auto& transientLocalStorageNamespace : m_transientLocalStorageNamespaces.values())
            transientLocalStorageNamespace->clearAllStorageAreas();

        for (const auto& origin : originsToDelete) {
            for (auto& localStorageNamespace : m_localStorageNamespaces.values())
                localStorageNamespace->clearStorageAreasMatchingOrigin(origin);
            
            m_localStorageDatabaseTracker->deleteDatabaseWithOrigin(origin);
        }

        RunLoop::main().dispatch(WTFMove(completionHandler));
    });
}

void StorageManager::deleteLocalStorageEntriesForOrigins(const Vector<WebCore::SecurityOriginData>& origins, Function<void()>&& completionHandler)
{
    Vector<SecurityOriginData> copiedOrigins;
    copiedOrigins.reserveInitialCapacity(origins.size());

    for (auto& origin : origins)
        copiedOrigins.uncheckedAppend(origin.isolatedCopy());

    m_queue->dispatch([this, protectedThis = makeRef(*this), copiedOrigins = WTFMove(copiedOrigins), completionHandler = WTFMove(completionHandler)]() mutable {
        for (auto& origin : copiedOrigins) {
            for (auto& localStorageNamespace : m_localStorageNamespaces.values())
                localStorageNamespace->clearStorageAreasMatchingOrigin(origin);

            for (auto& transientLocalStorageNamespace : m_transientLocalStorageNamespaces.values())
                transientLocalStorageNamespace->clearStorageAreasMatchingOrigin(origin);

            m_localStorageDatabaseTracker->deleteDatabaseWithOrigin(origin);
        }

        RunLoop::main().dispatch(WTFMove(completionHandler));
    });
}

void StorageManager::createLocalStorageMap(IPC::Connection& connection, uint64_t storageMapID, uint64_t storageNamespaceID, SecurityOriginData&& securityOriginData)
{
    ASSERT(!m_isEphemeral);
    std::pair<IPC::Connection::UniqueID, uint64_t> connectionAndStorageMapIDPair(connection.uniqueID(), storageMapID);

    // FIXME: This should be a message check.
    ASSERT((HashMap<std::pair<IPC::Connection::UniqueID, uint64_t>, RefPtr<StorageArea>>::isValidKey(connectionAndStorageMapIDPair)));

    auto result = m_storageAreasByConnection.add(connectionAndStorageMapIDPair, nullptr);

    // FIXME: These should be a message checks.
    ASSERT(result.isNewEntry);
    ASSERT((HashMap<uint64_t, RefPtr<LocalStorageNamespace>>::isValidKey(storageNamespaceID)));

    LocalStorageNamespace* localStorageNamespace = getOrCreateLocalStorageNamespace(storageNamespaceID);

    // FIXME: This should be a message check.
    ASSERT(localStorageNamespace);

    auto storageArea = localStorageNamespace->getOrCreateStorageArea(WTFMove(securityOriginData));
    storageArea->addListener(connection.uniqueID(), storageMapID);

    result.iterator->value = WTFMove(storageArea);
}

void StorageManager::createTransientLocalStorageMap(IPC::Connection& connection, uint64_t storageMapID, uint64_t storageNamespaceID, SecurityOriginData&& topLevelOriginData, SecurityOriginData&& origin)
{
    // FIXME: This should be a message check.
    ASSERT(m_storageAreasByConnection.isValidKey({ connection.uniqueID(), storageMapID }));

    // See if we already have session storage for this connection/origin combo.
    // If so, update the map with the new ID, otherwise keep on trucking.
    for (auto it = m_storageAreasByConnection.begin(), end = m_storageAreasByConnection.end(); it != end; ++it) {
        if (it->key.first != connection.uniqueID())
            continue;
        Ref<StorageArea> area = *it->value;
        if (!area->isSessionStorage())
            continue;
        if (!origin.securityOrigin()->isSameSchemeHostPort(area->securityOrigin().securityOrigin().get()))
            continue;
        area->addListener(connection.uniqueID(), storageMapID);
        // If the storageMapID used as key in m_storageAreasByConnection is no longer one of the StorageArea's listeners, then this means
        // that destroyStorageMap() was already called for that storageMapID but it decided not to remove it from m_storageAreasByConnection
        // so that we could reuse it later on for the same connection/origin combo. In this case, it is safe to remove the previous
        // storageMapID from m_storageAreasByConnection.
        if (!area->hasListener(connection.uniqueID(), it->key.second))
            m_storageAreasByConnection.remove(it);
        m_storageAreasByConnection.add({ connection.uniqueID(), storageMapID }, WTFMove(area));
        return;
    }

    auto& slot = m_storageAreasByConnection.add({ connection.uniqueID(), storageMapID }, nullptr).iterator->value;

    // FIXME: This should be a message check.
    ASSERT(!slot);

    TransientLocalStorageNamespace* transientLocalStorageNamespace = getOrCreateTransientLocalStorageNamespace(storageNamespaceID, WTFMove(topLevelOriginData));

    auto storageArea = transientLocalStorageNamespace->getOrCreateStorageArea(WTFMove(origin));
    storageArea->addListener(connection.uniqueID(), storageMapID);

    slot = WTFMove(storageArea);
}

void StorageManager::createSessionStorageMap(IPC::Connection& connection, uint64_t storageMapID, uint64_t storageNamespaceID, SecurityOriginData&& securityOriginData)
{
    if (m_isEphemeral) {
        m_ephemeralStorage.add(securityOriginData, WebCore::StorageMap::create(localStorageDatabaseQuotaInBytes));
        return;
    }
    // FIXME: This should be a message check.
    ASSERT(m_sessionStorageNamespaces.isValidKey(storageNamespaceID));

    SessionStorageNamespace* sessionStorageNamespace = m_sessionStorageNamespaces.get(storageNamespaceID);
    if (!sessionStorageNamespace) {
        // We're getting an incoming message from the web process that's for session storage for a web page
        // that has already been closed, just ignore it.
        return;
    }

    // FIXME: This should be a message check.
    ASSERT(m_storageAreasByConnection.isValidKey({ connection.uniqueID(), storageMapID }));

    auto& slot = m_storageAreasByConnection.add({ connection.uniqueID(), storageMapID }, nullptr).iterator->value;

    // FIXME: This should be a message check.
    ASSERT(!slot);

    // FIXME: This should be a message check.
    ASSERT(sessionStorageNamespace->allowedConnections().contains(connection.uniqueID()));

    auto storageArea = sessionStorageNamespace->getOrCreateStorageArea(WTFMove(securityOriginData));
    storageArea->addListener(connection.uniqueID(), storageMapID);

    slot = WTFMove(storageArea);
}

void StorageManager::destroyStorageMap(IPC::Connection& connection, uint64_t storageMapID)
{
    std::pair<IPC::Connection::UniqueID, uint64_t> connectionAndStorageMapIDPair(connection.uniqueID(), storageMapID);

    // FIXME: This should be a message check.
    ASSERT(m_storageAreasByConnection.isValidKey(connectionAndStorageMapIDPair));

    auto it = m_storageAreasByConnection.find(connectionAndStorageMapIDPair);
    if (it == m_storageAreasByConnection.end()) {
        // The connection has been removed because the last page was closed.
        return;
    }

    it->value->removeListener(connection.uniqueID(), storageMapID);

    // Don't remove session storage maps. The web process may reconnect and expect the data to still be around.
    if (it->value->isSessionStorage())
        return;

    m_storageAreasByConnection.remove(connectionAndStorageMapIDPair);
}

void StorageManager::getValues(IPC::Connection& connection, WebCore::SecurityOriginData&& securityOriginData, uint64_t storageMapID, uint64_t storageMapSeed, CompletionHandler<void(const HashMap<String, String>&)>&& completionHandler)
{
    StorageArea* storageArea = findStorageArea(connection, storageMapID);
    if (!storageArea) {
        if (m_isEphemeral) {
            if (auto storageMap = m_ephemeralStorage.get(securityOriginData))
                return completionHandler(storageMap->items());
        }
        // This is a session storage area for a page that has already been closed. Ignore it.
        return completionHandler({ });
    }

    completionHandler(storageArea->items());
    connection.send(Messages::StorageAreaMap::DidGetValues(storageMapSeed), storageMapID);
}

void StorageManager::setItem(IPC::Connection& connection, WebCore::SecurityOriginData&& securityOriginData, uint64_t storageMapID, uint64_t sourceStorageAreaID, uint64_t storageMapSeed, const String& key, const String& value, const String& urlString)
{
    StorageArea* storageArea = findStorageArea(connection, storageMapID);
    if (!storageArea) {
        if (m_isEphemeral) {
            if (auto storageMap = m_ephemeralStorage.get(securityOriginData)) {
                String oldValue;
                bool quotaException;
                storageMap->setItem(key, value, oldValue, quotaException);
            }
        }
        // This is a session storage area for a page that has already been closed. Ignore it.
        return;
    }

    bool quotaError;
    storageArea->setItem(connection.uniqueID(), sourceStorageAreaID, key, value, urlString, quotaError);
    connection.send(Messages::StorageAreaMap::DidSetItem(storageMapSeed, key, quotaError), storageMapID);
}

void StorageManager::removeItem(IPC::Connection& connection, WebCore::SecurityOriginData&& securityOriginData, uint64_t storageMapID, uint64_t sourceStorageAreaID, uint64_t storageMapSeed, const String& key, const String& urlString)
{
    StorageArea* storageArea = findStorageArea(connection, storageMapID);
    if (!storageArea) {
        if (m_isEphemeral) {
            if (auto storageMap = m_ephemeralStorage.get(securityOriginData)) {
                String oldValue;
                storageMap->removeItem(key, oldValue);
            }
        }
        // This is a session storage area for a page that has already been closed. Ignore it.
        return;
    }

    storageArea->removeItem(connection.uniqueID(), sourceStorageAreaID, key, urlString);
    connection.send(Messages::StorageAreaMap::DidRemoveItem(storageMapSeed, key), storageMapID);
}

void StorageManager::clear(IPC::Connection& connection, WebCore::SecurityOriginData&& securityOriginData, uint64_t storageMapID, uint64_t sourceStorageAreaID, uint64_t storageMapSeed, const String& urlString)
{
    StorageArea* storageArea = findStorageArea(connection, storageMapID);
    if (!storageArea) {
        if (m_isEphemeral)
            m_ephemeralStorage.remove(securityOriginData);
        // This is a session storage area for a page that has already been closed. Ignore it.
        return;
    }

    storageArea->clear(connection.uniqueID(), sourceStorageAreaID, urlString);
    connection.send(Messages::StorageAreaMap::DidClear(storageMapSeed), storageMapID);
}

void StorageManager::applicationWillTerminate()
{
    BinarySemaphore semaphore;
    m_queue->dispatch([this, &semaphore] {
        Vector<std::pair<IPC::Connection::UniqueID, uint64_t>> connectionAndStorageMapIDPairsToRemove;
        for (auto& connectionStorageAreaPair : m_storageAreasByConnection) {
            connectionStorageAreaPair.value->removeListener(connectionStorageAreaPair.key.first, connectionStorageAreaPair.key.second);
            connectionAndStorageMapIDPairsToRemove.append(connectionStorageAreaPair.key);
        }

        for (auto& connectionStorageAreaPair : connectionAndStorageMapIDPairsToRemove)
            m_storageAreasByConnection.remove(connectionStorageAreaPair);

        semaphore.signal();
    });
    semaphore.wait();
}

StorageManager::StorageArea* StorageManager::findStorageArea(IPC::Connection& connection, uint64_t storageMapID) const
{
    std::pair<IPC::Connection::UniqueID, uint64_t> connectionAndStorageMapIDPair(connection.uniqueID(), storageMapID);

    if (!m_storageAreasByConnection.isValidKey(connectionAndStorageMapIDPair))
        return nullptr;

    return m_storageAreasByConnection.get(connectionAndStorageMapIDPair);
}

StorageManager::LocalStorageNamespace* StorageManager::getOrCreateLocalStorageNamespace(uint64_t storageNamespaceID)
{
    if (!m_localStorageNamespaces.isValidKey(storageNamespaceID))
        return nullptr;

    auto& slot = m_localStorageNamespaces.add(storageNamespaceID, nullptr).iterator->value;
    if (!slot)
        slot = LocalStorageNamespace::create(this, storageNamespaceID);

    return slot.get();
}

StorageManager::TransientLocalStorageNamespace* StorageManager::getOrCreateTransientLocalStorageNamespace(uint64_t storageNamespaceID, WebCore::SecurityOriginData&& topLevelOrigin)
{
    if (!m_transientLocalStorageNamespaces.isValidKey({ storageNamespaceID, topLevelOrigin }))
        return nullptr;

    auto& slot = m_transientLocalStorageNamespaces.add({ storageNamespaceID, WTFMove(topLevelOrigin) }, nullptr).iterator->value;
    if (!slot)
        slot = TransientLocalStorageNamespace::create();

    return slot.get();
}

} // namespace WebKit

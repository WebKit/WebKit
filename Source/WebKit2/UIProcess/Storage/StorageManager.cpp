/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

using namespace WebCore;

namespace WebKit {

class StorageManager::StorageArea : public ThreadSafeRefCounted<StorageManager::StorageArea> {
public:
    static Ref<StorageArea> create(LocalStorageNamespace*, Ref<SecurityOrigin>&&, unsigned quotaInBytes);
    ~StorageArea();

    SecurityOrigin& securityOrigin() { return m_securityOrigin.get(); }

    void addListener(IPC::Connection&, uint64_t storageMapID);
    void removeListener(IPC::Connection&, uint64_t storageMapID);

    Ref<StorageArea> clone() const;

    void setItem(IPC::Connection* sourceConnection, uint64_t sourceStorageAreaID, const String& key, const String& value, const String& urlString, bool& quotaException);
    void removeItem(IPC::Connection* sourceConnection, uint64_t sourceStorageAreaID, const String& key, const String& urlString);
    void clear(IPC::Connection* sourceConnection, uint64_t sourceStorageAreaID, const String& urlString);

    const HashMap<String, String>& items();
    void clear();

    bool isSessionStorage() const { return !m_localStorageNamespace; }

private:
    explicit StorageArea(LocalStorageNamespace*, Ref<SecurityOrigin>&&, unsigned quotaInBytes);

    void openDatabaseAndImportItemsIfNeeded();

    void dispatchEvents(IPC::Connection* sourceConnection, uint64_t sourceStorageAreaID, const String& key, const String& oldValue, const String& newValue, const String& urlString) const;

    // Will be null if the storage area belongs to a session storage namespace.
    LocalStorageNamespace* m_localStorageNamespace;
    RefPtr<LocalStorageDatabase> m_localStorageDatabase;
    bool m_didImportItemsFromDatabase;

    Ref<SecurityOrigin> m_securityOrigin;
    unsigned m_quotaInBytes;

    RefPtr<StorageMap> m_storageMap;
    HashSet<std::pair<RefPtr<IPC::Connection>, uint64_t>> m_eventListeners;
};

class StorageManager::LocalStorageNamespace : public ThreadSafeRefCounted<LocalStorageNamespace> {
public:
    static Ref<LocalStorageNamespace> create(StorageManager*, uint64_t storageManagerID);
    ~LocalStorageNamespace();

    StorageManager* storageManager() const { return m_storageManager; }

    Ref<StorageArea> getOrCreateStorageArea(Ref<SecurityOrigin>&&);
    void didDestroyStorageArea(StorageArea*);

    void clearStorageAreasMatchingOrigin(const SecurityOrigin&);
    void clearAllStorageAreas();

private:
    explicit LocalStorageNamespace(StorageManager*, uint64_t storageManagerID);

    StorageManager* m_storageManager;
    uint64_t m_storageNamespaceID;
    unsigned m_quotaInBytes;

    // We don't hold an explicit reference to the StorageAreas; they are kept alive by the m_storageAreasByConnection map in StorageManager.
    HashMap<RefPtr<SecurityOrigin>, StorageArea*> m_storageAreaMap;
};

class StorageManager::TransientLocalStorageNamespace : public ThreadSafeRefCounted<TransientLocalStorageNamespace> {
public:
    static Ref<TransientLocalStorageNamespace> create()
    {
        return adoptRef(*new TransientLocalStorageNamespace());
    }

    ~TransientLocalStorageNamespace()
    {
    }

    Ref<StorageArea> getOrCreateStorageArea(Ref<SecurityOrigin>&& securityOrigin)
    {
        auto& slot = m_storageAreaMap.add(securityOrigin.ptr(), nullptr).iterator->value;
        if (slot)
            return *slot;

        auto storageArea = StorageArea::create(nullptr, WTFMove(securityOrigin), m_quotaInBytes);
        slot = &storageArea.get();

        return storageArea;
    }

    Vector<Ref<SecurityOrigin>> origins() const
    {
        Vector<Ref<SecurityOrigin>> origins;

        for (const auto& storageArea : m_storageAreaMap.values()) {
            if (!storageArea->items().isEmpty())
                origins.append(storageArea->securityOrigin());
        }

        return origins;
    }

    void clearStorageAreasMatchingOrigin(const SecurityOrigin& securityOrigin)
    {
        for (auto& storageArea : m_storageAreaMap.values()) {
            if (storageArea->securityOrigin().equal(&securityOrigin))
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

    const unsigned m_quotaInBytes = 5 * 1024 * 1024;

    HashMap<RefPtr<SecurityOrigin>, RefPtr<StorageArea>> m_storageAreaMap;
};

Ref<StorageManager::StorageArea> StorageManager::StorageArea::create(LocalStorageNamespace* localStorageNamespace, Ref<SecurityOrigin>&& securityOrigin, unsigned quotaInBytes)
{
    return adoptRef(*new StorageArea(localStorageNamespace, WTFMove(securityOrigin), quotaInBytes));
}

StorageManager::StorageArea::StorageArea(LocalStorageNamespace* localStorageNamespace, Ref<SecurityOrigin>&& securityOrigin, unsigned quotaInBytes)
    : m_localStorageNamespace(localStorageNamespace)
    , m_didImportItemsFromDatabase(false)
    , m_securityOrigin(WTFMove(securityOrigin))
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

void StorageManager::StorageArea::addListener(IPC::Connection& connection, uint64_t storageMapID)
{
    ASSERT(!m_eventListeners.contains(std::make_pair(&connection, storageMapID)));
    m_eventListeners.add(std::make_pair(&connection, storageMapID));
}

void StorageManager::StorageArea::removeListener(IPC::Connection& connection, uint64_t storageMapID)
{
    ASSERT(isSessionStorage() || m_eventListeners.contains(std::make_pair(&connection, storageMapID)));
    m_eventListeners.remove(std::make_pair(&connection, storageMapID));
}

Ref<StorageManager::StorageArea> StorageManager::StorageArea::clone() const
{
    ASSERT(!m_localStorageNamespace);

    auto storageArea = StorageArea::create(0, m_securityOrigin.copyRef(), m_quotaInBytes);
    storageArea->m_storageMap = m_storageMap;

    return storageArea;
}

void StorageManager::StorageArea::setItem(IPC::Connection* sourceConnection, uint64_t sourceStorageAreaID, const String& key, const String& value, const String& urlString, bool& quotaException)
{
    openDatabaseAndImportItemsIfNeeded();

    String oldValue;

    RefPtr<StorageMap> newStorageMap = m_storageMap->setItem(key, value, oldValue, quotaException);
    if (newStorageMap)
        m_storageMap = newStorageMap.release();

    if (quotaException)
        return;

    if (m_localStorageDatabase)
        m_localStorageDatabase->setItem(key, value);

    dispatchEvents(sourceConnection, sourceStorageAreaID, key, oldValue, value, urlString);
}

void StorageManager::StorageArea::removeItem(IPC::Connection* sourceConnection, uint64_t sourceStorageAreaID, const String& key, const String& urlString)
{
    openDatabaseAndImportItemsIfNeeded();

    String oldValue;
    RefPtr<StorageMap> newStorageMap = m_storageMap->removeItem(key, oldValue);
    if (newStorageMap)
        m_storageMap = newStorageMap.release();

    if (oldValue.isNull())
        return;

    if (m_localStorageDatabase)
        m_localStorageDatabase->removeItem(key);

    dispatchEvents(sourceConnection, sourceStorageAreaID, key, oldValue, String(), urlString);
}

void StorageManager::StorageArea::clear(IPC::Connection* sourceConnection, uint64_t sourceStorageAreaID, const String& urlString)
{
    openDatabaseAndImportItemsIfNeeded();

    if (!m_storageMap->length())
        return;

    m_storageMap = StorageMap::create(m_quotaInBytes);

    if (m_localStorageDatabase)
        m_localStorageDatabase->clear();

    dispatchEvents(sourceConnection, sourceStorageAreaID, String(), String(), String(), urlString);
}

const HashMap<String, String>& StorageManager::StorageArea::items()
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

    for (auto it = m_eventListeners.begin(), end = m_eventListeners.end(); it != end; ++it)
        it->first->send(Messages::StorageAreaMap::ClearCache(), it->second);
}

void StorageManager::StorageArea::openDatabaseAndImportItemsIfNeeded()
{
    if (!m_localStorageNamespace)
        return;

    // We open the database here even if we've already imported our items to ensure that the database is open if we need to write to it.
    if (!m_localStorageDatabase)
        m_localStorageDatabase = LocalStorageDatabase::create(m_localStorageNamespace->storageManager()->m_queue, m_localStorageNamespace->storageManager()->m_localStorageDatabaseTracker, m_securityOrigin.copyRef());

    if (m_didImportItemsFromDatabase)
        return;

    m_localStorageDatabase->importItems(*m_storageMap);
    m_didImportItemsFromDatabase = true;
}

void StorageManager::StorageArea::dispatchEvents(IPC::Connection* sourceConnection, uint64_t sourceStorageAreaID, const String& key, const String& oldValue, const String& newValue, const String& urlString) const
{
    for (HashSet<std::pair<RefPtr<IPC::Connection>, uint64_t>>::const_iterator it = m_eventListeners.begin(), end = m_eventListeners.end(); it != end; ++it) {
        uint64_t storageAreaID = it->first == sourceConnection ? sourceStorageAreaID : 0;

        it->first->send(Messages::StorageAreaMap::DispatchStorageEvent(storageAreaID, key, oldValue, newValue, urlString), it->second);
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
    , m_quotaInBytes(5 * 1024 * 1024)
{
}

StorageManager::LocalStorageNamespace::~LocalStorageNamespace()
{
    ASSERT(m_storageAreaMap.isEmpty());
}

Ref<StorageManager::StorageArea> StorageManager::LocalStorageNamespace::getOrCreateStorageArea(Ref<SecurityOrigin>&& securityOrigin)
{
    auto& slot = m_storageAreaMap.add(securityOrigin.ptr(), nullptr).iterator->value;
    if (slot)
        return *slot;

    auto storageArea = StorageArea::create(this, WTFMove(securityOrigin), m_quotaInBytes);
    slot = &storageArea.get();

    return storageArea;
}

void StorageManager::LocalStorageNamespace::didDestroyStorageArea(StorageArea* storageArea)
{
    ASSERT(m_storageAreaMap.contains(&storageArea->securityOrigin()));

    m_storageAreaMap.remove(&storageArea->securityOrigin());
    if (!m_storageAreaMap.isEmpty())
        return;

    ASSERT(m_storageManager->m_localStorageNamespaces.contains(m_storageNamespaceID));
    m_storageManager->m_localStorageNamespaces.remove(m_storageNamespaceID);
}

void StorageManager::LocalStorageNamespace::clearStorageAreasMatchingOrigin(const SecurityOrigin& securityOrigin)
{
    for (const auto& originAndStorageArea : m_storageAreaMap) {
        if (originAndStorageArea.key->equal(&securityOrigin))
            originAndStorageArea.value->clear();
    }
}

void StorageManager::LocalStorageNamespace::clearAllStorageAreas()
{
    for (auto it = m_storageAreaMap.begin(), end = m_storageAreaMap.end(); it != end; ++it)
        it->value->clear();
}

class StorageManager::SessionStorageNamespace : public ThreadSafeRefCounted<SessionStorageNamespace> {
public:
    static Ref<SessionStorageNamespace> create(unsigned quotaInBytes);
    ~SessionStorageNamespace();

    bool isEmpty() const { return m_storageAreaMap.isEmpty(); }

    IPC::Connection* allowedConnection() const { return m_allowedConnection.get(); }
    void setAllowedConnection(IPC::Connection*);

    Ref<StorageArea> getOrCreateStorageArea(Ref<SecurityOrigin>&&);

    void cloneTo(SessionStorageNamespace& newSessionStorageNamespace);

    Vector<Ref<SecurityOrigin>> origins() const
    {
        Vector<Ref<SecurityOrigin>> origins;

        for (const auto& storageArea : m_storageAreaMap.values()) {
            if (!storageArea->items().isEmpty())
                origins.append(storageArea->securityOrigin());
        }

        return origins;
    }

    void clearStorageAreasMatchingOrigin(const SecurityOrigin& securityOrigin)
    {
        for (auto& storageArea : m_storageAreaMap.values()) {
            if (storageArea->securityOrigin().equal(&securityOrigin))
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

    RefPtr<IPC::Connection> m_allowedConnection;
    unsigned m_quotaInBytes;

    HashMap<RefPtr<SecurityOrigin>, RefPtr<StorageArea>> m_storageAreaMap;
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

void StorageManager::SessionStorageNamespace::setAllowedConnection(IPC::Connection* allowedConnection)
{
    ASSERT(!allowedConnection || !m_allowedConnection);

    m_allowedConnection = allowedConnection;
}

Ref<StorageManager::StorageArea> StorageManager::SessionStorageNamespace::getOrCreateStorageArea(Ref<SecurityOrigin>&& securityOrigin)
{
    auto& slot = m_storageAreaMap.add(securityOrigin.ptr(), nullptr).iterator->value;
    if (!slot)
        slot = StorageArea::create(0, WTFMove(securityOrigin), m_quotaInBytes);

    return *slot;
}

void StorageManager::SessionStorageNamespace::cloneTo(SessionStorageNamespace& newSessionStorageNamespace)
{
    ASSERT_UNUSED(newSessionStorageNamespace, newSessionStorageNamespace.isEmpty());

    for (HashMap<RefPtr<SecurityOrigin>, RefPtr<StorageArea>>::const_iterator it = m_storageAreaMap.begin(), end = m_storageAreaMap.end(); it != end; ++it)
        newSessionStorageNamespace.m_storageAreaMap.add(it->key, it->value->clone());
}

Ref<StorageManager> StorageManager::create(const String& localStorageDirectory)
{
    return adoptRef(*new StorageManager(localStorageDirectory));
}

StorageManager::StorageManager(const String& localStorageDirectory)
    : m_queue(WorkQueue::create("com.apple.WebKit.StorageManager"))
    , m_localStorageDatabaseTracker(LocalStorageDatabaseTracker::create(m_queue, localStorageDirectory))
{
    // Make sure the encoding is initialized before we start dispatching things to the queue.
    UTF8Encoding();
}

StorageManager::~StorageManager()
{
}

void StorageManager::createSessionStorageNamespace(uint64_t storageNamespaceID, unsigned quotaInBytes)
{
    RefPtr<StorageManager> storageManager(this);

    m_queue->dispatch([storageManager, storageNamespaceID, quotaInBytes] {
        ASSERT(!storageManager->m_sessionStorageNamespaces.contains(storageNamespaceID));

        storageManager->m_sessionStorageNamespaces.set(storageNamespaceID, SessionStorageNamespace::create(quotaInBytes));
    });
}

void StorageManager::destroySessionStorageNamespace(uint64_t storageNamespaceID)
{
    RefPtr<StorageManager> storageManager(this);

    m_queue->dispatch([storageManager, storageNamespaceID] {
        ASSERT(storageManager->m_sessionStorageNamespaces.contains(storageNamespaceID));
        storageManager->m_sessionStorageNamespaces.remove(storageNamespaceID);
    });
}

void StorageManager::setAllowedSessionStorageNamespaceConnection(uint64_t storageNamespaceID, IPC::Connection* allowedConnection)
{
    RefPtr<StorageManager> storageManager(this);
    RefPtr<IPC::Connection> connection(allowedConnection);

    m_queue->dispatch([storageManager, connection, storageNamespaceID] {
        ASSERT(storageManager->m_sessionStorageNamespaces.contains(storageNamespaceID));

        storageManager->m_sessionStorageNamespaces.get(storageNamespaceID)->setAllowedConnection(connection.get());
    });
}

void StorageManager::cloneSessionStorageNamespace(uint64_t storageNamespaceID, uint64_t newStorageNamespaceID)
{
    RefPtr<StorageManager> storageManager(this);

    m_queue->dispatch([storageManager, storageNamespaceID, newStorageNamespaceID] {
        SessionStorageNamespace* sessionStorageNamespace = storageManager->m_sessionStorageNamespaces.get(storageNamespaceID);
        if (!sessionStorageNamespace) {
            // FIXME: We can get into this situation if someone closes the originating page from within a
            // createNewPage callback. We bail for now, but we should really find a way to keep the session storage alive
            // so we we'll clone the session storage correctly.
            return;
        }

        SessionStorageNamespace* newSessionStorageNamespace = storageManager->m_sessionStorageNamespaces.get(newStorageNamespaceID);
        ASSERT(newSessionStorageNamespace);

        sessionStorageNamespace->cloneTo(*newSessionStorageNamespace);
    });
}

void StorageManager::processWillOpenConnection(WebProcessProxy&, IPC::Connection& connection)
{
    connection.addWorkQueueMessageReceiver(Messages::StorageManager::messageReceiverName(), m_queue.get(), this);
}

void StorageManager::processDidCloseConnection(WebProcessProxy&, IPC::Connection& connection)
{
    connection.removeWorkQueueMessageReceiver(Messages::StorageManager::messageReceiverName());

    RefPtr<StorageManager> storageManager(this);
    RefPtr<IPC::Connection> protectedConnection(&connection);

    m_queue->dispatch([storageManager, protectedConnection] {
        Vector<std::pair<RefPtr<IPC::Connection>, uint64_t>> connectionAndStorageMapIDPairsToRemove;
        auto storageAreasByConnection = storageManager->m_storageAreasByConnection;

        for (auto it = storageAreasByConnection.begin(), end = storageAreasByConnection.end(); it != end; ++it) {
            if (it->key.first != protectedConnection)
                continue;

            it->value->removeListener(*it->key.first, it->key.second);
            connectionAndStorageMapIDPairsToRemove.append(it->key);
        }

        for (size_t i = 0; i < connectionAndStorageMapIDPairsToRemove.size(); ++i)
            storageManager->m_storageAreasByConnection.remove(connectionAndStorageMapIDPairsToRemove[i]);
    });
}

void StorageManager::getSessionStorageOrigins(std::function<void (HashSet<RefPtr<WebCore::SecurityOrigin>>&&)> completionHandler)
{
    RefPtr<StorageManager> storageManager(this);

    m_queue->dispatch([storageManager, completionHandler] {
        HashSet<RefPtr<SecurityOrigin>> origins;

        for (const auto& sessionStorageNamespace : storageManager->m_sessionStorageNamespaces.values()) {
            for (auto& origin : sessionStorageNamespace->origins())
                origins.add(WTFMove(origin));
        }

        RunLoop::main().dispatch([origins, completionHandler]() mutable {
            completionHandler(WTFMove(origins));
        });
    });
}

void StorageManager::deleteSessionStorageOrigins(std::function<void ()> completionHandler)
{
    RefPtr<StorageManager> storageManager(this);

    m_queue->dispatch([storageManager, completionHandler] {
        for (auto& sessionStorageNamespace : storageManager->m_sessionStorageNamespaces.values())
            sessionStorageNamespace->clearAllStorageAreas();

        RunLoop::main().dispatch(completionHandler);
    });
}

void StorageManager::deleteSessionStorageEntriesForOrigins(const Vector<RefPtr<WebCore::SecurityOrigin>>& origins, std::function<void ()> completionHandler)
{
    Vector<RefPtr<WebCore::SecurityOrigin>> copiedOrigins;
    copiedOrigins.reserveInitialCapacity(origins.size());

    for (auto& origin : origins)
        copiedOrigins.uncheckedAppend(origin->isolatedCopy());

    RefPtr<StorageManager> storageManager(this);
    m_queue->dispatch([storageManager, copiedOrigins, completionHandler] {
        for (auto& origin : copiedOrigins) {
            for (auto& sessionStorageNamespace : storageManager->m_sessionStorageNamespaces.values())
                sessionStorageNamespace->clearStorageAreasMatchingOrigin(*origin);
        }

        RunLoop::main().dispatch(completionHandler);
    });
}

void StorageManager::getLocalStorageOrigins(std::function<void (HashSet<RefPtr<WebCore::SecurityOrigin>>&&)> completionHandler)
{
    RefPtr<StorageManager> storageManager(this);

    m_queue->dispatch([storageManager, completionHandler] {
        HashSet<RefPtr<SecurityOrigin>> origins;

        for (auto& origin : storageManager->m_localStorageDatabaseTracker->origins())
            origins.add(WTFMove(origin));

        for (auto& transientLocalStorageNamespace : storageManager->m_transientLocalStorageNamespaces.values()) {
            for (auto& origin : transientLocalStorageNamespace->origins())
                origins.add(WTFMove(origin));
        }

        RunLoop::main().dispatch([origins, completionHandler]() mutable {
            completionHandler(WTFMove(origins));
        });
    });
}

void StorageManager::getLocalStorageOriginDetails(std::function<void (Vector<LocalStorageDatabaseTracker::OriginDetails>)> completionHandler)
{
    RefPtr<StorageManager> storageManager(this);

    m_queue->dispatch([storageManager, completionHandler] {
        auto originDetails = storageManager->m_localStorageDatabaseTracker->originDetails();

        RunLoop::main().dispatch([originDetails, completionHandler]() mutable {
            completionHandler(WTFMove(originDetails));
        });
    });
}

void StorageManager::deleteLocalStorageEntriesForOrigin(const SecurityOrigin& securityOrigin)
{
    RefPtr<StorageManager> storageManager(this);

    RefPtr<SecurityOrigin> copiedOrigin = securityOrigin.isolatedCopy();
    m_queue->dispatch([storageManager, copiedOrigin] {
        for (auto& localStorageNamespace : storageManager->m_localStorageNamespaces.values())
            localStorageNamespace->clearStorageAreasMatchingOrigin(*copiedOrigin);

        for (auto& transientLocalStorageNamespace : storageManager->m_transientLocalStorageNamespaces.values())
            transientLocalStorageNamespace->clearStorageAreasMatchingOrigin(*copiedOrigin);

        storageManager->m_localStorageDatabaseTracker->deleteDatabaseWithOrigin(copiedOrigin.get());
    });
}

void StorageManager::deleteLocalStorageOriginsModifiedSince(std::chrono::system_clock::time_point time, std::function<void ()> completionHandler)
{
    RefPtr<StorageManager> storageManager(this);

    m_queue->dispatch([storageManager, time, completionHandler] {
        auto deletedOrigins = storageManager->m_localStorageDatabaseTracker->deleteDatabasesModifiedSince(time);

        for (const auto& origin : deletedOrigins) {
            for (auto& localStorageNamespace : storageManager->m_localStorageNamespaces.values())
                localStorageNamespace->clearStorageAreasMatchingOrigin(origin.get());
        }

        for (auto& transientLocalStorageNamespace : storageManager->m_transientLocalStorageNamespaces.values())
            transientLocalStorageNamespace->clearAllStorageAreas();

        RunLoop::main().dispatch(completionHandler);
    });
}

void StorageManager::deleteLocalStorageEntriesForOrigins(const Vector<RefPtr<WebCore::SecurityOrigin>>& origins, std::function<void ()> completionHandler)
{
    Vector<RefPtr<WebCore::SecurityOrigin>> copiedOrigins;
    copiedOrigins.reserveInitialCapacity(origins.size());

    for (auto& origin : origins)
        copiedOrigins.uncheckedAppend(origin->isolatedCopy());

    RefPtr<StorageManager> storageManager(this);
    m_queue->dispatch([storageManager, copiedOrigins, completionHandler] {
        for (auto& origin : copiedOrigins) {
            for (auto& localStorageNamespace : storageManager->m_localStorageNamespaces.values())
                localStorageNamespace->clearStorageAreasMatchingOrigin(*origin);

            for (auto& transientLocalStorageNamespace : storageManager->m_transientLocalStorageNamespaces.values())
                transientLocalStorageNamespace->clearStorageAreasMatchingOrigin(*origin);

            storageManager->m_localStorageDatabaseTracker->deleteDatabaseWithOrigin(origin.get());
        }

        RunLoop::main().dispatch(completionHandler);
    });
}

void StorageManager::createLocalStorageMap(IPC::Connection& connection, uint64_t storageMapID, uint64_t storageNamespaceID, const SecurityOriginData& securityOriginData)
{
    std::pair<RefPtr<IPC::Connection>, uint64_t> connectionAndStorageMapIDPair(&connection, storageMapID);

    // FIXME: This should be a message check.
    ASSERT((HashMap<std::pair<RefPtr<IPC::Connection>, uint64_t>, RefPtr<StorageArea>>::isValidKey(connectionAndStorageMapIDPair)));

    HashMap<std::pair<RefPtr<IPC::Connection>, uint64_t>, RefPtr<StorageArea>>::AddResult result = m_storageAreasByConnection.add(connectionAndStorageMapIDPair, nullptr);

    // FIXME: These should be a message checks.
    ASSERT(result.isNewEntry);
    ASSERT((HashMap<uint64_t, RefPtr<LocalStorageNamespace>>::isValidKey(storageNamespaceID)));

    LocalStorageNamespace* localStorageNamespace = getOrCreateLocalStorageNamespace(storageNamespaceID);

    // FIXME: This should be a message check.
    ASSERT(localStorageNamespace);

    RefPtr<StorageArea> storageArea = localStorageNamespace->getOrCreateStorageArea(securityOriginData.securityOrigin());
    storageArea->addListener(connection, storageMapID);

    result.iterator->value = storageArea.release();
}

void StorageManager::createTransientLocalStorageMap(IPC::Connection& connection, uint64_t storageMapID, uint64_t storageNamespaceID, const SecurityOriginData& topLevelOriginData, const SecurityOriginData& securityOriginData)
{
    // FIXME: This should be a message check.
    ASSERT(m_storageAreasByConnection.isValidKey({ &connection, storageMapID }));

    Ref<SecurityOrigin> origin = securityOriginData.securityOrigin();

    // See if we already have session storage for this connection/origin combo.
    // If so, update the map with the new ID, otherwise keep on trucking.
    for (auto it = m_storageAreasByConnection.begin(), end = m_storageAreasByConnection.end(); it != end; ++it) {
        if (it->key.first != &connection)
            continue;
        Ref<StorageArea> area = *it->value;
        if (!area->isSessionStorage())
            continue;
        if (!origin->isSameSchemeHostPort(&area->securityOrigin()))
            continue;
        area->addListener(connection, storageMapID);
        m_storageAreasByConnection.remove(it);
        m_storageAreasByConnection.add({ &connection, storageMapID }, WTFMove(area));
        return;
    }

    auto& slot = m_storageAreasByConnection.add({ &connection, storageMapID }, nullptr).iterator->value;

    // FIXME: This should be a message check.
    ASSERT(!slot);

    TransientLocalStorageNamespace* transientLocalStorageNamespace = getOrCreateTransientLocalStorageNamespace(storageNamespaceID, topLevelOriginData.securityOrigin());

    auto storageArea = transientLocalStorageNamespace->getOrCreateStorageArea(securityOriginData.securityOrigin());
    storageArea->addListener(connection, storageMapID);

    slot = WTFMove(storageArea);
}

void StorageManager::createSessionStorageMap(IPC::Connection& connection, uint64_t storageMapID, uint64_t storageNamespaceID, const SecurityOriginData& securityOriginData)
{
    // FIXME: This should be a message check.
    ASSERT(m_sessionStorageNamespaces.isValidKey(storageNamespaceID));

    SessionStorageNamespace* sessionStorageNamespace = m_sessionStorageNamespaces.get(storageNamespaceID);
    if (!sessionStorageNamespace) {
        // We're getting an incoming message from the web process that's for session storage for a web page
        // that has already been closed, just ignore it.
        return;
    }

    // FIXME: This should be a message check.
    ASSERT(m_storageAreasByConnection.isValidKey({ &connection, storageMapID }));

    auto& slot = m_storageAreasByConnection.add({ &connection, storageMapID }, nullptr).iterator->value;

    // FIXME: This should be a message check.
    ASSERT(!slot);

    // FIXME: This should be a message check.
    ASSERT(&connection == sessionStorageNamespace->allowedConnection());

    auto storageArea = sessionStorageNamespace->getOrCreateStorageArea(securityOriginData.securityOrigin());
    storageArea->addListener(connection, storageMapID);

    slot = WTFMove(storageArea);
}

void StorageManager::destroyStorageMap(IPC::Connection& connection, uint64_t storageMapID)
{
    std::pair<RefPtr<IPC::Connection>, uint64_t> connectionAndStorageMapIDPair(&connection, storageMapID);

    // FIXME: This should be a message check.
    ASSERT(m_storageAreasByConnection.isValidKey(connectionAndStorageMapIDPair));

    auto it = m_storageAreasByConnection.find(connectionAndStorageMapIDPair);
    if (it == m_storageAreasByConnection.end()) {
        // The connection has been removed because the last page was closed.
        return;
    }

    it->value->removeListener(connection, storageMapID);

    // Don't remove session storage maps. The web process may reconnect and expect the data to still be around.
    if (it->value->isSessionStorage())
        return;

    m_storageAreasByConnection.remove(connectionAndStorageMapIDPair);
}

void StorageManager::getValues(IPC::Connection& connection, uint64_t storageMapID, uint64_t storageMapSeed, HashMap<String, String>& values)
{
    StorageArea* storageArea = findStorageArea(connection, storageMapID);
    if (!storageArea) {
        // This is a session storage area for a page that has already been closed. Ignore it.
        return;
    }

    values = storageArea->items();
    connection.send(Messages::StorageAreaMap::DidGetValues(storageMapSeed), storageMapID);
}

void StorageManager::setItem(IPC::Connection& connection, uint64_t storageMapID, uint64_t sourceStorageAreaID, uint64_t storageMapSeed, const String& key, const String& value, const String& urlString)
{
    StorageArea* storageArea = findStorageArea(connection, storageMapID);
    if (!storageArea) {
        // This is a session storage area for a page that has already been closed. Ignore it.
        return;
    }

    bool quotaError;
    storageArea->setItem(&connection, sourceStorageAreaID, key, value, urlString, quotaError);
    connection.send(Messages::StorageAreaMap::DidSetItem(storageMapSeed, key, quotaError), storageMapID);
}

void StorageManager::removeItem(IPC::Connection& connection, uint64_t storageMapID, uint64_t sourceStorageAreaID, uint64_t storageMapSeed, const String& key, const String& urlString)
{
    StorageArea* storageArea = findStorageArea(connection, storageMapID);
    if (!storageArea) {
        // This is a session storage area for a page that has already been closed. Ignore it.
        return;
    }

    storageArea->removeItem(&connection, sourceStorageAreaID, key, urlString);
    connection.send(Messages::StorageAreaMap::DidRemoveItem(storageMapSeed, key), storageMapID);
}

void StorageManager::clear(IPC::Connection& connection, uint64_t storageMapID, uint64_t sourceStorageAreaID, uint64_t storageMapSeed, const String& urlString)
{
    StorageArea* storageArea = findStorageArea(connection, storageMapID);
    if (!storageArea) {
        // This is a session storage area for a page that has already been closed. Ignore it.
        return;
    }

    storageArea->clear(&connection, sourceStorageAreaID, urlString);
    connection.send(Messages::StorageAreaMap::DidClear(storageMapSeed), storageMapID);
}

void StorageManager::applicationWillTerminate()
{
    BinarySemaphore semaphore;
    m_queue->dispatch([this, &semaphore] {
        Vector<std::pair<RefPtr<IPC::Connection>, uint64_t>> connectionAndStorageMapIDPairsToRemove;
        for (auto& connectionStorageAreaPair : m_storageAreasByConnection) {
            connectionStorageAreaPair.value->removeListener(*connectionStorageAreaPair.key.first, connectionStorageAreaPair.key.second);
            connectionAndStorageMapIDPairsToRemove.append(connectionStorageAreaPair.key);
        }

        for (auto& connectionStorageAreaPair : connectionAndStorageMapIDPairsToRemove)
            m_storageAreasByConnection.remove(connectionStorageAreaPair);

        semaphore.signal();
    });
    semaphore.wait(std::numeric_limits<double>::max());
}

StorageManager::StorageArea* StorageManager::findStorageArea(IPC::Connection& connection, uint64_t storageMapID) const
{
    std::pair<IPC::Connection*, uint64_t> connectionAndStorageMapIDPair(&connection, storageMapID);

    if (!m_storageAreasByConnection.isValidKey(connectionAndStorageMapIDPair))
        return nullptr;

    return m_storageAreasByConnection.get(connectionAndStorageMapIDPair);
}

StorageManager::LocalStorageNamespace* StorageManager::getOrCreateLocalStorageNamespace(uint64_t storageNamespaceID)
{
    if (!m_localStorageNamespaces.isValidKey(storageNamespaceID))
        return 0;

    auto& slot = m_localStorageNamespaces.add(storageNamespaceID, nullptr).iterator->value;
    if (!slot)
        slot = LocalStorageNamespace::create(this, storageNamespaceID);

    return slot.get();
}

StorageManager::TransientLocalStorageNamespace* StorageManager::getOrCreateTransientLocalStorageNamespace(uint64_t storageNamespaceID, WebCore::SecurityOrigin& topLevelOrigin)
{
    if (!m_transientLocalStorageNamespaces.isValidKey({ storageNamespaceID, &topLevelOrigin }))
        return nullptr;

    auto& slot = m_transientLocalStorageNamespaces.add({ storageNamespaceID, &topLevelOrigin }, nullptr).iterator->value;
    if (!slot)
        slot = TransientLocalStorageNamespace::create();

    return slot.get();
}

} // namespace WebKit

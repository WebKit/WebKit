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
#include "StorageManager.h"

#include "LocalStorageDatabase.h"
#include "LocalStorageDatabaseTracker.h"
#include "LocalStorageNamespace.h"
#include "SessionStorageNamespace.h"
#include "StorageArea.h"
#include "StorageAreaMapMessages.h"
#include "StorageManagerMessages.h"
#include "TransientLocalStorageNamespace.h"
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

// Suggested by https://www.w3.org/TR/webstorage/#disk-space
const unsigned StorageManager::localStorageDatabaseQuotaInBytes = 5 * 1024 * 1024;

StorageManager::StorageManager(String&& localStorageDirectory)
    : m_queue(WorkQueue::create("com.apple.WebKit.StorageManager"))
{
    ASSERT(RunLoop::isMain());

    // Make sure the encoding is initialized before we start dispatching things to the queue.
    UTF8Encoding();
    if (!localStorageDirectory.isNull())
        m_localStorageDatabaseTracker = LocalStorageDatabaseTracker::create(m_queue.copyRef(), WTFMove(localStorageDirectory));
}

StorageManager::~StorageManager()
{
    ASSERT(RunLoop::isMain());
}

void StorageManager::createSessionStorageNamespace(uint64_t storageNamespaceID, unsigned quotaInBytes)
{
    m_queue->dispatch([this, protectedThis = makeRef(*this), storageNamespaceID, quotaInBytes]() mutable {
        m_sessionStorageNamespaces.ensure(storageNamespaceID, [quotaInBytes] {
            return SessionStorageNamespace::create(quotaInBytes);
        });
    });
}

void StorageManager::destroySessionStorageNamespace(uint64_t storageNamespaceID)
{
    m_queue->dispatch([this, protectedThis = makeRef(*this), storageNamespaceID] {
        ASSERT(m_sessionStorageNamespaces.contains(storageNamespaceID));
        if (auto* sessionStorageNamespace = m_sessionStorageNamespaces.get(storageNamespaceID)) {
            if (sessionStorageNamespace->allowedConnections().isEmpty())
                m_sessionStorageNamespaces.remove(storageNamespaceID);
        }
    });
}

void StorageManager::addAllowedSessionStorageNamespaceConnection(uint64_t storageNamespaceID, IPC::Connection& allowedConnection)
{
    auto allowedConnectionID = allowedConnection.uniqueID();
    auto addResult = m_connections.add(allowedConnectionID);
    if (addResult.isNewEntry)
        allowedConnection.addWorkQueueMessageReceiver(Messages::StorageManager::messageReceiverName(), m_queue.get(), this);

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
        if (auto* sessionStorageNamespace = m_sessionStorageNamespaces.get(storageNamespaceID))
            sessionStorageNamespace->removeAllowedConnection(allowedConnectionID);
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

        if (!m_localStorageDatabaseTracker) {
            if (auto* localStorageNamespace = m_localStorageNamespaces.get(storageNamespaceID)) {
                LocalStorageNamespace* newlocalStorageNamespace = getOrCreateLocalStorageNamespace(newStorageNamespaceID);
                localStorageNamespace->cloneTo(*newlocalStorageNamespace);
            }
        }
    });
}

void StorageManager::processDidCloseConnection(IPC::Connection& connection)
{
    if (m_connections.remove(connection.uniqueID()))
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

        Vector<uint64_t> sessionStorageNameSpaceIDsToRemove;
        for (auto& sessionStorageNamespace : m_sessionStorageNamespaces) {
            if (sessionStorageNamespace.value->allowedConnections().contains(connectionID))
                sessionStorageNamespace.value->removeAllowedConnection(connectionID);
            
            if (sessionStorageNamespace.value->allowedConnections().isEmpty())
                sessionStorageNameSpaceIDsToRemove.append(sessionStorageNamespace.key);
        }
        
        for (auto id : sessionStorageNameSpaceIDsToRemove)
            m_sessionStorageNamespaces.remove(id);
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
    m_queue->dispatch([this, protectedThis = makeRef(*this), copiedOrigins = crossThreadCopy(origins), completionHandler = WTFMove(completionHandler)]() mutable {
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

        if (m_localStorageDatabaseTracker) {
            for (auto& origin : m_localStorageDatabaseTracker->origins())
                origins.add(origin.isolatedCopy());
        } else {
            for (const auto& localStorageNameSpace : m_localStorageNamespaces.values()) {
                for (auto& origin : localStorageNameSpace->ephemeralOrigins())
                    origins.add(origin.isolatedCopy());
            }
        }

        for (auto& transientLocalStorageNamespace : m_transientLocalStorageNamespaces.values()) {
            for (auto& origin : transientLocalStorageNamespace->origins())
                origins.add(origin.isolatedCopy());
        }

        RunLoop::main().dispatch([origins = WTFMove(origins), completionHandler = WTFMove(completionHandler)]() mutable {
            completionHandler(WTFMove(origins));
        });
    });
}

void StorageManager::getLocalStorageOriginDetails(Function<void(Vector<LocalStorageDatabaseTracker::OriginDetails>&&)>&& completionHandler)
{
    m_queue->dispatch([this, protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)]() mutable {
        Vector<LocalStorageDatabaseTracker::OriginDetails> originDetails;
        if (m_localStorageDatabaseTracker)
            originDetails = m_localStorageDatabaseTracker->originDetails().isolatedCopy();

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

        if (m_localStorageDatabaseTracker)
            m_localStorageDatabaseTracker->deleteDatabaseWithOrigin(copiedOrigin);
    });
}

void StorageManager::deleteLocalStorageOriginsModifiedSince(WallTime time, Function<void()>&& completionHandler)
{
    m_queue->dispatch([this, protectedThis = makeRef(*this), time, completionHandler = WTFMove(completionHandler)]() mutable {
        if (m_localStorageDatabaseTracker) {
            auto originsToDelete = m_localStorageDatabaseTracker->databasesModifiedSince(time);
            
            for (auto& transientLocalStorageNamespace : m_transientLocalStorageNamespaces.values())
                transientLocalStorageNamespace->clearAllStorageAreas();

            for (const auto& origin : originsToDelete) {
                for (auto& localStorageNamespace : m_localStorageNamespaces.values())
                    localStorageNamespace->clearStorageAreasMatchingOrigin(origin);
                
                m_localStorageDatabaseTracker->deleteDatabaseWithOrigin(origin);
            }
        } else {
            for (auto& localStorageNamespace : m_localStorageNamespaces.values())
                localStorageNamespace->clearAllStorageAreas();
        }

        RunLoop::main().dispatch(WTFMove(completionHandler));
    });
}

void StorageManager::deleteLocalStorageEntriesForOrigins(const Vector<WebCore::SecurityOriginData>& origins, Function<void()>&& completionHandler)
{
    m_queue->dispatch([this, protectedThis = makeRef(*this), copiedOrigins = crossThreadCopy(origins), completionHandler = WTFMove(completionHandler)]() mutable {
        for (auto& origin : copiedOrigins) {
            for (auto& localStorageNamespace : m_localStorageNamespaces.values())
                localStorageNamespace->clearStorageAreasMatchingOrigin(origin);

            for (auto& transientLocalStorageNamespace : m_transientLocalStorageNamespaces.values())
                transientLocalStorageNamespace->clearStorageAreasMatchingOrigin(origin);

            if (m_localStorageDatabaseTracker)
                m_localStorageDatabaseTracker->deleteDatabaseWithOrigin(origin);
        }

        RunLoop::main().dispatch(WTFMove(completionHandler));
    });
}

void StorageManager::createLocalStorageMap(IPC::Connection& connection, uint64_t storageMapID, uint64_t storageNamespaceID, SecurityOriginData&& securityOriginData)
{
    ASSERT(!RunLoop::isMain());
    auto connectionID = connection.uniqueID();
    std::pair<IPC::Connection::UniqueID, uint64_t> connectionAndStorageMapIDPair(connectionID, storageMapID);

    ASSERT((HashMap<std::pair<IPC::Connection::UniqueID, uint64_t>, RefPtr<StorageArea>>::isValidKey(connectionAndStorageMapIDPair)));

    auto result = m_storageAreasByConnection.add(connectionAndStorageMapIDPair, nullptr);
    ASSERT(result.isNewEntry);
    ASSERT((HashMap<uint64_t, RefPtr<LocalStorageNamespace>>::isValidKey(storageNamespaceID)));

    LocalStorageNamespace* localStorageNamespace = getOrCreateLocalStorageNamespace(storageNamespaceID);
    ASSERT(localStorageNamespace);

    auto storageArea = localStorageNamespace->getOrCreateStorageArea(WTFMove(securityOriginData), m_localStorageDatabaseTracker ? LocalStorageNamespace::IsEphemeral::No : LocalStorageNamespace::IsEphemeral::Yes);
    storageArea->addListener(connectionID, storageMapID);

    result.iterator->value = WTFMove(storageArea);
}

void StorageManager::createTransientLocalStorageMap(IPC::Connection& connection, uint64_t storageMapID, uint64_t storageNamespaceID, SecurityOriginData&& topLevelOriginData, SecurityOriginData&& origin)
{
    ASSERT(!RunLoop::isMain());
    auto connectionID = connection.uniqueID();

    ASSERT(m_storageAreasByConnection.isValidKey({ connectionID, storageMapID }));

    // See if we already have session storage for this connection/origin combo.
    // If so, update the map with the new ID, otherwise keep on trucking.
    for (auto it = m_storageAreasByConnection.begin(), end = m_storageAreasByConnection.end(); it != end; ++it) {
        if (it->key.first != connectionID)
            continue;
        Ref<StorageArea> area = *it->value;
        if (!area->isEphemeral())
            continue;
        if (!origin.securityOrigin()->isSameSchemeHostPort(area->securityOrigin().securityOrigin().get()))
            continue;
        area->addListener(connectionID, storageMapID);
        // If the storageMapID used as key in m_storageAreasByConnection is no longer one of the StorageArea's listeners, then this means
        // that destroyStorageMap() was already called for that storageMapID but it decided not to remove it from m_storageAreasByConnection
        // so that we could reuse it later on for the same connection/origin combo. In this case, it is safe to remove the previous
        // storageMapID from m_storageAreasByConnection.
        if (!area->hasListener(connectionID, it->key.second))
            m_storageAreasByConnection.remove(it);
        m_storageAreasByConnection.add({ connectionID, storageMapID }, WTFMove(area));
        return;
    }

    auto& slot = m_storageAreasByConnection.add({ connectionID, storageMapID }, nullptr).iterator->value;
    ASSERT(!slot);

    auto* transientLocalStorageNamespace = getOrCreateTransientLocalStorageNamespace(storageNamespaceID, WTFMove(topLevelOriginData));

    auto storageArea = transientLocalStorageNamespace->getOrCreateStorageArea(WTFMove(origin));
    storageArea->addListener(connectionID, storageMapID);

    slot = WTFMove(storageArea);
}

void StorageManager::createSessionStorageMap(IPC::Connection& connection, uint64_t storageMapID, uint64_t storageNamespaceID, SecurityOriginData&& securityOriginData)
{
    ASSERT(!RunLoop::isMain());
    auto connectionID = connection.uniqueID();
    ASSERT(m_sessionStorageNamespaces.isValidKey(storageNamespaceID));

    SessionStorageNamespace* sessionStorageNamespace = m_sessionStorageNamespaces.get(storageNamespaceID);
    if (!sessionStorageNamespace) {
        // We're getting an incoming message from the web process that's for session storage for a web page
        // that has already been closed, just ignore it.
        return;
    }

    ASSERT(m_storageAreasByConnection.isValidKey({ connectionID, storageMapID }));

    auto& slot = m_storageAreasByConnection.add({ connectionID, storageMapID }, nullptr).iterator->value;
    ASSERT(!slot);
    ASSERT(sessionStorageNamespace->allowedConnections().contains(connectionID));

    auto storageArea = sessionStorageNamespace->getOrCreateStorageArea(WTFMove(securityOriginData));
    storageArea->addListener(connectionID, storageMapID);

    slot = WTFMove(storageArea);
}

void StorageManager::destroyStorageMap(IPC::Connection& connection, uint64_t storageMapID)
{
    ASSERT(!RunLoop::isMain());
    auto connectionID = connection.uniqueID();

    std::pair<IPC::Connection::UniqueID, uint64_t> connectionAndStorageMapIDPair(connectionID, storageMapID);
    ASSERT(m_storageAreasByConnection.isValidKey(connectionAndStorageMapIDPair));

    auto it = m_storageAreasByConnection.find(connectionAndStorageMapIDPair);
    if (it == m_storageAreasByConnection.end()) {
        // The connection has been removed because the last page was closed.
        return;
    }

    it->value->removeListener(connectionID, storageMapID);

    // Don't remove session storage maps. The web process may reconnect and expect the data to still be around.
    if (it->value->isEphemeral())
        return;

    m_storageAreasByConnection.remove(connectionAndStorageMapIDPair);
}

void StorageManager::prewarm(IPC::Connection& connection, uint64_t storageMapID)
{
    ASSERT(!RunLoop::isMain());
    if (auto* storageArea = findStorageArea(connection, storageMapID))
        storageArea->openDatabaseAndImportItemsIfNeeded();
}

void StorageManager::getValues(IPC::Connection& connection, uint64_t storageMapID, uint64_t storageMapSeed, GetValuesCallback&& completionHandler)
{
    ASSERT(!RunLoop::isMain());
    auto* storageArea = findStorageArea(connection, storageMapID);

    // This is a session storage area for a page that has already been closed. Ignore it.
    if (!storageArea)
        return completionHandler({ });

    completionHandler(storageArea->items());
    connection.send(Messages::StorageAreaMap::DidGetValues(storageMapSeed), storageMapID);
}

void StorageManager::setItem(IPC::Connection& connection, WebCore::SecurityOriginData&& securityOriginData, uint64_t storageMapID, uint64_t sourceStorageAreaID, uint64_t storageMapSeed, const String& key, const String& value, const String& urlString)
{
    ASSERT(!RunLoop::isMain());
    auto* storageArea = findStorageArea(connection, storageMapID);

    // This is a session storage area for a page that has already been closed. Ignore it.
    if (!storageArea)
        return;

    bool quotaError;
    storageArea->setItem(connection.uniqueID(), sourceStorageAreaID, key, value, urlString, quotaError);
    connection.send(Messages::StorageAreaMap::DidSetItem(storageMapSeed, key, quotaError), storageMapID);
}

void StorageManager::setItems(IPC::Connection& connection, uint64_t storageMapID, const HashMap<String, String>& items)
{
    ASSERT(!RunLoop::isMain());
    if (auto* storageArea = findStorageArea(connection, storageMapID))
        storageArea->setItems(items);
}

void StorageManager::removeItem(IPC::Connection& connection, WebCore::SecurityOriginData&& securityOriginData, uint64_t storageMapID, uint64_t sourceStorageAreaID, uint64_t storageMapSeed, const String& key, const String& urlString)
{
    ASSERT(!RunLoop::isMain());
    auto* storageArea = findStorageArea(connection, storageMapID);

    // This is a session storage area for a page that has already been closed. Ignore it.
    if (!storageArea)
        return;

    storageArea->removeItem(connection.uniqueID(), sourceStorageAreaID, key, urlString);
    connection.send(Messages::StorageAreaMap::DidRemoveItem(storageMapSeed, key), storageMapID);
}

void StorageManager::clear(IPC::Connection& connection, WebCore::SecurityOriginData&& securityOriginData, uint64_t storageMapID, uint64_t sourceStorageAreaID, uint64_t storageMapSeed, const String& urlString)
{
    ASSERT(!RunLoop::isMain());
    auto* storageArea = findStorageArea(connection, storageMapID);

    // This is a session storage area for a page that has already been closed. Ignore it.
    if (!storageArea)
        return;

    storageArea->clear(connection.uniqueID(), sourceStorageAreaID, urlString);
    connection.send(Messages::StorageAreaMap::DidClear(storageMapSeed), storageMapID);
}

void StorageManager::waitUntilTasksFinished()
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

        m_localStorageNamespaces.clear();

        semaphore.signal();
    });
    semaphore.wait();
}

void StorageManager::suspend(CompletionHandler<void()>&& completionHandler)
{
    CompletionHandlerCallingScope completionHandlerCaller(WTFMove(completionHandler));
    if (!m_localStorageDatabaseTracker)
        return;

    Locker<Lock> stateLocker(m_stateLock);
    if (m_state != State::Running)
        return;
    m_state = State::WillSuspend;

    m_queue->dispatch([this, protectedThis = makeRef(*this), completionHandler = completionHandlerCaller.release()] () mutable {
        Locker<Lock> stateLocker(m_stateLock);
        ASSERT(m_state != State::Suspended);

        if (m_state != State::WillSuspend) {
            RunLoop::main().dispatch(WTFMove(completionHandler));
            return;
        }

        m_state = State::Suspended;
        RunLoop::main().dispatch(WTFMove(completionHandler));
        
        while (m_state == State::Suspended)
            m_stateChangeCondition.wait(m_stateLock);
        ASSERT(m_state == State::Running);
    });
}

void StorageManager::resume()
{
    if (!m_localStorageDatabaseTracker)
        return;

    Locker<Lock> stateLocker(m_stateLock);
    auto previousState = m_state;
    m_state = State::Running;
    if (previousState == State::Suspended)
        m_stateChangeCondition.notifyOne();
}

StorageArea* StorageManager::findStorageArea(IPC::Connection& connection, uint64_t storageMapID) const
{
    std::pair<IPC::Connection::UniqueID, uint64_t> connectionAndStorageMapIDPair(connection.uniqueID(), storageMapID);

    if (!m_storageAreasByConnection.isValidKey(connectionAndStorageMapIDPair))
        return nullptr;

    return m_storageAreasByConnection.get(connectionAndStorageMapIDPair);
}

LocalStorageNamespace* StorageManager::getOrCreateLocalStorageNamespace(uint64_t storageNamespaceID)
{
    if (!m_localStorageNamespaces.isValidKey(storageNamespaceID))
        return nullptr;

    return m_localStorageNamespaces.ensure(storageNamespaceID, [this, storageNamespaceID]() {
        return LocalStorageNamespace::create(*this, storageNamespaceID);
    }).iterator->value.get();
}

TransientLocalStorageNamespace* StorageManager::getOrCreateTransientLocalStorageNamespace(uint64_t storageNamespaceID, WebCore::SecurityOriginData&& topLevelOrigin)
{
    if (!m_transientLocalStorageNamespaces.isValidKey({ storageNamespaceID, topLevelOrigin }))
        return nullptr;

    return m_transientLocalStorageNamespaces.ensure({ storageNamespaceID, WTFMove(topLevelOrigin) }, [](){
        return TransientLocalStorageNamespace::create();
    }).iterator->value.get();
}

} // namespace WebKit

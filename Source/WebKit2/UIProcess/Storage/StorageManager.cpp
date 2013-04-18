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

#include "SecurityOriginData.h"
#include "StorageAreaMapMessages.h"
#include "StorageManagerMessages.h"
#include "WebProcessProxy.h"
#include "WorkQueue.h"
#include <WebCore/SecurityOriginHash.h>
#include <WebCore/StorageMap.h>

using namespace WebCore;

namespace WebKit {

class StorageManager::StorageArea : public ThreadSafeRefCounted<StorageManager::StorageArea> {
public:
    static PassRefPtr<StorageArea> create(unsigned quotaInBytes);
    ~StorageArea();

    void addListener(CoreIPC::Connection*, uint64_t storageMapID);
    void removeListener(CoreIPC::Connection*, uint64_t storageMapID);

    PassRefPtr<StorageArea> clone() const;

    void setItem(CoreIPC::Connection* sourceConnection, uint64_t sourceStorageAreaID, const String& key, const String& value, const String& urlString, bool& quotaException);
    void removeItem(CoreIPC::Connection* sourceConnection, uint64_t sourceStorageAreaID, const String& key, const String& urlString);
    void clear(CoreIPC::Connection* sourceConnection, uint64_t sourceStorageAreaID, const String& urlString);

    const HashMap<String, String>& items() const { return m_storageMap->items(); }

private:
    explicit StorageArea(unsigned quotaInBytes);

    void dispatchEvents(CoreIPC::Connection* sourceConnection, uint64_t sourceStorageAreaID, const String& key, const String& oldValue, const String& newValue, const String& urlString) const;

    unsigned m_quotaInBytes;
    RefPtr<StorageMap> m_storageMap;
    HashSet<std::pair<RefPtr<CoreIPC::Connection>, uint64_t> > m_eventListeners;
};

PassRefPtr<StorageManager::StorageArea> StorageManager::StorageArea::create(unsigned quotaInBytes)
{
    return adoptRef(new StorageArea(quotaInBytes));
}

StorageManager::StorageArea::StorageArea(unsigned quotaInBytes)
    : m_quotaInBytes(quotaInBytes)
    , m_storageMap(StorageMap::create(m_quotaInBytes))
{
}

StorageManager::StorageArea::~StorageArea()
{
    ASSERT(m_eventListeners.isEmpty());
}

void StorageManager::StorageArea::addListener(CoreIPC::Connection* connection, uint64_t storageMapID)
{
    ASSERT(!m_eventListeners.contains(std::make_pair(connection, storageMapID)));
    m_eventListeners.add(std::make_pair(connection, storageMapID));
}

void StorageManager::StorageArea::removeListener(CoreIPC::Connection* connection, uint64_t storageMapID)
{
    ASSERT(m_eventListeners.contains(std::make_pair(connection, storageMapID)));
    m_eventListeners.remove(std::make_pair(connection, storageMapID));
}

PassRefPtr<StorageManager::StorageArea> StorageManager::StorageArea::clone() const
{
    RefPtr<StorageArea> storageArea = StorageArea::create(m_quotaInBytes);
    storageArea->m_storageMap = m_storageMap;

    return storageArea.release();
}

void StorageManager::StorageArea::setItem(CoreIPC::Connection* sourceConnection, uint64_t sourceStorageAreaID, const String& key, const String& value, const String& urlString, bool& quotaException)
{
    String oldValue;

    RefPtr<StorageMap> newStorageMap = m_storageMap->setItem(key, value, oldValue, quotaException);
    if (newStorageMap)
        m_storageMap = newStorageMap.release();

    if (!quotaException)
        dispatchEvents(sourceConnection, sourceStorageAreaID, key, oldValue, value, urlString);
}

void StorageManager::StorageArea::removeItem(CoreIPC::Connection* sourceConnection, uint64_t sourceStorageAreaID, const String& key, const String& urlString)
{
    String oldValue;
    RefPtr<StorageMap> newStorageMap = m_storageMap->removeItem(key, oldValue);
    if (newStorageMap)
        m_storageMap = newStorageMap.release();

    if (oldValue.isNull())
        return;

    dispatchEvents(sourceConnection, sourceStorageAreaID, key, oldValue, String(), urlString);
}

void StorageManager::StorageArea::clear(CoreIPC::Connection* sourceConnection, uint64_t sourceStorageAreaID, const String& urlString)
{
    if (!m_storageMap->length())
        return;

    m_storageMap = StorageMap::create(m_quotaInBytes);

    dispatchEvents(sourceConnection, sourceStorageAreaID, String(), String(), String(), urlString);
}

void StorageManager::StorageArea::dispatchEvents(CoreIPC::Connection* sourceConnection, uint64_t sourceStorageAreaID, const String& key, const String& oldValue, const String& newValue, const String& urlString) const
{
    for (HashSet<std::pair<RefPtr<CoreIPC::Connection>, uint64_t> >::const_iterator it = m_eventListeners.begin(), end = m_eventListeners.end(); it != end; ++it) {
        // FIXME: If this is sent to another process, the source storage area ID will be bogus, since storage are IDs are per process.
        it->first->send(Messages::StorageAreaMap::DispatchStorageEvent(sourceStorageAreaID, key, oldValue, newValue, urlString), it->second);
    }
}

class StorageManager::SessionStorageNamespace : public ThreadSafeRefCounted<SessionStorageNamespace> {
public:
    static PassRefPtr<SessionStorageNamespace> create(CoreIPC::Connection* allowedConnection, unsigned quotaInBytes);
    ~SessionStorageNamespace();

    bool isEmpty() const { return m_storageAreaMap.isEmpty(); }

    CoreIPC::Connection* allowedConnection() const { return m_allowedConnection.get(); }
    void setAllowedConnection(CoreIPC::Connection*);

    PassRefPtr<StorageArea> getOrCreateStorageArea(PassRefPtr<SecurityOrigin>);

    void cloneTo(SessionStorageNamespace& newSessionStorageNamespace);

private:
    SessionStorageNamespace(CoreIPC::Connection* allowedConnection, unsigned quotaInBytes);

    RefPtr<CoreIPC::Connection> m_allowedConnection;
    unsigned m_quotaInBytes;

    HashMap<RefPtr<SecurityOrigin>, RefPtr<StorageArea> > m_storageAreaMap;
};

PassRefPtr<StorageManager::SessionStorageNamespace> StorageManager::SessionStorageNamespace::create(CoreIPC::Connection* allowedConnection, unsigned quotaInBytes)
{
    return adoptRef(new SessionStorageNamespace(allowedConnection, quotaInBytes));
}

StorageManager::SessionStorageNamespace::SessionStorageNamespace(CoreIPC::Connection* allowedConnection, unsigned quotaInBytes)
    : m_allowedConnection(allowedConnection)
    , m_quotaInBytes(quotaInBytes)
{
}

StorageManager::SessionStorageNamespace::~SessionStorageNamespace()
{
}

void StorageManager::SessionStorageNamespace::setAllowedConnection(CoreIPC::Connection* allowedConnection)
{
    ASSERT(!allowedConnection || !m_allowedConnection);

    m_allowedConnection = allowedConnection;
}

PassRefPtr<StorageManager::StorageArea> StorageManager::SessionStorageNamespace::getOrCreateStorageArea(PassRefPtr<SecurityOrigin> securityOrigin)
{
    HashMap<RefPtr<SecurityOrigin>, RefPtr<StorageArea> >::AddResult result = m_storageAreaMap.add(securityOrigin, 0);
    if (result.isNewEntry)
        result.iterator->value = StorageArea::create(m_quotaInBytes);

    return result.iterator->value;
}

void StorageManager::SessionStorageNamespace::cloneTo(SessionStorageNamespace& newSessionStorageNamespace)
{
    ASSERT_UNUSED(newSessionStorageNamespace, newSessionStorageNamespace.isEmpty());

    for (HashMap<RefPtr<SecurityOrigin>, RefPtr<StorageArea> >::const_iterator it = m_storageAreaMap.begin(), end = m_storageAreaMap.end(); it != end; ++it)
        newSessionStorageNamespace.m_storageAreaMap.add(it->key, it->value->clone());
}

PassRefPtr<StorageManager> StorageManager::create()
{
    return adoptRef(new StorageManager);
}

StorageManager::StorageManager()
    : m_queue(WorkQueue::create("com.apple.WebKit.StorageManager"))
{
}

StorageManager::~StorageManager()
{
}

void StorageManager::setLocalStorageDirectory(const String& localStorageDirectory)
{
    m_queue->dispatch(bind(&StorageManager::setLocalStorageDirectoryInternal, this, localStorageDirectory.isolatedCopy()));
}

void StorageManager::createSessionStorageNamespace(uint64_t storageNamespaceID, CoreIPC::Connection* allowedConnection, unsigned quotaInBytes)
{
    m_queue->dispatch(bind(&StorageManager::createSessionStorageNamespaceInternal, this, storageNamespaceID, RefPtr<CoreIPC::Connection>(allowedConnection), quotaInBytes));
}

void StorageManager::destroySessionStorageNamespace(uint64_t storageNamespaceID)
{
    m_queue->dispatch(bind(&StorageManager::destroySessionStorageNamespaceInternal, this, storageNamespaceID));
}

void StorageManager::setAllowedSessionStorageNamespaceConnection(uint64_t storageNamespaceID, CoreIPC::Connection* allowedConnection)
{
    m_queue->dispatch(bind(&StorageManager::setAllowedSessionStorageNamespaceConnectionInternal, this, storageNamespaceID, RefPtr<CoreIPC::Connection>(allowedConnection)));
}

void StorageManager::cloneSessionStorageNamespace(uint64_t storageNamespaceID, uint64_t newStorageNamespaceID)
{
    m_queue->dispatch(bind(&StorageManager::cloneSessionStorageNamespaceInternal, this, storageNamespaceID, newStorageNamespaceID));
}

void StorageManager::processWillOpenConnection(WebProcessProxy* webProcessProxy)
{
    webProcessProxy->connection()->addWorkQueueMessageReceiver(Messages::StorageManager::messageReceiverName(), m_queue.get(), this);
}

void StorageManager::processWillCloseConnection(WebProcessProxy* webProcessProxy)
{
    webProcessProxy->connection()->removeWorkQueueMessageReceiver(Messages::StorageManager::messageReceiverName());

    m_queue->dispatch(bind(&StorageManager::invalidateConnectionInternal, this, RefPtr<CoreIPC::Connection>(webProcessProxy->connection())));
}

void StorageManager::createLocalStorageMap(CoreIPC::Connection*, uint64_t storageMapID, uint64_t storageNamespaceID, const SecurityOriginData&)
{
    // FIXME: Implement.
}

void StorageManager::createSessionStorageMap(CoreIPC::Connection* connection, uint64_t storageMapID, uint64_t storageNamespaceID, const SecurityOriginData& securityOriginData)
{
    std::pair<RefPtr<CoreIPC::Connection>, uint64_t> connectionAndStorageMapIDPair(connection, storageMapID);

    // FIXME: This should be a message check.
    ASSERT((HashMap<std::pair<RefPtr<CoreIPC::Connection>, uint64_t>, RefPtr<StorageArea> >::isValidKey(connectionAndStorageMapIDPair)));

    HashMap<std::pair<RefPtr<CoreIPC::Connection>, uint64_t>, RefPtr<StorageArea> >::AddResult result = m_storageAreasByConnection.add(connectionAndStorageMapIDPair, 0);

    // FIXME: This should be a message check.
    ASSERT(result.isNewEntry);

    ASSERT((HashMap<uint64_t, RefPtr<SessionStorageNamespace> >::isValidKey(storageNamespaceID)));
    SessionStorageNamespace* sessionStorageNamespace = m_sessionStorageNamespaces.get(storageNamespaceID).get();

    // FIXME: These should be message checks.
    ASSERT(sessionStorageNamespace);
    ASSERT(connection == sessionStorageNamespace->allowedConnection());

    RefPtr<StorageArea> storageArea = sessionStorageNamespace->getOrCreateStorageArea(securityOriginData.securityOrigin());
    storageArea->addListener(connection, storageMapID);

    result.iterator->value = storageArea.release();
}

void StorageManager::destroyStorageMap(CoreIPC::Connection* connection, uint64_t storageMapID)
{
    std::pair<RefPtr<CoreIPC::Connection>, uint64_t> connectionAndStorageMapIDPair(connection, storageMapID);

    // FIXME: This should be a message check.
    ASSERT((HashMap<std::pair<RefPtr<CoreIPC::Connection>, uint64_t>, RefPtr<StorageArea> >::isValidKey(connectionAndStorageMapIDPair)));

    HashMap<std::pair<RefPtr<CoreIPC::Connection>, uint64_t>, RefPtr<StorageArea> >::iterator it = m_storageAreasByConnection.find(connectionAndStorageMapIDPair);

    // FIXME: This should be a message check.
    ASSERT(it != m_storageAreasByConnection.end());

    it->value->removeListener(connection, storageMapID);
    m_storageAreasByConnection.remove(connectionAndStorageMapIDPair);
}

void StorageManager::getValues(CoreIPC::Connection* connection, uint64_t storageMapID, HashMap<String, String>& values)
{
    StorageArea* storageArea = findStorageArea(connection, storageMapID);

    // FIXME: This should be a message check.
    ASSERT(storageArea);

    values = storageArea->items();
}

void StorageManager::setItem(CoreIPC::Connection* connection, uint64_t storageMapID, uint64_t sourceStorageAreaID, const String& key, const String& value, const String& urlString)
{
    StorageArea* storageArea = findStorageArea(connection, storageMapID);

    // FIXME: This should be a message check.
    ASSERT(storageArea);

    bool quotaError;
    storageArea->setItem(connection, sourceStorageAreaID, key, value, urlString, quotaError);
    connection->send(Messages::StorageAreaMap::DidSetItem(key, quotaError), storageMapID);
}

void StorageManager::removeItem(CoreIPC::Connection* connection, uint64_t storageMapID, uint64_t sourceStorageAreaID, const String& key, const String& urlString)
{
    StorageArea* storageArea = findStorageArea(connection, storageMapID);

    // FIXME: This should be a message check.
    ASSERT(storageArea);

    storageArea->removeItem(connection, sourceStorageAreaID, key, urlString);
    connection->send(Messages::StorageAreaMap::DidRemoveItem(key), storageMapID);
}

void StorageManager::clear(CoreIPC::Connection* connection, uint64_t storageMapID, uint64_t sourceStorageAreaID, const String& urlString)
{
    StorageArea* storageArea = findStorageArea(connection, storageMapID);

    // FIXME: This should be a message check.
    ASSERT(storageArea);

    storageArea->clear(connection, sourceStorageAreaID, urlString);
    connection->send(Messages::StorageAreaMap::DidClear(), storageMapID);
}

void StorageManager::setLocalStorageDirectoryInternal(const String& localStorageDirectory)
{
    m_localStorageDirectory = localStorageDirectory;
}

void StorageManager::createSessionStorageNamespaceInternal(uint64_t storageNamespaceID, CoreIPC::Connection* allowedConnection, unsigned quotaInBytes)
{
    ASSERT(!m_sessionStorageNamespaces.contains(storageNamespaceID));

    m_sessionStorageNamespaces.set(storageNamespaceID, SessionStorageNamespace::create(allowedConnection, quotaInBytes));
}

void StorageManager::destroySessionStorageNamespaceInternal(uint64_t storageNamespaceID)
{
    ASSERT(m_sessionStorageNamespaces.contains(storageNamespaceID));

    m_sessionStorageNamespaces.remove(storageNamespaceID);
}

void StorageManager::setAllowedSessionStorageNamespaceConnectionInternal(uint64_t storageNamespaceID, CoreIPC::Connection* allowedConnection)
{
    ASSERT(m_sessionStorageNamespaces.contains(storageNamespaceID));

    m_sessionStorageNamespaces.get(storageNamespaceID)->setAllowedConnection(allowedConnection);
}

void StorageManager::cloneSessionStorageNamespaceInternal(uint64_t storageNamespaceID, uint64_t newStorageNamespaceID)
{
    SessionStorageNamespace* sessionStorageNamespace = m_sessionStorageNamespaces.get(storageNamespaceID).get();
    ASSERT(sessionStorageNamespace);

    SessionStorageNamespace* newSessionStorageNamespace = m_sessionStorageNamespaces.get(newStorageNamespaceID).get();
    ASSERT(newSessionStorageNamespace);

    sessionStorageNamespace->cloneTo(*newSessionStorageNamespace);
}

void StorageManager::invalidateConnectionInternal(CoreIPC::Connection* connection)
{
    Vector<std::pair<RefPtr<CoreIPC::Connection>, uint64_t> > connectionAndStorageMapIDPairsToRemove;
    HashMap<std::pair<RefPtr<CoreIPC::Connection>, uint64_t>, RefPtr<StorageArea> > storageAreasByConnection = m_storageAreasByConnection;
    for (HashMap<std::pair<RefPtr<CoreIPC::Connection>, uint64_t>, RefPtr<StorageArea> >::const_iterator it = storageAreasByConnection.begin(), end = storageAreasByConnection.end(); it != end; ++it) {
        if (it->key.first != connection)
            continue;

        it->value->removeListener(it->key.first.get(), it->key.second);
        connectionAndStorageMapIDPairsToRemove.append(it->key);
    }

    for (size_t i = 0; i < connectionAndStorageMapIDPairsToRemove.size(); ++i)
        m_storageAreasByConnection.remove(connectionAndStorageMapIDPairsToRemove[i]);
}

StorageManager::StorageArea* StorageManager::findStorageArea(CoreIPC::Connection* connection, uint64_t storageMapID) const
{
    std::pair<CoreIPC::Connection*, uint64_t> connectionAndStorageMapIDPair(connection, storageMapID);
    if (!HashMap<std::pair<RefPtr<CoreIPC::Connection>, uint64_t>, RefPtr<StorageArea> >::isValidKey(connectionAndStorageMapIDPair))
        return 0;

    return m_storageAreasByConnection.get(connectionAndStorageMapIDPair).get();
}

} // namespace WebKit

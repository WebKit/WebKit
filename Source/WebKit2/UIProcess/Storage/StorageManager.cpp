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
#include "StorageAreaProxyMessages.h"
#include "StorageManagerMessages.h"
#include "WebProcessProxy.h"
#include "WorkQueue.h"
#include <WebCore/SecurityOriginHash.h>

using namespace WebCore;

namespace WebKit {

class StorageManager::StorageArea : public ThreadSafeRefCounted<StorageManager::StorageArea> {
public:
    static PassRefPtr<StorageArea> create();
    ~StorageArea();

    void addListener(CoreIPC::Connection*, uint64_t storageAreaID);
    void removeListener(CoreIPC::Connection*, uint64_t storageAreaID);

    bool setItem(CoreIPC::Connection*, uint64_t storageAreaID, const String& key, const String& value, const String& urlString);

private:
    StorageArea();

    void dispatchEvents(CoreIPC::Connection*, uint64_t storageAreaID, const String& key, const String& oldValue, const String& newValue, const String& urlString) const;

    HashSet<std::pair<RefPtr<CoreIPC::Connection>, uint64_t>> m_eventListeners;
};

PassRefPtr<StorageManager::StorageArea> StorageManager::StorageArea::create()
{
    return adoptRef(new StorageArea());
}

StorageManager::StorageArea::StorageArea()
{
}

StorageManager::StorageArea::~StorageArea()
{
    ASSERT(m_eventListeners.isEmpty());
}

void StorageManager::StorageArea::addListener(CoreIPC::Connection* connection, uint64_t storageAreaID)
{
    ASSERT(!m_eventListeners.contains(std::make_pair(connection, storageAreaID)));
    m_eventListeners.add(std::make_pair(connection, storageAreaID));
}

void StorageManager::StorageArea::removeListener(CoreIPC::Connection* connection, uint64_t storageAreaID)
{
    ASSERT(m_eventListeners.contains(std::make_pair(connection, storageAreaID)));
    m_eventListeners.remove(std::make_pair(connection, storageAreaID));
}

bool StorageManager::StorageArea::setItem(CoreIPC::Connection* connection, uint64_t storageAreaID, const String& key, const String& value, const String& urlString)
{
    // FIXME: Actually set the item.

    String oldValue;
    dispatchEvents(connection, storageAreaID, key, oldValue, value, urlString);
    
    return true;
}

void StorageManager::StorageArea::dispatchEvents(CoreIPC::Connection* connection, uint64_t storageAreaID, const String& key, const String& oldValue, const String& newValue, const String& urlString) const
{
    for (HashSet<std::pair<RefPtr<CoreIPC::Connection>, uint64_t> >::const_iterator it = m_eventListeners.begin(), end = m_eventListeners.end(); it != end; ++it) {
        if (it->first == connection && it->second == storageAreaID) {
            // We don't want to dispatch events to the storage area that originated the event.
            continue;
        }

        it->first->send(Messages::StorageAreaProxy::DispatchStorageEvent(key, oldValue, newValue, urlString), it->second);
    }
}

class StorageManager::SessionStorageNamespace : public ThreadSafeRefCounted<SessionStorageNamespace> {
public:
    static PassRefPtr<SessionStorageNamespace> create(CoreIPC::Connection* allowedConnection);
    ~SessionStorageNamespace();

    bool isEmpty() const { return m_storageAreaMap.isEmpty(); }

    CoreIPC::Connection* allowedConnection() const { return m_allowedConnection.get(); }
    void setAllowedConnection(CoreIPC::Connection*);

    PassRefPtr<StorageArea> getOrCreateStorageArea(PassRefPtr<SecurityOrigin>);

    void cloneTo(SessionStorageNamespace& newSessionStorageNamespace);

private:
    explicit SessionStorageNamespace(CoreIPC::Connection* allowedConnection);

    RefPtr<CoreIPC::Connection> m_allowedConnection;
    HashMap<RefPtr<SecurityOrigin>, RefPtr<StorageArea> > m_storageAreaMap;
};

PassRefPtr<StorageManager::SessionStorageNamespace> StorageManager::SessionStorageNamespace::create(CoreIPC::Connection* allowedConnection)
{
    return adoptRef(new SessionStorageNamespace(allowedConnection));
}

StorageManager::SessionStorageNamespace::SessionStorageNamespace(CoreIPC::Connection* allowedConnection)
    : m_allowedConnection(allowedConnection)
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
        result.iterator->value = StorageArea::create();

    return result.iterator->value;
}

void StorageManager::SessionStorageNamespace::cloneTo(SessionStorageNamespace& newSessionStorageNamespace)
{
    ASSERT(newSessionStorageNamespace.isEmpty());

    // FIXME: Implement.
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

void StorageManager::createSessionStorageNamespace(uint64_t storageNamespaceID, CoreIPC::Connection* allowedConnection)
{
    m_queue->dispatch(bind(&StorageManager::createSessionStorageNamespaceInternal, this, storageNamespaceID, RefPtr<CoreIPC::Connection>(allowedConnection)));
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
}

void StorageManager::createStorageArea(CoreIPC::Connection* connection, uint64_t storageAreaID, uint64_t storageNamespaceID, const SecurityOriginData& securityOriginData)
{
    std::pair<RefPtr<CoreIPC::Connection>, uint64_t> connectionAndStorageAreaIDPair(connection, storageAreaID);

    // FIXME: This should be a message check.
    ASSERT((HashMap<std::pair<RefPtr<CoreIPC::Connection>, uint64_t>, RefPtr<StorageArea> >::isValidKey(connectionAndStorageAreaIDPair)));

    HashMap<std::pair<RefPtr<CoreIPC::Connection>, uint64_t>, RefPtr<StorageArea> >::AddResult result = m_storageAreas.add(connectionAndStorageAreaIDPair, 0);

    // FIXME: This should be a message check.
    ASSERT(result.isNewEntry);

    if (!storageNamespaceID) {
        // FIXME: This is a local storage namespace. Do something.
        ASSERT_NOT_REACHED();
    }

    ASSERT((HashMap<uint64_t, RefPtr<SessionStorageNamespace> >::isValidKey(storageNamespaceID)));
    SessionStorageNamespace* sessionStorageNamespace = m_sessionStorageNamespaces.get(storageNamespaceID).get();

    // FIXME: These should be message checks.
    ASSERT(sessionStorageNamespace);
    ASSERT(connection == sessionStorageNamespace->allowedConnection());

    RefPtr<StorageArea> storageArea = sessionStorageNamespace->getOrCreateStorageArea(securityOriginData.securityOrigin());
    storageArea->addListener(connection, storageAreaID);

    result.iterator->value = storageArea.release();
}

void StorageManager::destroyStorageArea(CoreIPC::Connection* connection, uint64_t storageAreaID)
{
    std::pair<RefPtr<CoreIPC::Connection>, uint64_t> connectionAndStorageAreaIDPair(connection, storageAreaID);

    // FIXME: This should be a message check.
    ASSERT((HashMap<std::pair<RefPtr<CoreIPC::Connection>, uint64_t>, RefPtr<StorageArea> >::isValidKey(connectionAndStorageAreaIDPair)));

    HashMap<std::pair<RefPtr<CoreIPC::Connection>, uint64_t>, RefPtr<StorageArea> >::iterator it = m_storageAreas.find(connectionAndStorageAreaIDPair);

    // FIXME: This should be a message check.
    ASSERT(it != m_storageAreas.end());

    it->value->removeListener(connection, storageAreaID);

    m_storageAreas.remove(connectionAndStorageAreaIDPair);
}

void StorageManager::getValues(CoreIPC::Connection*, uint64_t, HashMap<String, String>&)
{
    // FIXME: Implement this.
}

void StorageManager::setItem(CoreIPC::Connection* connection, uint64_t storageAreaID, const String& key, const String& value, const String& urlString)
{
    StorageArea* storageArea = findStorageArea(connection, storageAreaID);

    // FIXME: This should be a message check.
    ASSERT(storageArea);

    bool quotaError = storageArea->setItem(connection, storageAreaID, key, value, urlString);

    connection->send(Messages::StorageAreaProxy::DidSetItem(key, quotaError), storageAreaID);
}

void StorageManager::createSessionStorageNamespaceInternal(uint64_t storageNamespaceID, CoreIPC::Connection* allowedConnection)
{
    ASSERT(!m_sessionStorageNamespaces.contains(storageNamespaceID));

    m_sessionStorageNamespaces.set(storageNamespaceID, SessionStorageNamespace::create(allowedConnection));
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

StorageManager::StorageArea* StorageManager::findStorageArea(CoreIPC::Connection* connection, uint64_t storageAreaID) const
{
    std::pair<CoreIPC::Connection*, uint64_t> connectionAndStorageAreaIDPair(connection, storageAreaID);
    if (!HashMap<std::pair<RefPtr<CoreIPC::Connection>, uint64_t>, RefPtr<StorageArea> >::isValidKey(connectionAndStorageAreaIDPair))
        return 0;

    return m_storageAreas.get(connectionAndStorageAreaIDPair).get();
}

} // namespace WebKit

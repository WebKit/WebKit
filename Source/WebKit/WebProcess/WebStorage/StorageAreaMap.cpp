/*
 * Copyright (C) 2013-2022 Apple Inc. All rights reserved.
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
#include "StorageAreaMap.h"

#include "Logging.h"
#include "NetworkProcessConnection.h"
#include "NetworkStorageManagerMessages.h"
#include "StorageAreaImpl.h"
#include "StorageAreaMapMessages.h"
#include "StorageNamespaceImpl.h"
#include "WebPage.h"
#include "WebProcess.h"
#include <WebCore/DOMWindow.h>
#include <WebCore/Document.h>
#include <WebCore/EventNames.h>
#include <WebCore/Frame.h>
#include <WebCore/Page.h>
#include <WebCore/SecurityOriginData.h>
#include <WebCore/Storage.h>
#include <WebCore/StorageEventDispatcher.h>
#include <WebCore/StorageMap.h>
#include <WebCore/StorageType.h>

namespace WebKit {
using namespace WebCore;

StorageAreaMap::StorageAreaMap(StorageNamespaceImpl& storageNamespace, Ref<const WebCore::SecurityOrigin>&& securityOrigin)
    : m_identifier(StorageAreaMapIdentifier::generate())
    , m_namespace(storageNamespace)
    , m_securityOrigin(WTFMove(securityOrigin))
    , m_quotaInBytes(storageNamespace.quotaInBytes())
    , m_type(storageNamespace.storageType())
{
    WebProcess::singleton().registerStorageAreaMap(*this);
}

StorageAreaMap::~StorageAreaMap()
{
    disconnect();
    WebProcess::singleton().unregisterStorageAreaMap(*this);
}

unsigned StorageAreaMap::length()
{
    return ensureMap().length();
}

String StorageAreaMap::key(unsigned index)
{
    return ensureMap().key(index);
}

String StorageAreaMap::item(const String& key)
{
    return ensureMap().getItem(key);
}

void StorageAreaMap::setItem(Frame& sourceFrame, StorageAreaImpl* sourceArea, const String& key, const String& value, bool& quotaException)
{
    auto& map = ensureMap();
    ASSERT(!map.isShared());

    String oldValue;
    quotaException = false;
    map.setItem(key, value, oldValue, quotaException);
    if (quotaException)
        return;

    if (oldValue == value)
        return;

    m_pendingValueChanges.add(key);

    if (!m_remoteAreaIdentifier) {
        RELEASE_LOG_ERROR(Storage, "StorageAreaMap::setItem failed because storage map ID is invalid");
        return;
    }

    auto callback = [weakThis = WeakPtr { *this }, seed = m_currentSeed, key](bool hasError, auto allItems) mutable {
        if (weakThis)
            weakThis->didSetItem(seed, key, hasError, WTFMove(allItems));
    };
    auto& connection = WebProcess::singleton().ensureNetworkProcessConnection().connection();
    connection.sendWithAsyncReply(Messages::NetworkStorageManager::SetItem(*m_remoteAreaIdentifier, sourceArea->identifier(), key, value, sourceFrame.document()->url().string()), WTFMove(callback));
}

void StorageAreaMap::removeItem(WebCore::Frame& sourceFrame, StorageAreaImpl* sourceArea, const String& key)
{
    auto& map = ensureMap();
    ASSERT(!map.isShared());

    String oldValue;
    map.removeItem(key, oldValue);

    if (oldValue.isNull())
        return;

    m_pendingValueChanges.add(key);

    if (!m_remoteAreaIdentifier) {
        RELEASE_LOG_ERROR(Storage, "StorageAreaMap::removeItem failed because storage map ID is invalid");
        return;
    }

    auto callback = [weakThis = WeakPtr { *this }, seed = m_currentSeed, key]() mutable {
        if (weakThis)
            weakThis->didRemoveItem(seed, key);
    };
    WebProcess::singleton().ensureNetworkProcessConnection().connection().sendWithAsyncReply(Messages::NetworkStorageManager::RemoveItem(*m_remoteAreaIdentifier, sourceArea->identifier(), key, sourceFrame.document()->url().string()), WTFMove(callback));
}

void StorageAreaMap::clear(WebCore::Frame& sourceFrame, StorageAreaImpl* sourceArea)
{
    ensureMap().clear();
    m_pendingValueChanges.clear();
    ++m_currentSeed;
    m_hasPendingClear = true;

    if (!m_remoteAreaIdentifier) {
        RELEASE_LOG_ERROR(Storage, "StorageAreaMap::clear failed because storage map ID is invalid");
        return;
    }

    auto callback = [weakThis = WeakPtr { *this }, seed = m_currentSeed]() mutable {
        if (weakThis)
            weakThis->didClear(seed);
    };
    WebProcess::singleton().ensureNetworkProcessConnection().connection().sendWithAsyncReply(Messages::NetworkStorageManager::Clear(*m_remoteAreaIdentifier, sourceArea->identifier(), sourceFrame.document()->url().string()), WTFMove(callback));
}

bool StorageAreaMap::contains(const String& key)
{
    return ensureMap().contains(key);
}

StorageMap& StorageAreaMap::ensureMap()
{
    connectSync();
    if (!m_map)
        m_map = makeUnique<StorageMap>(m_quotaInBytes);

    return *m_map;
}

void StorageAreaMap::didSetItem(uint64_t mapSeed, const String& key, bool hasError, HashMap<String, String>&& remoteItems)
{
    if (m_currentSeed != mapSeed)
        return;

    ASSERT(m_pendingValueChanges.contains(key));
    m_pendingValueChanges.remove(key);

    if (hasError)
        syncItems(WTFMove(remoteItems));
}

void StorageAreaMap::didRemoveItem(uint64_t mapSeed, const String& key)
{
    if (m_currentSeed != mapSeed)
        return;

    ASSERT(m_pendingValueChanges.contains(key));
    m_pendingValueChanges.remove(key);
}

void StorageAreaMap::didClear(uint64_t mapSeed)
{
    if (m_currentSeed != mapSeed)
        return;

    ASSERT(m_hasPendingClear);
    m_hasPendingClear = false;
}

void StorageAreaMap::applyChange(const String& key, const String& newValue)
{
    ASSERT(!m_map || !m_map->isShared());

    // A null key means clear.
    if (!key) {
        syncItems(HashMap<String, String> { });
        return;
    }

    syncOneItem(key, newValue);
}

void StorageAreaMap::dispatchStorageEvent(const std::optional<StorageAreaImplIdentifier>& storageAreaImplID, const String& key, const String& oldValue, const String& newValue, const String& urlString, uint64_t messageIdentifier)
{
    if (messageIdentifier < m_lastHandledMessageIdentifier)
        return;

    m_lastHandledMessageIdentifier = messageIdentifier;
    if (!storageAreaImplID) {
        // This storage event originates from another process so we need to apply the change to our storage area map.
        applyChange(key, newValue);
    }

    if (type() == StorageType::Session)
        dispatchSessionStorageEvent(storageAreaImplID, key, oldValue, newValue, urlString);
    else
        dispatchLocalStorageEvent(storageAreaImplID, key, oldValue, newValue, urlString);
}

void StorageAreaMap::clearCache(uint64_t messageIdentifier)
{
    if (messageIdentifier < m_lastHandledMessageIdentifier)
        return;

    m_lastHandledMessageIdentifier = messageIdentifier;
    syncItems(HashMap<String, String> { });
}

void StorageAreaMap::dispatchSessionStorageEvent(const std::optional<StorageAreaImplIdentifier>& storageAreaImplID, const String& key, const String& oldValue, const String& newValue, const String& urlString)
{
    // Namespace IDs for session storage namespaces are equivalent to web page IDs
    // so we can get the right page here.
    auto* webPage = WebProcess::singleton().webPage(m_namespace.sessionStoragePageID());
    if (!webPage)
        return;

    auto* page = webPage->corePage();
    if (!page)
        return;

    StorageEventDispatcher::dispatchSessionStorageEvents(key, oldValue, newValue, *page, m_securityOrigin, urlString, [storageAreaImplID](auto& storage) {
        return static_cast<StorageAreaImpl&>(storage.area()).identifier() == storageAreaImplID;
    });
}

void StorageAreaMap::dispatchLocalStorageEvent(const std::optional<StorageAreaImplIdentifier>& storageAreaImplID, const String& key, const String& oldValue, const String& newValue, const String& urlString)
{
    ASSERT(isLocalStorage(type()));

    StorageEventDispatcher::dispatchLocalStorageEvents(key, oldValue, newValue, nullptr, m_securityOrigin, urlString, [storageAreaImplID](auto& storage) {
        return static_cast<StorageAreaImpl&>(storage.area()).identifier() == storageAreaImplID;
    });
}

StorageType StorageAreaMap::computeStorageType() const
{
    auto type = m_type;
    if ((type == StorageType::Local || type == StorageType::TransientLocal) && m_namespace.topLevelOrigin())
        type = StorageType::TransientLocal;

    return type;
}

WebCore::ClientOrigin StorageAreaMap::clientOrigin() const
{
    auto originData = m_securityOrigin->data();
    auto topOriginData = m_namespace.topLevelOrigin() ? m_namespace.topLevelOrigin()->data() : originData;
    return WebCore::ClientOrigin { topOriginData, originData };
}

void StorageAreaMap::sendConnectMessage(SendMode mode)
{
    m_isWaitingForConnectReply = true;
    auto& ipcConnection = WebProcess::singleton().ensureNetworkProcessConnection().connection();
    auto namespaceIdentifier = m_namespace.storageNamespaceID();
    auto origin = clientOrigin();
    auto type = computeStorageType();

    if (mode == SendMode::Sync) {
        auto sendResult = ipcConnection.sendSync(Messages::NetworkStorageManager::ConnectToStorageAreaSync(type, m_identifier, namespaceIdentifier, origin), 0);
        auto [remoteAreaIdentifier, items, messageIdentifier] = sendResult.takeReplyOr(StorageAreaIdentifier { }, HashMap<String, String> { }, 0);
        didConnect(remoteAreaIdentifier, WTFMove(items), messageIdentifier);
        return;
    }

    auto completionHandler = [this, weakThis = WeakPtr { *this }](auto remoteAreaIdentifier, auto items, auto messageIdentifier) mutable {
        if (weakThis)
            return didConnect(remoteAreaIdentifier, WTFMove(items), messageIdentifier);
    };

    ipcConnection.sendWithAsyncReply(Messages::NetworkStorageManager::ConnectToStorageArea(type, m_identifier, namespaceIdentifier, origin), WTFMove(completionHandler));
}

void StorageAreaMap::connectSync()
{
    if (m_remoteAreaIdentifier)
        return;

    sendConnectMessage(SendMode::Sync);
}

void StorageAreaMap::connect()
{
    if (m_remoteAreaIdentifier)
        return;

    sendConnectMessage(SendMode::Async);
}

void StorageAreaMap::didConnect(StorageAreaIdentifier remoteAreaIdentifier, HashMap<String, String>&& items, uint64_t messageIdentifier)
{
    m_isWaitingForConnectReply = false;
    if (messageIdentifier < m_lastHandledMessageIdentifier)
        return;

    m_lastHandledMessageIdentifier = messageIdentifier;
    if (!remoteAreaIdentifier.isValid())
        return;

    m_remoteAreaIdentifier = remoteAreaIdentifier;
    m_map = makeUnique<StorageMap>(m_quotaInBytes);
    m_map->importItems(WTFMove(items));
}

void StorageAreaMap::disconnect()
{
    if (!m_remoteAreaIdentifier) {
        auto* networkProcessConnection = WebProcess::singleton().existingNetworkProcessConnection();
        if (m_isWaitingForConnectReply && networkProcessConnection)
            networkProcessConnection->connection().send(Messages::NetworkStorageManager::CancelConnectToStorageArea(computeStorageType(), m_namespace.storageNamespaceID(), clientOrigin()), 0);

        return;
    }

    if (auto* networkProcessConnection = WebProcess::singleton().existingNetworkProcessConnection())
        networkProcessConnection->connection().send(Messages::NetworkStorageManager::DisconnectFromStorageArea(*m_remoteAreaIdentifier), 0);

    m_remoteAreaIdentifier = { };
    m_lastHandledMessageIdentifier = 0;
}

void StorageAreaMap::incrementUseCount()
{
    ++m_useCount;
}

void StorageAreaMap::decrementUseCount()
{
    if (!--m_useCount)
        m_namespace.destroyStorageAreaMap(*this);
}

void StorageAreaMap::syncOneItem(const String& key, const String& value)
{
    // Once the pending value change request is handled by remote area, this item will be changed.
    if (!m_map || m_pendingValueChanges.contains(key))
        return;

    if (!value) {
        String oldValue;
        m_map->removeItem(key, oldValue);
        return;
    }

    m_map->setItemIgnoringQuota(key, value);
}

void StorageAreaMap::syncItems(HashMap<String, String>&& items)
{
    // Once the pending clear request is handled by remote area, these items will be gone.
    if (!m_map || m_hasPendingClear)
        return;

    auto oldMap = std::exchange(m_map, makeUnique<StorageMap>(m_quotaInBytes));
    m_map->importItems(WTFMove(items));
    for (auto change : m_pendingValueChanges) {
        String value = oldMap->getItem(change.key);
        if (!value.isNull())
            m_map->setItemIgnoringQuota(change.key, value);
        else {
            String oldValue;
            m_map->removeItem(change.key, oldValue);
        }
    }
}

} // namespace WebKit


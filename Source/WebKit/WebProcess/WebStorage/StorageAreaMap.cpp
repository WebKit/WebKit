/*
 * Copyright (C) 2013-2020 Apple Inc. All rights reserved.
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
#include "StorageAreaImpl.h"
#include "StorageAreaMapMessages.h"
#include "StorageManagerSetMessages.h"
#include "StorageNamespaceImpl.h"
#include "WebPage.h"
#include "WebPageGroupProxy.h"
#include "WebProcess.h"
#include <WebCore/DOMWindow.h>
#include <WebCore/Document.h>
#include <WebCore/Frame.h>
#include <WebCore/Page.h>
#include <WebCore/PageGroup.h>
#include <WebCore/SecurityOriginData.h>
#include <WebCore/Storage.h>
#include <WebCore/StorageEventDispatcher.h>
#include <WebCore/StorageMap.h>
#include <WebCore/StorageType.h>

namespace WebKit {
using namespace WebCore;

StorageAreaMap::StorageAreaMap(StorageNamespaceImpl& storageNamespace, Ref<WebCore::SecurityOrigin>&& securityOrigin)
    : m_namespace(storageNamespace)
    , m_securityOrigin(WTFMove(securityOrigin))
    , m_quotaInBytes(storageNamespace.quotaInBytes())
    , m_type(storageNamespace.storageType())
{
    connect();
}

StorageAreaMap::~StorageAreaMap()
{
    disconnect();
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

void StorageAreaMap::setItem(Frame* sourceFrame, StorageAreaImpl* sourceArea, const String& key, const String& value, bool& quotaException)
{
    auto& map = ensureMap();
    ASSERT(map.hasOneRef());

    String oldValue;
    quotaException = false;
    map.setItem(key, value, oldValue, quotaException);
    if (quotaException)
        return;

    if (oldValue == value)
        return;

    m_pendingValueChanges.add(key);

    if (m_mapID)
        WebProcess::singleton().ensureNetworkProcessConnection().connection().send(Messages::StorageManagerSet::SetItem(*m_mapID, sourceArea->identifier(), m_currentSeed, key, value, sourceFrame->document()->url()), 0);
    else
        RELEASE_LOG_ERROR(Storage, "StorageAreaMap::setItem failed because storage map ID is invalid");
}

void StorageAreaMap::removeItem(WebCore::Frame* sourceFrame, StorageAreaImpl* sourceArea, const String& key)
{
    auto& map = ensureMap();
    ASSERT(map.hasOneRef());

    String oldValue;
    map.removeItem(key, oldValue);

    if (oldValue.isNull())
        return;

    m_pendingValueChanges.add(key);

    if (m_mapID)
        WebProcess::singleton().ensureNetworkProcessConnection().connection().send(Messages::StorageManagerSet::RemoveItem(*m_mapID, sourceArea->identifier(), m_currentSeed, key, sourceFrame->document()->url()), 0);
    else
        RELEASE_LOG_ERROR(Storage, "StorageAreaMap::removeItem failed because storage map ID is invalid");
}

void StorageAreaMap::clear(WebCore::Frame* sourceFrame, StorageAreaImpl* sourceArea)
{
    connect();

    resetValues();

    m_hasPendingClear = true;
    m_map = StorageMap::create(m_quotaInBytes);

    if (m_mapID)
        WebProcess::singleton().ensureNetworkProcessConnection().connection().send(Messages::StorageManagerSet::Clear(*m_mapID, sourceArea->identifier(), m_currentSeed, sourceFrame->document()->url()), 0);
    else
        RELEASE_LOG_ERROR(Storage, "StorageAreaMap::clear failed because storage map ID is invalid");
}

bool StorageAreaMap::contains(const String& key)
{
    return ensureMap().contains(key);
}

void StorageAreaMap::resetValues()
{
    m_map = nullptr;

    m_pendingValueChanges.clear();
    m_hasPendingClear = false;
    ++m_currentSeed;
}

StorageMap& StorageAreaMap::ensureMap()
{
    connect();

    if (!m_map) {
        m_map = StorageMap::create(m_quotaInBytes);

        if (m_mapID) {
            // We need to use a IPC::UnboundedSynchronousIPCScope to prevent UIProcess hangs in case we receive a synchronous IPC from the UIProcess while we're waiting for a response
            // from our StorageManagerSet::GetValues() IPC. This IPC may be very slow because it may need to fetch the values from disk and there may be a lot of data.
            IPC::UnboundedSynchronousIPCScope unboundedSynchronousIPCScope;
            HashMap<String, String> values;
            WebProcess::singleton().ensureNetworkProcessConnection().connection().sendSync(Messages::StorageManagerSet::GetValues(*m_mapID), Messages::StorageManagerSet::GetValues::Reply(values), 0);
            m_map->importItems(WTFMove(values));
        } else
            RELEASE_LOG_ERROR(Storage, "StorageAreaMap::ensureMap failed to load from network process because storage map ID is invalid");
    }
    return *m_map;
}

void StorageAreaMap::didSetItem(uint64_t mapSeed, const String& key, bool quotaError)
{
    if (m_currentSeed != mapSeed)
        return;

    ASSERT(m_pendingValueChanges.contains(key));

    if (quotaError) {
        resetValues();
        return;
    }

    m_pendingValueChanges.remove(key);
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

bool StorageAreaMap::shouldApplyChangeForKey(const String& key) const
{
    // We have not yet loaded anything from this storage map.
    if (!m_map)
        return false;

    // Check if this storage area is currently waiting for the storage manager to update the given key.
    // If that is the case, we don't want to apply any changes made by other storage areas, since
    // our change was made last.
    if (m_pendingValueChanges.contains(key))
        return false;

    return true;
}

void StorageAreaMap::applyChange(const String& key, const String& newValue)
{
    ASSERT(!m_map || m_map->hasOneRef());

    // There is at least one clear pending we don't want to apply any changes until we get the corresponding DidClear messages.
    if (m_hasPendingClear)
        return;

    if (!key) {
        // A null key means clear.
        auto newMap = StorageMap::create(m_quotaInBytes);

        // Any changes that were made locally after the clear must still be kept around in the new map.
        for (auto& change : m_pendingValueChanges) {
            auto& key = change.key;
            String value = m_map->getItem(key);
            if (!value) {
                // This change must have been a pending remove, ignore it.
                continue;
            }

            String oldValue;
            newMap->setItemIgnoringQuota(key, oldValue);
        }

        m_map = WTFMove(newMap);
        return;
    }

    if (!shouldApplyChangeForKey(key))
        return;

    if (!newValue) {
        // A null new value means that the item should be removed.
        String oldValue;
        m_map->removeItem(key, oldValue);
        return;
    }

    m_map->setItemIgnoringQuota(key, newValue);
}

void StorageAreaMap::dispatchStorageEvent(const Optional<StorageAreaImplIdentifier>& storageAreaImplID, const String& key, const String& oldValue, const String& newValue, const String& urlString)
{
    if (!storageAreaImplID) {
        // This storage event originates from another process so we need to apply the change to our storage area map.
        applyChange(key, newValue);
    }

    if (type() == StorageType::Session)
        dispatchSessionStorageEvent(storageAreaImplID, key, oldValue, newValue, urlString);
    else
        dispatchLocalStorageEvent(storageAreaImplID, key, oldValue, newValue, urlString);
}

void StorageAreaMap::clearCache()
{
    resetValues();
}

static Vector<RefPtr<Frame>> framesForEventDispatching(Page& page, SecurityOrigin& origin, const Optional<StorageAreaImplIdentifier>& storageAreaImplID)
{
    Vector<RefPtr<Frame>> frames;
    page.forEachDocument([&](auto& document) {
        if (!document.securityOrigin().equal(&origin))
            return;
        
        auto* storage = document.domWindow() ? document.domWindow()->optionalSessionStorage() : nullptr;
        if (!storage)
            return;
        
        auto& storageArea = static_cast<StorageAreaImpl&>(storage->area());
        if (storageArea.identifier() == storageAreaImplID) {
            // This is the storage area that caused the event to be dispatched.
            return;
        }
       
        if (auto* frame = document.frame()) 
            frames.append(frame);
    });
    return frames;
}

void StorageAreaMap::dispatchSessionStorageEvent(const Optional<StorageAreaImplIdentifier>& storageAreaImplID, const String& key, const String& oldValue, const String& newValue, const String& urlString)
{
    // Namespace IDs for session storage namespaces are equivalent to web page IDs
    // so we can get the right page here.
    auto* webPage = WebProcess::singleton().webPage(m_namespace.sessionStoragePageID());
    if (!webPage)
        return;

    auto* page = webPage->corePage();
    if (!page)
        return;

    auto frames = framesForEventDispatching(*page, m_securityOrigin, storageAreaImplID);
    StorageEventDispatcher::dispatchSessionStorageEventsToFrames(*page, frames, key, oldValue, newValue, urlString, m_securityOrigin->data());
}

void StorageAreaMap::dispatchLocalStorageEvent(const Optional<StorageAreaImplIdentifier>& storageAreaImplID, const String& key, const String& oldValue, const String& newValue, const String& urlString)
{
    ASSERT(isLocalStorage(type()));

    Vector<RefPtr<Frame>> frames;

    // Namespace IDs for local storage namespaces are equivalent to web page group IDs.
    auto& pageGroup = *WebProcess::singleton().webPageGroup(m_namespace.pageGroupID())->corePageGroup();
    for (auto* page : pageGroup.pages())
        frames.appendVector(framesForEventDispatching(*page, m_securityOrigin, storageAreaImplID));

    StorageEventDispatcher::dispatchLocalStorageEventsToFrames(pageGroup, frames, key, oldValue, newValue, urlString, m_securityOrigin->data());
}

void StorageAreaMap::connect()
{
    if (m_mapID)
        return;

    switch (m_type) {
    case StorageType::Local:
    case StorageType::TransientLocal:
        if (SecurityOrigin* topLevelOrigin = m_namespace.topLevelOrigin())
            WebProcess::singleton().ensureNetworkProcessConnection().connection().sendSync(Messages::StorageManagerSet::ConnectToTransientLocalStorageArea(WebProcess::singleton().sessionID(), m_namespace.storageNamespaceID(), topLevelOrigin->data(), m_securityOrigin->data()), Messages::StorageManagerSet::ConnectToTransientLocalStorageArea::Reply(m_mapID), 0);
        else
            WebProcess::singleton().ensureNetworkProcessConnection().connection().sendSync(Messages::StorageManagerSet::ConnectToLocalStorageArea(WebProcess::singleton().sessionID(), m_namespace.storageNamespaceID(), m_securityOrigin->data()), Messages::StorageManagerSet::ConnectToLocalStorageArea::Reply(m_mapID), 0);
        break;
    case StorageType::Session:
        WebProcess::singleton().ensureNetworkProcessConnection().connection().sendSync(Messages::StorageManagerSet::ConnectToSessionStorageArea(WebProcess::singleton().sessionID(), m_namespace.storageNamespaceID(), m_securityOrigin->data()), Messages::StorageManagerSet::ConnectToSessionStorageArea::Reply(m_mapID), 0);
    }

    if (m_mapID)
        WebProcess::singleton().registerStorageAreaMap(*this);
}

void StorageAreaMap::disconnect()
{
    if (!m_mapID)
        return;

    resetValues();
    WebProcess::singleton().unregisterStorageAreaMap(*this);

    if (auto* networkProcessConnection = WebProcess::singleton().existingNetworkProcessConnection())
        networkProcessConnection->connection().send(Messages::StorageManagerSet::DisconnectFromStorageArea(*m_mapID), 0);

    m_mapID = { };
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

} // namespace WebKit

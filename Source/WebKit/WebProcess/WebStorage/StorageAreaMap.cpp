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
#include "StorageAreaMap.h"

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

Ref<StorageAreaMap> StorageAreaMap::create(StorageNamespaceImpl* storageNamespace, Ref<WebCore::SecurityOrigin>&& securityOrigin)
{
    return adoptRef(*new StorageAreaMap(storageNamespace, WTFMove(securityOrigin)));
}

StorageAreaMap::StorageAreaMap(StorageNamespaceImpl* storageNamespace, Ref<WebCore::SecurityOrigin>&& securityOrigin)
    : m_storageNamespace(storageNamespace)
    , m_storageType(storageNamespace->storageType())
    , m_quotaInBytes(storageNamespace->quotaInBytes())
    , m_securityOrigin(WTFMove(securityOrigin))
    , m_currentSeed(0)
    , m_hasPendingClear(false)
{
    connect();
}

StorageAreaMap::~StorageAreaMap()
{
    disconnect();
}

unsigned StorageAreaMap::length()
{
    loadValuesIfNeeded();

    return m_storageMap->length();
}

String StorageAreaMap::key(unsigned index)
{
    loadValuesIfNeeded();

    return m_storageMap->key(index);
}

String StorageAreaMap::item(const String& key)
{
    loadValuesIfNeeded();

    return m_storageMap->getItem(key);
}

void StorageAreaMap::setItem(Frame* sourceFrame, StorageAreaImpl* sourceArea, const String& key, const String& value, bool& quotaException)
{
    loadValuesIfNeeded();

    ASSERT(m_storageMap->hasOneRef());

    String oldValue;
    quotaException = false;
    m_storageMap->setItem(key, value, oldValue, quotaException);
    if (quotaException)
        return;

    if (oldValue == value)
        return;

    m_pendingValueChanges.add(key);

    WebProcess::singleton().ensureNetworkProcessConnection().connection().send(Messages::StorageManagerSet::SetItem(*m_storageMapID, sourceArea->identifier(), m_currentSeed, key, value, sourceFrame->document()->url()), 0);
}

void StorageAreaMap::removeItem(WebCore::Frame* sourceFrame, StorageAreaImpl* sourceArea, const String& key)
{
    loadValuesIfNeeded();
    ASSERT(m_storageMap->hasOneRef());

    String oldValue;
    m_storageMap->removeItem(key, oldValue);

    if (oldValue.isNull())
        return;

    m_pendingValueChanges.add(key);

    WebProcess::singleton().ensureNetworkProcessConnection().connection().send(Messages::StorageManagerSet::RemoveItem(*m_storageMapID, sourceArea->identifier(), m_currentSeed, key, sourceFrame->document()->url()), 0);
}

void StorageAreaMap::clear(WebCore::Frame* sourceFrame, StorageAreaImpl* sourceArea)
{
    connect();

    resetValues();

    m_hasPendingClear = true;
    m_storageMap = StorageMap::create(m_quotaInBytes);
    WebProcess::singleton().ensureNetworkProcessConnection().connection().send(Messages::StorageManagerSet::Clear(*m_storageMapID, sourceArea->identifier(), m_currentSeed, sourceFrame->document()->url()), 0);
}

bool StorageAreaMap::contains(const String& key)
{
    loadValuesIfNeeded();

    return m_storageMap->contains(key);
}

void StorageAreaMap::resetValues()
{
    m_storageMap = nullptr;

    m_pendingValueChanges.clear();
    m_hasPendingClear = false;
    m_currentSeed++;
}

void StorageAreaMap::loadValuesIfNeeded()
{
    connect();

    if (m_storageMap)
        return;

    HashMap<String, String> values;
    // FIXME: This should use a special sendSync flag to indicate that we don't want to process incoming messages while waiting for a reply.
    // (This flag does not yet exist). Since loadValuesIfNeeded() ends up being called from within JavaScript code, processing incoming synchronous messages
    // could lead to weird reentrency bugs otherwise.
    WebProcess::singleton().ensureNetworkProcessConnection().connection().sendSync(Messages::StorageManagerSet::GetValues(*m_storageMapID), Messages::StorageManagerSet::GetValues::Reply(values), 0);

    m_storageMap = StorageMap::create(m_quotaInBytes);
    m_storageMap->importItems(WTFMove(values));
}

void StorageAreaMap::didSetItem(uint64_t storageMapSeed, const String& key, bool quotaError)
{
    if (m_currentSeed != storageMapSeed)
        return;

    ASSERT(m_pendingValueChanges.contains(key));

    if (quotaError) {
        resetValues();
        return;
    }

    m_pendingValueChanges.remove(key);
}

void StorageAreaMap::didRemoveItem(uint64_t storageMapSeed, const String& key)
{
    if (m_currentSeed != storageMapSeed)
        return;

    ASSERT(m_pendingValueChanges.contains(key));
    m_pendingValueChanges.remove(key);
}

void StorageAreaMap::didClear(uint64_t storageMapSeed)
{
    if (m_currentSeed != storageMapSeed)
        return;

    ASSERT(m_hasPendingClear);
    m_hasPendingClear = false;
}

bool StorageAreaMap::shouldApplyChangeForKey(const String& key) const
{
    // We have not yet loaded anything from this storage map.
    if (!m_storageMap)
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
    ASSERT(!m_storageMap || m_storageMap->hasOneRef());

    // There is at least one clear pending we don't want to apply any changes until we get the corresponding DidClear messages.
    if (m_hasPendingClear)
        return;

    if (!key) {
        // A null key means clear.
        auto newStorageMap = StorageMap::create(m_quotaInBytes);

        // Any changes that were made locally after the clear must still be kept around in the new map.
        for (auto it = m_pendingValueChanges.begin().keys(), end = m_pendingValueChanges.end().keys(); it != end; ++it) {
            const String& key = *it;

            String value = m_storageMap->getItem(key);
            if (!value) {
                // This change must have been a pending remove, ignore it.
                continue;
            }

            String oldValue;
            newStorageMap->setItemIgnoringQuota(key, oldValue);
        }

        m_storageMap = WTFMove(newStorageMap);
        return;
    }

    if (!shouldApplyChangeForKey(key))
        return;

    if (!newValue) {
        // A null new value means that the item should be removed.
        String oldValue;
        m_storageMap->removeItem(key, oldValue);
        return;
    }

    m_storageMap->setItemIgnoringQuota(key, newValue);
}

void StorageAreaMap::dispatchStorageEvent(const Optional<StorageAreaImplIdentifier>& storageAreaImplID, const String& key, const String& oldValue, const String& newValue, const String& urlString)
{
    if (!storageAreaImplID) {
        // This storage event originates from another process so we need to apply the change to our storage area map.
        applyChange(key, newValue);
    }

    if (storageType() == StorageType::Session)
        dispatchSessionStorageEvent(storageAreaImplID, key, oldValue, newValue, urlString);
    else
        dispatchLocalStorageEvent(storageAreaImplID, key, oldValue, newValue, urlString);
}

void StorageAreaMap::clearCache()
{
    resetValues();
}

void StorageAreaMap::dispatchSessionStorageEvent(const Optional<StorageAreaImplIdentifier>& storageAreaImplID, const String& key, const String& oldValue, const String& newValue, const String& urlString)
{
    // Namespace IDs for session storage namespaces are equivalent to web page IDs
    // so we can get the right page here.
    WebPage* webPage = WebProcess::singleton().webPage(m_storageNamespace->sessionStoragePageID());
    if (!webPage)
        return;

    Vector<RefPtr<Frame>> frames;

    Page* page = webPage->corePage();
    for (Frame* frame = &page->mainFrame(); frame; frame = frame->tree().traverseNext()) {
        Document* document = frame->document();
        if (!document->securityOrigin().equal(m_securityOrigin.ptr()))
            continue;

        Storage* storage = document->domWindow()->optionalSessionStorage();
        if (!storage)
            continue;

        StorageAreaImpl& storageArea = static_cast<StorageAreaImpl&>(storage->area());
        if (storageArea.identifier() == storageAreaImplID) {
            // This is the storage area that caused the event to be dispatched.
            continue;
        }

        frames.append(frame);
    }

    StorageEventDispatcher::dispatchSessionStorageEventsToFrames(*page, frames, key, oldValue, newValue, urlString, m_securityOrigin->data());
}

void StorageAreaMap::dispatchLocalStorageEvent(const Optional<StorageAreaImplIdentifier>& storageAreaImplID, const String& key, const String& oldValue, const String& newValue, const String& urlString)
{
    ASSERT(isLocalStorage(storageType()));

    Vector<RefPtr<Frame>> frames;

    // Namespace IDs for local storage namespaces are equivalent to web page group IDs.
    PageGroup& pageGroup = *WebProcess::singleton().webPageGroup(m_storageNamespace->pageGroupID())->corePageGroup();
    const HashSet<Page*>& pages = pageGroup.pages();
    for (HashSet<Page*>::const_iterator it = pages.begin(), end = pages.end(); it != end; ++it) {
        for (Frame* frame = &(*it)->mainFrame(); frame; frame = frame->tree().traverseNext()) {
            Document* document = frame->document();
            if (!document->securityOrigin().equal(m_securityOrigin.ptr()))
                continue;

            Storage* storage = document->domWindow()->optionalLocalStorage();
            if (!storage)
                continue;

            StorageAreaImpl& storageArea = static_cast<StorageAreaImpl&>(storage->area());
            if (storageArea.identifier() == storageAreaImplID) {
                // This is the storage area that caused the event to be dispatched.
                continue;
            }

            frames.append(frame);
        }
    }

    StorageEventDispatcher::dispatchLocalStorageEventsToFrames(pageGroup, frames, key, oldValue, newValue, urlString, m_securityOrigin->data());
}

void StorageAreaMap::connect()
{
    if (m_storageMapID)
        return;

    switch (m_storageType) {
    case StorageType::Local:
    case StorageType::TransientLocal:
        if (SecurityOrigin* topLevelOrigin = m_storageNamespace->topLevelOrigin())
            WebProcess::singleton().ensureNetworkProcessConnection().connection().sendSync(Messages::StorageManagerSet::ConnectToTransientLocalStorageArea(m_storageNamespace->sessionID(), m_storageNamespace->storageNamespaceID(), topLevelOrigin->data(), m_securityOrigin->data()), Messages::StorageManagerSet::ConnectToTransientLocalStorageArea::Reply(m_storageMapID), 0);
        else
            WebProcess::singleton().ensureNetworkProcessConnection().connection().sendSync(Messages::StorageManagerSet::ConnectToLocalStorageArea(m_storageNamespace->sessionID(), m_storageNamespace->storageNamespaceID(), m_securityOrigin->data()), Messages::StorageManagerSet::ConnectToLocalStorageArea::Reply(m_storageMapID), 0);
        break;
    case StorageType::Session:
        WebProcess::singleton().ensureNetworkProcessConnection().connection().sendSync(Messages::StorageManagerSet::ConnectToSessionStorageArea(m_storageNamespace->sessionID(), m_storageNamespace->storageNamespaceID(), m_securityOrigin->data()), Messages::StorageManagerSet::ConnectToSessionStorageArea::Reply(m_storageMapID), 0);
    }

    if (m_storageMapID)
        WebProcess::singleton().registerStorageAreaMap(*this);
}

void StorageAreaMap::disconnect()
{
    if (!m_storageMapID)
        return;

    resetValues();
    WebProcess::singleton().unregisterStorageAreaMap(*this);

    if (auto networkProcessConnection = WebProcess::singleton().existingNetworkProcessConnection())
        networkProcessConnection->connection().send(Messages::StorageManagerSet::DisconnectFromStorageArea(*m_storageMapID), 0);

    m_storageMapID = { };
}

} // namespace WebKit

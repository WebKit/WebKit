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

#include "SecurityOriginData.h"
#include "StorageAreaImpl.h"
#include "StorageAreaMapMessages.h"
#include "StorageManagerMessages.h"
#include "StorageNamespaceImpl.h"
#include "WebPage.h"
#include "WebPageGroupProxy.h"
#include "WebProcess.h"
#include <WebCore/DOMWindow.h>
#include <WebCore/Frame.h>
#include <WebCore/Page.h>
#include <WebCore/PageGroup.h>
#include <WebCore/Storage.h>
#include <WebCore/StorageEventDispatcher.h>
#include <WebCore/StorageMap.h>

using namespace WebCore;

namespace WebKit {

static uint64_t generateStorageMapID()
{
    static uint64_t storageMapID;
    return ++storageMapID;
}

PassRefPtr<StorageAreaMap> StorageAreaMap::create(StorageNamespaceImpl* storageNamespace, PassRefPtr<WebCore::SecurityOrigin> securityOrigin)
{
    return adoptRef(new StorageAreaMap(storageNamespace, securityOrigin));
}

StorageAreaMap::StorageAreaMap(StorageNamespaceImpl* storageNamespace, PassRefPtr<WebCore::SecurityOrigin> securityOrigin)
    : m_storageMapID(generateStorageMapID())
    , m_storageType(storageNamespace->storageType())
    , m_storageNamespaceID(storageNamespace->storageNamespaceID())
    , m_quotaInBytes(storageNamespace->quotaInBytes())
    , m_securityOrigin(securityOrigin)
    , m_hasPendingClear(false)
{
    if (m_storageType == LocalStorage)
        WebProcess::shared().connection()->send(Messages::StorageManager::CreateLocalStorageMap(m_storageMapID, storageNamespace->storageNamespaceID(), SecurityOriginData::fromSecurityOrigin(m_securityOrigin.get())), 0);
    else
        WebProcess::shared().connection()->send(Messages::StorageManager::CreateSessionStorageMap(m_storageMapID, storageNamespace->storageNamespaceID(), SecurityOriginData::fromSecurityOrigin(m_securityOrigin.get())), 0);
    WebProcess::shared().addMessageReceiver(Messages::StorageAreaMap::messageReceiverName(), m_storageMapID, this);
}

StorageAreaMap::~StorageAreaMap()
{
    WebProcess::shared().connection()->send(Messages::StorageManager::DestroyStorageMap(m_storageMapID), 0);
    WebProcess::shared().removeMessageReceiver(Messages::StorageAreaMap::messageReceiverName(), m_storageMapID);
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

    WebProcess::shared().connection()->send(Messages::StorageManager::SetItem(m_storageMapID, sourceArea->storageAreaID(), key, value, sourceFrame->document()->url()), 0);
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

    WebProcess::shared().connection()->send(Messages::StorageManager::RemoveItem(m_storageMapID, sourceArea->storageAreaID(), key, sourceFrame->document()->url()), 0);
}

void StorageAreaMap::clear(WebCore::Frame* sourceFrame, StorageAreaImpl* sourceArea)
{
    resetValues();

    m_hasPendingClear = true;
    m_storageMap = StorageMap::create(m_quotaInBytes);
    WebProcess::shared().connection()->send(Messages::StorageManager::Clear(m_storageMapID, sourceArea->storageAreaID(), sourceFrame->document()->url()), 0);
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
}

void StorageAreaMap::loadValuesIfNeeded()
{
    if (m_storageMap)
        return;

    HashMap<String, String> values;
    // FIXME: This should use a special sendSync flag to indicate that we don't want to process incoming messages while waiting for a reply.
    // (This flag does not yet exist). Since loadValuesIfNeeded() ends up being called from within JavaScript code, processing incoming synchronous messages
    // could lead to weird reentrency bugs otherwise.
    WebProcess::shared().connection()->sendSync(Messages::StorageManager::GetValues(m_storageMapID), Messages::StorageManager::GetValues::Reply(values), 0);

    m_storageMap = StorageMap::create(m_quotaInBytes);
    m_storageMap->importItems(values);

    // We want to ignore all changes until we get the DidGetValues message, so treat this as a pending clear.
    m_hasPendingClear = true;
}

void StorageAreaMap::didGetValues()
{
    ASSERT(m_hasPendingClear);
    m_hasPendingClear = false;
}

void StorageAreaMap::didSetItem(const String& key, bool quotaError)
{
    ASSERT(m_pendingValueChanges.contains(key));

    if (quotaError) {
        resetValues();
        return;
    }

    m_pendingValueChanges.remove(key);
}

void StorageAreaMap::didRemoveItem(const String& key)
{
    ASSERT(m_pendingValueChanges.contains(key));

    m_pendingValueChanges.remove(key);
}

void StorageAreaMap::didClear()
{
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
    ASSERT(m_storageMap->hasOneRef());

    // There's a clear pending, we don't want any changes until we've gotten the DidClear message.
    if (m_hasPendingClear)
        return;

    // FIXME: Handle clear.
    ASSERT(!key.isNull());

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

void StorageAreaMap::dispatchStorageEvent(uint64_t sourceStorageAreaID, const String& key, const String& oldValue, const String& newValue, const String& urlString)
{
    if (!sourceStorageAreaID) {
        // This storage event originates from another process so we need to apply the change to our storage area map.
        applyChange(key, newValue);
    }

    if (storageType() == SessionStorage)
        dispatchSessionStorageEvent(sourceStorageAreaID, key, oldValue, newValue, urlString);
    else
        dispatchLocalStorageEvent(sourceStorageAreaID, key, oldValue, newValue, urlString);
}

void StorageAreaMap::dispatchSessionStorageEvent(uint64_t sourceStorageAreaID, const String& key, const String& oldValue, const String& newValue, const String& urlString)
{
    ASSERT(storageType() == SessionStorage);

    // Namespace IDs for session storage namespaces are equivalent to web page IDs
    // so we can get the right page here.
    WebPage* webPage = WebProcess::shared().webPage(m_storageNamespaceID);
    if (!webPage)
        return;

    Vector<RefPtr<Frame> > frames;

    Page* page = webPage->corePage();
    for (Frame* frame = page->mainFrame(); frame; frame = frame->tree()->traverseNext()) {
        Document* document = frame->document();
        if (!document->securityOrigin()->equal(m_securityOrigin.get()))
            continue;

        Storage* storage = document->domWindow()->optionalSessionStorage();
        if (!storage)
            continue;

        StorageAreaImpl* storageArea = static_cast<StorageAreaImpl*>(storage->area());
        if (storageArea->storageAreaID() == sourceStorageAreaID) {
            // This is the storage area that caused the event to be dispatched.
            continue;
        }

        frames.append(frame);
    }

    StorageEventDispatcher::dispatchLocalStorageEventsToFrames(page->group(), frames, key, oldValue, newValue, urlString, m_securityOrigin.get());
}

void StorageAreaMap::dispatchLocalStorageEvent(uint64_t sourceStorageAreaID, const String& key, const String& oldValue, const String& newValue, const String& urlString)
{
    ASSERT(storageType() == LocalStorage);

    Vector<RefPtr<Frame> > frames;

    PageGroup& pageGroup = *WebProcess::shared().webPageGroup(m_storageNamespaceID)->corePageGroup();
    const HashSet<Page*>& pages = pageGroup.pages();
    for (HashSet<Page*>::const_iterator it = pages.begin(), end = pages.end(); it != end; ++it) {
        for (Frame* frame = (*it)->mainFrame(); frame; frame = frame->tree()->traverseNext()) {
            Document* document = frame->document();
            if (!document->securityOrigin()->equal(m_securityOrigin.get()))
                continue;

            Storage* storage = document->domWindow()->optionalLocalStorage();
            if (!storage)
                continue;

            StorageAreaImpl* storageArea = static_cast<StorageAreaImpl*>(storage->area());
            if (storageArea->storageAreaID() == sourceStorageAreaID) {
                // This is the storage area that caused the event to be dispatched.
                continue;
            }

            frames.append(frame);
        }
    }

    StorageEventDispatcher::dispatchLocalStorageEventsToFrames(pageGroup, frames, key, oldValue, newValue, urlString, m_securityOrigin.get());
}

} // namespace WebKit

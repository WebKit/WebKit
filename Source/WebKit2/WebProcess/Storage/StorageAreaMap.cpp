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
#include "WebProcess.h"
#include <WebCore/Frame.h>
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
    , m_storageNamespaceID(storageNamespace->storageNamespaceID())
    , m_quotaInBytes(storageNamespace->quotaInBytes())
    , m_securityOrigin(securityOrigin)
{
    WebProcess::shared().connection()->send(Messages::StorageManager::CreateStorageMap(m_storageMapID, storageNamespace->storageNamespaceID(), SecurityOriginData::fromSecurityOrigin(m_securityOrigin.get())), 0);
    WebProcess::shared().addMessageReceiver(Messages::StorageAreaMap::messageReceiverName(), m_storageMapID, this);
}

StorageAreaMap::~StorageAreaMap()
{
    WebProcess::shared().connection()->send(Messages::StorageManager::DestroyStorageMap(m_storageMapID), 0);
    WebProcess::shared().removeMessageReceiver(Messages::StorageAreaMap::messageReceiverName(), m_storageMapID);
}

StorageType StorageAreaMap::storageType() const
{
    // A zero storage namespace ID is used for local storage.
    if (!m_storageNamespaceID)
        return LocalStorage;

    return SessionStorage;
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

bool StorageAreaMap::contains(const String& key)
{
    loadValuesIfNeeded();

    return m_storageMap->contains(key);
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
}

void StorageAreaMap::didSetItem(const String& key, bool quotaError)
{
    // FIXME: Implement.
}

void StorageAreaMap::dispatchStorageEvent(uint64_t sourceStorageAreaID, const String& key, const String& oldValue, const String& newValue, const String& urlString)
{
    // FIXME: Implement.
}

} // namespace WebKit

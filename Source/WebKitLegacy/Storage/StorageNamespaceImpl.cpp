/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "StorageNamespaceImpl.h"

#include "StorageAreaImpl.h"
#include "StorageSyncManager.h"
#include "StorageTracker.h"
#include <WebCore/StorageMap.h>
#include <WebCore/StorageType.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/StringHash.h>

using namespace WebCore;

namespace WebKit {

static HashMap<String, StorageNamespaceImpl*>& localStorageNamespaceMap()
{
    static NeverDestroyed<HashMap<String, StorageNamespaceImpl*>> localStorageNamespaceMap;

    return localStorageNamespaceMap;
}

Ref<StorageNamespaceImpl> StorageNamespaceImpl::createSessionStorageNamespace(unsigned quota)
{
    return adoptRef(*new StorageNamespaceImpl(StorageType::Session, String(), quota));
}

Ref<StorageNamespaceImpl> StorageNamespaceImpl::createEphemeralLocalStorageNamespace(unsigned quota)
{
    return adoptRef(*new StorageNamespaceImpl(StorageType::EphemeralLocal, String(), quota));
}

Ref<StorageNamespaceImpl> StorageNamespaceImpl::getOrCreateLocalStorageNamespace(const String& databasePath, unsigned quota)
{
    ASSERT(!databasePath.isNull());

    auto& slot = localStorageNamespaceMap().add(databasePath, nullptr).iterator->value;
    if (slot)
        return *slot;

    Ref<StorageNamespaceImpl> storageNamespace = adoptRef(*new StorageNamespaceImpl(StorageType::Local, databasePath, quota));
    slot = storageNamespace.ptr();

    return storageNamespace;
}

StorageNamespaceImpl::StorageNamespaceImpl(StorageType storageType, const String& path, unsigned quota)
    : m_storageType(storageType)
    , m_path(path.isolatedCopy())
    , m_syncManager(0)
    , m_quota(quota)
    , m_isShutdown(false)
{
    if (isPersistentLocalStorage(m_storageType) && !m_path.isEmpty())
        m_syncManager = StorageSyncManager::create(m_path);
}

StorageNamespaceImpl::~StorageNamespaceImpl()
{
    ASSERT(isMainThread());

    if (isPersistentLocalStorage(m_storageType)) {
        ASSERT(localStorageNamespaceMap().get(m_path) == this);
        localStorageNamespaceMap().remove(m_path);
    }

    if (!m_isShutdown)
        close();
}

Ref<StorageNamespace> StorageNamespaceImpl::copy(Page*)
{
    ASSERT(isMainThread());
    ASSERT(!m_isShutdown);
    ASSERT(m_storageType == StorageType::Session || m_storageType == StorageType::EphemeralLocal);

    auto newNamespace = adoptRef(*new StorageNamespaceImpl(m_storageType, m_path, m_quota));
    for (auto& iter : m_storageAreaMap)
        newNamespace->m_storageAreaMap.set(iter.key, iter.value->copy());

    return WTFMove(newNamespace);
}

Ref<StorageArea> StorageNamespaceImpl::storageArea(const SecurityOriginData& origin)
{
    ASSERT(isMainThread());
    ASSERT(!m_isShutdown);

    if (RefPtr<StorageAreaImpl> storageArea = m_storageAreaMap.get(origin))
        return storageArea.releaseNonNull();

    auto storageArea = StorageAreaImpl::create(m_storageType, origin, m_syncManager.get(), m_quota);
    m_storageAreaMap.set(origin, storageArea.ptr());
    return WTFMove(storageArea);
}

void StorageNamespaceImpl::close()
{
    ASSERT(isMainThread());

    if (m_isShutdown)
        return;

    // If we're not a persistent storage, we shouldn't need to do any work here.
    if (m_storageType == StorageType::Session || m_storageType == StorageType::EphemeralLocal) {
        ASSERT(!m_syncManager);
        return;
    }

    StorageAreaMap::iterator end = m_storageAreaMap.end();
    for (StorageAreaMap::iterator it = m_storageAreaMap.begin(); it != end; ++it)
        it->value->close();

    if (m_syncManager)
        m_syncManager->close();

    m_isShutdown = true;
}

void StorageNamespaceImpl::clearOriginForDeletion(const SecurityOriginData& origin)
{
    ASSERT(isMainThread());

    RefPtr<StorageAreaImpl> storageArea = m_storageAreaMap.get(origin);
    if (storageArea)
        storageArea->clearForOriginDeletion();
}

void StorageNamespaceImpl::clearAllOriginsForDeletion()
{
    ASSERT(isMainThread());

    StorageAreaMap::iterator end = m_storageAreaMap.end();
    for (StorageAreaMap::iterator it = m_storageAreaMap.begin(); it != end; ++it)
        it->value->clearForOriginDeletion();
}
    
void StorageNamespaceImpl::sync()
{
    ASSERT(isMainThread());
    StorageAreaMap::iterator end = m_storageAreaMap.end();
    for (StorageAreaMap::iterator it = m_storageAreaMap.begin(); it != end; ++it)
        it->value->sync();
}

void StorageNamespaceImpl::closeIdleLocalStorageDatabases()
{
    ASSERT(isMainThread());
    StorageAreaMap::iterator end = m_storageAreaMap.end();
    for (StorageAreaMap::iterator it = m_storageAreaMap.begin(); it != end; ++it)
        it->value->closeDatabaseIfIdle();
}

} // namespace WebCore

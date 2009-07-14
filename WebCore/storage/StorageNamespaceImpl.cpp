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

#include "config.h"
#include "StorageNamespaceImpl.h"

#if ENABLE(DOM_STORAGE)

#include <wtf/StdLibExtras.h>

namespace WebCore {

typedef HashMap<String, StorageNamespace*> LocalStorageNamespaceMap;

static LocalStorageNamespaceMap& localStorageNamespaceMap()
{
    DEFINE_STATIC_LOCAL(LocalStorageNamespaceMap, localStorageNamespaceMap, ());
    return localStorageNamespaceMap;
}

PassRefPtr<StorageNamespace> StorageNamespaceImpl::localStorageNamespace(const String& path)
{
    const String lookupPath = path.isNull() ? String("") : path;
    LocalStorageNamespaceMap::iterator it = localStorageNamespaceMap().find(lookupPath);
    if (it == localStorageNamespaceMap().end()) {
        RefPtr<StorageNamespace> storageNamespace = adoptRef(new StorageNamespaceImpl(LocalStorage, lookupPath));
        localStorageNamespaceMap().set(lookupPath, storageNamespace.get());
        return storageNamespace.release();
    }

    return it->second;
}

PassRefPtr<StorageNamespace> StorageNamespaceImpl::sessionStorageNamespace()
{
    return adoptRef(new StorageNamespaceImpl(SessionStorage, String()));
}

StorageNamespaceImpl::StorageNamespaceImpl(StorageType storageType, const String& path)
    : m_storageType(storageType)
    , m_path(path.copy())  // FIXME: Is the .copy necessary?
    , m_syncManager(0)
#ifndef NDEBUG
    , m_isShutdown(false)
#endif
{
    if (m_storageType == LocalStorage && !m_path.isEmpty())
        m_syncManager = StorageSyncManager::create(m_path);
}

StorageNamespaceImpl::~StorageNamespaceImpl()
{
    ASSERT(isMainThread());

    if (m_storageType == LocalStorage) {
        ASSERT(localStorageNamespaceMap().get(m_path) == this);
        localStorageNamespaceMap().remove(m_path);
    }
}

PassRefPtr<StorageNamespace> StorageNamespaceImpl::copy()
{
    ASSERT(isMainThread());
    ASSERT(!m_isShutdown);

    StorageNamespaceImpl* newNamespace = new StorageNamespaceImpl(m_storageType, m_path);

    StorageAreaMap::iterator end = m_storageAreaMap.end();
    for (StorageAreaMap::iterator i = m_storageAreaMap.begin(); i != end; ++i) {
        RefPtr<StorageArea> areaCopy = i->second->copy(i->first.get());
        newNamespace->m_storageAreaMap.set(i->first, areaCopy.release());
    }

    return adoptRef(newNamespace);
}

PassRefPtr<StorageArea> StorageNamespaceImpl::storageArea(SecurityOrigin* origin)
{
    ASSERT(isMainThread());
    ASSERT(!m_isShutdown);

    RefPtr<StorageArea> storageArea;
    if (storageArea = m_storageAreaMap.get(origin))
        return storageArea.release();

    storageArea = StorageArea::create(m_storageType, origin, m_syncManager);
    m_storageAreaMap.set(origin, storageArea);
    return storageArea.release();
}

void StorageNamespaceImpl::close()
{
    ASSERT(isMainThread());
    ASSERT(!m_isShutdown);

    StorageAreaMap::iterator end = m_storageAreaMap.end();
    for (StorageAreaMap::iterator it = m_storageAreaMap.begin(); it != end; ++it)
        it->second->close();

#ifndef NDEBUG
    m_isShutdown = true;
#endif
}

} // namespace WebCore

#endif // ENABLE(DOM_STORAGE)

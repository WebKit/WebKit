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
#include "LocalStorage.h"

#if ENABLE(DOM_STORAGE)

#include "CString.h"
#include "EventNames.h"
#include "FileSystem.h"
#include "Frame.h"
#include "FrameTree.h"
#include "LocalStorageArea.h"
#include "Page.h"
#include "PageGroup.h"
#include "StorageArea.h"
#include "StorageSyncManager.h"
#include <wtf/StdLibExtras.h>

namespace WebCore {

typedef HashMap<String, LocalStorage*> LocalStorageMap;

static LocalStorageMap& localStorageMap()
{
    DEFINE_STATIC_LOCAL(LocalStorageMap, localStorageMap, ());
    return localStorageMap;
}

PassRefPtr<LocalStorage> LocalStorage::localStorage(const String& path)
{
    const String lookupPath = path.isNull() ? String("") : path;
    LocalStorageMap::iterator it = localStorageMap().find(lookupPath);
    if (it == localStorageMap().end()) {
        RefPtr<LocalStorage> localStorage = adoptRef(new LocalStorage(lookupPath));
        localStorageMap().set(lookupPath, localStorage.get());
        return localStorage.release();
    }
    
    return it->second;
}

LocalStorage::LocalStorage(const String& path)
    : m_path(path.copy())
{
    if (!m_path.isEmpty())
        m_syncManager = StorageSyncManager::create(m_path);
}

LocalStorage::~LocalStorage()
{
    ASSERT(localStorageMap().get(m_path) == this);
    localStorageMap().remove(m_path);
}

PassRefPtr<StorageArea> LocalStorage::storageArea(SecurityOrigin* origin)
{
    ASSERT(isMainThread());

    // FIXME: If the security origin in question has never had a storage area established,
    // we need to ask a client call if establishing it is okay.  If the client denies the request,
    // this method will return null.
    // The sourceFrame argument exists for the purpose of asking a client.
    // To know if an area has previously been established, we need to wait until this LocalStorage 
    // object has finished it's AreaImport task.

    // FIXME: If the storage area is being established for the first time here, we need to 
    // sync its existance and quota out to disk via an task of type AreaSync

    RefPtr<LocalStorageArea> storageArea;
    if (storageArea = m_storageAreaMap.get(origin))
        return storageArea.release();

    storageArea = LocalStorageArea::create(origin, m_syncManager);
    m_storageAreaMap.set(origin, storageArea);
    return storageArea.release();
}

void LocalStorage::close()
{
    ASSERT(isMainThread());

    LocalStorageAreaMap::iterator end = m_storageAreaMap.end();
    for (LocalStorageAreaMap::iterator it = m_storageAreaMap.begin(); it != end; ++it)
        it->second->scheduleFinalSync();
    
    m_syncManager = 0;
}

} // namespace WebCore

#endif // ENABLE(DOM_STORAGE)


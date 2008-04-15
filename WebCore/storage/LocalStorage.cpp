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

#include "StorageArea.h"

namespace WebCore {

LocalStorage& LocalStorage::sharedLocalStorage()
{
    static LocalStorage* sharedLocalStorage = 0;
    
    if (!sharedLocalStorage)
        sharedLocalStorage = new LocalStorage();
        
    return *sharedLocalStorage;
}

LocalStorage::LocalStorage()
{
}

PassRefPtr<StorageArea> LocalStorage::storageArea(SecurityOrigin* origin)
{
    RefPtr<StorageArea> storageArea;
    if (storageArea = m_storageAreaMap.get(origin))
        return storageArea.release();
        
    storageArea = StorageArea::create(origin, 0, this);
    m_storageAreaMap.set(origin, storageArea);
    return storageArea.release();
}

void LocalStorage::itemChanged(StorageArea* area, const String& key, const String& oldValue, const String& newValue, Frame* sourceFrame)
{
    // FIXME: Flag this change to be written out to the persistent store

    dispatchStorageEvent(area, key, oldValue, newValue, sourceFrame);
}

void LocalStorage::itemRemoved(StorageArea* area, const String& key, const String& oldValue, Frame* sourceFrame)
{
    // FIXME: Flag this removal to be written out to the persistent store

    dispatchStorageEvent(area, key, oldValue, String(), sourceFrame);
}

void LocalStorage::dispatchStorageEvent(StorageArea* area, const String& key, const String& oldValue, const String& newValue, Frame* sourceFrame)
{
    // FIXME: StorageEvents for local storage areas need to be dispatched to every WebView currently displaying a document associated with this
    // StorageArea.  This will involved refering to all open Pages, mapped by SecurityOrigin
}

} // namespace WebCore

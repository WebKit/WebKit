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
#include "SessionStorage.h"

#include "EventNames.h"
#include "Frame.h"
#include "FrameTree.h"
#include "Page.h"
#include "SecurityOrigin.h"
#include "StorageArea.h"
#include "StorageMap.h"

namespace WebCore {

PassRefPtr<SessionStorage> SessionStorage::create(Page* page)
{
    return adoptRef(new SessionStorage(page));
}

SessionStorage::SessionStorage(Page* page)
    : m_page(page)
{
    ASSERT(m_page);
}

PassRefPtr<SessionStorage> SessionStorage::copy(Page* newPage)
{
    ASSERT(newPage);
    RefPtr<SessionStorage> newSession = SessionStorage::create(newPage);
    
    SessionStorageAreaMap::iterator end = m_storageAreaMap.end();
    for (SessionStorageAreaMap::iterator i = m_storageAreaMap.begin(); i != end; ++i) {
        RefPtr<SessionStorageArea> areaCopy = i->second->copy(i->first.get(), newPage);
        newSession->m_storageAreaMap.set(i->first, areaCopy.release());
    }
    
    return newSession.release();
}

PassRefPtr<StorageArea> SessionStorage::storageArea(SecurityOrigin* origin)
{
    RefPtr<SessionStorageArea> storageArea;
    if (storageArea = m_storageAreaMap.get(origin))
        return storageArea.release();
        
    storageArea = SessionStorageArea::create(origin, m_page);
    m_storageAreaMap.set(origin, storageArea);
    return storageArea.release();
}

}

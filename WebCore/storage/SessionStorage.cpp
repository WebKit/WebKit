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
    
    StorageAreaMap::iterator end = m_storageAreaMap.end();
    for (StorageAreaMap::iterator i = m_storageAreaMap.begin(); i != end; ++i) {
        RefPtr<StorageArea> areaCopy = i->second->copy(i->first.get(), newPage);
        areaCopy->setClient(newSession.get());
        newSession->m_storageAreaMap.set(i->first, areaCopy.release());
    }
    
    return newSession.release();
}

PassRefPtr<StorageArea> SessionStorage::storageArea(SecurityOrigin* origin)
{
    RefPtr<StorageArea> storageArea;
    if (storageArea = m_storageAreaMap.get(origin))
        return storageArea.release();
        
    storageArea = StorageArea::create(origin, m_page, this);
    m_storageAreaMap.set(origin, storageArea);
    return storageArea.release();
}

void SessionStorage::itemChanged(StorageArea* area, const String& key, const String& oldValue, const String& newValue, Frame* sourceFrame)
{
    dispatchStorageEvent(area, key, oldValue, newValue, sourceFrame);
}

void SessionStorage::itemRemoved(StorageArea* area, const String& key, const String& oldValue, Frame* sourceFrame)
{
    dispatchStorageEvent(area, key, oldValue, String(), sourceFrame);
}

void SessionStorage::dispatchStorageEvent(StorageArea* area, const String& key, const String& oldValue, const String& newValue, Frame* sourceFrame)
{
    ASSERT(area->page() == m_page);
    
    // For SessionStorage events, each frame in the page's frametree with the same origin as this Storage needs to be notified of the change
    Vector<RefPtr<Frame> > frames;
    for (Frame* frame = m_page->mainFrame(); frame; frame = frame->tree()->traverseNext())
        frames.append(frame);
        
    for (unsigned i = 0; i < frames.size(); ++i) {
        if (frames[i]->document()->securityOrigin()->equal(area->securityOrigin())) {
            if (HTMLElement* body = frames[i]->document()->body())
                body->dispatchStorageEvent(EventNames::storageEvent, key, oldValue, newValue, sourceFrame);        
        }
    }
}

}

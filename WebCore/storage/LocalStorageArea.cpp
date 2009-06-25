/*
 * Copyright (C) 2008, 2009 Apple Inc. All Rights Reserved.
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
#include "LocalStorageArea.h"

#if ENABLE(DOM_STORAGE)

#include "CString.h"
#include "DOMWindow.h"
#include "EventNames.h"
#include "Frame.h"
#include "HTMLElement.h"
#include "Page.h"
#include "PageGroup.h"
#include "StorageEvent.h"
#include "StorageAreaSync.h"
#include "StorageSyncManager.h"

namespace WebCore {

PassRefPtr<LocalStorageArea> LocalStorageArea::create(SecurityOrigin* origin, PassRefPtr<StorageSyncManager> syncManager)
{
    return adoptRef(new LocalStorageArea(origin, syncManager));
}

LocalStorageArea::LocalStorageArea(SecurityOrigin* origin, PassRefPtr<StorageSyncManager> syncManager)
    : StorageArea(origin)
    , m_storageAreaSync(StorageAreaSync::create(syncManager, this))
    , m_storageSyncManager(syncManager)
{
}

void LocalStorageArea::scheduleFinalSync()
{
    m_storageAreaSync->scheduleFinalSync();
}

void LocalStorageArea::itemChanged(const String& key, const String& oldValue, const String& newValue, Frame* sourceFrame)
{
    m_storageAreaSync->scheduleItemForSync(key, newValue);
    dispatchStorageEvent(key, oldValue, newValue, sourceFrame);
}

void LocalStorageArea::itemRemoved(const String& key, const String& oldValue, Frame* sourceFrame)
{
    m_storageAreaSync->scheduleItemForSync(key, String());
    dispatchStorageEvent(key, oldValue, String(), sourceFrame);
}

void LocalStorageArea::areaCleared(Frame* sourceFrame)
{
    m_storageAreaSync->scheduleClear();
    dispatchStorageEvent(String(), String(), String(), sourceFrame);
}

void LocalStorageArea::blockUntilImportComplete() const
{
    m_storageAreaSync->blockUntilImportComplete();
}

void LocalStorageArea::dispatchStorageEvent(const String& key, const String& oldValue, const String& newValue, Frame* sourceFrame)
{
    ASSERT(isMainThread());

    Page* page = sourceFrame->page();
    if (!page)
        return;

    // Need to copy all relevant frames from every page to a vector, since sending the event to one frame might mutate the frame tree
    // of any given page in the group, or mutate the page group itself
    Vector<RefPtr<Frame> > frames;
    const HashSet<Page*>& pages = page->group().pages();
    
    HashSet<Page*>::const_iterator end = pages.end();
    for (HashSet<Page*>::const_iterator it = pages.begin(); it != end; ++it) {
        for (Frame* frame = (*it)->mainFrame(); frame; frame = frame->tree()->traverseNext()) {
            if (frame->document()->securityOrigin()->equal(securityOrigin()))
                frames.append(frame);
        }
    }

    for (unsigned i = 0; i < frames.size(); ++i)
        frames[i]->document()->dispatchWindowEvent(StorageEvent::create(eventNames().storageEvent, key, oldValue, newValue, sourceFrame->document()->documentURI(), sourceFrame->domWindow(), frames[i]->domWindow()->localStorage()));
}

} // namespace WebCore

#endif // ENABLE(DOM_STORAGE)


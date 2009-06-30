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
#include "StorageArea.h"

#if ENABLE(DOM_STORAGE)

#include "DOMWindow.h"
#include "EventNames.h"
#include "ExceptionCode.h"
#include "Frame.h"
#include "Page.h"
#include "PageGroup.h"
#include "SecurityOrigin.h"
#include "Settings.h"
#include "StorageEvent.h"
#include "StorageAreaSync.h"
#include "StorageMap.h"
#include "StorageSyncManager.h"

namespace WebCore {

PassRefPtr<StorageArea> StorageArea::create(StorageType storageType, SecurityOrigin* origin, PassRefPtr<StorageSyncManager> syncManager)
{
    return adoptRef(new StorageArea(storageType, origin, syncManager));
}

StorageArea::StorageArea(StorageType storageType, SecurityOrigin* origin, PassRefPtr<StorageSyncManager> syncManager)
    : m_storageType(storageType)
    , m_securityOrigin(origin)
    , m_storageMap(StorageMap::create())
    , m_storageSyncManager(syncManager)
#ifndef NDEBUG
    , m_isShutdown(false)
#endif
{
    ASSERT(m_securityOrigin);
    ASSERT(m_storageMap);

    // FIXME: If there's no backing storage for LocalStorage, the default WebKit behavior should be that of private browsing,
    // not silently ignoring it.  https://bugs.webkit.org/show_bug.cgi?id=25894
    if (m_storageSyncManager) {
        m_storageAreaSync = StorageAreaSync::create(m_storageSyncManager, this);
        ASSERT(m_storageAreaSync);
    }
}

PassRefPtr<StorageArea> StorageArea::copy(SecurityOrigin* origin)
{
    ASSERT(!m_isShutdown);
    return adoptRef(new StorageArea(origin, this));
}

StorageArea::StorageArea(SecurityOrigin* origin, StorageArea* area)
    : m_storageType(area->m_storageType)
    , m_securityOrigin(origin)
    , m_storageMap(area->m_storageMap)
    , m_storageSyncManager(area->m_storageSyncManager)
#ifndef NDEBUG
    , m_isShutdown(area->m_isShutdown)
#endif
{
    ASSERT(m_securityOrigin);
    ASSERT(m_storageMap);
    ASSERT(!m_isShutdown);
}

unsigned StorageArea::length() const
{
    ASSERT(!m_isShutdown);
    return m_storageMap->length();
}

String StorageArea::key(unsigned index, ExceptionCode& ec) const
{
    ASSERT(!m_isShutdown);
    blockUntilImportComplete();
    
    String key;
    
    if (!m_storageMap->key(index, key)) {
        ec = INDEX_SIZE_ERR;
        return String();
    }
        
    return key;
}

String StorageArea::getItem(const String& key) const
{
    ASSERT(!m_isShutdown);
    blockUntilImportComplete();
    
    return m_storageMap->getItem(key);
}

void StorageArea::setItem(const String& key, const String& value, ExceptionCode& ec, Frame* frame)
{
    ASSERT(!m_isShutdown);
    ASSERT(!value.isNull());
    blockUntilImportComplete();
    
    if (frame->page()->settings()->privateBrowsingEnabled()) {
        ec = QUOTA_EXCEEDED_ERR;
        return;
    }

    // FIXME: For LocalStorage where a disk quota will be enforced, here is where we need to do quota checking.
    //        If we decide to enforce a memory quota for SessionStorage, this is where we'd do that, also.
    // if (<over quota>) {
    //     ec = QUOTA_EXCEEDED_ERR;
    //     return;
    // }
    
    String oldValue;   
    RefPtr<StorageMap> newMap = m_storageMap->setItem(key, value, oldValue);
    
    if (newMap)
        m_storageMap = newMap.release();

    // Only notify the client if an item was actually changed
    if (oldValue != value) {
        if (m_storageAreaSync)
            m_storageAreaSync->scheduleItemForSync(key, value);
        dispatchStorageEvent(key, oldValue, value, frame);
    }
}

void StorageArea::removeItem(const String& key, Frame* frame)
{
    ASSERT(!m_isShutdown);
    blockUntilImportComplete();
    
    if (frame->page()->settings()->privateBrowsingEnabled())
        return;

    String oldValue;
    RefPtr<StorageMap> newMap = m_storageMap->removeItem(key, oldValue);
    if (newMap)
        m_storageMap = newMap.release();

    // Only notify the client if an item was actually removed
    if (!oldValue.isNull()) {
        if (m_storageAreaSync)
            m_storageAreaSync->scheduleItemForSync(key, String());
        dispatchStorageEvent(key, oldValue, String(), frame);
    }
}

void StorageArea::clear(Frame* frame)
{
    ASSERT(!m_isShutdown);
    blockUntilImportComplete();
    
    if (frame->page()->settings()->privateBrowsingEnabled())
        return;
    
    m_storageMap = StorageMap::create();
    
    if (m_storageAreaSync)
        m_storageAreaSync->scheduleClear();
    dispatchStorageEvent(String(), String(), String(), frame);
}

bool StorageArea::contains(const String& key) const
{
    ASSERT(!m_isShutdown);
    blockUntilImportComplete();
    
    return m_storageMap->contains(key);
}

void StorageArea::importItem(const String& key, const String& value)
{
    ASSERT(!m_isShutdown);
    m_storageMap->importItem(key, value);
}

void StorageArea::close()
{
    if (m_storageAreaSync)
        m_storageAreaSync->scheduleFinalSync();

#ifndef NDEBUG
    m_isShutdown = true;
#endif
}

void StorageArea::blockUntilImportComplete() const
{
    if (m_storageAreaSync)
        m_storageAreaSync->blockUntilImportComplete();
}

void StorageArea::dispatchStorageEvent(const String& key, const String& oldValue, const String& newValue, Frame* sourceFrame)
{
    // We need to copy all relevant frames from every page to a vector since sending the event to one frame might mutate the frame tree
    // of any given page in the group or mutate the page group itself.
    Vector<RefPtr<Frame> > frames;

    // FIXME: When can this occur?
    Page* page = sourceFrame->page();
    if (!page)
        return;

    if (m_storageType == SessionStorage) {
        // Send events only to our page.
        for (Frame* frame = page->mainFrame(); frame; frame = frame->tree()->traverseNext()) {
            if (frame->document()->securityOrigin()->equal(securityOrigin()))
                frames.append(frame);
        }
        
        for (unsigned i = 0; i < frames.size(); ++i)
            frames[i]->document()->dispatchWindowEvent(StorageEvent::create(eventNames().storageEvent, key, oldValue, newValue, sourceFrame->document()->documentURI(), sourceFrame->domWindow(), frames[i]->domWindow()->sessionStorage()));
    } else {
        // Send events to every page.
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
}

}

#endif // ENABLE(DOM_STORAGE)


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
#include "OriginStorage.h"

#include "CString.h"
#include "EventNames.h"
#include "ExceptionCode.h"
#include "Frame.h"
#include "FrameTree.h"
#include "Page.h"
#include "PlatformString.h"
#include "SecurityOrigin.h"
#include "StorageMap.h"

namespace WebCore {

PassRefPtr<OriginStorage> OriginStorage::create(Page* page, SecurityOrigin* origin)
{
    return adoptRef(new OriginStorage(page, origin));
}

OriginStorage::OriginStorage(Page* page, SecurityOrigin* origin)
    : m_page(page)
    , m_securityOrigin(origin)
    , m_storageMap(StorageMap::create())
{
}

OriginStorage::OriginStorage(Page* page, SecurityOrigin* origin, PassRefPtr<StorageMap> map)
    : m_page(page)
    , m_securityOrigin(origin)
    , m_storageMap(map)
{
}

OriginStorage::~OriginStorage()
{
}

PassRefPtr<OriginStorage> OriginStorage::copy(Page* newPage, SecurityOrigin* origin)
{
    return adoptRef(new OriginStorage(newPage, origin, m_storageMap));
}

unsigned OriginStorage::length() const
{
    return m_storageMap->length();
}

String OriginStorage::key(unsigned index, ExceptionCode& ec) const
{
    String key;
    
    if (!m_storageMap->key(index, key)) {
        ec = INDEX_SIZE_ERR;
        return String();
    }
        
    return key;
}

String OriginStorage::getItem(const String& key) const
{
    return m_storageMap->getItem(key);
}

void OriginStorage::setItem(const String& key, const String& value, ExceptionCode& ec, Frame* frame)
{
    // Per the spec, inserting a NULL value into the map is the same as removing the key altogether
    if (value.isNull()) {
        removeItem(key, frame);
        return;
    }
    
    // FIXME: For LocalStorage where a disk quota will be enforced, here is where we need to do quota checking.
    //        If we decide to enforce a memory quota for SessionStorage, this is where we'd do that, also.
    // if (<over quota>) {
    //     ec = INVALID_ACCESS_ERR;
    //     return;
    // }
    
    String oldValue;   
    RefPtr<StorageMap> newMap = m_storageMap->setItem(key, value, oldValue);
    
    if (newMap)
        m_storageMap = newMap.release();
    
    dispatchStorageEvent(key, oldValue, value, frame);
}

void OriginStorage::removeItem(const String& key, Frame* frame)
{    
    String oldValue;
    RefPtr<StorageMap> newMap = m_storageMap->removeItem(key, oldValue);
    if (newMap)
        m_storageMap = newMap.release();
    
    // Fire a StorageEvent only if an item was actually removed
    if (!oldValue.isNull())
        dispatchStorageEvent(key, oldValue, String(), frame);
}

bool OriginStorage::contains(const String& key) const
{
    return m_storageMap->contains(key);
}

void OriginStorage::dispatchStorageEvent(const String& key, const String& oldValue, const String& newValue, Frame* sourceFrame)
{
    // For SessionStorage events, each frame in the page's frametree with the same origin as this Storage needs to be notified of the change
    for (Frame* frame = m_page->mainFrame(); frame; frame = frame->tree()->traverseNext()) {
        if (frame->document()->securityOrigin()->equal(m_securityOrigin.get())) {
            if (HTMLElement* body = frame->document()->body())
                body->dispatchStorageEvent(EventNames::storageEvent, key, oldValue, newValue, sourceFrame);        
        }
    }
}

}

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

#include "CString.h"
#include "ExceptionCode.h"
#include "PlatformString.h"
#include "SecurityOrigin.h"
#include "StorageAreaClient.h"
#include "StorageMap.h"

namespace WebCore {

PassRefPtr<StorageArea> StorageArea::create(SecurityOrigin* origin, Page* page, PassRefPtr<StorageAreaClient> client)
{
    return adoptRef(new StorageArea(origin, page, client));
}

StorageArea::StorageArea(SecurityOrigin* origin, Page* page, PassRefPtr<StorageAreaClient> client)
    : m_page(page)
    , m_securityOrigin(origin)
    , m_storageMap(StorageMap::create())
    , m_client(client)
{
    ASSERT(m_client);
}

StorageArea::StorageArea(SecurityOrigin* origin, Page* page, PassRefPtr<StorageMap> map, PassRefPtr<StorageAreaClient> client)
    : m_page(page)
    , m_securityOrigin(origin)
    , m_storageMap(map)
    , m_client(client)
{
    ASSERT(m_client);
}

StorageArea::~StorageArea()
{
}

PassRefPtr<StorageArea> StorageArea::copy(SecurityOrigin* origin, Page* newPage)
{
    return adoptRef(new StorageArea(origin, newPage, m_storageMap, m_client));
}

unsigned StorageArea::length() const
{
    return m_storageMap->length();
}

String StorageArea::key(unsigned index, ExceptionCode& ec) const
{
    String key;
    
    if (!m_storageMap->key(index, key)) {
        ec = INDEX_SIZE_ERR;
        return String();
    }
        
    return key;
}

String StorageArea::getItem(const String& key) const
{
    return m_storageMap->getItem(key);
}

void StorageArea::setItem(const String& key, const String& value, ExceptionCode& ec, Frame* frame)
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

    // Only notify the client if an item was actually changed
    if (oldValue != value)
        m_client->itemChanged(this, key, oldValue, value, frame);
}

void StorageArea::removeItem(const String& key, Frame* frame)
{    
    String oldValue;
    RefPtr<StorageMap> newMap = m_storageMap->removeItem(key, oldValue);
    if (newMap)
        m_storageMap = newMap.release();

    // Only notify the client if an item was actually removed
    if (!oldValue.isNull())
        m_client->itemRemoved(this, key, oldValue, frame);
}

bool StorageArea::contains(const String& key) const
{
    return m_storageMap->contains(key);
}

void StorageArea::setClient(PassRefPtr<StorageAreaClient> client)
{
    ASSERT(client);
    m_client = client;
}

}

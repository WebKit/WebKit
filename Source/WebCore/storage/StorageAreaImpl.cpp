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
#include "StorageAreaImpl.h"

#include "Document.h"
#include "ExceptionCode.h"
#include "Frame.h"
#include "Page.h"
#include "SchemeRegistry.h"
#include "SecurityOrigin.h"
#include "Settings.h"
#include "StorageAreaSync.h"
#include "StorageEventDispatcher.h"
#include "StorageMap.h"
#include "StorageSyncManager.h"
#include "StorageTracker.h"
#include <wtf/MainThread.h>

namespace WebCore {

StorageAreaImpl::~StorageAreaImpl()
{
    ASSERT(isMainThread());
}

inline StorageAreaImpl::StorageAreaImpl(StorageType storageType, PassRefPtr<SecurityOrigin> origin, PassRefPtr<StorageSyncManager> syncManager, unsigned quota)
    : m_storageType(storageType)
    , m_securityOrigin(origin)
    , m_storageMap(StorageMap::create(quota))
    , m_storageSyncManager(syncManager)
#ifndef NDEBUG
    , m_isShutdown(false)
#endif
{
    ASSERT(isMainThread());
    ASSERT(m_securityOrigin);
    ASSERT(m_storageMap);
    
    // Accessing the shared global StorageTracker when a StorageArea is created 
    // ensures that the tracker is properly initialized before anyone actually needs to use it.
    StorageTracker::tracker();
}

PassRefPtr<StorageAreaImpl> StorageAreaImpl::create(StorageType storageType, PassRefPtr<SecurityOrigin> origin, PassRefPtr<StorageSyncManager> syncManager, unsigned quota)
{
    RefPtr<StorageAreaImpl> area = adoptRef(new StorageAreaImpl(storageType, origin, syncManager, quota));

    // FIXME: If there's no backing storage for LocalStorage, the default WebKit behavior should be that of private browsing,
    // not silently ignoring it. https://bugs.webkit.org/show_bug.cgi?id=25894
    if (area->m_storageSyncManager) {
        area->m_storageAreaSync = StorageAreaSync::create(area->m_storageSyncManager, area.get(), area->m_securityOrigin->databaseIdentifier());
        ASSERT(area->m_storageAreaSync);
    }

    return area.release();
}

PassRefPtr<StorageAreaImpl> StorageAreaImpl::copy()
{
    ASSERT(!m_isShutdown);
    return adoptRef(new StorageAreaImpl(this));
}

StorageAreaImpl::StorageAreaImpl(StorageAreaImpl* area)
    : m_storageType(area->m_storageType)
    , m_securityOrigin(area->m_securityOrigin)
    , m_storageMap(area->m_storageMap)
    , m_storageSyncManager(area->m_storageSyncManager)
#ifndef NDEBUG
    , m_isShutdown(area->m_isShutdown)
#endif
{
    ASSERT(isMainThread());
    ASSERT(m_securityOrigin);
    ASSERT(m_storageMap);
    ASSERT(!m_isShutdown);
}

bool StorageAreaImpl::disabledByPrivateBrowsingInFrame(const Frame* frame) const
{
    if (!frame->page())
        return true;
    if (!frame->page()->settings()->privateBrowsingEnabled())
        return false;
    if (m_storageType != LocalStorage)
        return true;
    return !SchemeRegistry::allowsLocalStorageAccessInPrivateBrowsing(frame->document()->securityOrigin()->protocol());
}

unsigned StorageAreaImpl::length(Frame*) const
{
    ASSERT(!m_isShutdown);
    blockUntilImportComplete();

    return m_storageMap->length();
}

String StorageAreaImpl::key(unsigned index, Frame*) const
{
    ASSERT(!m_isShutdown);
    blockUntilImportComplete();

    return m_storageMap->key(index);
}

String StorageAreaImpl::getItem(const String& key, Frame*) const
{
    ASSERT(!m_isShutdown);
    blockUntilImportComplete();

    return m_storageMap->getItem(key);
}

void StorageAreaImpl::setItem(const String& key, const String& value, ExceptionCode& ec, Frame* frame)
{
    ASSERT(!m_isShutdown);
    ASSERT(!value.isNull());
    blockUntilImportComplete();

    if (disabledByPrivateBrowsingInFrame(frame)) {
        ec = QUOTA_EXCEEDED_ERR;
        return;
    }

    String oldValue;
    bool quotaException;
    RefPtr<StorageMap> newMap = m_storageMap->setItem(key, value, oldValue, quotaException);
    if (newMap)
        m_storageMap = newMap.release();

    if (quotaException) {
        ec = QUOTA_EXCEEDED_ERR;
        return;
    }

    if (oldValue == value)
        return;

    if (m_storageAreaSync)
        m_storageAreaSync->scheduleItemForSync(key, value);
    StorageEventDispatcher::dispatch(key, oldValue, value, m_storageType, m_securityOrigin.get(), frame);
}

void StorageAreaImpl::removeItem(const String& key, Frame* frame)
{
    ASSERT(!m_isShutdown);
    blockUntilImportComplete();

    if (disabledByPrivateBrowsingInFrame(frame))
        return;

    String oldValue;
    RefPtr<StorageMap> newMap = m_storageMap->removeItem(key, oldValue);
    if (newMap)
        m_storageMap = newMap.release();

    if (oldValue.isNull())
        return;

    if (m_storageAreaSync)
        m_storageAreaSync->scheduleItemForSync(key, String());
    StorageEventDispatcher::dispatch(key, oldValue, String(), m_storageType, m_securityOrigin.get(), frame);
}

void StorageAreaImpl::clear(Frame* frame)
{
    ASSERT(!m_isShutdown);
    blockUntilImportComplete();

    if (disabledByPrivateBrowsingInFrame(frame))
        return;

    if (!m_storageMap->length())
        return;

    unsigned quota = m_storageMap->quota();
    m_storageMap = StorageMap::create(quota);

    if (m_storageAreaSync)
        m_storageAreaSync->scheduleClear();
    StorageEventDispatcher::dispatch(String(), String(), String(), m_storageType, m_securityOrigin.get(), frame);
}

bool StorageAreaImpl::contains(const String& key, Frame*) const
{
    ASSERT(!m_isShutdown);
    blockUntilImportComplete();

    return m_storageMap->contains(key);
}

void StorageAreaImpl::importItem(const String& key, const String& value)
{
    ASSERT(!m_isShutdown);
    m_storageMap->importItem(key, value);
}

void StorageAreaImpl::close()
{
    if (m_storageAreaSync)
        m_storageAreaSync->scheduleFinalSync();

#ifndef NDEBUG
    m_isShutdown = true;
#endif
}

void StorageAreaImpl::clearForOriginDeletion()
{
    ASSERT(!m_isShutdown);
    blockUntilImportComplete();
    
    if (m_storageMap->length()) {
        unsigned quota = m_storageMap->quota();
        m_storageMap = StorageMap::create(quota);
    }

    if (m_storageAreaSync) {
        m_storageAreaSync->scheduleClear();
        m_storageAreaSync->scheduleCloseDatabase();
    }
}
    
void StorageAreaImpl::sync()
{
    ASSERT(!m_isShutdown);
    blockUntilImportComplete();
    
    if (m_storageAreaSync)
        m_storageAreaSync->scheduleSync();
}

void StorageAreaImpl::blockUntilImportComplete() const
{
    if (m_storageAreaSync)
        m_storageAreaSync->blockUntilImportComplete();
}

}

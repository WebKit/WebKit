/*
 * Copyright (C) 2013-2021 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "MemoryStorageArea.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(MemoryStorageArea);

Ref<MemoryStorageArea> MemoryStorageArea::create(const WebCore::ClientOrigin& origin, StorageAreaBase::StorageType type)
{
    return adoptRef(*new MemoryStorageArea(origin, type));
}

MemoryStorageArea::MemoryStorageArea(const WebCore::ClientOrigin& origin, StorageAreaBase::StorageType type)
    : StorageAreaBase(WebCore::StorageMap::noQuota, origin)
    , m_map(WebCore::StorageMap(WebCore::StorageMap::noQuota))
    , m_storageType(type)
{
}

bool MemoryStorageArea::isEmpty()
{
    return !m_map.length();
}

void MemoryStorageArea::clear()
{
    m_map.clear();
    notifyListenersAboutClear();
}

HashMap<String, String> MemoryStorageArea::allItems()
{
    return m_map.items();
}

Expected<void, StorageError> MemoryStorageArea::setItem(IPC::Connection::UniqueID connection, StorageAreaImplIdentifier storageAreaImplID, String&& key, String&& value, const String& urlString)
{
    String oldValue;
    bool hasQuotaError = false;
    m_map.setItem(key, value, oldValue, hasQuotaError);
    if (hasQuotaError)
        return makeUnexpected(StorageError::QuotaExceeded);

    dispatchEvents(connection, storageAreaImplID, key, oldValue, value, urlString);

    return { };
}

Expected<void, StorageError> MemoryStorageArea::removeItem(IPC::Connection::UniqueID connection, StorageAreaImplIdentifier storageAreaImplID, const String& key, const String& urlString)
{
    String oldValue;
    m_map.removeItem(key, oldValue);
    dispatchEvents(connection, storageAreaImplID, key, oldValue, String(), urlString);

    return { };
}

Expected<void, StorageError> MemoryStorageArea::clear(IPC::Connection::UniqueID connection, StorageAreaImplIdentifier implIdentifier, const String& urlString)
{
    if (!m_map.length())
        return { };

    m_map.clear();
    dispatchEvents(connection, implIdentifier, String(), String(), String(), urlString);

    return { };
}

Ref<MemoryStorageArea> MemoryStorageArea::clone() const
{
    Ref storageArea = MemoryStorageArea::create(origin(), m_storageType);
    storageArea->m_map = m_map;
    return storageArea;
}

} // namespace WebKit

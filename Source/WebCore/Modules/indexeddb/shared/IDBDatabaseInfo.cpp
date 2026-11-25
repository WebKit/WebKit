/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#include "IDBDatabaseInfo.h"

#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(IDBDatabaseInfo);

IDBDatabaseInfo::IDBDatabaseInfo(StringImpl* name, uint64_t version, uint64_t maxIndexID, HashMap<IDBObjectStoreIdentifier, IDBObjectStoreInfo>&& objectStoreMap)
    : m_name(name)
    , m_version(version)
    , m_maxIndexID(maxIndexID)
    , m_objectStoreMap(WTF::move(objectStoreMap))
{
}

IDBDatabaseInfo::IDBDatabaseInfo(const IDBDatabaseInfo& other, IsolatedCopyTag)
    : m_name(WTF::isolatedCopy(other.m_name))
    , m_version(other.m_version)
    , m_maxIndexID(other.m_maxIndexID)
{
    for (const auto& entry : other.m_objectStoreMap)
        m_objectStoreMap.set(entry.key, entry.value.isolatedCopy());
}

IDBDatabaseInfo IDBDatabaseInfo::isolatedCopy() const
{
    return { *this, IDBDatabaseInfo::IsolatedCopy };
}

bool IDBDatabaseInfo::hasObjectStore(StringImpl* name) const
{
    for (auto& objectStore : m_objectStoreMap.values()) {
        if (equal(objectStore.name(), name))
            return true;
    }

    return false;
}

IDBObjectStoreInfo IDBDatabaseInfo::createNewObjectStore(StringImpl* name, std::optional<IDBKeyPath>&& keyPath, bool autoIncrement)
{
    // ObjectIdentifier generation begins anew from 1 each time the program is restarted. But the IDBObjectStores
    // held in m_objectStoreMap and their identifiers are perisisted to disk. So we must ensure that newly created
    // identifiers do not conflict with identifiers for pre-existing IDBObjectStores loaded from disk.
    auto objectStoreIdentifier = IDBObjectStoreIdentifier::generate();
    while (m_objectStoreMap.contains(objectStoreIdentifier))
        objectStoreIdentifier = IDBObjectStoreIdentifier::generate();

    IDBObjectStoreInfo info(objectStoreIdentifier, name, WTF::move(keyPath), autoIncrement);
    m_objectStoreMap.set(info.identifier(), info);
    return info;
}

void IDBDatabaseInfo::addExistingObjectStore(const IDBObjectStoreInfo& info)
{
    ASSERT(!m_objectStoreMap.contains(info.identifier()));
    m_objectStoreMap.set(info.identifier(), info);
}

IDBObjectStoreInfo* IDBDatabaseInfo::getInfoForExistingObjectStore(IDBObjectStoreIdentifier objectStoreIdentifier)
{
    auto iterator = m_objectStoreMap.find(objectStoreIdentifier);
    if (iterator == m_objectStoreMap.end())
        return nullptr;

    return &iterator->value;
}

IDBObjectStoreInfo* IDBDatabaseInfo::getInfoForExistingObjectStore(StringImpl* name)
{
    for (auto& objectStore : m_objectStoreMap.values()) {
        if (equal(objectStore.name(), name))
            return &objectStore;
    }

    return nullptr;
}

const IDBObjectStoreInfo* IDBDatabaseInfo::infoForExistingObjectStore(IDBObjectStoreIdentifier objectStoreIdentifier) const
{
    return const_cast<IDBDatabaseInfo*>(this)->getInfoForExistingObjectStore(objectStoreIdentifier);
}

IDBObjectStoreInfo* IDBDatabaseInfo::infoForExistingObjectStore(IDBObjectStoreIdentifier objectStoreIdentifier)
{
    return getInfoForExistingObjectStore(objectStoreIdentifier);
}

const IDBObjectStoreInfo* IDBDatabaseInfo::infoForExistingObjectStore(StringImpl* name) const
{
    return const_cast<IDBDatabaseInfo*>(this)->getInfoForExistingObjectStore(name);
}

IDBObjectStoreInfo* IDBDatabaseInfo::infoForExistingObjectStore(StringImpl* name)
{
    return getInfoForExistingObjectStore(name);
}

void IDBDatabaseInfo::renameObjectStore(IDBObjectStoreIdentifier objectStoreIdentifier, StringImpl* newName)
{
    auto* info = infoForExistingObjectStore(objectStoreIdentifier);
    if (!info)
        return;

    info->rename(newName);
}

Vector<RefPtr<StringImpl>> IDBDatabaseInfo::objectStoreNames() const
{
    return WTF::map(m_objectStoreMap, [](auto& pair) {
        return pair.value.name();
    });
}

void IDBDatabaseInfo::deleteObjectStore(StringImpl* objectStoreName)
{
    auto* info = infoForExistingObjectStore(objectStoreName);
    if (!info)
        return;

    m_objectStoreMap.remove(info->identifier());
}

void IDBDatabaseInfo::deleteObjectStore(IDBObjectStoreIdentifier objectStoreIdentifier)
{
    m_objectStoreMap.remove(objectStoreIdentifier);
}

#if !LOG_DISABLED

RefPtr<StringImpl> IDBDatabaseInfo::loggingString() const
{
    StringBuilder builder;
    builder.append("Database:"_s, m_name, " version "_s, m_version, '\n');
    for (auto& objectStore : m_objectStoreMap.values())
        builder.append(objectStore.loggingString(1), '\n');
    return builder.toString();
}

#endif

void IDBDatabaseInfo::setMaxIndexID(uint64_t maxIndexID)
{
    ASSERT(maxIndexID > m_maxIndexID || (!maxIndexID && !m_maxIndexID));
    m_maxIndexID = maxIndexID;
}

} // namespace WebCore

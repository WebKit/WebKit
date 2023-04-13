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

#include <wtf/IsoMallocInlines.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(IDBDatabaseInfo);

IDBDatabaseInfo::IDBDatabaseInfo(const String& name, uint64_t version, uint64_t maxIndexID, uint64_t maxObjectStoreID, HashMap<uint64_t, IDBObjectStoreInfo>&& objectStoreMap)
    : m_name(name)
    , m_version(version)
    , m_maxIndexID(maxIndexID)
    , m_maxObjectStoreID(maxObjectStoreID)
    , m_objectStoreMap(WTFMove(objectStoreMap))
{
}

IDBDatabaseInfo::IDBDatabaseInfo(const IDBDatabaseInfo& other, IsolatedCopyTag)
    : m_name(other.m_name.isolatedCopy())
    , m_version(other.m_version)
    , m_maxIndexID(other.m_maxIndexID)
    , m_maxObjectStoreID(other.m_maxObjectStoreID)
{
    for (const auto& entry : other.m_objectStoreMap)
        m_objectStoreMap.set(entry.key, entry.value.isolatedCopy());
}

IDBDatabaseInfo IDBDatabaseInfo::isolatedCopy() const
{
    return { *this, IDBDatabaseInfo::IsolatedCopy };
}

bool IDBDatabaseInfo::hasObjectStore(const String& name) const
{
    for (auto& objectStore : m_objectStoreMap.values()) {
        if (objectStore.name() == name)
            return true;
    }

    return false;
}

IDBObjectStoreInfo IDBDatabaseInfo::createNewObjectStore(const String& name, std::optional<IDBKeyPath>&& keyPath, bool autoIncrement)
{
    IDBObjectStoreInfo info(++m_maxObjectStoreID, name, WTFMove(keyPath), autoIncrement);
    m_objectStoreMap.set(info.identifier(), info);
    return info;
}

void IDBDatabaseInfo::addExistingObjectStore(const IDBObjectStoreInfo& info)
{
    ASSERT(!m_objectStoreMap.contains(info.identifier()));

    if (info.identifier() > m_maxObjectStoreID)
        m_maxObjectStoreID = info.identifier();

    m_objectStoreMap.set(info.identifier(), info);
}

IDBObjectStoreInfo* IDBDatabaseInfo::getInfoForExistingObjectStore(uint64_t objectStoreIdentifier)
{
    auto iterator = m_objectStoreMap.find(objectStoreIdentifier);
    if (iterator == m_objectStoreMap.end())
        return nullptr;

    return &iterator->value;
}

IDBObjectStoreInfo* IDBDatabaseInfo::getInfoForExistingObjectStore(const String& name)
{
    for (auto& objectStore : m_objectStoreMap.values()) {
        if (objectStore.name() == name)
            return &objectStore;
    }

    return nullptr;
}

const IDBObjectStoreInfo* IDBDatabaseInfo::infoForExistingObjectStore(uint64_t objectStoreIdentifier) const
{
    return const_cast<IDBDatabaseInfo*>(this)->getInfoForExistingObjectStore(objectStoreIdentifier);
}

IDBObjectStoreInfo* IDBDatabaseInfo::infoForExistingObjectStore(uint64_t objectStoreIdentifier)
{
    return getInfoForExistingObjectStore(objectStoreIdentifier);
}

const IDBObjectStoreInfo* IDBDatabaseInfo::infoForExistingObjectStore(const String& name) const
{
    return const_cast<IDBDatabaseInfo*>(this)->getInfoForExistingObjectStore(name);
}

IDBObjectStoreInfo* IDBDatabaseInfo::infoForExistingObjectStore(const String& name)
{
    return getInfoForExistingObjectStore(name);
}

void IDBDatabaseInfo::renameObjectStore(uint64_t objectStoreIdentifier, const String& newName)
{
    auto* info = infoForExistingObjectStore(objectStoreIdentifier);
    if (!info)
        return;

    info->rename(newName);
}

Vector<String> IDBDatabaseInfo::objectStoreNames() const
{
    return WTF::map(m_objectStoreMap, [](auto& pair) {
        return pair.value.name();
    });
}

void IDBDatabaseInfo::deleteObjectStore(const String& objectStoreName)
{
    auto* info = infoForExistingObjectStore(objectStoreName);
    if (!info)
        return;

    m_objectStoreMap.remove(info->identifier());
}

void IDBDatabaseInfo::deleteObjectStore(uint64_t objectStoreIdentifier)
{
    m_objectStoreMap.remove(objectStoreIdentifier);
}

#if !LOG_DISABLED

String IDBDatabaseInfo::loggingString() const
{
    StringBuilder builder;
    builder.append("Database:", m_name, " version ", m_version, '\n');
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

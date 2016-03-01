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

#if ENABLE(INDEXED_DATABASE)

namespace WebCore {

IDBDatabaseInfo::IDBDatabaseInfo()
{
}

IDBDatabaseInfo::IDBDatabaseInfo(const String& name, uint64_t version)
    : m_name(name)
    , m_version(version)
{
}

IDBDatabaseInfo::IDBDatabaseInfo(const IDBDatabaseInfo& other, IsolatedCopyTag)
    : m_name(other.m_name.isolatedCopy())
    , m_version(other.m_version)
    , m_maxObjectStoreID(other.m_maxObjectStoreID)
{
    for (auto entry : other.m_objectStoreMap)
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

IDBObjectStoreInfo IDBDatabaseInfo::createNewObjectStore(const String& name, const IDBKeyPath& keyPath, bool autoIncrement)
{
    IDBObjectStoreInfo info(++m_maxObjectStoreID, name, keyPath, autoIncrement);
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

Vector<String> IDBDatabaseInfo::objectStoreNames() const
{
    Vector<String> names;
    names.reserveCapacity(m_objectStoreMap.size());
    for (auto& objectStore : m_objectStoreMap.values())
        names.uncheckedAppend(objectStore.name());

    return names;
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

#ifndef NDEBUG
String IDBDatabaseInfo::loggingString() const
{
    String top = makeString("Database: ", m_name, " version ", String::number(m_version), "\n");
    for (auto objectStore : m_objectStoreMap.values())
        top.append(makeString(objectStore.loggingString(1), "\n"));

    return top; 
}
#endif

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)

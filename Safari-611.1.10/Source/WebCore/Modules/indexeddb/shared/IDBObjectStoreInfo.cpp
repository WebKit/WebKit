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
#include "IDBObjectStoreInfo.h"
#include <wtf/text/StringBuilder.h>

#if ENABLE(INDEXED_DATABASE)

namespace WebCore {

IDBObjectStoreInfo::IDBObjectStoreInfo()
{
}

IDBObjectStoreInfo::IDBObjectStoreInfo(uint64_t identifier, const String& name, Optional<IDBKeyPath>&& keyPath, bool autoIncrement)
    : m_identifier(identifier)
    , m_name(name)
    , m_keyPath(WTFMove(keyPath))
    , m_autoIncrement(autoIncrement)
{
}

IDBIndexInfo IDBObjectStoreInfo::createNewIndex(uint64_t indexID, const String& name, IDBKeyPath&& keyPath, bool unique, bool multiEntry)
{
    IDBIndexInfo info(indexID, m_identifier, name, WTFMove(keyPath), unique, multiEntry);
    m_indexMap.set(info.identifier(), info);
    return info;
}

void IDBObjectStoreInfo::addExistingIndex(const IDBIndexInfo& info)
{
    if (m_indexMap.contains(info.identifier()))
        LOG_ERROR("Adding an index '%s' with existing Index ID", info.name().utf8().data());

    m_indexMap.set(info.identifier(), info);
}

bool IDBObjectStoreInfo::hasIndex(const String& name) const
{
    for (auto& index : m_indexMap.values()) {
        if (index.name() == name)
            return true;
    }

    return false;
}

bool IDBObjectStoreInfo::hasIndex(uint64_t indexIdentifier) const
{
    return m_indexMap.contains(indexIdentifier);
}

IDBIndexInfo* IDBObjectStoreInfo::infoForExistingIndex(const String& name)
{
    for (auto& index : m_indexMap.values()) {
        if (index.name() == name)
            return &index;
    }

    return nullptr;
}

IDBIndexInfo* IDBObjectStoreInfo::infoForExistingIndex(uint64_t identifier)
{
    auto iterator = m_indexMap.find(identifier);
    if (iterator == m_indexMap.end())
        return nullptr;

    return &iterator->value;
}

IDBObjectStoreInfo IDBObjectStoreInfo::isolatedCopy() const
{
    IDBObjectStoreInfo result = { m_identifier, m_name.isolatedCopy(), WebCore::isolatedCopy(m_keyPath), m_autoIncrement };

    for (auto& iterator : m_indexMap)
        result.m_indexMap.set(iterator.key, iterator.value.isolatedCopy());

    return result;
}

Vector<String> IDBObjectStoreInfo::indexNames() const
{
    Vector<String> names;
    names.reserveCapacity(m_indexMap.size());
    for (auto& index : m_indexMap.values())
        names.uncheckedAppend(index.name());

    return names;
}

void IDBObjectStoreInfo::deleteIndex(const String& indexName)
{
    auto* info = infoForExistingIndex(indexName);
    if (!info)
        return;

    m_indexMap.remove(info->identifier());
}

void IDBObjectStoreInfo::deleteIndex(uint64_t indexIdentifier)
{
    m_indexMap.remove(indexIdentifier);
}

#if !LOG_DISABLED

String IDBObjectStoreInfo::loggingString(int indent) const
{
    StringBuilder builder;
    for (int i = 0; i < indent; ++i)
        builder.append(' ');
    builder.append("Object store: ", m_name, m_identifier);
    for (auto index : m_indexMap.values())
        builder.append(index.loggingString(indent + 1), '\n');
    return builder.toString();
}

String IDBObjectStoreInfo::condensedLoggingString() const
{
    return makeString("<OS: ", m_name, " (", m_identifier, ")>");
}

#endif

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)

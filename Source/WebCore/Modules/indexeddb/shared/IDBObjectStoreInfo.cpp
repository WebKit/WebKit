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

#include <wtf/CrossThreadCopier.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

IDBObjectStoreInfo::IDBObjectStoreInfo(IDBObjectStoreIdentifier identifier, StringImpl* name, std::optional<IDBKeyPath>&& keyPath, bool autoIncrement, HashMap<IDBIndexIdentifier, IDBIndexInfo>&& indexMap)
    : m_identifier(identifier)
    , m_name(name)
    , m_keyPath(WTF::move(keyPath))
    , m_autoIncrement(autoIncrement)
    , m_indexMap(WTF::move(indexMap))
{
}

IDBIndexInfo IDBObjectStoreInfo::createNewIndex(IDBIndexIdentifier indexID, StringImpl* name, IDBKeyPath&& keyPath, bool unique, bool multiEntry)
{
    IDBIndexInfo info(indexID, m_identifier, name, WTF::move(keyPath), unique, multiEntry);
    m_indexMap.set(info.identifier(), info);
    return info;
}

void IDBObjectStoreInfo::addExistingIndex(const IDBIndexInfo& info)
{
    if (m_indexMap.contains(info.identifier()))
        LOG_ERROR("Adding an index '%s' with existing Index ID", utf8(info.name()));

    m_indexMap.set(info.identifier(), info);
}

bool IDBObjectStoreInfo::hasIndex(StringImpl* name) const
{
    for (auto& index : m_indexMap.values()) {
        if (equal(index.name(), name))
            return true;
    }

    return false;
}

bool IDBObjectStoreInfo::hasIndex(IDBIndexIdentifier indexIdentifier) const
{
    return m_indexMap.contains(indexIdentifier);
}

IDBIndexInfo* IDBObjectStoreInfo::infoForExistingIndex(StringImpl* name)
{
    for (auto& index : m_indexMap.values()) {
        if (equal(index.name(), name))
            return &index;
    }

    return nullptr;
}

IDBIndexInfo* IDBObjectStoreInfo::infoForExistingIndex(IDBIndexIdentifier identifier)
{
    auto iterator = m_indexMap.find(identifier);
    if (iterator == m_indexMap.end())
        return nullptr;

    return &iterator->value;
}

IDBObjectStoreInfo IDBObjectStoreInfo::isolatedCopy() const &
{
    IDBObjectStoreInfo result = { m_identifier, WTF::isolatedCopy(m_name), crossThreadCopy(m_keyPath), m_autoIncrement };
    result.m_indexMap = crossThreadCopy(m_indexMap);
    return result;
}

IDBObjectStoreInfo IDBObjectStoreInfo::isolatedCopy() &&
{
    IDBObjectStoreInfo result = { m_identifier, WTF::isolatedCopy(WTF::move(m_name)), crossThreadCopy(WTF::move(m_keyPath)), m_autoIncrement };
    result.m_indexMap = crossThreadCopy(WTF::move(m_indexMap));
    return result;
}

Vector<RefPtr<StringImpl>> IDBObjectStoreInfo::indexNames() const
{
    return WTF::map(m_indexMap, [](auto& pair) -> RefPtr<StringImpl> {
        return pair.value.name();
    });
}

void IDBObjectStoreInfo::deleteIndex(StringImpl* indexName)
{
    auto* info = infoForExistingIndex(indexName);
    if (!info)
        return;

    m_indexMap.remove(info->identifier());
}

void IDBObjectStoreInfo::deleteIndex(IDBIndexIdentifier indexIdentifier)
{
    m_indexMap.remove(indexIdentifier);
}

#if !LOG_DISABLED

RefPtr<StringImpl> IDBObjectStoreInfo::loggingString(int indent) const
{
    StringBuilder builder;
    for (int i = 0; i < indent; ++i)
        builder.append(' ');
    builder.append("Object store: "_s, m_name, m_identifier);
    for (auto index : m_indexMap.values())
        builder.append(index.loggingString(indent + 1), '\n');
    return builder.toString();
}

RefPtr<StringImpl> IDBObjectStoreInfo::condensedLoggingString() const
{
    return makeString("<OS: "_s, m_name, " ("_s, m_identifier, ")>"_s);
}

#endif

} // namespace WebCore

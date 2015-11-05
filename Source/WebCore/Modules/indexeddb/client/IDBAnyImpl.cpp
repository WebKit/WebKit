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
#include "IDBAnyImpl.h"

#if ENABLE(INDEXED_DATABASE)

#include "IDBAny.h"
#include "IDBCursorWithValue.h"
#include "IDBFactory.h"
#include "IDBIndex.h"

namespace WebCore {
namespace IDBClient {

IDBAny::IDBAny(IDBAny::Type type)
    : m_type(type)
{
}

IDBAny::IDBAny(Ref<IDBDatabase>&& database)
    : m_type(IDBAny::Type::IDBDatabase)
    , m_database(adoptRef(&database.leakRef()))
{
}

IDBAny::IDBAny(Ref<IDBObjectStore>&& objectStore)
    : m_type(IDBAny::Type::IDBObjectStore)
    , m_objectStore(WTF::move(objectStore))
{
}

IDBAny::IDBAny(Ref<IDBIndex>&& index)
    : m_type(IDBAny::Type::IDBIndex)
    , m_index(WTF::move(index))
{
}

IDBAny::IDBAny(const IDBKeyPath& keyPath)
    : m_type(IDBAny::Type::KeyPath)
    , m_idbKeyPath(keyPath)
{
}

IDBAny::IDBAny(const Deprecated::ScriptValue& value)
    : m_type(IDBAny::Type::ScriptValue)
    , m_scriptValue(value)
{
}

IDBAny::~IDBAny()
{
}

RefPtr<WebCore::IDBDatabase> IDBAny::idbDatabase()
{
    ASSERT(m_type == IDBAny::Type::IDBDatabase);
    return m_database.get();
}

RefPtr<WebCore::DOMStringList> IDBAny::domStringList()
{
    return nullptr;
}

RefPtr<WebCore::IDBCursor> IDBAny::idbCursor()
{
    return nullptr;
}

RefPtr<WebCore::IDBCursorWithValue> IDBAny::idbCursorWithValue()
{
    return nullptr;
}

RefPtr<WebCore::IDBFactory> IDBAny::idbFactory()
{
    return nullptr;
}

RefPtr<WebCore::IDBIndex> IDBAny::idbIndex()
{
    return nullptr;
}

RefPtr<WebCore::IDBObjectStore> IDBAny::idbObjectStore()
{
    return nullptr;
}

IDBObjectStore* IDBAny::modernIDBObjectStore()
{
    ASSERT(m_type == IDBAny::Type::IDBObjectStore);
    return m_objectStore.get();
}

IDBIndex* IDBAny::modernIDBIndex()
{
    ASSERT(m_type == IDBAny::Type::IDBIndex);
    return m_index.get();
}

RefPtr<WebCore::IDBTransaction> IDBAny::idbTransaction()
{
    return nullptr;
}

const Deprecated::ScriptValue& IDBAny::scriptValue()
{
    return m_scriptValue;
}

int64_t IDBAny::integer()
{
    return m_integer;
}

const String& IDBAny::string()
{
    return m_string;
}

const IDBKeyPath& IDBAny::keyPath()
{
    return m_idbKeyPath;
}

} // namespace IDBClient
} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)

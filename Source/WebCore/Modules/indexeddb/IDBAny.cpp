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
#include "IDBAny.h"

#if ENABLE(INDEXED_DATABASE)

#include "DOMStringList.h"
#include "IDBCursorWithValue.h"
#include "IDBDatabase.h"
#include "IDBFactory.h"
#include "IDBIndex.h"
#include "IDBObjectStore.h"
#include "IDBTransaction.h"

namespace WebCore {

IDBAny::IDBAny(IDBAny::Type type)
    : m_type(type)
{
}

IDBAny::IDBAny(Ref<IDBDatabase>&& database)
    : m_type(IDBAny::Type::IDBDatabase)
    , m_database(WTFMove(database))
{
}

IDBAny::IDBAny(Ref<IDBObjectStore>&& objectStore)
    : m_type(IDBAny::Type::IDBObjectStore)
    , m_objectStore(WTFMove(objectStore))
{
}

IDBAny::IDBAny(Ref<IDBIndex>&& index)
    : m_type(IDBAny::Type::IDBIndex)
    , m_index(WTFMove(index))
{
}

IDBAny::IDBAny(Ref<IDBCursor>&& cursor)
{
    if (cursor->isKeyCursor()) {
        m_type = IDBAny::Type::IDBCursor;
        m_cursor = WTFMove(cursor);
    } else {
        m_type = IDBAny::Type::IDBCursorWithValue;
        m_cursorWithValue = static_cast<IDBCursorWithValue*>(&cursor.get());
    }
}

IDBAny::IDBAny(const IDBKeyPath& keyPath)
    : m_type(IDBAny::Type::KeyPath)
    , m_idbKeyPath(keyPath)
{
}

IDBAny::IDBAny(const JSC::Strong<JSC::Unknown>& value)
    : m_type(IDBAny::Type::ScriptValue)
    , m_scriptValue(value)
{
}

IDBAny::~IDBAny()
{
}

RefPtr<IDBDatabase> IDBAny::idbDatabase()
{
    ASSERT(m_type == IDBAny::Type::IDBDatabase);
    return m_database.get();
}

RefPtr<DOMStringList> IDBAny::domStringList()
{
    return nullptr;
}

RefPtr<IDBCursor> IDBAny::idbCursor()
{
    ASSERT(m_type == IDBAny::Type::IDBCursor || m_type == IDBAny::Type::IDBCursorWithValue);
    return m_cursor ? m_cursor.get() : m_cursorWithValue.get();
}

RefPtr<IDBCursorWithValue> IDBAny::idbCursorWithValue()
{
    ASSERT(m_type == IDBAny::Type::IDBCursorWithValue);
    return m_cursorWithValue.get();
}

RefPtr<IDBFactory> IDBAny::idbFactory()
{
    return nullptr;
}

RefPtr<IDBIndex> IDBAny::idbIndex()
{
    ASSERT(m_type == IDBAny::Type::IDBIndex);
    return m_index.get();
}

RefPtr<IDBObjectStore> IDBAny::idbObjectStore()
{
    ASSERT(m_type == IDBAny::Type::IDBObjectStore);
    return m_objectStore.get();
}

RefPtr<IDBTransaction> IDBAny::idbTransaction()
{
    return nullptr;
}

JSC::JSValue IDBAny::scriptValue()
{
    return m_scriptValue.get();
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

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)

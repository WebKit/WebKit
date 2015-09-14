/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "LegacyAny.h"

#if ENABLE(INDEXED_DATABASE)

#include "DOMStringList.h"
#include "IDBKeyPath.h"
#include "LegacyCursorWithValue.h"
#include "LegacyDatabase.h"
#include "LegacyFactory.h"
#include "LegacyIndex.h"
#include "LegacyObjectStore.h"

namespace WebCore {

PassRefPtr<LegacyAny> LegacyAny::createInvalid()
{
    return adoptRef(new LegacyAny(UndefinedType));
}

PassRefPtr<LegacyAny> LegacyAny::createNull()
{
    return adoptRef(new LegacyAny(NullType));
}

PassRefPtr<LegacyAny> LegacyAny::createString(const String& value)
{
    return adoptRef(new LegacyAny(value));
}

LegacyAny::LegacyAny(Type type)
    : m_type(type)
    , m_integer(0)
{
    ASSERT(type == UndefinedType || type == NullType);
}

LegacyAny::~LegacyAny()
{
}

PassRefPtr<DOMStringList> LegacyAny::domStringList()
{
    ASSERT(m_type == DOMStringListType);
    return m_domStringList;
}

PassRefPtr<IDBCursor> LegacyAny::idbCursor()
{
    ASSERT(m_type == IDBCursorType);
    return m_idbCursor;
}

PassRefPtr<IDBCursorWithValue> LegacyAny::idbCursorWithValue()
{
    ASSERT(m_type == IDBCursorWithValueType);
    return m_idbCursorWithValue;
}

PassRefPtr<IDBDatabase> LegacyAny::idbDatabase()
{
    ASSERT(m_type == IDBDatabaseType);
    return m_idbDatabase;
}

PassRefPtr<IDBFactory> LegacyAny::idbFactory()
{
    ASSERT(m_type == IDBFactoryType);
    return m_idbFactory;
}

PassRefPtr<IDBIndex> LegacyAny::idbIndex()
{
    ASSERT(m_type == IDBIndexType);
    return m_idbIndex;
}

PassRefPtr<IDBObjectStore> LegacyAny::idbObjectStore()
{
    ASSERT(m_type == IDBObjectStoreType);
    return m_idbObjectStore;
}

PassRefPtr<IDBTransaction> LegacyAny::idbTransaction()
{
    ASSERT(m_type == IDBTransactionType);
    return m_idbTransaction;
}

const Deprecated::ScriptValue& LegacyAny::scriptValue()
{
    ASSERT(m_type == ScriptValueType);
    return m_scriptValue;
}

const String& LegacyAny::string()
{
    ASSERT(m_type == StringType);
    return m_string;
}

int64_t LegacyAny::integer()
{
    ASSERT(m_type == IntegerType);
    return m_integer;
}

LegacyAny::LegacyAny(PassRefPtr<DOMStringList> value)
    : m_type(DOMStringListType)
    , m_domStringList(value)
    , m_integer(0)
{
}

LegacyAny::LegacyAny(PassRefPtr<LegacyCursorWithValue> value)
    : m_type(IDBCursorWithValueType)
    , m_idbCursorWithValue(value)
    , m_integer(0)
{
}

LegacyAny::LegacyAny(PassRefPtr<LegacyCursor> value)
    : m_type(IDBCursorType)
    , m_idbCursor(value)
    , m_integer(0)
{
}

LegacyAny::LegacyAny(PassRefPtr<LegacyDatabase> value)
    : m_type(IDBDatabaseType)
    , m_idbDatabase(value)
    , m_integer(0)
{
}

LegacyAny::LegacyAny(PassRefPtr<LegacyFactory> value)
    : m_type(IDBFactoryType)
    , m_idbFactory(value)
    , m_integer(0)
{
}

LegacyAny::LegacyAny(PassRefPtr<LegacyIndex> value)
    : m_type(IDBIndexType)
    , m_idbIndex(value)
    , m_integer(0)
{
}

LegacyAny::LegacyAny(PassRefPtr<LegacyTransaction> value)
    : m_type(IDBTransactionType)
    , m_idbTransaction(value)
    , m_integer(0)
{
}

LegacyAny::LegacyAny(PassRefPtr<LegacyObjectStore> value)
    : m_type(IDBObjectStoreType)
    , m_idbObjectStore(value)
    , m_integer(0)
{
}

LegacyAny::LegacyAny(const Deprecated::ScriptValue& value)
    : m_type(ScriptValueType)
    , m_scriptValue(value)
    , m_integer(0)
{
}

LegacyAny::LegacyAny(const IDBKeyPath& value)
    : m_type(KeyPathType)
    , m_idbKeyPath(value)
    , m_integer(0)
{
}

LegacyAny::LegacyAny(const String& value)
    : m_type(StringType)
    , m_string(value)
    , m_integer(0)
{
}

LegacyAny::LegacyAny(int64_t value)
    : m_type(IntegerType)
    , m_integer(value)
{
}

LegacyCursor* LegacyAny::legacyCursor()
{
    ASSERT(m_type == IDBCursorType);
    return m_idbCursor.get();
}

LegacyCursorWithValue* LegacyAny::legacyCursorWithValue()
{
    ASSERT(m_type == IDBCursorWithValueType);
    return m_idbCursorWithValue.get();
}

LegacyDatabase* LegacyAny::legacyDatabase()
{
    ASSERT(m_type == IDBDatabaseType);
    return m_idbDatabase.get();
}

LegacyFactory* LegacyAny::legacyFactory()
{
    ASSERT(m_type == IDBFactoryType);
    return m_idbFactory.get();
}

LegacyIndex* LegacyAny::legacyIndex()
{
    ASSERT(m_type == IDBIndexType);
    return m_idbIndex.get();
}

LegacyObjectStore* LegacyAny::legacyObjectStore()
{
    ASSERT(m_type == IDBObjectStoreType);
    return m_idbObjectStore.get();
}

LegacyTransaction* LegacyAny::legacyTransaction()
{
    ASSERT(m_type == IDBTransactionType);
    return m_idbTransaction.get();
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)


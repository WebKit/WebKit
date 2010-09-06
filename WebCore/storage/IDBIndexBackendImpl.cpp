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
#include "IDBIndexBackendImpl.h"

#if ENABLE(INDEXED_DATABASE)

#include "IDBDatabaseBackendImpl.h"
#include "IDBObjectStoreBackendImpl.h"
#include "SQLiteStatement.h"

namespace WebCore {

IDBIndexBackendImpl::IDBIndexBackendImpl(IDBObjectStoreBackendImpl* objectStore, int64_t id, const String& name, const String& keyPath, bool unique)
    : m_objectStore(objectStore)
    , m_id(id)
    , m_name(name)
    , m_keyPath(keyPath)
    , m_unique(unique)
{
}

IDBIndexBackendImpl::~IDBIndexBackendImpl()
{
}

static String whereClause(IDBKey* key)
{
    return "WHERE indexId = ?  AND  " + key->whereSyntax();
}

static void bindWhereClause(SQLiteStatement& query, int64_t id, IDBKey* key)
{
    query.bindInt64(1, id);
    key->bind(query, 2);
}

bool IDBIndexBackendImpl::addingKeyAllowed(IDBKey* key)
{
    if (!m_unique)
        return true;

    SQLiteStatement query(sqliteDatabase(), "SELECT id FROM IndexData " + whereClause(key));
    bool ok = query.prepare() == SQLResultOk;
    ASSERT_UNUSED(ok, ok); // FIXME: Better error handling?
    bindWhereClause(query, m_id, key);
    bool existingValue = query.step() == SQLResultRow;

    return !existingValue;
}

SQLiteDatabase& IDBIndexBackendImpl::sqliteDatabase() const
{
    return m_objectStore->database()->sqliteDatabase();
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)

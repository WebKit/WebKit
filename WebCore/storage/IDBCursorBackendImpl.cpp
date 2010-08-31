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
#include "IDBCursorBackendImpl.h"

#if ENABLE(INDEXED_DATABASE)

#include "IDBCallbacks.h"
#include "IDBDatabaseBackendImpl.h"
#include "IDBDatabaseError.h"
#include "IDBDatabaseException.h"
#include "IDBKeyRange.h"
#include "IDBObjectStoreBackendImpl.h"
#include "IDBRequest.h"
#include "SQLiteDatabase.h"
#include "SQLiteStatement.h"
#include "SerializedScriptValue.h"

namespace WebCore {

IDBCursorBackendImpl::IDBCursorBackendImpl(PassRefPtr<IDBObjectStoreBackendImpl> idbObjectStore, PassRefPtr<IDBKeyRange> keyRange, IDBCursor::Direction direction, PassOwnPtr<SQLiteStatement> query)
    : m_idbObjectStore(idbObjectStore)
    , m_keyRange(keyRange)
    , m_direction(direction)
    , m_query(query)
{
    loadCurrentRow();
}

IDBCursorBackendImpl::~IDBCursorBackendImpl()
{
}

unsigned short IDBCursorBackendImpl::direction() const
{
    return m_direction;
}

PassRefPtr<IDBKey> IDBCursorBackendImpl::key() const
{
    
    return m_currentKey;
}

PassRefPtr<SerializedScriptValue> IDBCursorBackendImpl::value() const
{
    return m_currentValue;
}

void IDBCursorBackendImpl::update(PassRefPtr<SerializedScriptValue> prpValue, PassRefPtr<IDBCallbacks> callbacks)
{
    RefPtr<SerializedScriptValue> value = prpValue;

    if (!m_query || m_currentId == InvalidId) {
        // FIXME: Use the proper error code when it's specced.
        callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::UNKNOWN_ERR, "Operation not possible."));
        return;
    }

    String sql = "UPDATE ObjectStoreData SET value = ? WHERE id = ?";
    SQLiteStatement updateQuery(m_idbObjectStore->database()->sqliteDatabase(), sql);
    
    bool ok = updateQuery.prepare() == SQLResultOk;
    ASSERT_UNUSED(ok, ok); // FIXME: Better error handling.
    updateQuery.bindText(1, value->toWireString());
    updateQuery.bindInt64(2, m_currentId);
    ok = updateQuery.step() == SQLResultDone;
    ASSERT_UNUSED(ok, ok); // FIXME: Better error handling.

    m_currentValue = value.release();
    callbacks->onSuccess();
}

void IDBCursorBackendImpl::continueFunction(PassRefPtr<IDBKey> prpKey, PassRefPtr<IDBCallbacks> callbacks)
{
    RefPtr<IDBKey> key = prpKey;
    while (true) {
        if (!m_query || m_query->step() != SQLResultRow) {
            m_query = 0;
            m_currentId = InvalidId;
            m_currentKey = 0;
            m_currentValue = 0;
            callbacks->onSuccess();
            return;
        }

        RefPtr<IDBKey> oldKey = m_currentKey;
        loadCurrentRow();

        // If a key was supplied, we must loop until we find that key (or hit the end).
        if (key && !key->isEqual(m_currentKey.get()))
            continue;

        // If we don't have a uniqueness constraint, we can stop now.
        if (m_direction == IDBCursor::NEXT || m_direction == IDBCursor::PREV)
            break;
        if (!m_currentKey->isEqual(oldKey.get()))
            break;
    }

    callbacks->onSuccess(this);
}

void IDBCursorBackendImpl::remove(PassRefPtr<IDBCallbacks> callbacks)
{
    if (!m_query || m_currentId == InvalidId) {
        // FIXME: Use the proper error code when it's specced.
        callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::UNKNOWN_ERR, "Operation not possible."));
        return;
    }

    String sql = "DELETE FROM ObjectStoreData WHERE id = ?";
    SQLiteStatement deleteQuery(m_idbObjectStore->database()->sqliteDatabase(), sql);
    
    bool ok = deleteQuery.prepare() == SQLResultOk;
    ASSERT_UNUSED(ok, ok); // FIXME: Better error handling.
    deleteQuery.bindInt64(1, m_currentId);
    ok = deleteQuery.step() == SQLResultDone;
    ASSERT_UNUSED(ok, ok); // FIXME: Better error handling.

    m_currentId = InvalidId;
    m_currentKey = 0;
    m_currentValue = 0;
    callbacks->onSuccess();
}

void IDBCursorBackendImpl::loadCurrentRow()
{
    // The column numbers depend on the query in IDBObjectStoreBackendImpl::openCursor.
    m_currentId = m_query->getColumnInt64(0);
    if (!m_query->isColumnNull(1))
        m_currentKey = IDBKey::create(m_query->getColumnText(1));
    else if (!m_query->isColumnNull(2)) {
        ASSERT_NOT_REACHED(); // FIXME: Implement date.
        m_currentKey = IDBKey::create();
    } else if (!m_query->isColumnNull(3))
        m_currentKey = IDBKey::create(m_query->getColumnInt(3));
    else
        m_currentKey = IDBKey::create();
    m_currentValue = SerializedScriptValue::createFromWire(m_query->getColumnText(4));
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)

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


#ifndef IDBCursorBackend_h
#define IDBCursorBackend_h

#if ENABLE(INDEXED_DATABASE)

#include "IDBBackingStoreCursorInterface.h"
#include "IDBDatabaseBackend.h"
#include "IDBTransactionBackend.h"
#include "SharedBuffer.h"
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class IDBKeyRange;

class IDBCursorBackend : public RefCounted<IDBCursorBackend> {
public:
    static PassRefPtr<IDBCursorBackend> create(PassRefPtr<IDBBackingStoreCursorInterface> cursor, IndexedDB::CursorType cursorType, IDBTransactionBackend* transaction, int64_t objectStoreId)
    {
        return adoptRef(new IDBCursorBackend(cursor, cursorType, IDBDatabaseBackend::NormalTask, transaction, objectStoreId));
    }
    static PassRefPtr<IDBCursorBackend> create(PassRefPtr<IDBBackingStoreCursorInterface> cursor, IndexedDB::CursorType cursorType, IDBDatabaseBackend::TaskType taskType, IDBTransactionBackend* transaction, int64_t objectStoreId)
    {
        return adoptRef(new IDBCursorBackend(cursor, cursorType, taskType, transaction, objectStoreId));
    }
    ~IDBCursorBackend();

    void advance(unsigned long, PassRefPtr<IDBCallbacks>, ExceptionCode&);
    void continueFunction(PassRefPtr<IDBKey>, PassRefPtr<IDBCallbacks>, ExceptionCode&);
    void deleteFunction(PassRefPtr<IDBCallbacks>, ExceptionCode&);
    void prefetchContinue(int numberToFetch, PassRefPtr<IDBCallbacks>, ExceptionCode&);
    void prefetchReset(int usedPrefetches, int unusedPrefetches);
    void postSuccessHandlerCallback() { }

    IDBKey* key() const { return m_cursor->key().get(); }
    IDBKey* primaryKey() const { return m_cursor->primaryKey().get(); }
    SharedBuffer* value() const { return (m_cursorType == IndexedDB::CursorKeyOnly) ? 0 : m_cursor->value().get(); }

    void close();

    IndexedDB::CursorType cursorType() const { return m_cursorType; }

    IDBBackingStoreCursorInterface* deprecatedBackingStoreCursor() { return m_cursor.get(); }
    void deprecatedSetBackingStoreCursor(PassRefPtr<IDBBackingStoreCursorInterface> cursor) { m_cursor = cursor; }
    void deprecatedSetSavedBackingStoreCursor(PassRefPtr<IDBBackingStoreCursorInterface> cursor) { m_savedCursor = cursor; }

private:
    IDBCursorBackend(PassRefPtr<IDBBackingStoreCursorInterface>, IndexedDB::CursorType, IDBDatabaseBackend::TaskType, IDBTransactionBackend*, int64_t objectStoreId);

    IDBDatabaseBackend::TaskType m_taskType;
    IndexedDB::CursorType m_cursorType;
    const RefPtr<IDBDatabaseBackend> m_database;
    RefPtr<IDBTransactionBackend> m_transaction;
    const int64_t m_objectStoreId;

    RefPtr<IDBBackingStoreCursorInterface> m_cursor; // Must be destroyed before m_transaction.
    RefPtr<IDBBackingStoreCursorInterface> m_savedCursor; // Must be destroyed before m_transaction.

    bool m_closed;
};

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)

#endif // IDBCursorBackend_h

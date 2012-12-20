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


#ifndef IDBCursorBackendImpl_h
#define IDBCursorBackendImpl_h

#if ENABLE(INDEXED_DATABASE)

#include "IDBBackingStore.h"
#include "IDBCursor.h"
#include "IDBCursorBackendInterface.h"
#include "IDBTransactionBackendImpl.h"
#include "SerializedScriptValue.h"
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class IDBDatabaseBackendImpl;
class IDBIndexBackendImpl;
class IDBKeyRange;
class IDBObjectStoreBackendImpl;

class IDBCursorBackendImpl : public IDBCursorBackendInterface {
public:
    static PassRefPtr<IDBCursorBackendImpl> create(PassRefPtr<IDBBackingStore::Cursor> cursor, CursorType cursorType, IDBTransactionBackendImpl* transaction, IDBObjectStoreBackendImpl* objectStore)
    {
        return adoptRef(new IDBCursorBackendImpl(cursor, cursorType, IDBTransactionBackendInterface::NormalTask, transaction, objectStore));
    }
    static PassRefPtr<IDBCursorBackendImpl> create(PassRefPtr<IDBBackingStore::Cursor> cursor, CursorType cursorType, IDBTransactionBackendInterface::TaskType taskType, IDBTransactionBackendImpl* transaction, IDBObjectStoreBackendImpl* objectStore)
    {
        return adoptRef(new IDBCursorBackendImpl(cursor, cursorType, taskType, transaction, objectStore));
    }
    virtual ~IDBCursorBackendImpl();

    // IDBCursorBackendInterface
    virtual void advance(unsigned long, PassRefPtr<IDBCallbacks>, ExceptionCode&);
    virtual void continueFunction(PassRefPtr<IDBKey>, PassRefPtr<IDBCallbacks>, ExceptionCode&);
    virtual void deleteFunction(PassRefPtr<IDBCallbacks>, ExceptionCode&);
    virtual void prefetchContinue(int numberToFetch, PassRefPtr<IDBCallbacks>, ExceptionCode&);
    virtual void prefetchReset(int usedPrefetches, int unusedPrefetches);
    virtual void postSuccessHandlerCallback() { }

    PassRefPtr<IDBKey> key() const { return m_cursor->key(); }
    PassRefPtr<IDBKey> primaryKey() const { return m_cursor->primaryKey(); }
    PassRefPtr<SerializedScriptValue> value() const { return (m_cursorType == KeyOnly) ? 0 : SerializedScriptValue::createFromWireBytes(m_cursor->value()); }
    void close();

private:
    IDBCursorBackendImpl(PassRefPtr<IDBBackingStore::Cursor>, CursorType, IDBTransactionBackendInterface::TaskType, IDBTransactionBackendImpl*, IDBObjectStoreBackendImpl*);

    class CursorIterationOperation;
    class CursorAdvanceOperation;
    class CursorPrefetchIterationOperation;

    RefPtr<IDBBackingStore::Cursor> m_cursor;
    RefPtr<IDBBackingStore::Cursor> m_savedCursor;
    IDBTransactionBackendInterface::TaskType m_taskType;
    CursorType m_cursorType;
    RefPtr<IDBTransactionBackendImpl> m_transaction;
    RefPtr<IDBObjectStoreBackendImpl> m_objectStore;

    bool m_closed;
};

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)

#endif // IDBCursorBackendImpl_h

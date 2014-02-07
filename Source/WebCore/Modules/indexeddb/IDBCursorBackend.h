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
    static PassRefPtr<IDBCursorBackend> create(int64_t cursorID, IndexedDB::CursorType cursorType, IDBTransactionBackend& transaction, int64_t objectStoreID)
    {
        return adoptRef(new IDBCursorBackend(cursorID, cursorType, IDBDatabaseBackend::NormalTask, transaction, objectStoreID));
    }
    static PassRefPtr<IDBCursorBackend> create(int64_t cursorID, IndexedDB::CursorType cursorType, IDBDatabaseBackend::TaskType taskType, IDBTransactionBackend& transaction, int64_t objectStoreID)
    {
        return adoptRef(new IDBCursorBackend(cursorID, cursorType, taskType, transaction, objectStoreID));
    }
    ~IDBCursorBackend();

    void advance(unsigned long, PassRefPtr<IDBCallbacks>, ExceptionCode&);
    void continueFunction(PassRefPtr<IDBKey>, PassRefPtr<IDBCallbacks>, ExceptionCode&);
    void deleteFunction(PassRefPtr<IDBCallbacks>, ExceptionCode&);
    void postSuccessHandlerCallback() { }

    IDBKey* key() const { return m_currentKey.get(); }
    IDBKey* primaryKey() const { return m_currentPrimaryKey.get(); }
    SharedBuffer* valueBuffer() const { return (m_cursorType == IndexedDB::CursorType::KeyOnly) ? nullptr : m_currentValueBuffer.get(); }
    void updateCursorData(IDBKey*, IDBKey* primaryKey, SharedBuffer* valueBuffer);

    void close();

    IndexedDB::CursorType cursorType() const { return m_cursorType; }

    int64_t id() const { return m_cursorID; }

    IDBTransactionBackend& transaction() { return *m_transaction; }

    void clear();
    void setSavedCursorID(int64_t cursorID) { m_savedCursorID = cursorID; }

private:
    IDBCursorBackend(int64_t cursorID, IndexedDB::CursorType, IDBDatabaseBackend::TaskType, IDBTransactionBackend&, int64_t objectStoreID);

    IDBDatabaseBackend::TaskType m_taskType;
    IndexedDB::CursorType m_cursorType;
    RefPtr<IDBTransactionBackend> m_transaction;
    const int64_t m_objectStoreID;

    int64_t m_cursorID;
    int64_t m_savedCursorID;

    RefPtr<IDBKey> m_currentKey;
    RefPtr<IDBKey> m_currentPrimaryKey;
    RefPtr<SharedBuffer> m_currentValueBuffer;

    bool m_closed;
};

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)

#endif // IDBCursorBackend_h

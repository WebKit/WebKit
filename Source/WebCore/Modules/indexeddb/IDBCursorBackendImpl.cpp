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

#include "CrossThreadTask.h"
#include "IDBBackingStore.h"
#include "IDBCallbacks.h"
#include "IDBDatabaseError.h"
#include "IDBDatabaseException.h"
#include "IDBKeyRange.h"
#include "IDBObjectStoreBackendImpl.h"
#include "IDBRequest.h"
#include "IDBTracing.h"
#include "IDBTransactionBackendImpl.h"
#include "SerializedScriptValue.h"

namespace WebCore {

IDBCursorBackendImpl::IDBCursorBackendImpl(PassRefPtr<IDBBackingStore::Cursor> cursor, CursorType cursorType, IDBTransactionBackendInterface::TaskType taskType, IDBTransactionBackendImpl* transaction, IDBObjectStoreBackendImpl* objectStore)
    : m_cursor(cursor)
    , m_taskType(taskType)
    , m_cursorType(cursorType)
    , m_transaction(transaction)
    , m_objectStore(objectStore)
    , m_closed(false)
{
    m_transaction->registerOpenCursor(this);
}

IDBCursorBackendImpl::~IDBCursorBackendImpl()
{
    m_transaction->unregisterOpenCursor(this);
    // Order is important, the cursors have to be destructed before the objectStore.
    m_cursor.clear();
    m_savedCursor.clear();

    m_objectStore.clear();
}

void IDBCursorBackendImpl::continueFunction(PassRefPtr<IDBKey> prpKey, PassRefPtr<IDBCallbacks> prpCallbacks, ExceptionCode& ec)
{
    IDB_TRACE("IDBCursorBackendImpl::continue");

    if (!m_transaction->scheduleTask(m_taskType, createCallbackTask(&IDBCursorBackendImpl::continueFunctionInternal, this, prpKey, prpCallbacks)))
        ec = IDBDatabaseException::TRANSACTION_INACTIVE_ERR;
}

void IDBCursorBackendImpl::advance(unsigned long count, PassRefPtr<IDBCallbacks> prpCallbacks, ExceptionCode& ec)
{
    IDB_TRACE("IDBCursorBackendImpl::advance");

    if (!m_transaction->scheduleTask(createCallbackTask(&IDBCursorBackendImpl::advanceInternal, this, count, prpCallbacks)))
        ec = IDBDatabaseException::TRANSACTION_INACTIVE_ERR;
}

void IDBCursorBackendImpl::advanceInternal(ScriptExecutionContext*, PassRefPtr<IDBCursorBackendImpl> prpCursor, unsigned long count, PassRefPtr<IDBCallbacks> callbacks)
{
    IDB_TRACE("IDBCursorBackendImpl::advanceInternal");
    RefPtr<IDBCursorBackendImpl> cursor = prpCursor;
    if (!cursor->m_cursor || !cursor->m_cursor->advance(count)) {
        cursor->m_cursor = 0;
        callbacks->onSuccess(SerializedScriptValue::nullValue());
        return;
    }

    callbacks->onSuccess(cursor->key(), cursor->primaryKey(), cursor->value());
}

void IDBCursorBackendImpl::continueFunctionInternal(ScriptExecutionContext*, PassRefPtr<IDBCursorBackendImpl> prpCursor, PassRefPtr<IDBKey> prpKey, PassRefPtr<IDBCallbacks> callbacks)
{
    IDB_TRACE("IDBCursorBackendImpl::continueInternal");
    RefPtr<IDBCursorBackendImpl> cursor = prpCursor;
    RefPtr<IDBKey> key = prpKey;

    if (!cursor->m_cursor || !cursor->m_cursor->continueFunction(key.get())) {
        cursor->m_cursor = 0;
        callbacks->onSuccess(SerializedScriptValue::nullValue());
        return;
    }

    callbacks->onSuccess(cursor->key(), cursor->primaryKey(), cursor->value());
}

void IDBCursorBackendImpl::deleteFunction(PassRefPtr<IDBCallbacks> prpCallbacks, ExceptionCode& ec)
{
    IDB_TRACE("IDBCursorBackendImpl::delete");
    ASSERT(m_transaction->mode() != IDBTransaction::READ_ONLY);

    if (!m_cursor || m_cursorType == IndexKeyCursor) {
        ec = IDBDatabaseException::IDB_INVALID_STATE_ERR;
        return;
    }

    RefPtr<IDBKeyRange> keyRange = IDBKeyRange::only(m_cursor->primaryKey(), ec);
    ASSERT(!ec);

    m_objectStore->deleteFunction(keyRange.release(), prpCallbacks, m_transaction.get(), ec);
}

void IDBCursorBackendImpl::prefetchContinue(int numberToFetch, PassRefPtr<IDBCallbacks> prpCallbacks, ExceptionCode& ec)
{
    IDB_TRACE("IDBCursorBackendImpl::prefetchContinue");
    if (!m_transaction->scheduleTask(m_taskType, createCallbackTask(&IDBCursorBackendImpl::prefetchContinueInternal, this, numberToFetch, prpCallbacks)))
        ec = IDBDatabaseException::TRANSACTION_INACTIVE_ERR;
}

void IDBCursorBackendImpl::prefetchContinueInternal(ScriptExecutionContext*, PassRefPtr<IDBCursorBackendImpl> prpCursor, int numberToFetch, PassRefPtr<IDBCallbacks> callbacks)
{
    IDB_TRACE("IDBCursorBackendImpl::prefetchContinueInternal");
    RefPtr<IDBCursorBackendImpl> cursor = prpCursor;

    Vector<RefPtr<IDBKey> > foundKeys;
    Vector<RefPtr<IDBKey> > foundPrimaryKeys;
    Vector<RefPtr<SerializedScriptValue> > foundValues;

    if (cursor->m_cursor)
        cursor->m_savedCursor = cursor->m_cursor->clone();

    const size_t maxSizeEstimate = 10 * 1024 * 1024;
    size_t sizeEstimate = 0;

    for (int i = 0; i < numberToFetch; ++i) {
        if (!cursor->m_cursor || !cursor->m_cursor->continueFunction(0)) {
            cursor->m_cursor = 0;
            break;
        }

        foundKeys.append(cursor->m_cursor->key());
        foundPrimaryKeys.append(cursor->m_cursor->primaryKey());

        if (cursor->m_cursorType != IDBCursorBackendInterface::IndexKeyCursor)
            foundValues.append(SerializedScriptValue::createFromWire(cursor->m_cursor->value()));
        else
            foundValues.append(SerializedScriptValue::create());

        sizeEstimate += cursor->m_cursor->key()->sizeEstimate();
        sizeEstimate += cursor->m_cursor->primaryKey()->sizeEstimate();
        if (cursor->m_cursorType != IDBCursorBackendInterface::IndexKeyCursor)
            sizeEstimate += cursor->m_cursor->value().length() * sizeof(UChar);

        if (sizeEstimate > maxSizeEstimate)
            break;
    }

    if (!foundKeys.size()) {
        callbacks->onSuccess(SerializedScriptValue::nullValue());
        return;
    }

    cursor->m_transaction->addPendingEvents(foundKeys.size() - 1);
    callbacks->onSuccessWithPrefetch(foundKeys, foundPrimaryKeys, foundValues);
}

void IDBCursorBackendImpl::prefetchReset(int usedPrefetches, int unusedPrefetches)
{
    IDB_TRACE("IDBCursorBackendImpl::prefetchReset");
    m_transaction->addPendingEvents(-unusedPrefetches);
    m_cursor = m_savedCursor;
    m_savedCursor = 0;

    if (m_closed)
        return;
    if (m_cursor) {
        for (int i = 0; i < usedPrefetches; ++i) {
            bool ok = m_cursor->continueFunction();
            ASSERT_UNUSED(ok, ok);
        }
    }
}

void IDBCursorBackendImpl::close()
{
    IDB_TRACE("IDBCursorBackendImpl::close");
    m_closed = true;
    if (m_cursor)
        m_cursor->close();
    m_cursor.clear();
    m_savedCursor.clear();
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)

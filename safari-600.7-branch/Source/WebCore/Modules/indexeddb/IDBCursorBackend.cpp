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
#include "IDBCursorBackend.h"

#if ENABLE(INDEXED_DATABASE)

#include "IDBCallbacks.h"
#include "IDBCursorBackendOperations.h"
#include "IDBDatabaseBackend.h"
#include "IDBDatabaseCallbacks.h"
#include "IDBDatabaseError.h"
#include "IDBDatabaseException.h"
#include "IDBKeyRange.h"
#include "IDBOperation.h"
#include "IDBServerConnection.h"
#include "Logging.h"
#include "SharedBuffer.h"

namespace WebCore {

IDBCursorBackend::IDBCursorBackend(int64_t cursorID, IndexedDB::CursorType cursorType, IDBDatabaseBackend::TaskType taskType, IDBTransactionBackend& transaction, int64_t objectStoreID)
    : m_taskType(taskType)
    , m_cursorType(cursorType)
    , m_transaction(&transaction)
    , m_objectStoreID(objectStoreID)
    , m_cursorID(cursorID)
    , m_savedCursorID(0)
    , m_closed(false)
{
    m_transaction->registerOpenCursor(this);
}

IDBCursorBackend::~IDBCursorBackend()
{
    m_transaction->unregisterOpenCursor(this);
}

void IDBCursorBackend::continueFunction(PassRefPtr<IDBKey> key, PassRefPtr<IDBCallbacks> prpCallbacks, ExceptionCode&)
{
    LOG(StorageAPI, "IDBCursorBackend::continue");
    RefPtr<IDBCallbacks> callbacks = prpCallbacks;
    m_transaction->scheduleTask(m_taskType, CursorIterationOperation::create(this, key, callbacks));
}

void IDBCursorBackend::advance(unsigned long count, PassRefPtr<IDBCallbacks> prpCallbacks, ExceptionCode&)
{
    LOG(StorageAPI, "IDBCursorBackend::advance");
    RefPtr<IDBCallbacks> callbacks = prpCallbacks;
    m_transaction->scheduleTask(CursorAdvanceOperation::create(this, count, callbacks));
}

void IDBCursorBackend::deleteFunction(PassRefPtr<IDBCallbacks> prpCallbacks, ExceptionCode&)
{
    LOG(StorageAPI, "IDBCursorBackend::delete");
    ASSERT(m_transaction->mode() != IndexedDB::TransactionMode::ReadOnly);
    RefPtr<IDBKeyRange> keyRange = IDBKeyRange::create(primaryKey());
    m_transaction->database().deleteRange(m_transaction->id(), m_objectStoreID, keyRange.release(), prpCallbacks);
}

void IDBCursorBackend::close()
{
    LOG(StorageAPI, "IDBCursorBackend::close");
    clear();
    m_closed = true;
    m_savedCursorID = 0;
}

void IDBCursorBackend::updateCursorData(IDBKey* key, IDBKey* primaryKey, SharedBuffer* valueBuffer)
{
    m_currentKey = key;
    m_currentPrimaryKey = primaryKey;
    m_currentValueBuffer = valueBuffer;
}

void IDBCursorBackend::clear()
{
    m_cursorID = 0;
    m_currentKey = nullptr;
    m_currentPrimaryKey = nullptr;
    m_currentValueBuffer = nullptr;
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)

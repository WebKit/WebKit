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
#include "IDBObjectStore.h"

#if ENABLE(INDEXED_DATABASE)

#include "DOMStringList.h"
#include "IDBAny.h"
#include "IDBDatabaseException.h"
#include "IDBIndex.h"
#include "IDBKey.h"
#include "IDBKeyRange.h"
#include "IDBTransactionBackendInterface.h"
#include "SerializedScriptValue.h"
#include <wtf/UnusedParam.h>

namespace WebCore {

static const unsigned short defaultDirection = IDBCursor::NEXT;

IDBObjectStore::IDBObjectStore(PassRefPtr<IDBObjectStoreBackendInterface> idbObjectStore, IDBTransactionBackendInterface* transaction)
    : m_objectStore(idbObjectStore)
    , m_transaction(transaction)
{
    ASSERT(m_objectStore);
    ASSERT(m_transaction);
    // We pass a reference to this object before it can be adopted.
    relaxAdoptionRequirement();
}

String IDBObjectStore::name() const
{
    return m_objectStore->name();
}

String IDBObjectStore::keyPath() const
{
    return m_objectStore->keyPath();
}

PassRefPtr<DOMStringList> IDBObjectStore::indexNames() const
{
    return m_objectStore->indexNames();
}

PassRefPtr<IDBRequest> IDBObjectStore::get(ScriptExecutionContext* context, PassRefPtr<IDBKey> key, ExceptionCode& ec)
{
    RefPtr<IDBRequest> request = IDBRequest::create(context, IDBAny::create(this), m_transaction.get());
    m_objectStore->get(key, request, m_transaction.get(), ec);
    if (ec)
        return 0;
    return request.release();
}

PassRefPtr<IDBRequest> IDBObjectStore::add(ScriptExecutionContext* context, PassRefPtr<SerializedScriptValue> value, PassRefPtr<IDBKey> key, ExceptionCode& ec)
{
    RefPtr<IDBRequest> request = IDBRequest::create(context, IDBAny::create(this), m_transaction.get());
    m_objectStore->put(value, key, true, request, m_transaction.get(), ec);
    if (ec)
        return 0;
    return request;
}

PassRefPtr<IDBRequest> IDBObjectStore::put(ScriptExecutionContext* context, PassRefPtr<SerializedScriptValue> value, PassRefPtr<IDBKey> key, ExceptionCode& ec)
{
    RefPtr<IDBRequest> request = IDBRequest::create(context, IDBAny::create(this), m_transaction.get());
    m_objectStore->put(value, key, false, request, m_transaction.get(), ec);
    if (ec)
        return 0;
    return request;
}

PassRefPtr<IDBRequest> IDBObjectStore::deleteFunction(ScriptExecutionContext* context, PassRefPtr<IDBKey> key, ExceptionCode& ec)
{
    RefPtr<IDBRequest> request = IDBRequest::create(context, IDBAny::create(this), m_transaction.get());
    m_objectStore->deleteFunction(key, request, m_transaction.get(), ec);
    if (ec)
        return 0;
    return request;
}

PassRefPtr<IDBIndex> IDBObjectStore::createIndex(const String& name, const String& keyPath, const OptionsObject& options, ExceptionCode& ec)
{
    bool unique = false;
    options.getKeyBool("unique", unique);

    RefPtr<IDBIndexBackendInterface> index = m_objectStore->createIndex(name, keyPath, unique, m_transaction.get(), ec);
    ASSERT(!index != !ec); // If we didn't get an index, we should have gotten an exception code. And vice versa.
    if (!index)
        return 0;
    return IDBIndex::create(index.release(), m_transaction.get());
}

PassRefPtr<IDBIndex> IDBObjectStore::index(const String& name, ExceptionCode& ec)
{
    RefPtr<IDBIndexBackendInterface> index = m_objectStore->index(name, ec);
    ASSERT(!index != !ec); // If we didn't get an index, we should have gotten an exception code. And vice versa.
    if (!index)
        return 0;
    return IDBIndex::create(index.release(), m_transaction.get());
}

void IDBObjectStore::deleteIndex(const String& name, ExceptionCode& ec)
{
    m_objectStore->deleteIndex(name, m_transaction.get(), ec);
}

PassRefPtr<IDBRequest> IDBObjectStore::openCursor(ScriptExecutionContext* context, const OptionsObject& options, ExceptionCode& ec)
{
    RefPtr<IDBKeyRange> range = options.getKeyKeyRange("range");

    // Converted to an unsigned short.
    int32_t direction = defaultDirection;
    options.getKeyInt32("direction", direction);
    if (direction != IDBCursor::NEXT && direction != IDBCursor::NEXT_NO_DUPLICATE && direction != IDBCursor::PREV && direction != IDBCursor::PREV_NO_DUPLICATE) {
        // FIXME: May need to change when specced: http://www.w3.org/Bugs/Public/show_bug.cgi?id=11406
        ec = IDBDatabaseException::CONSTRAINT_ERR;
        return 0;
    }

    RefPtr<IDBRequest> request = IDBRequest::create(context, IDBAny::create(this), m_transaction.get());
    m_objectStore->openCursor(range, direction, request, m_transaction.get(), ec);
    if (ec)
        return 0;
    return request.release();
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)

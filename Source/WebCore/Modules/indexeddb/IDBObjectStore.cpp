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
#include "IDBKeyPath.h"
#include "IDBKeyRange.h"
#include "IDBTracing.h"
#include "IDBTransaction.h"
#include "SerializedScriptValue.h"
#include <wtf/UnusedParam.h>

namespace WebCore {

static const unsigned short defaultDirection = IDBCursor::NEXT;

IDBObjectStore::IDBObjectStore(PassRefPtr<IDBObjectStoreBackendInterface> idbObjectStore, IDBTransaction* transaction)
    : m_backend(idbObjectStore)
    , m_transaction(transaction)
{
    ASSERT(m_backend);
    ASSERT(m_transaction);
    // We pass a reference to this object before it can be adopted.
    relaxAdoptionRequirement();
}

String IDBObjectStore::name() const
{
    IDB_TRACE("IDBObjectStore::name");
    return m_backend->name();
}

String IDBObjectStore::keyPath() const
{
    IDB_TRACE("IDBObjectStore::keyPath");
    return m_backend->keyPath();
}

PassRefPtr<DOMStringList> IDBObjectStore::indexNames() const
{
    IDB_TRACE("IDBObjectStore::indexNames");
    return m_backend->indexNames();
}

IDBTransaction* IDBObjectStore::transaction() const
{
    IDB_TRACE("IDBObjectStore::transaction");
    return m_transaction.get();
}

PassRefPtr<IDBRequest> IDBObjectStore::get(ScriptExecutionContext* context, PassRefPtr<IDBKeyRange> keyRange, ExceptionCode& ec)
{
    IDB_TRACE("IDBObjectStore::get");

    if (!keyRange) {
        ec = IDBDatabaseException::DATA_ERR;
        return 0;
    }
    RefPtr<IDBRequest> request = IDBRequest::create(context, IDBAny::create(this), m_transaction.get());
    m_backend->get(keyRange, request, m_transaction->backend(), ec);
    if (ec) {
        request->markEarlyDeath();
        return 0;
    }
    return request.release();
}

PassRefPtr<IDBRequest> IDBObjectStore::get(ScriptExecutionContext* context, PassRefPtr<IDBKey> key, ExceptionCode& ec)
{
    IDB_TRACE("IDBObjectStore::get");
    if (!key || (key->type() == IDBKey::InvalidType)) {
        ec = IDBDatabaseException::DATA_ERR;
        return 0;
    }

    RefPtr<IDBKeyRange> keyRange = IDBKeyRange::only(key, ec);
    if (ec)
        return 0;
    return get(context, keyRange.release(), ec);
}

PassRefPtr<IDBRequest> IDBObjectStore::add(ScriptExecutionContext* context, PassRefPtr<SerializedScriptValue> prpValue, PassRefPtr<IDBKey> key, ExceptionCode& ec)
{
    IDB_TRACE("IDBObjectStore::add");
    if (key && (key->type() == IDBKey::InvalidType)) {
        ec = IDBDatabaseException::DATA_ERR;
        return 0;
    }

    RefPtr<SerializedScriptValue> value = prpValue;
    if (value->blobURLs().size() > 0) {
        // FIXME: Add Blob/File/FileList support
        ec = DATA_CLONE_ERR;
        return 0;
    }

    RefPtr<IDBRequest> request = IDBRequest::create(context, IDBAny::create(this), m_transaction.get());
    m_backend->put(value, key, IDBObjectStoreBackendInterface::AddOnly, request, m_transaction->backend(), ec);
    if (ec) {
        request->markEarlyDeath();
        return 0;
    }
    return request.release();
}

PassRefPtr<IDBRequest> IDBObjectStore::put(ScriptExecutionContext* context, PassRefPtr<SerializedScriptValue> prpValue, PassRefPtr<IDBKey> key, ExceptionCode& ec)
{
    IDB_TRACE("IDBObjectStore::put");
    if (key && (key->type() == IDBKey::InvalidType)) {
        ec = IDBDatabaseException::DATA_ERR;
        return 0;
    }

    RefPtr<SerializedScriptValue> value = prpValue;
    if (value->blobURLs().size() > 0) {
        // FIXME: Add Blob/File/FileList support
        ec = DATA_CLONE_ERR;
        return 0;
    }

    RefPtr<IDBRequest> request = IDBRequest::create(context, IDBAny::create(this), m_transaction.get());
    m_backend->put(value, key, IDBObjectStoreBackendInterface::AddOrUpdate, request, m_transaction->backend(), ec);
    if (ec) {
        request->markEarlyDeath();
        return 0;
    }
    return request.release();
}

PassRefPtr<IDBRequest> IDBObjectStore::deleteFunction(ScriptExecutionContext* context, PassRefPtr<IDBKeyRange> keyRange, ExceptionCode& ec)
{
    IDB_TRACE("IDBObjectStore::delete");
    if (!keyRange) {
        ec = IDBDatabaseException::DATA_ERR;
        return 0;
    }

    RefPtr<IDBRequest> request = IDBRequest::create(context, IDBAny::create(this), m_transaction.get());
    m_backend->deleteFunction(keyRange, request, m_transaction->backend(), ec);
    if (ec) {
        request->markEarlyDeath();
        return 0;
    }
    return request.release();
}

PassRefPtr<IDBRequest> IDBObjectStore::deleteFunction(ScriptExecutionContext* context, PassRefPtr<IDBKey> key, ExceptionCode& ec)
{
    IDB_TRACE("IDBObjectStore::delete");
    if (!key || !key->valid()) {
        ec = IDBDatabaseException::DATA_ERR;
        return 0;
    }

    RefPtr<IDBRequest> request = IDBRequest::create(context, IDBAny::create(this), m_transaction.get());
    m_backend->deleteFunction(key, request, m_transaction->backend(), ec);
    if (ec) {
        request->markEarlyDeath();
        return 0;
    }
    return request.release();
}

PassRefPtr<IDBRequest> IDBObjectStore::clear(ScriptExecutionContext* context, ExceptionCode& ec)
{
    IDB_TRACE("IDBObjectStore::clear");
    RefPtr<IDBRequest> request = IDBRequest::create(context, IDBAny::create(this), m_transaction.get());
    m_backend->clear(request, m_transaction->backend(), ec);
    if (ec) {
        request->markEarlyDeath();
        return 0;
    }
    return request.release();
}

PassRefPtr<IDBIndex> IDBObjectStore::createIndex(const String& name, const String& keyPath, const Dictionary& options, ExceptionCode& ec)
{
    IDB_TRACE("IDBObjectStore::createIndex");
    if (!IDBIsValidKeyPath(keyPath)) {
        ec = IDBDatabaseException::NON_TRANSIENT_ERR;
        return 0;
    }

    bool unique = false;
    options.get("unique", unique);

    bool multiEntry = false;
    options.get("multiEntry", multiEntry);

    // FIXME: When Array-type keyPaths are supported, throw exception if keyPath is Array and multiEntry is true.

    RefPtr<IDBIndexBackendInterface> indexBackend = m_backend->createIndex(name, keyPath, unique, multiEntry, m_transaction->backend(), ec);
    ASSERT(!indexBackend != !ec); // If we didn't get an index, we should have gotten an exception code. And vice versa.
    if (!indexBackend)
        return 0;
    RefPtr<IDBIndex> index = IDBIndex::create(indexBackend.release(), this, m_transaction.get());
    m_indexMap.set(name, index);
    return index.release();
}

PassRefPtr<IDBIndex> IDBObjectStore::index(const String& name, ExceptionCode& ec)
{
    IDB_TRACE("IDBObjectStore::index");
    if (m_transaction->finished()) {
        ec = IDBDatabaseException::NOT_ALLOWED_ERR;
        return 0;
    }

    IDBIndexMap::iterator it = m_indexMap.find(name);
    if (it != m_indexMap.end())
        return it->second;

    RefPtr<IDBIndexBackendInterface> indexBackend = m_backend->index(name, ec);
    ASSERT(!indexBackend != !ec); // If we didn't get an index, we should have gotten an exception code. And vice versa.
    if (!indexBackend)
        return 0;

    RefPtr<IDBIndex> index = IDBIndex::create(indexBackend.release(), this, m_transaction.get());
    m_indexMap.set(name, index);
    return index.release();
}

void IDBObjectStore::deleteIndex(const String& name, ExceptionCode& ec)
{
    m_backend->deleteIndex(name, m_transaction->backend(), ec);
}

PassRefPtr<IDBRequest> IDBObjectStore::openCursor(ScriptExecutionContext* context, PassRefPtr<IDBKeyRange> range, unsigned short direction, ExceptionCode& ec)
{
    IDB_TRACE("IDBObjectStore::openCursor");
    if (direction != IDBCursor::NEXT && direction != IDBCursor::NEXT_NO_DUPLICATE && direction != IDBCursor::PREV && direction != IDBCursor::PREV_NO_DUPLICATE) {
        // FIXME: May need to change when specced: http://www.w3.org/Bugs/Public/show_bug.cgi?id=11406
        ec = IDBDatabaseException::CONSTRAINT_ERR;
        return 0;
    }

    RefPtr<IDBRequest> request = IDBRequest::create(context, IDBAny::create(this), m_transaction.get());
    request->setCursorType(IDBCursorBackendInterface::ObjectStoreCursor);
    m_backend->openCursor(range, direction, request, m_transaction->backend(), ec);
    if (ec) {
        request->markEarlyDeath();
        return 0;
    }
    return request.release();
}

PassRefPtr<IDBRequest> IDBObjectStore::openCursor(ScriptExecutionContext* context, PassRefPtr<IDBKey> key, unsigned short direction, ExceptionCode& ec)
{
    IDB_TRACE("IDBObjectStore::openCursor");
    RefPtr<IDBKeyRange> keyRange = IDBKeyRange::only(key, ec);
    if (ec)
        return 0;
    return openCursor(context, keyRange.release(), ec);
}

PassRefPtr<IDBRequest> IDBObjectStore::count(ScriptExecutionContext* context, PassRefPtr<IDBKeyRange> range, ExceptionCode& ec)
{
    IDB_TRACE("IDBObjectStore::count");
    RefPtr<IDBRequest> request = IDBRequest::create(context, IDBAny::create(this), m_transaction.get());
    m_backend->count(range, request, m_transaction->backend(), ec);
    if (ec) {
        request->markEarlyDeath();
        return 0;
    }
    return request.release();
}

PassRefPtr<IDBRequest> IDBObjectStore::count(ScriptExecutionContext* context, PassRefPtr<IDBKey> key, ExceptionCode& ec)
{
    IDB_TRACE("IDBObjectStore::count");
    RefPtr<IDBKeyRange> keyRange = IDBKeyRange::only(key, ec);
    if (ec)
        return 0;
    return count(context, keyRange.release(), ec);
}

void IDBObjectStore::transactionFinished()
{
    ASSERT(m_transaction->finished());

    // Break reference cycles.
    m_indexMap.clear();
}


} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)

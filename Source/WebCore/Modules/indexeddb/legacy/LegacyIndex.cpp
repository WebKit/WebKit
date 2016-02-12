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
#include "LegacyIndex.h"

#if ENABLE(INDEXED_DATABASE)

#include "IDBDatabaseException.h"
#include "IDBKey.h"
#include "IDBKeyRange.h"
#include "IDBObjectStore.h"
#include "IDBRequest.h"
#include "IDBTransaction.h"
#include "Logging.h"
#include "ScriptExecutionContext.h"

namespace WebCore {

LegacyIndex::LegacyIndex(const IDBIndexMetadata& metadata, LegacyObjectStore* objectStore, LegacyTransaction* transaction)
    : m_metadata(metadata)
    , m_objectStore(objectStore)
    , m_transaction(transaction)
    , m_deleted(false)
{
    ASSERT(m_objectStore);
    ASSERT(m_transaction);
    ASSERT(m_metadata.id != IDBIndexMetadata::InvalidId);
}

LegacyIndex::~LegacyIndex()
{
}

void LegacyIndex::ref()
{
    ++m_refCount;
}

void LegacyIndex::deref()
{
    if (--m_refCount)
        return;

    delete this;
}

RefPtr<IDBRequest> LegacyIndex::openCursor(ScriptExecutionContext* context, IDBKeyRange* keyRange, const String& directionString, ExceptionCodeWithMessage& ec)
{
    LOG(StorageAPI, "LegacyIndex::openCursor");
    if (m_deleted) {
        ec.code = IDBDatabaseException::InvalidStateError;
        return 0;
    }
    if (!m_transaction->isActive()) {
        ec.code = IDBDatabaseException::TransactionInactiveError;
        return 0;
    }
    IndexedDB::CursorDirection direction = IDBCursor::stringToDirection(directionString, ec.code);
    if (ec.code)
        return 0;

    RefPtr<LegacyRequest> request = LegacyRequest::create(context, LegacyAny::create(this), m_transaction.get());
    request->setCursorDetails(IndexedDB::CursorType::KeyAndValue, direction);
    backendDB()->openCursor(m_transaction->id(), m_objectStore->id(), m_metadata.id, keyRange, direction, false, IDBDatabaseBackend::NormalTask, request);
    return request;
}

RefPtr<IDBRequest> LegacyIndex::openCursor(ScriptExecutionContext* context, const Deprecated::ScriptValue& key, const String& direction, ExceptionCodeWithMessage& ec)
{
    LOG(StorageAPI, "LegacyIndex::openCursor");
    RefPtr<IDBKeyRange> keyRange = IDBKeyRange::only(context, key, ec.code);
    if (ec.code)
        return 0;
    return openCursor(context, keyRange.get(), direction, ec);
}

RefPtr<IDBRequest> LegacyIndex::count(ScriptExecutionContext* context, IDBKeyRange* keyRange, ExceptionCodeWithMessage& ec)
{
    LOG(StorageAPI, "LegacyIndex::count");
    if (m_deleted) {
        ec.code = IDBDatabaseException::InvalidStateError;
        return 0;
    }
    if (!m_transaction->isActive()) {
        ec.code = IDBDatabaseException::TransactionInactiveError;
        return 0;
    }
    RefPtr<LegacyRequest> request = LegacyRequest::create(context, LegacyAny::create(this), m_transaction.get());
    backendDB()->count(m_transaction->id(), m_objectStore->id(), m_metadata.id, keyRange, request);
    return request;
}

RefPtr<IDBRequest> LegacyIndex::count(ScriptExecutionContext* context, const Deprecated::ScriptValue& key, ExceptionCodeWithMessage& ec)
{
    LOG(StorageAPI, "LegacyIndex::count");
    RefPtr<IDBKeyRange> keyRange = IDBKeyRange::only(context, key, ec.code);
    if (ec.code)
        return 0;
    return count(context, keyRange.get(), ec);
}

RefPtr<IDBRequest> LegacyIndex::openKeyCursor(ScriptExecutionContext* context, IDBKeyRange* keyRange, const String& directionString, ExceptionCodeWithMessage& ec)
{
    LOG(StorageAPI, "LegacyIndex::openKeyCursor");
    if (m_deleted) {
        ec.code = IDBDatabaseException::InvalidStateError;
        return 0;
    }
    if (!m_transaction->isActive()) {
        ec.code = IDBDatabaseException::TransactionInactiveError;
        return 0;
    }
    IndexedDB::CursorDirection direction = IDBCursor::stringToDirection(directionString, ec.code);
    if (ec.code)
        return 0;

    RefPtr<LegacyRequest> request = LegacyRequest::create(context, LegacyAny::create(this), m_transaction.get());
    request->setCursorDetails(IndexedDB::CursorType::KeyOnly, direction);
    backendDB()->openCursor(m_transaction->id(), m_objectStore->id(), m_metadata.id, keyRange, direction, true, IDBDatabaseBackend::NormalTask, request);
    return request;
}

RefPtr<IDBRequest> LegacyIndex::openKeyCursor(ScriptExecutionContext* context, const Deprecated::ScriptValue& key, const String& direction, ExceptionCodeWithMessage& ec)
{
    LOG(StorageAPI, "LegacyIndex::openKeyCursor");
    RefPtr<IDBKeyRange> keyRange = IDBKeyRange::only(context, key, ec.code);
    if (ec.code)
        return 0;
    return openKeyCursor(context, keyRange.get(), direction, ec);
}

RefPtr<IDBRequest> LegacyIndex::get(ScriptExecutionContext* context, const Deprecated::ScriptValue& key, ExceptionCodeWithMessage& ec)
{
    LOG(StorageAPI, "LegacyIndex::get");
    RefPtr<IDBKeyRange> keyRange = IDBKeyRange::only(context, key, ec.code);
    if (ec.code)
        return 0;
    return get(context, keyRange.get(), ec);
}

RefPtr<IDBRequest> LegacyIndex::get(ScriptExecutionContext* context, IDBKeyRange* keyRange, ExceptionCodeWithMessage& ec)
{
    LOG(StorageAPI, "LegacyIndex::get");
    if (m_deleted) {
        ec.code = IDBDatabaseException::InvalidStateError;
        return 0;
    }
    if (!m_transaction->isActive()) {
        ec.code = IDBDatabaseException::TransactionInactiveError;
        return 0;
    }
    if (!keyRange) {
        ec.code = IDBDatabaseException::DataError;
        return 0;
    }

    RefPtr<LegacyRequest> request = LegacyRequest::create(context, LegacyAny::create(this), m_transaction.get());
    backendDB()->get(m_transaction->id(), m_objectStore->id(), m_metadata.id, keyRange, false, request);
    return request;
}

RefPtr<IDBRequest> LegacyIndex::getKey(ScriptExecutionContext* context, const Deprecated::ScriptValue& key, ExceptionCodeWithMessage& ec)
{
    LOG(StorageAPI, "LegacyIndex::getKey");
    RefPtr<IDBKeyRange> keyRange = IDBKeyRange::only(context, key, ec.code);
    if (ec.code)
        return 0;

    return getKey(context, keyRange.get(), ec);
}

RefPtr<IDBRequest> LegacyIndex::getKey(ScriptExecutionContext* context, IDBKeyRange* keyRange, ExceptionCodeWithMessage& ec)
{
    LOG(StorageAPI, "LegacyIndex::getKey");
    if (m_deleted) {
        ec.code = IDBDatabaseException::InvalidStateError;
        return 0;
    }
    if (!m_transaction->isActive()) {
        ec.code = IDBDatabaseException::TransactionInactiveError;
        return 0;
    }
    if (!keyRange) {
        ec.code = IDBDatabaseException::DataError;
        return 0;
    }

    RefPtr<LegacyRequest> request = LegacyRequest::create(context, LegacyAny::create(this), m_transaction.get());
    backendDB()->get(m_transaction->id(), m_objectStore->id(), m_metadata.id, keyRange, true, request);
    return request;
}

IDBDatabaseBackend* LegacyIndex::backendDB() const
{
    return m_transaction->backendDB();
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)

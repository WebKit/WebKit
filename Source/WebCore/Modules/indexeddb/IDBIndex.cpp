/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "IDBIndex.h"

#if ENABLE(INDEXED_DATABASE)

#include "IDBBindingUtilities.h"
#include "IDBCursor.h"
#include "IDBDatabase.h"
#include "IDBDatabaseException.h"
#include "IDBKeyRangeData.h"
#include "IDBObjectStore.h"
#include "IDBRequest.h"
#include "IDBTransaction.h"
#include "Logging.h"

using namespace JSC;

namespace WebCore {

IDBIndex::IDBIndex(ScriptExecutionContext& context, const IDBIndexInfo& info, IDBObjectStore& objectStore)
    : ActiveDOMObject(&context)
    , m_info(info)
    , m_objectStore(objectStore)
{
    ASSERT(currentThread() == m_objectStore.transaction().database().originThreadID());

    suspendIfNeeded();
}

IDBIndex::~IDBIndex()
{
    ASSERT(currentThread() == m_objectStore.transaction().database().originThreadID());
}

const char* IDBIndex::activeDOMObjectName() const
{
    return "IDBIndex";
}

bool IDBIndex::canSuspendForDocumentSuspension() const
{
    return false;
}

bool IDBIndex::hasPendingActivity() const
{
    return !m_objectStore.transaction().isFinished();
}

const String& IDBIndex::name() const
{
    ASSERT(currentThread() == m_objectStore.transaction().database().originThreadID());
    return m_info.name();
}

IDBObjectStore& IDBIndex::objectStore()
{
    ASSERT(currentThread() == m_objectStore.transaction().database().originThreadID());
    return m_objectStore;
}

const IDBKeyPath& IDBIndex::keyPath() const
{
    ASSERT(currentThread() == m_objectStore.transaction().database().originThreadID());
    return m_info.keyPath();
}

bool IDBIndex::unique() const
{
    ASSERT(currentThread() == m_objectStore.transaction().database().originThreadID());
    return m_info.unique();
}

bool IDBIndex::multiEntry() const
{
    ASSERT(currentThread() == m_objectStore.transaction().database().originThreadID());
    return m_info.multiEntry();
}

RefPtr<IDBRequest> IDBIndex::openCursor(ExecState& execState, IDBKeyRange* range, const String& directionString, ExceptionCodeWithMessage& ec)
{
    LOG(IndexedDB, "IDBIndex::openCursor");
    ASSERT(currentThread() == m_objectStore.transaction().database().originThreadID());

    if (m_deleted || m_objectStore.isDeleted()) {
        ec.code = IDBDatabaseException::InvalidStateError;
        ec.message = ASCIILiteral("Failed to execute 'openCursor' on 'IDBIndex': The index or its object store has been deleted.");
        return nullptr;
    }

    if (!m_objectStore.transaction().isActive()) {
        ec.code = IDBDatabaseException::TransactionInactiveError;
        ec.message = ASCIILiteral("Failed to execute 'openCursor' on 'IDBIndex': The transaction is inactive or finished.");
        return nullptr;
    }

    IndexedDB::CursorDirection direction = IDBCursor::stringToDirection(directionString, ec.code);
    if (ec.code) {
        ec.message = ASCIILiteral("Failed to execute 'openCursor' on 'IDBIndex': The direction provided ('invalid-direction') is not one of 'next', 'nextunique', 'prev', or 'prevunique'.");
        return nullptr;
    }

    IDBKeyRangeData rangeData = range;
    if (rangeData.lowerKey.isNull())
        rangeData.lowerKey = IDBKeyData::minimum();
    if (rangeData.upperKey.isNull())
        rangeData.upperKey = IDBKeyData::maximum();

    auto info = IDBCursorInfo::indexCursor(m_objectStore.transaction(), m_objectStore.info().identifier(), m_info.identifier(), rangeData, direction, IndexedDB::CursorType::KeyAndValue);
    return m_objectStore.transaction().requestOpenCursor(execState, *this, info);
}

RefPtr<IDBRequest> IDBIndex::openCursor(ExecState& execState, JSValue key, const String& direction, ExceptionCodeWithMessage& ec)
{
    LOG(IndexedDB, "IDBIndex::openCursor");
    ASSERT(currentThread() == m_objectStore.transaction().database().originThreadID());

    RefPtr<IDBKeyRange> keyRange = IDBKeyRange::only(execState, key, ec.code);
    if (ec.code) {
        ec.message = ASCIILiteral("Failed to execute 'openCursor' on 'IDBIndex': The parameter is not a valid key.");
        return nullptr;
    }

    return openCursor(execState, keyRange.get(), direction, ec);
}

RefPtr<IDBRequest> IDBIndex::count(ExecState& execState, IDBKeyRange* range, ExceptionCodeWithMessage& ec)
{
    LOG(IndexedDB, "IDBIndex::count");

    return doCount(execState, range ? IDBKeyRangeData(range) : IDBKeyRangeData::allKeys(), ec);
}

RefPtr<IDBRequest> IDBIndex::count(ExecState& execState, JSValue key, ExceptionCodeWithMessage& ec)
{
    LOG(IndexedDB, "IDBIndex::count");

    Ref<IDBKey> idbKey = scriptValueToIDBKey(execState, key);
    if (!idbKey->isValid()) {
        ec.code = IDBDatabaseException::DataError;
        ec.message = ASCIILiteral("Failed to execute 'count' on 'IDBIndex': The parameter is not a valid key.");
        return nullptr;
    }

    return doCount(execState, IDBKeyRangeData(idbKey.ptr()), ec);
}

RefPtr<IDBRequest> IDBIndex::doCount(ExecState& execState, const IDBKeyRangeData& range, ExceptionCodeWithMessage& ec)
{
    ASSERT(currentThread() == m_objectStore.transaction().database().originThreadID());

    if (m_deleted || m_objectStore.isDeleted()) {
        ec.code = IDBDatabaseException::InvalidStateError;
        ec.message = ASCIILiteral("Failed to execute 'count' on 'IDBIndex': The index or its object store has been deleted.");
        return nullptr;
    }

    if (!range.isValid()) {
        ec.code = IDBDatabaseException::DataError;
        return nullptr;
    }

    auto& transaction = m_objectStore.transaction();
    if (!transaction.isActive()) {
        ec.code = IDBDatabaseException::TransactionInactiveError;
        ec.message = ASCIILiteral("Failed to execute 'count' on 'IDBIndex': The transaction is inactive or finished.");
        return nullptr;
    }

    return transaction.requestCount(execState, *this, range);
}

RefPtr<IDBRequest> IDBIndex::openKeyCursor(ExecState& execState, IDBKeyRange* range, const String& directionString, ExceptionCodeWithMessage& ec)
{
    LOG(IndexedDB, "IDBIndex::openKeyCursor");
    ASSERT(currentThread() == m_objectStore.transaction().database().originThreadID());

    if (m_deleted || m_objectStore.isDeleted()) {
        ec.code = IDBDatabaseException::InvalidStateError;
        ec.message = ASCIILiteral("Failed to execute 'openKeyCursor' on 'IDBIndex': The index or its object store has been deleted.");
        return nullptr;
    }

    if (!m_objectStore.transaction().isActive()) {
        ec.code = IDBDatabaseException::TransactionInactiveError;
        ec.message = ASCIILiteral("Failed to execute 'openKeyCursor' on 'IDBIndex': The transaction is inactive or finished.");
        return nullptr;
    }

    IndexedDB::CursorDirection direction = IDBCursor::stringToDirection(directionString, ec.code);
    if (ec.code) {
        ec.message = ASCIILiteral("Failed to execute 'openKeyCursor' on 'IDBIndex': The direction provided ('invalid-direction') is not one of 'next', 'nextunique', 'prev', or 'prevunique'.");
        return nullptr;
    }

    auto info = IDBCursorInfo::indexCursor(m_objectStore.transaction(), m_objectStore.info().identifier(), m_info.identifier(), range, direction, IndexedDB::CursorType::KeyOnly);
    return m_objectStore.transaction().requestOpenCursor(execState, *this, info);
}

RefPtr<IDBRequest> IDBIndex::openKeyCursor(ExecState& execState, JSValue key, const String& direction, ExceptionCodeWithMessage& ec)
{
    LOG(IndexedDB, "IDBIndex::openKeyCursor");
    RefPtr<IDBKeyRange> keyRange = IDBKeyRange::only(execState, key, ec.code);
    if (ec.code) {
        ec.message = ASCIILiteral("Failed to execute 'openKeyCursor' on 'IDBIndex': The parameter is not a valid key.");
        return nullptr;
    }
    return openKeyCursor(execState, keyRange.get(), direction, ec);
}

RefPtr<IDBRequest> IDBIndex::get(ExecState& execState, IDBKeyRange* range, ExceptionCodeWithMessage& ec)
{
    LOG(IndexedDB, "IDBIndex::get");

    return doGet(execState, IDBKeyRangeData(range), ec);
}

RefPtr<IDBRequest> IDBIndex::get(ExecState& execState, JSValue key, ExceptionCodeWithMessage& ec)
{
    LOG(IndexedDB, "IDBIndex::get");

    Ref<IDBKey> idbKey = scriptValueToIDBKey(execState, key);
    if (!idbKey->isValid()) {
        ec.code = IDBDatabaseException::DataError;
        ec.message = ASCIILiteral("Failed to execute 'get' on 'IDBIndex': The parameter is not a valid key.");
        return nullptr;
    }

    return doGet(execState, IDBKeyRangeData(idbKey.ptr()), ec);
}

RefPtr<IDBRequest> IDBIndex::doGet(ExecState& execState, const IDBKeyRangeData& range, ExceptionCodeWithMessage& ec)
{
    ASSERT(currentThread() == m_objectStore.transaction().database().originThreadID());

    if (m_deleted || m_objectStore.isDeleted()) {
        ec.code = IDBDatabaseException::InvalidStateError;
        ec.message = ASCIILiteral("Failed to execute 'get' on 'IDBIndex': The index or its object store has been deleted.");
        return nullptr;
    }

    if (range.isNull) {
        ec.code = IDBDatabaseException::DataError;
        return nullptr;
    }

    auto& transaction = m_objectStore.transaction();
    if (!transaction.isActive()) {
        ec.code = IDBDatabaseException::TransactionInactiveError;
        ec.message = ASCIILiteral("Failed to execute 'get' on 'IDBIndex': The transaction is inactive or finished.");
        return nullptr;
    }

    return transaction.requestGetValue(execState, *this, range);
}

RefPtr<IDBRequest> IDBIndex::getKey(ExecState& execState, IDBKeyRange* range, ExceptionCodeWithMessage& ec)
{
    LOG(IndexedDB, "IDBIndex::getKey");

    return doGetKey(execState, IDBKeyRangeData(range), ec);
}

RefPtr<IDBRequest> IDBIndex::getKey(ExecState& execState, JSValue key, ExceptionCodeWithMessage& ec)
{
    LOG(IndexedDB, "IDBIndex::getKey");

    Ref<IDBKey> idbKey = scriptValueToIDBKey(execState, key);
    if (!idbKey->isValid()) {
        ec.code = IDBDatabaseException::DataError;
        ec.message = ASCIILiteral("Failed to execute 'getKey' on 'IDBIndex': The parameter is not a valid key.");
        return nullptr;
    }

    return doGetKey(execState, IDBKeyRangeData(idbKey.ptr()), ec);
}

RefPtr<IDBRequest> IDBIndex::doGetKey(ExecState& execState, const IDBKeyRangeData& range, ExceptionCodeWithMessage& ec)
{
    ASSERT(currentThread() == m_objectStore.transaction().database().originThreadID());

    if (m_deleted || m_objectStore.isDeleted()) {
        ec.code = IDBDatabaseException::InvalidStateError;
        ec.message = ASCIILiteral("Failed to execute 'getKey' on 'IDBIndex': The index or its object store has been deleted.");
        return nullptr;
    }

    if (range.isNull) {
        ec.code = IDBDatabaseException::DataError;
        return nullptr;
    }

    auto& transaction = m_objectStore.transaction();
    if (!transaction.isActive()) {
        ec.code = IDBDatabaseException::TransactionInactiveError;
        ec.message = ASCIILiteral("Failed to execute 'getKey' on 'IDBIndex': The transaction is inactive or finished.");
        return nullptr;
    }

    return transaction.requestGetKey(execState, *this, range);
}

void IDBIndex::markAsDeleted()
{
    ASSERT(currentThread() == m_objectStore.transaction().database().originThreadID());

    ASSERT(!m_deleted);
    m_deleted = true;
}

void IDBIndex::ref()
{
    m_objectStore.ref();
}

void IDBIndex::deref()
{
    m_objectStore.deref();
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)

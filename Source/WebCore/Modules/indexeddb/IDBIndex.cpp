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
#include "IDBKeyRangeData.h"
#include "IDBObjectStore.h"
#include "IDBRequest.h"
#include "IDBTransaction.h"
#include "Logging.h"
#include <JavaScriptCore/HeapInlines.h>

namespace WebCore {
using namespace JSC;

IDBIndex::IDBIndex(ScriptExecutionContext& context, const IDBIndexInfo& info, IDBObjectStore& objectStore)
    : ActiveDOMObject(&context)
    , m_info(info)
    , m_originalInfo(info)
    , m_objectStore(objectStore)
{
    ASSERT(&m_objectStore.transaction().database().originThread() == &Thread::current());

    suspendIfNeeded();
}

IDBIndex::~IDBIndex()
{
    ASSERT(&m_objectStore.transaction().database().originThread() == &Thread::current());
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
    return m_objectStore.transaction().hasPendingActivity();
}

const String& IDBIndex::name() const
{
    ASSERT(&m_objectStore.transaction().database().originThread() == &Thread::current());
    return m_info.name();
}

ExceptionOr<void> IDBIndex::setName(const String& name)
{
    ASSERT(&m_objectStore.transaction().database().originThread() == &Thread::current());

    if (m_deleted)
        return Exception { InvalidStateError, "Failed set property 'name' on 'IDBIndex': The index has been deleted."_s };

    if (m_objectStore.isDeleted())
        return Exception { InvalidStateError, "Failed set property 'name' on 'IDBIndex': The index's object store has been deleted."_s };

    if (!m_objectStore.transaction().isVersionChange())
        return Exception { InvalidStateError, "Failed set property 'name' on 'IDBIndex': The index's transaction is not a version change transaction."_s };

    if (!m_objectStore.transaction().isActive())
        return Exception { TransactionInactiveError, "Failed set property 'name' on 'IDBIndex': The index's transaction is not active."_s };

    if (m_info.name() == name)
        return { };

    if (m_objectStore.info().hasIndex(name))
        return Exception { ConstraintError, makeString("Failed set property 'name' on 'IDBIndex': The owning object store already has an index named '", name, "'.") };

    m_objectStore.transaction().database().renameIndex(*this, name);
    m_info.rename(name);

    return { };
}

IDBObjectStore& IDBIndex::objectStore()
{
    ASSERT(&m_objectStore.transaction().database().originThread() == &Thread::current());
    return m_objectStore;
}

const IDBKeyPath& IDBIndex::keyPath() const
{
    ASSERT(&m_objectStore.transaction().database().originThread() == &Thread::current());
    return m_info.keyPath();
}

bool IDBIndex::unique() const
{
    ASSERT(&m_objectStore.transaction().database().originThread() == &Thread::current());
    return m_info.unique();
}

bool IDBIndex::multiEntry() const
{
    ASSERT(&m_objectStore.transaction().database().originThread() == &Thread::current());
    return m_info.multiEntry();
}

void IDBIndex::rollbackInfoForVersionChangeAbort()
{
    ASSERT(&m_objectStore.transaction().database().originThread() == &Thread::current());

    // Only rollback to the original info if this index still exists in the rolled-back database info.
    auto* objectStoreInfo = m_objectStore.transaction().database().info().infoForExistingObjectStore(m_objectStore.info().identifier());
    if (!objectStoreInfo)
        return;

    if (!objectStoreInfo->hasIndex(m_info.identifier())) {
        m_deleted = true;
        return;
    }

    m_info = m_originalInfo;
    m_deleted = false;
}

ExceptionOr<Ref<IDBRequest>> IDBIndex::openCursor(ExecState& execState, IDBKeyRange* range, IDBCursorDirection direction)
{
    LOG(IndexedDB, "IDBIndex::openCursor");
    ASSERT(&m_objectStore.transaction().database().originThread() == &Thread::current());

    if (m_deleted || m_objectStore.isDeleted())
        return Exception { InvalidStateError, "Failed to execute 'openCursor' on 'IDBIndex': The index or its object store has been deleted."_s };

    if (!m_objectStore.transaction().isActive())
        return Exception { TransactionInactiveError, "Failed to execute 'openCursor' on 'IDBIndex': The transaction is inactive or finished."_s };

    IDBKeyRangeData rangeData = range;
    if (rangeData.lowerKey.isNull())
        rangeData.lowerKey = IDBKeyData::minimum();
    if (rangeData.upperKey.isNull())
        rangeData.upperKey = IDBKeyData::maximum();

    auto info = IDBCursorInfo::indexCursor(m_objectStore.transaction(), m_objectStore.info().identifier(), m_info.identifier(), rangeData, direction, IndexedDB::CursorType::KeyAndValue);
    return m_objectStore.transaction().requestOpenCursor(execState, *this, info);
}

ExceptionOr<Ref<IDBRequest>> IDBIndex::openCursor(ExecState& execState, JSValue key, IDBCursorDirection direction)
{
    LOG(IndexedDB, "IDBIndex::openCursor");
    ASSERT(&m_objectStore.transaction().database().originThread() == &Thread::current());

    auto keyRange = IDBKeyRange::only(execState, key);
    if (keyRange.hasException())
        return Exception { DataError, "Failed to execute 'openCursor' on 'IDBIndex': The parameter is not a valid key."_s };

    return openCursor(execState, keyRange.releaseReturnValue().ptr(), direction);
}

ExceptionOr<Ref<IDBRequest>> IDBIndex::openKeyCursor(ExecState& execState, IDBKeyRange* range, IDBCursorDirection direction)
{
    LOG(IndexedDB, "IDBIndex::openKeyCursor");
    ASSERT(&m_objectStore.transaction().database().originThread() == &Thread::current());

    if (m_deleted || m_objectStore.isDeleted())
        return Exception { InvalidStateError, "Failed to execute 'openKeyCursor' on 'IDBIndex': The index or its object store has been deleted."_s };

    if (!m_objectStore.transaction().isActive())
        return Exception { TransactionInactiveError, "Failed to execute 'openKeyCursor' on 'IDBIndex': The transaction is inactive or finished."_s };

    auto info = IDBCursorInfo::indexCursor(m_objectStore.transaction(), m_objectStore.info().identifier(), m_info.identifier(), range, direction, IndexedDB::CursorType::KeyOnly);
    return m_objectStore.transaction().requestOpenCursor(execState, *this, info);
}

ExceptionOr<Ref<IDBRequest>> IDBIndex::openKeyCursor(ExecState& execState, JSValue key, IDBCursorDirection direction)
{
    LOG(IndexedDB, "IDBIndex::openKeyCursor");

    auto keyRange = IDBKeyRange::only(execState, key);
    if (keyRange.hasException())
        return Exception { DataError, "Failed to execute 'openKeyCursor' on 'IDBIndex': The parameter is not a valid key."_s };
    return openKeyCursor(execState, keyRange.releaseReturnValue().ptr(), direction);
}

ExceptionOr<Ref<IDBRequest>> IDBIndex::count(ExecState& execState, IDBKeyRange* range)
{
    LOG(IndexedDB, "IDBIndex::count");

    return doCount(execState, range ? IDBKeyRangeData(range) : IDBKeyRangeData::allKeys());
}

ExceptionOr<Ref<IDBRequest>> IDBIndex::count(ExecState& execState, JSValue key)
{
    LOG(IndexedDB, "IDBIndex::count");

    auto idbKey = scriptValueToIDBKey(execState, key);
    if (!idbKey->isValid())
        return Exception { DataError, "Failed to execute 'count' on 'IDBIndex': The parameter is not a valid key."_s };

    return doCount(execState, IDBKeyRangeData(idbKey.ptr()));
}

ExceptionOr<Ref<IDBRequest>> IDBIndex::doCount(ExecState& execState, const IDBKeyRangeData& range)
{
    ASSERT(&m_objectStore.transaction().database().originThread() == &Thread::current());

    if (m_deleted || m_objectStore.isDeleted())
        return Exception { InvalidStateError, "Failed to execute 'count' on 'IDBIndex': The index or its object store has been deleted."_s };

    if (!range.isValid())
        return Exception { DataError };

    auto& transaction = m_objectStore.transaction();
    if (!transaction.isActive())
        return Exception { TransactionInactiveError, "Failed to execute 'count' on 'IDBIndex': The transaction is inactive or finished."_s };

    return transaction.requestCount(execState, *this, range);
}

ExceptionOr<Ref<IDBRequest>> IDBIndex::get(ExecState& execState, IDBKeyRange* range)
{
    LOG(IndexedDB, "IDBIndex::get");

    return doGet(execState, IDBKeyRangeData(range));
}

ExceptionOr<Ref<IDBRequest>> IDBIndex::get(ExecState& execState, JSValue key)
{
    LOG(IndexedDB, "IDBIndex::get");

    auto idbKey = scriptValueToIDBKey(execState, key);
    if (!idbKey->isValid())
        return Exception { DataError, "Failed to execute 'get' on 'IDBIndex': The parameter is not a valid key."_s };

    return doGet(execState, IDBKeyRangeData(idbKey.ptr()));
}

ExceptionOr<Ref<IDBRequest>> IDBIndex::doGet(ExecState& execState, const IDBKeyRangeData& range)
{
    ASSERT(&m_objectStore.transaction().database().originThread() == &Thread::current());

    if (m_deleted || m_objectStore.isDeleted())
        return Exception { InvalidStateError, "Failed to execute 'get' on 'IDBIndex': The index or its object store has been deleted."_s };

    if (range.isNull)
        return Exception { DataError };

    auto& transaction = m_objectStore.transaction();
    if (!transaction.isActive())
        return Exception { TransactionInactiveError, "Failed to execute 'get' on 'IDBIndex': The transaction is inactive or finished."_s };

    return transaction.requestGetValue(execState, *this, range);
}

ExceptionOr<Ref<IDBRequest>> IDBIndex::getKey(ExecState& execState, IDBKeyRange* range)
{
    LOG(IndexedDB, "IDBIndex::getKey");

    return doGetKey(execState, IDBKeyRangeData(range));
}

ExceptionOr<Ref<IDBRequest>> IDBIndex::getKey(ExecState& execState, JSValue key)
{
    LOG(IndexedDB, "IDBIndex::getKey");

    auto idbKey = scriptValueToIDBKey(execState, key);
    if (!idbKey->isValid())
        return Exception { DataError, "Failed to execute 'getKey' on 'IDBIndex': The parameter is not a valid key."_s };

    return doGetKey(execState, IDBKeyRangeData(idbKey.ptr()));
}

ExceptionOr<Ref<IDBRequest>> IDBIndex::doGetKey(ExecState& execState, const IDBKeyRangeData& range)
{
    ASSERT(&m_objectStore.transaction().database().originThread() == &Thread::current());

    if (m_deleted || m_objectStore.isDeleted())
        return Exception { InvalidStateError, "Failed to execute 'getKey' on 'IDBIndex': The index or its object store has been deleted."_s };

    if (range.isNull)
        return Exception { DataError };

    auto& transaction = m_objectStore.transaction();
    if (!transaction.isActive())
        return Exception { TransactionInactiveError, "Failed to execute 'getKey' on 'IDBIndex': The transaction is inactive or finished."_s };

    return transaction.requestGetKey(execState, *this, range);
}

ExceptionOr<Ref<IDBRequest>> IDBIndex::getAll(ExecState& execState, RefPtr<IDBKeyRange> range, Optional<uint32_t> count)
{
    LOG(IndexedDB, "IDBIndex::getAll");
    ASSERT(&m_objectStore.transaction().database().originThread() == &Thread::current());

    if (m_deleted || m_objectStore.isDeleted())
        return Exception { InvalidStateError, "Failed to execute 'getAll' on 'IDBIndex': The index or its object store has been deleted."_s };

    if (!m_objectStore.transaction().isActive())
        return Exception { TransactionInactiveError, "Failed to execute 'getAll' on 'IDBIndex': The transaction is inactive or finished."_s };

    return m_objectStore.transaction().requestGetAllIndexRecords(execState, *this, range.get(), IndexedDB::GetAllType::Values, count);
}

ExceptionOr<Ref<IDBRequest>> IDBIndex::getAll(ExecState& execState, JSValue key, Optional<uint32_t> count)
{
    auto onlyResult = IDBKeyRange::only(execState, key);
    if (onlyResult.hasException())
        return Exception { DataError, "Failed to execute 'getAll' on 'IDBIndex': The parameter is not a valid key."_s };

    return getAll(execState, onlyResult.releaseReturnValue(), count);
}

ExceptionOr<Ref<IDBRequest>> IDBIndex::getAllKeys(ExecState& execState, RefPtr<IDBKeyRange> range, Optional<uint32_t> count)
{
    LOG(IndexedDB, "IDBIndex::getAllKeys");
    ASSERT(&m_objectStore.transaction().database().originThread() == &Thread::current());

    if (m_deleted || m_objectStore.isDeleted())
        return Exception { InvalidStateError, "Failed to execute 'getAllKeys' on 'IDBIndex': The index or its object store has been deleted."_s };

    if (!m_objectStore.transaction().isActive())
        return Exception { TransactionInactiveError, "Failed to execute 'getAllKeys' on 'IDBIndex': The transaction is inactive or finished."_s };

    return m_objectStore.transaction().requestGetAllIndexRecords(execState, *this, range.get(), IndexedDB::GetAllType::Keys, count);
}

ExceptionOr<Ref<IDBRequest>> IDBIndex::getAllKeys(ExecState& execState, JSValue key, Optional<uint32_t> count)
{
    auto onlyResult = IDBKeyRange::only(execState, key);
    if (onlyResult.hasException())
        return Exception { DataError, "Failed to execute 'getAllKeys' on 'IDBIndex': The parameter is not a valid key."_s };

    return getAllKeys(execState, onlyResult.releaseReturnValue(), count);
}

void IDBIndex::markAsDeleted()
{
    ASSERT(&m_objectStore.transaction().database().originThread() == &Thread::current());

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

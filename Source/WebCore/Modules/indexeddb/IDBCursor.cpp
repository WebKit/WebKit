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
#include "IDBCursor.h"

#include "IDBBindingUtilities.h"
#include "IDBDatabase.h"
#include "IDBGetResult.h"
#include "IDBIndex.h"
#include "IDBIterateCursorData.h"
#include "IDBObjectStore.h"
#include "IDBRequest.h"
#include "IDBTransaction.h"
#include "Logging.h"
#include "ScriptExecutionContext.h"
#include "SerializedScriptValue.h"
#include <JavaScriptCore/HeapInlines.h>
#include <JavaScriptCore/JSCJSValueInlines.h>
#include <JavaScriptCore/StrongInlines.h>
#include <wtf/IsoMallocInlines.h>

namespace WebCore {
using namespace JSC;

WTF_MAKE_ISO_ALLOCATED_IMPL(IDBCursor);

Ref<IDBCursor> IDBCursor::create(IDBObjectStore& objectStore, const IDBCursorInfo& info)
{
    return adoptRef(*new IDBCursor(objectStore, info));
}

Ref<IDBCursor> IDBCursor::create(IDBIndex& index, const IDBCursorInfo& info)
{
    return adoptRef(*new IDBCursor(index, info));
}

IDBCursor::IDBCursor(IDBObjectStore& objectStore, const IDBCursorInfo& info)
    : m_info(info)
    , m_source(&objectStore)
{
    ASSERT(canCurrentThreadAccessThreadLocalData(effectiveObjectStore().transaction().database().originThread()));
}

IDBCursor::IDBCursor(IDBIndex& index, const IDBCursorInfo& info)
    : m_info(info)
    , m_source(&index)
{
    ASSERT(canCurrentThreadAccessThreadLocalData(effectiveObjectStore().transaction().database().originThread()));
}

IDBCursor::~IDBCursor()
{
    ASSERT(canCurrentThreadAccessThreadLocalData(effectiveObjectStore().transaction().database().originThread()));
}

bool IDBCursor::sourcesDeleted() const
{
    ASSERT(canCurrentThreadAccessThreadLocalData(effectiveObjectStore().transaction().database().originThread()));

    return WTF::switchOn(m_source,
        [] (const RefPtr<IDBObjectStore>& objectStore) { return objectStore->isDeleted(); },
        [] (const RefPtr<IDBIndex>& index) { return index->isDeleted() || index->objectStore().isDeleted(); }
    );
}

IDBObjectStore& IDBCursor::effectiveObjectStore() const
{
    return WTF::switchOn(m_source,
        [] (const RefPtr<IDBObjectStore>& objectStore) -> IDBObjectStore& { return *objectStore; },
        [] (const RefPtr<IDBIndex>& index) -> IDBObjectStore& { return index->objectStore(); }
    );
}

IDBTransaction& IDBCursor::transaction() const
{
    ASSERT(canCurrentThreadAccessThreadLocalData(effectiveObjectStore().transaction().database().originThread()));
    return effectiveObjectStore().transaction();
}

ExceptionOr<Ref<IDBRequest>> IDBCursor::update(JSGlobalObject& state, JSValue value)
{
    LOG(IndexedDB, "IDBCursor::update");
    ASSERT(canCurrentThreadAccessThreadLocalData(effectiveObjectStore().transaction().database().originThread()));

    if (sourcesDeleted())
        return Exception { ExceptionCode::InvalidStateError, "Failed to execute 'update' on 'IDBCursor': The cursor's source or effective object store has been deleted."_s };

    if (!transaction().isActive())
        return Exception { ExceptionCode::TransactionInactiveError, "Failed to execute 'update' on 'IDBCursor': The transaction is inactive or finished."_s };

    if (transaction().isReadOnly())
        return Exception { ExceptionCode::ReadonlyError, "Failed to execute 'update' on 'IDBCursor': The record may not be updated inside a read-only transaction."_s };

    if (!m_gotValue)
        return Exception { ExceptionCode::InvalidStateError, "Failed to execute 'update' on 'IDBCursor': The cursor is being iterated or has iterated past its end."_s };

    if (!isKeyCursorWithValue())
        return Exception { ExceptionCode::InvalidStateError, "Failed to execute 'update' on 'IDBCursor': The cursor is a key cursor."_s };

    VM& vm = state.vm();
    auto scope = DECLARE_CATCH_SCOPE(vm);

    // Transaction should be inactive during structured clone.
    Ref transaction = effectiveObjectStore().transaction();
    transaction->deactivate();
    auto serializedValue = SerializedScriptValue::create(state, value, SerializationForStorage::Yes);
    transaction->activate();

    if (UNLIKELY(scope.exception()))
        return Exception { ExceptionCode::DataCloneError, "Failed to store record in an IDBObjectStore: An object could not be cloned."_s };

    auto& objectStore = effectiveObjectStore();
    auto& optionalKeyPath = objectStore.info().keyPath();
    const bool usesInLineKeys = !!optionalKeyPath;
    if (usesInLineKeys) {
        auto clonedValue = serializedValue->deserialize(state, &state, SerializationErrorMode::NonThrowing);
        RefPtr<IDBKey> keyPathKey = maybeCreateIDBKeyFromScriptValueAndKeyPath(state, clonedValue, optionalKeyPath.value());
        IDBKeyData keyPathKeyData(keyPathKey.get());
        if (!keyPathKey || keyPathKeyData != m_primaryKeyData)
            return Exception { ExceptionCode::DataError, "Failed to execute 'update' on 'IDBCursor': The effective object store of this cursor uses in-line keys and evaluating the key path of the value parameter results in a different value than the cursor's effective key."_s };
    }

    auto putResult = effectiveObjectStore().putForCursorUpdate(state, value, m_primaryKey.copyRef(), WTFMove(serializedValue));
    if (putResult.hasException())
        return putResult.releaseException();

    auto request = putResult.releaseReturnValue();
    request->setSource(*this);

    return request;
}

ExceptionOr<void> IDBCursor::advance(unsigned count)
{
    LOG(IndexedDB, "IDBCursor::advance");
    ASSERT(canCurrentThreadAccessThreadLocalData(effectiveObjectStore().transaction().database().originThread()));

    if (!m_request)
        return Exception { ExceptionCode::InvalidStateError };

    if (!count)
        return Exception { ExceptionCode::TypeError, "Failed to execute 'advance' on 'IDBCursor': A count argument with value 0 (zero) was supplied, must be greater than 0."_s };

    if (!transaction().isActive())
        return Exception { ExceptionCode::TransactionInactiveError, "Failed to execute 'advance' on 'IDBCursor': The transaction is inactive or finished."_s };

    if (sourcesDeleted())
        return Exception { ExceptionCode::InvalidStateError, "Failed to execute 'advance' on 'IDBCursor': The cursor's source or effective object store has been deleted."_s };

    if (!m_gotValue)
        return Exception { ExceptionCode::InvalidStateError, "Failed to execute 'advance' on 'IDBCursor': The cursor is being iterated or has iterated past its end."_s };

    m_gotValue = false;

    uncheckedIterateCursor(IDBKeyData(), count);

    return { };
}

ExceptionOr<void> IDBCursor::continuePrimaryKey(JSGlobalObject& state, JSValue keyValue, JSValue primaryKeyValue)
{
    if (!m_request)
        return Exception { ExceptionCode::InvalidStateError };

    if (!transaction().isActive())
        return Exception { ExceptionCode::TransactionInactiveError, "Failed to execute 'continuePrimaryKey' on 'IDBCursor': The transaction is inactive or finished."_s };

    if (sourcesDeleted())
        return Exception { ExceptionCode::InvalidStateError, "Failed to execute 'continuePrimaryKey' on 'IDBCursor': The cursor's source or effective object store has been deleted."_s };

    if (!std::holds_alternative<RefPtr<IDBIndex>>(m_source))
        return Exception { ExceptionCode::InvalidAccessError, "Failed to execute 'continuePrimaryKey' on 'IDBCursor': The cursor's source is not an index."_s };

    auto direction = m_info.cursorDirection();
    if (direction != IndexedDB::CursorDirection::Next && direction != IndexedDB::CursorDirection::Prev)
        return Exception { ExceptionCode::InvalidAccessError, "Failed to execute 'continuePrimaryKey' on 'IDBCursor': The cursor's direction must be either \"next\" or \"prev\"."_s };

    if (!m_gotValue)
        return Exception { ExceptionCode::InvalidStateError, "Failed to execute 'continuePrimaryKey' on 'IDBCursor': The cursor is being iterated or has iterated past its end."_s };

    RefPtr<IDBKey> key = scriptValueToIDBKey(state, keyValue);
    if (!key->isValid())
        return Exception { ExceptionCode::DataError, "Failed to execute 'continuePrimaryKey' on 'IDBCursor': The first parameter is not a valid key."_s };

    RefPtr<IDBKey> primaryKey = scriptValueToIDBKey(state, primaryKeyValue);
    if (!primaryKey->isValid())
        return Exception { ExceptionCode::DataError, "Failed to execute 'continuePrimaryKey' on 'IDBCursor': The second parameter is not a valid key."_s };

    IDBKeyData keyData = { key.get() };
    IDBKeyData primaryKeyData = { primaryKey.get() };

    if (keyData < m_keyData && direction == IndexedDB::CursorDirection::Next)
        return Exception { ExceptionCode::DataError, "Failed to execute 'continuePrimaryKey' on 'IDBCursor': The first parameter is less than this cursor's position and this cursor's direction is \"next\"."_s };

    if (keyData > m_keyData && direction == IndexedDB::CursorDirection::Prev)
        return Exception { ExceptionCode::DataError, "Failed to execute 'continuePrimaryKey' on 'IDBCursor': The first parameter is greater than this cursor's position and this cursor's direction is \"prev\"."_s };

    if (keyData == m_keyData) {
        if (primaryKeyData <= m_primaryKeyData && direction == IndexedDB::CursorDirection::Next)
            return Exception { ExceptionCode::DataError, "Failed to execute 'continuePrimaryKey' on 'IDBCursor': The key parameters represent a position less-than-or-equal-to this cursor's position and this cursor's direction is \"next\"."_s };
        if (primaryKeyData >= m_primaryKeyData && direction == IndexedDB::CursorDirection::Prev)
            return Exception { ExceptionCode::DataError, "Failed to execute 'continuePrimaryKey' on 'IDBCursor': The key parameters represent a position greater-than-or-equal-to this cursor's position and this cursor's direction is \"prev\"."_s };
    }

    m_gotValue = false;

    uncheckedIterateCursor(keyData, primaryKeyData);

    return { };
}

ExceptionOr<void> IDBCursor::continueFunction(JSGlobalObject& execState, JSValue keyValue)
{
    RefPtr<IDBKey> key;
    if (!keyValue.isUndefined())
        key = scriptValueToIDBKey(execState, keyValue);

    return continueFunction(key.get());
}

ExceptionOr<void> IDBCursor::continueFunction(const IDBKeyData& key)
{
    LOG(IndexedDB, "IDBCursor::continueFunction (to key %s)", key.loggingString().utf8().data());
    ASSERT(canCurrentThreadAccessThreadLocalData(effectiveObjectStore().transaction().database().originThread()));

    if (!m_request)
        return Exception { ExceptionCode::InvalidStateError };

    if (!transaction().isActive())
        return Exception { ExceptionCode::TransactionInactiveError, "Failed to execute 'continue' on 'IDBCursor': The transaction is inactive or finished."_s };

    if (sourcesDeleted())
        return Exception { ExceptionCode::InvalidStateError, "Failed to execute 'continue' on 'IDBCursor': The cursor's source or effective object store has been deleted."_s };

    if (!m_gotValue)
        return Exception { ExceptionCode::InvalidStateError, "Failed to execute 'continue' on 'IDBCursor': The cursor is being iterated or has iterated past its end."_s };

    if (!key.isNull() && !key.isValid())
        return Exception { ExceptionCode::DataError, "Failed to execute 'continue' on 'IDBCursor': The parameter is not a valid key."_s };

    if (m_info.isDirectionForward()) {
        if (!key.isNull() && key.compare(m_keyData) <= 0)
            return Exception { ExceptionCode::DataError, "Failed to execute 'continue' on 'IDBCursor': The parameter is less than or equal to this cursor's position."_s };
    } else {
        if (!key.isNull() && key.compare(m_keyData) >= 0)
            return Exception { ExceptionCode::DataError, "Failed to execute 'continue' on 'IDBCursor': The parameter is greater than or equal to this cursor's position."_s };
    }

    m_gotValue = false;

    uncheckedIterateCursor(key, 0);

    return { };
}

void IDBCursor::uncheckedIterateCursor(const IDBKeyData& key, unsigned count)
{
    ASSERT(m_request);
    ASSERT(canCurrentThreadAccessThreadLocalData(effectiveObjectStore().transaction().database().originThread()));

    m_request->willIterateCursor(*this);
    transaction().iterateCursor(*this, { key, { }, count });
}

void IDBCursor::uncheckedIterateCursor(const IDBKeyData& key, const IDBKeyData& primaryKey)
{
    ASSERT(m_request);
    ASSERT(canCurrentThreadAccessThreadLocalData(effectiveObjectStore().transaction().database().originThread()));

    m_request->willIterateCursor(*this);
    transaction().iterateCursor(*this, { key, primaryKey, 0 });
}

ExceptionOr<Ref<WebCore::IDBRequest>> IDBCursor::deleteFunction()
{
    LOG(IndexedDB, "IDBCursor::deleteFunction");
    ASSERT(canCurrentThreadAccessThreadLocalData(effectiveObjectStore().transaction().database().originThread()));

    if (sourcesDeleted())
        return Exception { ExceptionCode::InvalidStateError, "Failed to execute 'delete' on 'IDBCursor': The cursor's source or effective object store has been deleted."_s };

    if (!transaction().isActive())
        return Exception { ExceptionCode::TransactionInactiveError, "Failed to execute 'delete' on 'IDBCursor': The transaction is inactive or finished."_s };

    if (transaction().isReadOnly())
        return Exception { ExceptionCode::ReadonlyError, "Failed to execute 'delete' on 'IDBCursor': The record may not be deleted inside a read-only transaction."_s };

    if (!m_gotValue)
        return Exception { ExceptionCode::InvalidStateError, "Failed to execute 'delete' on 'IDBCursor': The cursor is being iterated or has iterated past its end."_s };

    if (!isKeyCursorWithValue())
        return Exception { ExceptionCode::InvalidStateError, "Failed to execute 'delete' on 'IDBCursor': The cursor is a key cursor."_s };

    auto result = effectiveObjectStore().deleteFunction(IDBKeyRange::create(m_primaryKey.copyRef()).ptr());
    if (result.hasException())
        return result.releaseException();

    auto request = result.releaseReturnValue();
    request->setSource(*this);

    return request;
}

bool IDBCursor::setGetResult(IDBRequest& request, const IDBGetResult& getResult, uint64_t operationID)
{
    LOG(IndexedDB, "IDBCursor::setGetResult - current key %s", getResult.keyData().loggingString().left(100).utf8().data());
    ASSERT(canCurrentThreadAccessThreadLocalData(effectiveObjectStore().transaction().database().originThread()));

    RefPtr context = request.scriptExecutionContext();
    if (!context)
        return false;

    VM& vm = context->vm();
    JSLockHolder lock(vm);

    clearWrappers();

    if (!getResult.isDefined()) {
        m_keyData = { };
        m_key = nullptr;
        m_primaryKeyData = { };
        m_primaryKey = nullptr;
        m_value = { };

        m_gotValue = false;
        return false;
    }

    m_keyData = getResult.keyData();
    m_key = m_keyData.maybeCreateIDBKey();
    m_primaryKeyData = getResult.primaryKeyData();
    m_primaryKey = m_primaryKeyData.maybeCreateIDBKey();

    if (isKeyCursorWithValue()) {
        m_value = getResult.value();
        m_keyPath = getResult.keyPath();
    }

    auto prefetchedRecords = getResult.prefetchedRecords();
    if (!prefetchedRecords.isEmpty()) {
        for (auto& record : prefetchedRecords)
            m_prefetchedRecords.append(record);
        m_prefetchOperationID = operationID;
    }

    m_gotValue = true;
    return true;
}

void IDBCursor::clearWrappers()
{
    m_keyWrapper.clear();
    m_primaryKeyWrapper.clear();
    m_valueWrapper.clear();
}

std::optional<IDBGetResult> IDBCursor::iterateWithPrefetchedRecords(unsigned count, uint64_t lastWriteOperationID)
{
    unsigned step = count > 0 ? count : 1;
    if (step > m_prefetchedRecords.size() || m_prefetchOperationID <= lastWriteOperationID)
        return std::nullopt;

    while (--step)
        m_prefetchedRecords.removeFirst();

    auto record = m_prefetchedRecords.takeFirst();

    LOG(IndexedDB, "IDBTransaction::iterateWithPrefetchedRecords consumes %u records", count > 0 ? count : 1);
    return IDBGetResult(record.key, record.primaryKey, IDBValue(record.value), effectiveObjectStore().keyPath());
}

void IDBCursor::clearPrefetchedRecords()
{
    m_prefetchedRecords.clear();
}

} // namespace WebCore

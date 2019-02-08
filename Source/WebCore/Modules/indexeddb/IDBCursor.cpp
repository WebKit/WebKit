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

#if ENABLE(INDEXED_DATABASE)

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
#include <JavaScriptCore/HeapInlines.h>
#include <JavaScriptCore/JSCJSValueInlines.h>
#include <JavaScriptCore/StrongInlines.h>

namespace WebCore {
using namespace JSC;

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
    ASSERT(&effectiveObjectStore().transaction().database().originThread() == &Thread::current());
}

IDBCursor::IDBCursor(IDBIndex& index, const IDBCursorInfo& info)
    : m_info(info)
    , m_source(&index)
{
    ASSERT(&effectiveObjectStore().transaction().database().originThread() == &Thread::current());
}

IDBCursor::~IDBCursor()
{
    ASSERT(&effectiveObjectStore().transaction().database().originThread() == &Thread::current());
}

bool IDBCursor::sourcesDeleted() const
{
    ASSERT(&effectiveObjectStore().transaction().database().originThread() == &Thread::current());

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
    ASSERT(&effectiveObjectStore().transaction().database().originThread() == &Thread::current());
    return effectiveObjectStore().transaction();
}

ExceptionOr<Ref<IDBRequest>> IDBCursor::update(ExecState& state, JSValue value)
{
    LOG(IndexedDB, "IDBCursor::update");
    ASSERT(&effectiveObjectStore().transaction().database().originThread() == &Thread::current());

    if (sourcesDeleted())
        return Exception { InvalidStateError, "Failed to execute 'update' on 'IDBCursor': The cursor's source or effective object store has been deleted."_s };

    if (!transaction().isActive())
        return Exception { TransactionInactiveError, "Failed to execute 'update' on 'IDBCursor': The transaction is inactive or finished."_s };

    if (transaction().isReadOnly())
        return Exception { ReadonlyError, "Failed to execute 'update' on 'IDBCursor': The record may not be updated inside a read-only transaction."_s };

    if (!m_gotValue)
        return Exception { InvalidStateError, "Failed to execute 'update' on 'IDBCursor': The cursor is being iterated or has iterated past its end."_s };

    if (!isKeyCursorWithValue())
        return Exception { InvalidStateError, "Failed to execute 'update' on 'IDBCursor': The cursor is a key cursor."_s };

    auto& objectStore = effectiveObjectStore();
    auto& optionalKeyPath = objectStore.info().keyPath();
    const bool usesInLineKeys = !!optionalKeyPath;
    if (usesInLineKeys) {
        RefPtr<IDBKey> keyPathKey = maybeCreateIDBKeyFromScriptValueAndKeyPath(state, value, optionalKeyPath.value());
        IDBKeyData keyPathKeyData(keyPathKey.get());
        if (!keyPathKey || keyPathKeyData != m_primaryKeyData)
            return Exception { DataError, "Failed to execute 'update' on 'IDBCursor': The effective object store of this cursor uses in-line keys and evaluating the key path of the value parameter results in a different value than the cursor's effective key."_s };
    }

    auto putResult = effectiveObjectStore().putForCursorUpdate(state, value, m_primaryKey.copyRef());
    if (putResult.hasException())
        return putResult.releaseException();

    auto request = putResult.releaseReturnValue();
    request->setSource(*this);

    return WTFMove(request);
}

ExceptionOr<void> IDBCursor::advance(unsigned count)
{
    LOG(IndexedDB, "IDBCursor::advance");
    ASSERT(&effectiveObjectStore().transaction().database().originThread() == &Thread::current());

    if (!m_request)
        return Exception { InvalidStateError };

    if (!count)
        return Exception { TypeError, "Failed to execute 'advance' on 'IDBCursor': A count argument with value 0 (zero) was supplied, must be greater than 0."_s };

    if (!transaction().isActive())
        return Exception { TransactionInactiveError, "Failed to execute 'advance' on 'IDBCursor': The transaction is inactive or finished."_s };

    if (sourcesDeleted())
        return Exception { InvalidStateError, "Failed to execute 'advance' on 'IDBCursor': The cursor's source or effective object store has been deleted."_s };

    if (!m_gotValue)
        return Exception { InvalidStateError, "Failed to execute 'advance' on 'IDBCursor': The cursor is being iterated or has iterated past its end."_s };

    m_gotValue = false;

    uncheckedIterateCursor(IDBKeyData(), count);

    return { };
}

ExceptionOr<void> IDBCursor::continuePrimaryKey(ExecState& state, JSValue keyValue, JSValue primaryKeyValue)
{
    if (!m_request)
        return Exception { InvalidStateError };

    if (!transaction().isActive())
        return Exception { TransactionInactiveError, "Failed to execute 'continuePrimaryKey' on 'IDBCursor': The transaction is inactive or finished."_s };

    if (sourcesDeleted())
        return Exception { InvalidStateError, "Failed to execute 'continuePrimaryKey' on 'IDBCursor': The cursor's source or effective object store has been deleted."_s };

    if (!WTF::holds_alternative<RefPtr<IDBIndex>>(m_source))
        return Exception { InvalidAccessError, "Failed to execute 'continuePrimaryKey' on 'IDBCursor': The cursor's source is not an index."_s };

    auto direction = m_info.cursorDirection();
    if (direction != IndexedDB::CursorDirection::Next && direction != IndexedDB::CursorDirection::Prev)
        return Exception { InvalidAccessError, "Failed to execute 'continuePrimaryKey' on 'IDBCursor': The cursor's direction must be either \"next\" or \"prev\"."_s };

    if (!m_gotValue)
        return Exception { InvalidStateError, "Failed to execute 'continuePrimaryKey' on 'IDBCursor': The cursor is being iterated or has iterated past its end."_s };

    RefPtr<IDBKey> key = scriptValueToIDBKey(state, keyValue);
    if (!key->isValid())
        return Exception { DataError, "Failed to execute 'continuePrimaryKey' on 'IDBCursor': The first parameter is not a valid key."_s };

    RefPtr<IDBKey> primaryKey = scriptValueToIDBKey(state, primaryKeyValue);
    if (!primaryKey->isValid())
        return Exception { DataError, "Failed to execute 'continuePrimaryKey' on 'IDBCursor': The second parameter is not a valid key."_s };

    IDBKeyData keyData = { key.get() };
    IDBKeyData primaryKeyData = { primaryKey.get() };

    if (keyData < m_keyData && direction == IndexedDB::CursorDirection::Next)
        return Exception { DataError, "Failed to execute 'continuePrimaryKey' on 'IDBCursor': The first parameter is less than this cursor's position and this cursor's direction is \"next\"."_s };

    if (keyData > m_keyData && direction == IndexedDB::CursorDirection::Prev)
        return Exception { DataError, "Failed to execute 'continuePrimaryKey' on 'IDBCursor': The first parameter is greater than this cursor's position and this cursor's direction is \"prev\"."_s };

    if (keyData == m_keyData) {
        if (primaryKeyData <= m_primaryKeyData && direction == IndexedDB::CursorDirection::Next)
            return Exception { DataError, "Failed to execute 'continuePrimaryKey' on 'IDBCursor': The key parameters represent a position less-than-or-equal-to this cursor's position and this cursor's direction is \"next\"."_s };
        if (primaryKeyData >= m_primaryKeyData && direction == IndexedDB::CursorDirection::Prev)
            return Exception { DataError, "Failed to execute 'continuePrimaryKey' on 'IDBCursor': The key parameters represent a position greater-than-or-equal-to this cursor's position and this cursor's direction is \"prev\"."_s };
    }

    m_gotValue = false;

    uncheckedIterateCursor(keyData, primaryKeyData);

    return { };
}

ExceptionOr<void> IDBCursor::continueFunction(ExecState& execState, JSValue keyValue)
{
    RefPtr<IDBKey> key;
    if (!keyValue.isUndefined())
        key = scriptValueToIDBKey(execState, keyValue);

    return continueFunction(key.get());
}

ExceptionOr<void> IDBCursor::continueFunction(const IDBKeyData& key)
{
    LOG(IndexedDB, "IDBCursor::continueFunction (to key %s)", key.loggingString().utf8().data());
    ASSERT(&effectiveObjectStore().transaction().database().originThread() == &Thread::current());

    if (!m_request)
        return Exception { InvalidStateError };

    if (!transaction().isActive())
        return Exception { TransactionInactiveError, "Failed to execute 'continue' on 'IDBCursor': The transaction is inactive or finished."_s };

    if (sourcesDeleted())
        return Exception { InvalidStateError, "Failed to execute 'continue' on 'IDBCursor': The cursor's source or effective object store has been deleted."_s };

    if (!m_gotValue)
        return Exception { InvalidStateError, "Failed to execute 'continue' on 'IDBCursor': The cursor is being iterated or has iterated past its end."_s };

    if (!key.isNull() && !key.isValid())
        return Exception { DataError, "Failed to execute 'continue' on 'IDBCursor': The parameter is not a valid key."_s };

    if (m_info.isDirectionForward()) {
        if (!key.isNull() && key.compare(m_keyData) <= 0)
            return Exception { DataError, "Failed to execute 'continue' on 'IDBCursor': The parameter is less than or equal to this cursor's position."_s };
    } else {
        if (!key.isNull() && key.compare(m_keyData) >= 0)
            return Exception { DataError, "Failed to execute 'continue' on 'IDBCursor': The parameter is greater than or equal to this cursor's position."_s };
    }

    m_gotValue = false;

    uncheckedIterateCursor(key, 0);

    return { };
}

void IDBCursor::uncheckedIterateCursor(const IDBKeyData& key, unsigned count)
{
    ASSERT(m_request);
    ASSERT(&effectiveObjectStore().transaction().database().originThread() == &Thread::current());

    m_request->willIterateCursor(*this);
    transaction().iterateCursor(*this, { key, { }, count });
}

void IDBCursor::uncheckedIterateCursor(const IDBKeyData& key, const IDBKeyData& primaryKey)
{
    ASSERT(m_request);
    ASSERT(&effectiveObjectStore().transaction().database().originThread() == &Thread::current());

    m_request->willIterateCursor(*this);
    transaction().iterateCursor(*this, { key, primaryKey, 0 });
}

ExceptionOr<Ref<WebCore::IDBRequest>> IDBCursor::deleteFunction(ExecState& state)
{
    LOG(IndexedDB, "IDBCursor::deleteFunction");
    ASSERT(&effectiveObjectStore().transaction().database().originThread() == &Thread::current());

    if (sourcesDeleted())
        return Exception { InvalidStateError, "Failed to execute 'delete' on 'IDBCursor': The cursor's source or effective object store has been deleted."_s };

    if (!transaction().isActive())
        return Exception { TransactionInactiveError, "Failed to execute 'delete' on 'IDBCursor': The transaction is inactive or finished."_s };

    if (transaction().isReadOnly())
        return Exception { ReadonlyError, "Failed to execute 'delete' on 'IDBCursor': The record may not be deleted inside a read-only transaction."_s };

    if (!m_gotValue)
        return Exception { InvalidStateError, "Failed to execute 'delete' on 'IDBCursor': The cursor is being iterated or has iterated past its end."_s };

    if (!isKeyCursorWithValue())
        return Exception { InvalidStateError, "Failed to execute 'delete' on 'IDBCursor': The cursor is a key cursor."_s };

    auto result = effectiveObjectStore().deleteFunction(state, IDBKeyRange::create(m_primaryKey.copyRef()).ptr());
    if (result.hasException())
        return result.releaseException();

    auto request = result.releaseReturnValue();
    request->setSource(*this);

    return WTFMove(request);
}

void IDBCursor::setGetResult(IDBRequest&, const IDBGetResult& getResult)
{
    LOG(IndexedDB, "IDBCursor::setGetResult - current key %s", getResult.keyData().loggingString().substring(0, 100).utf8().data());
    ASSERT(&effectiveObjectStore().transaction().database().originThread() == &Thread::current());

    m_keyWrapper = { };
    m_primaryKeyWrapper = { };
    m_valueWrapper = { };

    if (!getResult.isDefined()) {
        m_keyData = { };
        m_key = nullptr;
        m_primaryKeyData = { };
        m_primaryKey = nullptr;
        m_value = { };

        m_gotValue = false;
        return;
    }

    m_keyData = getResult.keyData();
    m_key = m_keyData.maybeCreateIDBKey();
    m_primaryKeyData = getResult.primaryKeyData();
    m_primaryKey = m_primaryKeyData.maybeCreateIDBKey();

    if (isKeyCursorWithValue())
        m_value = getResult.value();

    m_gotValue = true;
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)

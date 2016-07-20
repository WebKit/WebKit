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

#include "ExceptionCode.h"
#include "IDBBindingUtilities.h"
#include "IDBDatabase.h"
#include "IDBDatabaseException.h"
#include "IDBGetResult.h"
#include "IDBIndex.h"
#include "IDBObjectStore.h"
#include "IDBRequest.h"
#include "IDBTransaction.h"
#include "Logging.h"
#include "ScriptExecutionContext.h"
#include <heap/StrongInlines.h>
#include <runtime/JSCJSValueInlines.h>
#include <wtf/NeverDestroyed.h>

using namespace JSC;

namespace WebCore {

const AtomicString& IDBCursor::directionNext()
{
    static NeverDestroyed<AtomicString> next("next", AtomicString::ConstructFromLiteral);
    return next;
}

const AtomicString& IDBCursor::directionNextUnique()
{
    static NeverDestroyed<AtomicString> nextunique("nextunique", AtomicString::ConstructFromLiteral);
    return nextunique;
}

const AtomicString& IDBCursor::directionPrev()
{
    static NeverDestroyed<AtomicString> prev("prev", AtomicString::ConstructFromLiteral);
    return prev;
}

const AtomicString& IDBCursor::directionPrevUnique()
{
    static NeverDestroyed<AtomicString> prevunique("prevunique", AtomicString::ConstructFromLiteral);
    return prevunique;
}

IndexedDB::CursorDirection IDBCursor::stringToDirection(const String& directionString, ExceptionCode& ec)
{
    if (directionString == directionNext())
        return IndexedDB::CursorDirection::Next;
    if (directionString == directionNextUnique())
        return IndexedDB::CursorDirection::NextNoDuplicate;
    if (directionString == directionPrev())
        return IndexedDB::CursorDirection::Prev;
    if (directionString == directionPrevUnique())
        return IndexedDB::CursorDirection::PrevNoDuplicate;

    ec = TypeError;
    return IndexedDB::CursorDirection::Next;
}

const AtomicString& IDBCursor::directionToString(IndexedDB::CursorDirection direction)
{
    switch (direction) {
    case IndexedDB::CursorDirection::Next:
        return directionNext();

    case IndexedDB::CursorDirection::NextNoDuplicate:
        return directionNextUnique();

    case IndexedDB::CursorDirection::Prev:
        return directionPrev();

    case IndexedDB::CursorDirection::PrevNoDuplicate:
        return directionPrevUnique();

    default:
        ASSERT_NOT_REACHED();
        return directionNext();
    }
}

Ref<IDBCursor> IDBCursor::create(IDBTransaction& transaction, IDBIndex& index, const IDBCursorInfo& info)
{
    return adoptRef(*new IDBCursor(transaction, index, info));
}

IDBCursor::IDBCursor(IDBTransaction& transaction, IDBObjectStore& objectStore, const IDBCursorInfo& info)
    : ActiveDOMObject(transaction.scriptExecutionContext())
    , m_info(info)
    , m_objectStore(&objectStore)
{
    ASSERT(currentThread() == effectiveObjectStore().transaction().database().originThreadID());

    suspendIfNeeded();
}

IDBCursor::IDBCursor(IDBTransaction& transaction, IDBIndex& index, const IDBCursorInfo& info)
    : ActiveDOMObject(transaction.scriptExecutionContext())
    , m_info(info)
    , m_index(&index)
{
    ASSERT(currentThread() == effectiveObjectStore().transaction().database().originThreadID());

    suspendIfNeeded();
}

IDBCursor::~IDBCursor()
{
    ASSERT(currentThread() == effectiveObjectStore().transaction().database().originThreadID());
}

bool IDBCursor::sourcesDeleted() const
{
    ASSERT(currentThread() == effectiveObjectStore().transaction().database().originThreadID());

    if (m_objectStore)
        return m_objectStore->isDeleted();

    ASSERT(m_index);
    return m_index->isDeleted() || m_index->objectStore().isDeleted();
}

IDBObjectStore& IDBCursor::effectiveObjectStore() const
{
    if (m_objectStore)
        return *m_objectStore;

    ASSERT(m_index);
    return m_index->objectStore();
}

IDBTransaction& IDBCursor::transaction() const
{
    ASSERT(currentThread() == effectiveObjectStore().transaction().database().originThreadID());
    return effectiveObjectStore().transaction();
}

const String& IDBCursor::direction() const
{
    ASSERT(currentThread() == effectiveObjectStore().transaction().database().originThreadID());
    return directionToString(m_info.cursorDirection());
}

RefPtr<WebCore::IDBRequest> IDBCursor::update(ExecState& exec, JSValue value, ExceptionCodeWithMessage& ec)
{
    LOG(IndexedDB, "IDBCursor::update");
    ASSERT(currentThread() == effectiveObjectStore().transaction().database().originThreadID());

    if (sourcesDeleted()) {
        ec.code = IDBDatabaseException::InvalidStateError;
        ec.message = ASCIILiteral("Failed to execute 'update' on 'IDBCursor': The cursor's source or effective object store has been deleted.");
        return nullptr;
    }

    if (!transaction().isActive()) {
        ec.code = IDBDatabaseException::TransactionInactiveError;
        ec.message = ASCIILiteral("Failed to execute 'update' on 'IDBCursor': The transaction is inactive or finished.");
        return nullptr;
    }

    if (transaction().isReadOnly()) {
        ec.code = IDBDatabaseException::ReadOnlyError;
        ec.message = ASCIILiteral("Failed to execute 'update' on 'IDBCursor': The record may not be updated inside a read-only transaction.");
        return nullptr;
    }

    if (!m_gotValue) {
        ec.code = IDBDatabaseException::InvalidStateError;
        ec.message = ASCIILiteral("Failed to execute 'update' on 'IDBCursor': The cursor is being iterated or has iterated past its end.");
        return nullptr;
    }

    if (!isKeyCursorWithValue()) {
        ec.code = IDBDatabaseException::InvalidStateError;
        ec.message = ASCIILiteral("Failed to execute 'update' on 'IDBCursor': The cursor is a key cursor.");
        return nullptr;
    }

    auto& objectStore = effectiveObjectStore();
    auto& keyPath = objectStore.info().keyPath();
    const bool usesInLineKeys = !keyPath.isNull();
    if (usesInLineKeys) {
        RefPtr<IDBKey> keyPathKey = maybeCreateIDBKeyFromScriptValueAndKeyPath(exec, value, keyPath);
        IDBKeyData keyPathKeyData(keyPathKey.get());
        if (!keyPathKey || keyPathKeyData != m_currentPrimaryKeyData) {
            ec.code = IDBDatabaseException::DataError;
            ec.message = ASCIILiteral("Failed to execute 'update' on 'IDBCursor': The effective object store of this cursor uses in-line keys and evaluating the key path of the value parameter results in a different value than the cursor's effective key.");
            return nullptr;
        }
    }

    auto request = effectiveObjectStore().putForCursorUpdate(exec, value, m_currentPrimaryKey.get(), ec);
    if (ec.code)
        return nullptr;

    ASSERT(request);
    request->setSource(*this);
    ++m_outstandingRequestCount;

    return request;
}

void IDBCursor::advance(unsigned count, ExceptionCodeWithMessage& ec)
{
    LOG(IndexedDB, "IDBCursor::advance");
    ASSERT(currentThread() == effectiveObjectStore().transaction().database().originThreadID());

    if (!m_request) {
        ec.code = IDBDatabaseException::InvalidStateError;
        return;
    }
    
    if (!count) {
        ec.code = TypeError;
        ec.message = ASCIILiteral("Failed to execute 'advance' on 'IDBCursor': A count argument with value 0 (zero) was supplied, must be greater than 0.");
        return;
    }

    if (sourcesDeleted()) {
        ec.code = IDBDatabaseException::InvalidStateError;
        ec.message = ASCIILiteral("Failed to execute 'advance' on 'IDBCursor': The cursor's source or effective object store has been deleted.");
        return;
    }

    if (!transaction().isActive()) {
        ec.code = IDBDatabaseException::TransactionInactiveError;
        ec.message = ASCIILiteral("Failed to execute 'advance' on 'IDBCursor': The transaction is inactive or finished.");
        return;
    }

    if (!m_gotValue) {
        ec.code = IDBDatabaseException::InvalidStateError;
        ec.message = ASCIILiteral("Failed to execute 'advance' on 'IDBCursor': The cursor is being iterated or has iterated past its end.");
        return;
    }

    m_gotValue = false;

    uncheckedIterateCursor(IDBKeyData(), count);
}

void IDBCursor::continueFunction(ExecState& execState, JSValue keyValue, ExceptionCodeWithMessage& ec)
{
    RefPtr<IDBKey> key;
    if (!keyValue.isUndefined())
        key = scriptValueToIDBKey(execState, keyValue);

    continueFunction(key.get(), ec);
}

void IDBCursor::continueFunction(const IDBKeyData& key, ExceptionCodeWithMessage& ec)
{
    LOG(IndexedDB, "IDBCursor::continueFunction (to key %s)", key.loggingString().utf8().data());
    ASSERT(currentThread() == effectiveObjectStore().transaction().database().originThreadID());

    if (!m_request) {
        ec.code = IDBDatabaseException::InvalidStateError;
        return;
    }

    if (sourcesDeleted()) {
        ec.code = IDBDatabaseException::InvalidStateError;
        ec.message = ASCIILiteral("Failed to execute 'continue' on 'IDBCursor': The cursor's source or effective object store has been deleted.");
        return;
    }

    if (!transaction().isActive()) {
        ec.code = IDBDatabaseException::TransactionInactiveError;
        ec.message = ASCIILiteral("Failed to execute 'continue' on 'IDBCursor': The transaction is inactive or finished.");
        return;
    }

    if (!m_gotValue) {
        ec.code = IDBDatabaseException::InvalidStateError;
        ec.message = ASCIILiteral("Failed to execute 'continue' on 'IDBCursor': The cursor is being iterated or has iterated past its end.");
        return;
    }

    if (!key.isNull() && !key.isValid()) {
        ec.code = IDBDatabaseException::DataError;
        ec.message = ASCIILiteral("Failed to execute 'continue' on 'IDBCursor': The parameter is not a valid key.");
        return;
    }

    if (m_info.isDirectionForward()) {
        if (!key.isNull() && key.compare(m_currentKeyData) <= 0) {
            ec.code = IDBDatabaseException::DataError;
            ec.message = ASCIILiteral("Failed to execute 'continue' on 'IDBCursor': The parameter is less than or equal to this cursor's position.");
            return;
        }
    } else if (!key.isNull() && key.compare(m_currentKeyData) >= 0) {
        ec.code = IDBDatabaseException::DataError;
        ec.message = ASCIILiteral("Failed to execute 'continue' on 'IDBCursor': The parameter is greater than or equal to this cursor's position.");
        return;
    }

    m_gotValue = false;

    uncheckedIterateCursor(key, 0);
}

void IDBCursor::uncheckedIterateCursor(const IDBKeyData& key, unsigned count)
{
    ASSERT(currentThread() == effectiveObjectStore().transaction().database().originThreadID());

    ++m_outstandingRequestCount;

    m_request->willIterateCursor(*this);
    transaction().iterateCursor(*this, key, count);
}

RefPtr<WebCore::IDBRequest> IDBCursor::deleteFunction(ExecState& execState, ExceptionCodeWithMessage& ec)
{
    LOG(IndexedDB, "IDBCursor::deleteFunction");
    ASSERT(currentThread() == effectiveObjectStore().transaction().database().originThreadID());

    if (sourcesDeleted()) {
        ec.code = IDBDatabaseException::InvalidStateError;
        ec.message = ASCIILiteral("Failed to execute 'delete' on 'IDBCursor': The cursor's source or effective object store has been deleted.");
        return nullptr;
    }

    if (!transaction().isActive()) {
        ec.code = IDBDatabaseException::TransactionInactiveError;
        ec.message = ASCIILiteral("Failed to execute 'delete' on 'IDBCursor': The transaction is inactive or finished.");
        return nullptr;
    }

    if (transaction().isReadOnly()) {
        ec.code = IDBDatabaseException::ReadOnlyError;
        ec.message = ASCIILiteral("Failed to execute 'delete' on 'IDBCursor': The record may not be deleted inside a read-only transaction.");
        return nullptr;
    }

    if (!m_gotValue) {
        ec.code = IDBDatabaseException::InvalidStateError;
        ec.message = ASCIILiteral("Failed to execute 'delete' on 'IDBCursor': The cursor is being iterated or has iterated past its end.");
        return nullptr;
    }

    if (!isKeyCursorWithValue()) {
        ec.code = IDBDatabaseException::InvalidStateError;
        ec.message = ASCIILiteral("Failed to execute 'delete' on 'IDBCursor': The cursor is a key cursor.");
        return nullptr;
    }

    auto request = effectiveObjectStore().deleteFunction(execState, m_currentPrimaryKey.get(), ec);
    if (ec.code)
        return nullptr;

    ASSERT(request);
    request->setSource(*this);
    ++m_outstandingRequestCount;

    return request;
}

void IDBCursor::setGetResult(IDBRequest& request, const IDBGetResult& getResult)
{
    LOG(IndexedDB, "IDBCursor::setGetResult - current key %s", getResult.keyData().loggingString().substring(0, 100).utf8().data());
    ASSERT(currentThread() == effectiveObjectStore().transaction().database().originThreadID());

    auto* context = request.scriptExecutionContext();
    if (!context)
        return;

    auto* exec = context->execState();
    if (!exec)
        return;

    if (!getResult.isDefined()) {
        m_currentKey = { };
        m_currentKeyData = { };
        m_currentPrimaryKey = { };
        m_currentPrimaryKeyData = { };
        m_currentValue = { };

        m_gotValue = false;
        return;
    }

    auto& vm = context->vm();

    m_currentKey = { vm, idbKeyDataToScriptValue(*exec, getResult.keyData()) };
    m_currentKeyData = getResult.keyData();
    m_currentPrimaryKey = { vm, idbKeyDataToScriptValue(*exec, getResult.primaryKeyData()) };
    m_currentPrimaryKeyData = getResult.primaryKeyData();

    if (isKeyCursorWithValue())
        m_currentValue = { vm, deserializeIDBValueToJSValue(*exec, getResult.value()) };
    else
        m_currentValue = { };

    m_gotValue = true;
}

const char* IDBCursor::activeDOMObjectName() const
{
    return "IDBCursor";
}

bool IDBCursor::canSuspendForDocumentSuspension() const
{
    return false;
}

bool IDBCursor::hasPendingActivity() const
{
    return m_outstandingRequestCount;
}

void IDBCursor::decrementOutstandingRequestCount()
{
    ASSERT(m_outstandingRequestCount);
    --m_outstandingRequestCount;
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)

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
#include "IDBCursorImpl.h"

#if ENABLE(INDEXED_DATABASE)

#include "DOMRequestState.h"
#include "ExceptionCode.h"
#include "IDBBindingUtilities.h"
#include "IDBDatabaseException.h"
#include "IDBGetResult.h"
#include "Logging.h"
#include "ScriptExecutionContext.h"

namespace WebCore {
namespace IDBClient {

Ref<IDBCursor> IDBCursor::create(IDBTransaction& transaction, IDBIndex& index, const IDBCursorInfo& info)
{
    return adoptRef(*new IDBCursor(transaction, index, info));
}

IDBCursor::IDBCursor(IDBTransaction&, IDBObjectStore& objectStore, const IDBCursorInfo& info)
    : m_info(info)
    , m_source(IDBAny::create(objectStore).leakRef())
    , m_objectStore(&objectStore)
{
}

IDBCursor::IDBCursor(IDBTransaction&, IDBIndex& index, const IDBCursorInfo& info)
    : m_info(info)
    , m_source(IDBAny::create(index).leakRef())
    , m_index(&index)
{
}

IDBCursor::~IDBCursor()
{
}

bool IDBCursor::sourcesDeleted() const
{
    if (m_objectStore)
        return m_objectStore->isDeleted();

    ASSERT(m_index);
    return m_index->isDeleted() || m_index->modernObjectStore().isDeleted();
}

IDBObjectStore& IDBCursor::effectiveObjectStore() const
{
    if (m_objectStore)
        return *m_objectStore;

    ASSERT(m_index);
    return m_index->modernObjectStore();
}

IDBTransaction& IDBCursor::transaction() const
{
    return effectiveObjectStore().modernTransaction();
}

const String& IDBCursor::direction() const
{
    return IDBCursor::directionToString(m_info.cursorDirection());
}

const Deprecated::ScriptValue& IDBCursor::key() const
{
    return m_deprecatedCurrentKey;
}

const Deprecated::ScriptValue& IDBCursor::primaryKey() const
{
    return m_deprecatedCurrentPrimaryKey;
}

const Deprecated::ScriptValue& IDBCursor::value() const
{
    return m_deprecatedCurrentValue;
}

IDBAny* IDBCursor::source()
{
    return &m_source.get();
}

RefPtr<WebCore::IDBRequest> IDBCursor::update(JSC::ExecState& exec, Deprecated::ScriptValue& value, ExceptionCode& ec)
{
    LOG(IndexedDB, "IDBCursor::advance");

    if (sourcesDeleted()) {
        ec = IDBDatabaseException::InvalidStateError;
        return nullptr;
    }

    if (!transaction().isActive()) {
        ec = IDBDatabaseException::TransactionInactiveError;
        return nullptr;
    }

    if (transaction().isReadOnly()) {
        ec = IDBDatabaseException::ReadOnlyError;
        return nullptr;
    }

    if (!m_gotValue) {
        ec = IDBDatabaseException::InvalidStateError;
        return nullptr;
    }

    if (isKeyCursor()) {
        ec = IDBDatabaseException::InvalidStateError;
        return nullptr;
    }

    return effectiveObjectStore().put(exec, value.jsValue(), m_deprecatedCurrentPrimaryKey.jsValue(), ec);
}

void IDBCursor::advance(unsigned long count, ExceptionCode& ec)
{
    LOG(IndexedDB, "IDBCursor::advance");

    if (!m_request) {
        ec = IDBDatabaseException::InvalidStateError;
        return;
    }
    
    if (!count) {
        ec = TypeError;
        return;
    }

    if (sourcesDeleted()) {
        ec = IDBDatabaseException::InvalidStateError;
        return;
    }

    if (!transaction().isActive()) {
        ec = IDBDatabaseException::TransactionInactiveError;
        return;
    }

    if (!m_gotValue) {
        ec = IDBDatabaseException::InvalidStateError;
        return;
    }

    uncheckedIteratorCursor(IDBKeyData(), count);
}

void IDBCursor::continueFunction(ScriptExecutionContext* context, ExceptionCode& ec)
{
    if (!context) {
        ec = IDBDatabaseException::InvalidStateError;
        return;
    }

    continueFunction(IDBKeyData(), ec);
}

void IDBCursor::continueFunction(ScriptExecutionContext* context, const Deprecated::ScriptValue& keyValue, ExceptionCode& ec)
{
    if (!context) {
        ec = IDBDatabaseException::InvalidStateError;
        return;
    }

    DOMRequestState requestState(context);
    RefPtr<IDBKey> key = scriptValueToIDBKey(&requestState, keyValue);
    continueFunction(key.get(), ec);
}

void IDBCursor::continueFunction(const IDBKeyData& key, ExceptionCode& ec)
{
    LOG(IndexedDB, "IDBCursor::continueFunction");

    if (!m_request) {
        ec = IDBDatabaseException::InvalidStateError;
        return;
    }

    if (sourcesDeleted()) {
        ec = IDBDatabaseException::InvalidStateError;
        return;
    }

    if (!transaction().isActive()) {
        ec = IDBDatabaseException::TransactionInactiveError;
        return;
    }

    if (!m_gotValue) {
        ec = IDBDatabaseException::InvalidStateError;
        return;
    }

    if (!key.isValid()) {
        ec = IDBDatabaseException::DataError;
        return;
    }

    if (m_info.isDirectionForward()) {
        if (key.compare(m_currentPrimaryKeyData) <= 0) {
            ec = IDBDatabaseException::DataError;
            return;
        }
    } else if (key.compare(m_currentPrimaryKeyData) >= 0) {
        ec = IDBDatabaseException::DataError;
        return;
    }

    uncheckedIteratorCursor(key, 0);
}

void IDBCursor::uncheckedIteratorCursor(const IDBKeyData& key, unsigned long count)
{
    m_request->willIterateCursor(*this);
    transaction().iterateCursor(*this, key, count);
}

RefPtr<WebCore::IDBRequest> IDBCursor::deleteFunction(ScriptExecutionContext* context, ExceptionCode& ec)
{
    LOG(IndexedDB, "IDBCursor::deleteFunction");

    if (!context) {
        ec = IDBDatabaseException::InvalidStateError;
        return nullptr;
    }

    if (sourcesDeleted()) {
        ec = IDBDatabaseException::InvalidStateError;
        return nullptr;
    }

    if (!transaction().isActive()) {
        ec = IDBDatabaseException::TransactionInactiveError;
        return nullptr;
    }

    if (transaction().isReadOnly()) {
        ec = IDBDatabaseException::ReadOnlyError;
        return nullptr;
    }

    if (!m_gotValue) {
        ec = IDBDatabaseException::InvalidStateError;
        return nullptr;
    }

    return effectiveObjectStore().deleteFunction(context, m_deprecatedCurrentPrimaryKey.jsValue(), ec);
}

void IDBCursor::setGetResult(const IDBGetResult&)
{
    LOG(IndexedDB, "IDBCursor::setGetResult");

    // FIXME: Implement.

    m_gotValue = true;
}

} // namespace IDBClient
} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)

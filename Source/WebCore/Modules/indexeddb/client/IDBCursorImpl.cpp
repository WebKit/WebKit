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

RefPtr<WebCore::IDBRequest> IDBCursor::update(JSC::ExecState& exec, Deprecated::ScriptValue& value, ExceptionCodeWithMessage& ec)
{
    LOG(IndexedDB, "IDBCursor::update");

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

    if (isKeyCursor()) {
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

    auto request = effectiveObjectStore().putForCursorUpdate(exec, value.jsValue(), m_deprecatedCurrentPrimaryKey.jsValue(), ec);
    if (ec.code)
        return nullptr;

    ASSERT(request);
    request->setSource(*this);
    return request;
}

void IDBCursor::advance(unsigned long count, ExceptionCodeWithMessage& ec)
{
    LOG(IndexedDB, "IDBCursor::advance");

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

void IDBCursor::continueFunction(ScriptExecutionContext* context, ExceptionCodeWithMessage& ec)
{
    if (!context) {
        ec.code = IDBDatabaseException::InvalidStateError;
        return;
    }

    continueFunction(IDBKeyData(), ec);
}

void IDBCursor::continueFunction(ScriptExecutionContext* context, const Deprecated::ScriptValue& keyValue, ExceptionCodeWithMessage& ec)
{
    if (!context) {
        ec.code = IDBDatabaseException::InvalidStateError;
        return;
    }

    DOMRequestState requestState(context);
    RefPtr<IDBKey> key;
    if (!keyValue.jsValue().isUndefined())
        key = scriptValueToIDBKey(&requestState, keyValue);

    continueFunction(key.get(), ec);
}

void IDBCursor::continueFunction(const IDBKeyData& key, ExceptionCodeWithMessage& ec)
{
    LOG(IndexedDB, "IDBCursor::continueFunction (to key %s)", key.loggingString().utf8().data());

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

void IDBCursor::uncheckedIterateCursor(const IDBKeyData& key, unsigned long count)
{
    m_request->willIterateCursor(*this);
    transaction().iterateCursor(*this, key, count);
}

RefPtr<WebCore::IDBRequest> IDBCursor::deleteFunction(ScriptExecutionContext* context, ExceptionCodeWithMessage& ec)
{
    LOG(IndexedDB, "IDBCursor::deleteFunction");

    if (!context) {
        ec.code = IDBDatabaseException::InvalidStateError;
        return nullptr;
    }

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

    if (isKeyCursor()) {
        ec.code = IDBDatabaseException::InvalidStateError;
        ec.message = ASCIILiteral("Failed to execute 'delete' on 'IDBCursor': The cursor is a key cursor.");
        return nullptr;
    }

    return effectiveObjectStore().deleteFunction(context, m_deprecatedCurrentPrimaryKey.jsValue(), ec);
}

void IDBCursor::setGetResult(IDBRequest& request, const IDBGetResult& getResult)
{
    LOG(IndexedDB, "IDBCursor::setGetResult - current key %s", getResult.keyData().loggingString().substring(0, 100).utf8().data());

    auto* context = request.scriptExecutionContext();
    if (!context)
        return;

    if (!getResult.isDefined()) {
        m_deprecatedCurrentKey = { };
        m_currentKeyData = { };
        m_deprecatedCurrentPrimaryKey = { };
        m_currentPrimaryKeyData = { };
        m_deprecatedCurrentValue = { };

        m_gotValue = false;
        return;
    }

    m_deprecatedCurrentKey = idbKeyDataToScriptValue(context, getResult.keyData());
    m_currentKeyData = getResult.keyData();
    m_deprecatedCurrentPrimaryKey = idbKeyDataToScriptValue(context, getResult.primaryKeyData());
    m_currentPrimaryKeyData = getResult.primaryKeyData();

    if (isKeyCursor())
        m_deprecatedCurrentValue = { };
    else
        m_deprecatedCurrentValue = deserializeIDBValueData(*context, getResult.valueBuffer());

    m_gotValue = true;
}

} // namespace IDBClient
} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)

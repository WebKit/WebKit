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
#include "LegacyCursor.h"

#if ENABLE(INDEXED_DATABASE)

#include "IDBAny.h"
#include "IDBBindingUtilities.h"
#include "IDBCallbacks.h"
#include "IDBCursorBackend.h"
#include "IDBKey.h"
#include "IDBObjectStore.h"
#include "IDBTransaction.h"
#include "LegacyRequest.h"
#include "Logging.h"
#include "ScriptExecutionContext.h"
#include <inspector/ScriptCallStack.h>
#include <limits>

namespace WebCore {

Ref<LegacyCursor> LegacyCursor::create(PassRefPtr<IDBCursorBackend> backend, IndexedDB::CursorDirection direction, LegacyRequest* request, LegacyAny* source, LegacyTransaction* transaction)
{
    return adoptRef(*new LegacyCursor(backend, direction, request, source, transaction));
}

LegacyCursor::LegacyCursor(PassRefPtr<IDBCursorBackend> backend, IndexedDB::CursorDirection direction, LegacyRequest* request, LegacyAny* source, LegacyTransaction* transaction)
    : m_backend(backend)
    , m_request(request)
    , m_direction(direction)
    , m_source(source)
    , m_transaction(transaction)
    , m_transactionNotifier(transaction, this)
    , m_gotValue(false)
{
    ASSERT(m_backend);
    ASSERT(m_request);
    ASSERT(m_source->type() == IDBAny::Type::IDBObjectStore || m_source->type() == IDBAny::Type::IDBIndex);
    ASSERT(m_transaction);
}

LegacyCursor::~LegacyCursor()
{
}

const String& LegacyCursor::direction() const
{
    LOG(StorageAPI, "LegacyCursor::direction");
    return directionToString(m_direction);
}

const Deprecated::ScriptValue& LegacyCursor::key() const
{
    LOG(StorageAPI, "LegacyCursor::key");
    return m_currentKeyValue;
}

const Deprecated::ScriptValue& LegacyCursor::primaryKey() const
{
    LOG(StorageAPI, "LegacyCursor::primaryKey");
    return m_currentPrimaryKeyValue;
}

const Deprecated::ScriptValue& LegacyCursor::value() const
{
    LOG(StorageAPI, "LegacyCursor::value");
    return m_currentValue;
}

IDBAny* LegacyCursor::source()
{
    return m_source.get();
}

RefPtr<IDBRequest> LegacyCursor::update(JSC::ExecState& state, Deprecated::ScriptValue& value, ExceptionCodeWithMessage& ec)
{
    LOG(StorageAPI, "LegacyCursor::update");

    if (!m_gotValue || isKeyCursor()) {
        ec.code = IDBDatabaseException::InvalidStateError;
        return 0;
    }
    if (!m_transaction->isActive()) {
        ec.code = IDBDatabaseException::TransactionInactiveError;
        return 0;
    }
    if (m_transaction->isReadOnly()) {
        ec.code = IDBDatabaseException::ReadOnlyError;
        return 0;
    }

    RefPtr<LegacyObjectStore> objectStore = effectiveObjectStore();
    const IDBKeyPath& keyPath = objectStore->metadata().keyPath;
    const bool usesInLineKeys = !keyPath.isNull();
    if (usesInLineKeys) {
        RefPtr<IDBKey> keyPathKey = createIDBKeyFromScriptValueAndKeyPath(m_request->requestState()->exec(), value, keyPath);
        if (!keyPathKey || !keyPathKey->isEqual(m_currentPrimaryKey.get())) {
            ec.code = IDBDatabaseException::DataError;
            return 0;
        }
    }

    return objectStore->put(IDBDatabaseBackend::CursorUpdate, LegacyAny::create(this), state, value, m_currentPrimaryKey, ec);
}

void LegacyCursor::advance(unsigned long count, ExceptionCodeWithMessage& ec)
{
    ec.code = 0;
    LOG(StorageAPI, "LegacyCursor::advance");
    if (!m_gotValue) {
        ec.code = IDBDatabaseException::InvalidStateError;
        return;
    }

    if (!m_transaction->isActive()) {
        ec.code = IDBDatabaseException::TransactionInactiveError;
        return;
    }

    if (!count) {
        ec.code = TypeError;
        return;
    }

    m_request->setPendingCursor(this);
    m_gotValue = false;
    m_backend->advance(count, m_request, ec.code);
    ASSERT(!ec.code);
}

void LegacyCursor::continueFunction(ScriptExecutionContext* context, const Deprecated::ScriptValue& keyValue, ExceptionCodeWithMessage& ec)
{
    DOMRequestState requestState(context);
    RefPtr<IDBKey> key = scriptValueToIDBKey(&requestState, keyValue);
    continueFunction(key.release(), ec);
}

void LegacyCursor::continueFunction(PassRefPtr<IDBKey> key, ExceptionCodeWithMessage& ec)
{
    ec.code = 0;
    LOG(StorageAPI, "LegacyCursor::continue");
    if (key && !key->isValid()) {
        ec.code = IDBDatabaseException::DataError;
        return;
    }

    if (!m_transaction->isActive()) {
        ec.code = IDBDatabaseException::TransactionInactiveError;
        return;
    }

    if (!m_gotValue) {
        ec.code = IDBDatabaseException::InvalidStateError;
        return;
    }

    if (key) {
        ASSERT(m_currentKey);
        if (m_direction == IndexedDB::CursorDirection::Next || m_direction == IndexedDB::CursorDirection::NextNoDuplicate) {
            if (!m_currentKey->isLessThan(key.get())) {
                ec.code = IDBDatabaseException::DataError;
                return;
            }
        } else {
            if (!key->isLessThan(m_currentKey.get())) {
                ec.code = IDBDatabaseException::DataError;
                return;
            }
        }
    }

    // FIXME: We're not using the context from when continue was called, which means the callback
    //        will be on the original context openCursor was called on. Is this right?
    m_request->setPendingCursor(this);
    m_gotValue = false;
    m_backend->continueFunction(key, m_request, ec.code);
    ASSERT(!ec.code);
}

RefPtr<IDBRequest> LegacyCursor::deleteFunction(ScriptExecutionContext* context, ExceptionCodeWithMessage& ec)
{
    ec.code = 0;
    LOG(StorageAPI, "LegacyCursor::delete");
    if (!m_transaction->isActive()) {
        ec.code = IDBDatabaseException::TransactionInactiveError;
        return 0;
    }
    if (m_transaction->isReadOnly()) {
        ec.code = IDBDatabaseException::ReadOnlyError;
        return 0;
    }

    if (!m_gotValue || isKeyCursor()) {
        ec.code = IDBDatabaseException::InvalidStateError;
        return 0;
    }
    RefPtr<LegacyRequest> request = LegacyRequest::create(context, LegacyAny::create(this), m_transaction.get());
    m_backend->deleteFunction(request, ec.code);
    ASSERT(!ec.code);
    return request.release();
}

void LegacyCursor::postSuccessHandlerCallback()
{
    m_backend->postSuccessHandlerCallback();
}

void LegacyCursor::close()
{
    m_transactionNotifier.cursorFinished();
    if (m_request) {
        m_request->finishCursor();
        m_request = nullptr;
    }
}

void LegacyCursor::setValueReady(DOMRequestState* state, PassRefPtr<IDBKey> key, PassRefPtr<IDBKey> primaryKey, Deprecated::ScriptValue& value)
{
    m_currentKey = key;
    m_currentKeyValue = idbKeyToScriptValue(state, m_currentKey);

    m_currentPrimaryKey = primaryKey;
    m_currentPrimaryKeyValue = idbKeyToScriptValue(state, m_currentPrimaryKey);

    if (!isKeyCursor()) {
        RefPtr<LegacyObjectStore> objectStore = effectiveObjectStore();
        const IDBObjectStoreMetadata metadata = objectStore->metadata();
        if (metadata.autoIncrement && !metadata.keyPath.isNull()) {
#ifndef NDEBUG
            RefPtr<IDBKey> expectedKey = createIDBKeyFromScriptValueAndKeyPath(m_request->requestState()->exec(), value, metadata.keyPath);
            ASSERT(!expectedKey || expectedKey->isEqual(m_currentPrimaryKey.get()));
#endif
            bool injected = injectIDBKeyIntoScriptValue(m_request->requestState(), m_currentPrimaryKey, value, metadata.keyPath);
            // FIXME: There is no way to report errors here. Move this into onSuccessWithContinuation so that we can abort the transaction there. See: https://bugs.webkit.org/show_bug.cgi?id=92278
            ASSERT_UNUSED(injected, injected);
        }
    }
    m_currentValue = value;

    m_gotValue = true;
}

PassRefPtr<LegacyObjectStore> LegacyCursor::effectiveObjectStore()
{
    if (m_source->type() == IDBAny::Type::IDBObjectStore)
        return m_source->legacyObjectStore();
    RefPtr<LegacyIndex> index = m_source->legacyIndex();
    return index->legacyObjectStore();
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)

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
#include "IDBObjectStoreImpl.h"

#if ENABLE(INDEXED_DATABASE)

#include "DOMRequestState.h"
#include "IDBBindingUtilities.h"
#include "IDBError.h"
#include "IDBKey.h"
#include "IDBRequestImpl.h"
#include "IDBTransactionImpl.h"
#include "IndexedDB.h"
#include "Logging.h"
#include "SerializedScriptValue.h"

namespace WebCore {
namespace IDBClient {

Ref<IDBObjectStore> IDBObjectStore::create(const IDBObjectStoreInfo& info, IDBTransaction& transaction)
{
    return adoptRef(*new IDBObjectStore(info, transaction));
}

IDBObjectStore::IDBObjectStore(const IDBObjectStoreInfo& info, IDBTransaction& transaction)
    : m_info(info)
    , m_transaction(transaction)
{
}

IDBObjectStore::~IDBObjectStore()
{
}

int64_t IDBObjectStore::id() const
{
    RELEASE_ASSERT_NOT_REACHED();
}

const String IDBObjectStore::name() const
{
    RELEASE_ASSERT_NOT_REACHED();
}

RefPtr<WebCore::IDBAny> IDBObjectStore::keyPathAny() const
{
    RELEASE_ASSERT_NOT_REACHED();
}

const IDBKeyPath IDBObjectStore::keyPath() const
{
    RELEASE_ASSERT_NOT_REACHED();
}

RefPtr<DOMStringList> IDBObjectStore::indexNames() const
{
    RELEASE_ASSERT_NOT_REACHED();
}

RefPtr<WebCore::IDBTransaction> IDBObjectStore::transaction() const
{
    RELEASE_ASSERT_NOT_REACHED();
}

bool IDBObjectStore::autoIncrement() const
{
    return m_info.autoIncrement();
}

RefPtr<WebCore::IDBRequest> IDBObjectStore::add(JSC::ExecState&, Deprecated::ScriptValue&, ExceptionCode&)
{
    RELEASE_ASSERT_NOT_REACHED();
}

RefPtr<WebCore::IDBRequest> IDBObjectStore::put(JSC::ExecState& state, Deprecated::ScriptValue& value, ExceptionCode& ec)
{
    return putOrAdd(state, value, nullptr, IndexedDB::ObjectStoreOverwriteMode::Overwrite, ec);
}

RefPtr<WebCore::IDBRequest> IDBObjectStore::openCursor(ScriptExecutionContext*, ExceptionCode&)
{
    RELEASE_ASSERT_NOT_REACHED();
}

RefPtr<WebCore::IDBRequest> IDBObjectStore::openCursor(ScriptExecutionContext*, IDBKeyRange*, ExceptionCode&)
{
    RELEASE_ASSERT_NOT_REACHED();
}

RefPtr<WebCore::IDBRequest> IDBObjectStore::openCursor(ScriptExecutionContext*, const Deprecated::ScriptValue&, ExceptionCode&)
{
    RELEASE_ASSERT_NOT_REACHED();
}

RefPtr<WebCore::IDBRequest> IDBObjectStore::openCursor(ScriptExecutionContext*, IDBKeyRange*, const String&, ExceptionCode&)
{
    RELEASE_ASSERT_NOT_REACHED();
}

RefPtr<WebCore::IDBRequest> IDBObjectStore::openCursor(ScriptExecutionContext*, const Deprecated::ScriptValue&, const String&, ExceptionCode&)
{
    RELEASE_ASSERT_NOT_REACHED();
}

RefPtr<WebCore::IDBRequest> IDBObjectStore::get(ScriptExecutionContext* context, const Deprecated::ScriptValue& key, ExceptionCode& ec)
{
    LOG(IndexedDB, "IDBObjectStore::get");

    if (!context) {
        ec = INVALID_STATE_ERR;
        return nullptr;
    }

    if (!m_transaction->isActive()) {
        ec = static_cast<ExceptionCode>(IDBExceptionCode::TransactionInactiveError);
        return nullptr;
    }

    if (m_deleted) {
        ec = INVALID_STATE_ERR;
        return nullptr;
    }

    DOMRequestState requestState(context);
    RefPtr<IDBKey> idbKey = scriptValueToIDBKey(&requestState, key);
    if (!idbKey || idbKey->type() == KeyType::Invalid) {
        ec = static_cast<ExceptionCode>(IDBExceptionCode::DataError);
        return nullptr;
    }

    Ref<IDBRequest> request = m_transaction->requestGetRecord(*context, *this, *idbKey);
    return adoptRef(request.leakRef());
}

RefPtr<WebCore::IDBRequest> IDBObjectStore::get(ScriptExecutionContext*, IDBKeyRange*, ExceptionCode&)
{
    RELEASE_ASSERT_NOT_REACHED();
}

RefPtr<WebCore::IDBRequest> IDBObjectStore::add(JSC::ExecState&, Deprecated::ScriptValue&, const Deprecated::ScriptValue&, ExceptionCode&)
{
    RELEASE_ASSERT_NOT_REACHED();
}

RefPtr<WebCore::IDBRequest> IDBObjectStore::put(JSC::ExecState& execState, Deprecated::ScriptValue& value, const Deprecated::ScriptValue& key, ExceptionCode& ec)
{
    auto idbKey = scriptValueToIDBKey(execState, key);
    return putOrAdd(execState, value, idbKey, IndexedDB::ObjectStoreOverwriteMode::Overwrite, ec);
}

RefPtr<WebCore::IDBRequest> IDBObjectStore::putOrAdd(JSC::ExecState& state, Deprecated::ScriptValue& value, RefPtr<IDBKey> key, IndexedDB::ObjectStoreOverwriteMode overwriteMode, ExceptionCode& ec)
{
    LOG(IndexedDB, "IDBObjectStore::putOrAdd");

    if (m_transaction->isReadOnly()) {
        ec = static_cast<ExceptionCode>(IDBExceptionCode::ReadOnlyError);
        return nullptr;
    }

    if (!m_transaction->isActive()) {
        ec = static_cast<ExceptionCode>(IDBExceptionCode::TransactionInactiveError);
        return nullptr;
    }

    if (m_deleted) {
        ec = INVALID_STATE_ERR;
        return nullptr;
    }

    RefPtr<SerializedScriptValue> serializedValue = SerializedScriptValue::create(&state, value.jsValue(), nullptr, nullptr);
    if (state.hadException()) {
        ec = DATA_CLONE_ERR;
        return nullptr;
    }

    if (serializedValue->hasBlobURLs()) {
        // FIXME: Add Blob/File/FileList support
        ec = DATA_CLONE_ERR;
        return nullptr;
    }

    if (key && key->type() == KeyType::Invalid) {
        ec = static_cast<ExceptionCode>(IDBExceptionCode::DataError);
        return nullptr;
    }

    bool usesInlineKeys = !m_info.keyPath().isNull();
    bool usesKeyGenerator = autoIncrement();
    if (usesInlineKeys) {
        if (key) {
            ec = static_cast<ExceptionCode>(IDBExceptionCode::DataError);
            return nullptr;
        }

        RefPtr<IDBKey> keyPathKey = maybeCreateIDBKeyFromScriptValueAndKeyPath(state, value, m_info.keyPath());
        if (keyPathKey && !keyPathKey->isValid()) {
            ec = static_cast<ExceptionCode>(IDBExceptionCode::DataError);
            return nullptr;
        }

        if (!keyPathKey) {
            if (usesKeyGenerator) {
                if (!canInjectIDBKeyIntoScriptValue(state, value, m_info.keyPath())) {
                    ec = static_cast<ExceptionCode>(IDBExceptionCode::DataError);
                    return nullptr;
                }
            } else {
                ec = static_cast<ExceptionCode>(IDBExceptionCode::DataError);
                return nullptr;
            }
        }

        if (keyPathKey) {
            ASSERT(!key);
            key = keyPathKey;
        }
    } else if (!usesKeyGenerator && !key) {
        ec = static_cast<ExceptionCode>(IDBExceptionCode::DataError);
        return nullptr;
    }

    auto context = scriptExecutionContextFromExecState(&state);
    if (!context) {
        ec = static_cast<ExceptionCode>(IDBExceptionCode::Unknown);
        return nullptr;
    }

    Ref<IDBRequest> request = m_transaction->requestPutOrAdd(*context, *this, key.get(), *serializedValue, overwriteMode);
    return adoptRef(request.leakRef());
}


RefPtr<WebCore::IDBRequest> IDBObjectStore::deleteFunction(ScriptExecutionContext*, IDBKeyRange*, ExceptionCode&)
{
    RELEASE_ASSERT_NOT_REACHED();
}

RefPtr<WebCore::IDBRequest> IDBObjectStore::deleteFunction(ScriptExecutionContext*, const Deprecated::ScriptValue&, ExceptionCode&)
{
    RELEASE_ASSERT_NOT_REACHED();
}

RefPtr<WebCore::IDBRequest> IDBObjectStore::clear(ScriptExecutionContext*, ExceptionCode&)
{
    RELEASE_ASSERT_NOT_REACHED();
}

RefPtr<WebCore::IDBIndex> IDBObjectStore::createIndex(ScriptExecutionContext*, const String&, const IDBKeyPath&, bool, bool, ExceptionCode&)
{
    RELEASE_ASSERT_NOT_REACHED();
}

RefPtr<WebCore::IDBIndex> IDBObjectStore::index(const String&, ExceptionCode&)
{
    RELEASE_ASSERT_NOT_REACHED();
}

void IDBObjectStore::deleteIndex(const String&, ExceptionCode&)
{
    RELEASE_ASSERT_NOT_REACHED();
}

RefPtr<WebCore::IDBRequest> IDBObjectStore::count(ScriptExecutionContext*, ExceptionCode&)
{
    RELEASE_ASSERT_NOT_REACHED();
}

RefPtr<WebCore::IDBRequest> IDBObjectStore::count(ScriptExecutionContext*, IDBKeyRange*, ExceptionCode&)
{
    RELEASE_ASSERT_NOT_REACHED();
}

RefPtr<WebCore::IDBRequest> IDBObjectStore::count(ScriptExecutionContext*, const Deprecated::ScriptValue&, ExceptionCode&)
{
    RELEASE_ASSERT_NOT_REACHED();
}

} // namespace IDBClient
} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)

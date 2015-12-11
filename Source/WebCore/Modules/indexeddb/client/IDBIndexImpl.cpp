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
#include "IDBIndexImpl.h"

#if ENABLE(INDEXED_DATABASE)

#include "DOMRequestState.h"
#include "IDBAnyImpl.h"
#include "IDBBindingUtilities.h"
#include "IDBCursorImpl.h"
#include "IDBDatabaseException.h"
#include "IDBKeyRangeData.h"
#include "IDBObjectStoreImpl.h"
#include "IDBTransactionImpl.h"
#include "Logging.h"

namespace WebCore {
namespace IDBClient {

Ref<IDBIndex> IDBIndex::create(const IDBIndexInfo& info, IDBObjectStore& objectStore)
{
    return adoptRef(*new IDBIndex(info, objectStore));
}

IDBIndex::IDBIndex(const IDBIndexInfo& info, IDBObjectStore& objectStore)
    : m_info(info)
    , m_objectStore(objectStore)
{
}

IDBIndex::~IDBIndex()
{
}

const String& IDBIndex::name() const
{
    return m_info.name();
}

RefPtr<WebCore::IDBObjectStore> IDBIndex::objectStore()
{
    return &m_objectStore.get();
}

RefPtr<WebCore::IDBAny> IDBIndex::keyPathAny() const
{
    return IDBAny::create(m_info.keyPath());
}

const IDBKeyPath& IDBIndex::keyPath() const
{
    return m_info.keyPath();
}

bool IDBIndex::unique() const
{
    return m_info.unique();
}

bool IDBIndex::multiEntry() const
{
    return m_info.multiEntry();
}

RefPtr<WebCore::IDBRequest> IDBIndex::openCursor(ScriptExecutionContext* context, IDBKeyRange* range, const String& directionString, ExceptionCodeWithMessage& ec)
{
    LOG(IndexedDB, "IDBIndex::openCursor");

    if (m_deleted || m_objectStore->isDeleted()) {
        ec.code = IDBDatabaseException::InvalidStateError;
        return nullptr;
    }

    if (!m_objectStore->modernTransaction().isActive()) {
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

    auto info = IDBCursorInfo::indexCursor(m_objectStore->modernTransaction(), m_objectStore->info().identifier(), m_info.identifier(), rangeData, direction, IndexedDB::CursorType::KeyAndValue);
    Ref<IDBRequest> request = m_objectStore->modernTransaction().requestOpenCursor(*context, *this, info);
    return WTF::move(request);
}

RefPtr<WebCore::IDBRequest> IDBIndex::openCursor(ScriptExecutionContext* context, const Deprecated::ScriptValue& key, const String& direction, ExceptionCodeWithMessage& ec)
{
    LOG(IndexedDB, "IDBIndex::openCursor");
    RefPtr<IDBKeyRange> keyRange = IDBKeyRange::only(context, key, ec.code);
    if (ec.code) {
        ec.message = ASCIILiteral("Failed to execute 'openCursor' on 'IDBIndex': The parameter is not a valid key.");
        return nullptr;
    }

    return openCursor(context, keyRange.get(), direction, ec);
}

RefPtr<WebCore::IDBRequest> IDBIndex::count(ScriptExecutionContext* context, ExceptionCodeWithMessage& ec)
{
    LOG(IndexedDB, "IDBIndex::count");

    if (!context) {
        ec.code = IDBDatabaseException::InvalidStateError;
        return nullptr;
    }

    return doCount(*context, IDBKeyRangeData::allKeys(), ec);}

RefPtr<WebCore::IDBRequest> IDBIndex::count(ScriptExecutionContext* context, IDBKeyRange* range, ExceptionCodeWithMessage& ec)
{
    LOG(IndexedDB, "IDBIndex::count");

    if (!context) {
        ec.code = IDBDatabaseException::InvalidStateError;
        return nullptr;
    }

    return doCount(*context, range ? IDBKeyRangeData(range) : IDBKeyRangeData::allKeys(), ec);
}

RefPtr<WebCore::IDBRequest> IDBIndex::count(ScriptExecutionContext* context, const Deprecated::ScriptValue& key, ExceptionCodeWithMessage& ec)
{
    LOG(IndexedDB, "IDBIndex::count");

    if (!context) {
        ec.code = IDBDatabaseException::InvalidStateError;
        return nullptr;
    }

    DOMRequestState requestState(context);
    RefPtr<IDBKey> idbKey = scriptValueToIDBKey(&requestState, key);
    if (!idbKey || idbKey->type() == KeyType::Invalid) {
        ec.code = IDBDatabaseException::DataError;
        ec.message = ASCIILiteral("Failed to execute 'count' on 'IDBIndex': The parameter is not a valid key.");
        return nullptr;
    }

    return doCount(*context, IDBKeyRangeData(idbKey.get()), ec);
}

RefPtr<WebCore::IDBRequest> IDBIndex::doCount(ScriptExecutionContext& context, const IDBKeyRangeData& range, ExceptionCodeWithMessage& ec)
{
    if (m_deleted || m_objectStore->isDeleted()) {
        ec.code = IDBDatabaseException::InvalidStateError;
        return nullptr;
    }

    if (!range.isValid()) {
        ec.code = IDBDatabaseException::DataError;
        return nullptr;
    }

    auto& transaction = m_objectStore->modernTransaction();
    if (!transaction.isActive()) {
        ec.code = IDBDatabaseException::TransactionInactiveError;
        ec.message = ASCIILiteral("Failed to execute 'count' on 'IDBIndex': The transaction is inactive or finished.");
        return nullptr;
    }

    return transaction.requestCount(context, *this, range);
}

RefPtr<WebCore::IDBRequest> IDBIndex::openKeyCursor(ScriptExecutionContext* context, IDBKeyRange* range, const String& directionString, ExceptionCodeWithMessage& ec)
{
    LOG(IndexedDB, "IDBIndex::openKeyCursor");

    if (m_deleted || m_objectStore->isDeleted()) {
        ec.code = IDBDatabaseException::InvalidStateError;
        return nullptr;
    }

    if (!m_objectStore->modernTransaction().isActive()) {
        ec.code = IDBDatabaseException::TransactionInactiveError;
        ec.message = ASCIILiteral("Failed to execute 'openKeyCursor' on 'IDBIndex': The transaction is inactive or finished.");
        return nullptr;
    }

    IndexedDB::CursorDirection direction = IDBCursor::stringToDirection(directionString, ec.code);
    if (ec.code) {
        ec.message = ASCIILiteral("Failed to execute 'openKeyCursor' on 'IDBIndex': The direction provided ('invalid-direction') is not one of 'next', 'nextunique', 'prev', or 'prevunique'.");
        return nullptr;
    }

    auto info = IDBCursorInfo::indexCursor(m_objectStore->modernTransaction(), m_objectStore->info().identifier(), m_info.identifier(), range, direction, IndexedDB::CursorType::KeyOnly);
    Ref<IDBRequest> request = m_objectStore->modernTransaction().requestOpenCursor(*context, *this, info);
    return WTF::move(request);
}

RefPtr<WebCore::IDBRequest> IDBIndex::openKeyCursor(ScriptExecutionContext* context, const Deprecated::ScriptValue& key, const String& direction, ExceptionCodeWithMessage& ec)
{
    LOG(IndexedDB, "IDBIndex::openKeyCursor");
    RefPtr<IDBKeyRange> keyRange = IDBKeyRange::only(context, key, ec.code);
    if (ec.code) {
        ec.message = ASCIILiteral("Failed to execute 'openKeyCursor' on 'IDBIndex': The parameter is not a valid key.");
        return nullptr;
    }
    return openKeyCursor(context, keyRange.get(), direction, ec);
}

RefPtr<WebCore::IDBRequest> IDBIndex::get(ScriptExecutionContext* context, IDBKeyRange* range, ExceptionCodeWithMessage& ec)
{
    LOG(IndexedDB, "IDBIndex::get");

    if (!context) {
        ec.code = IDBDatabaseException::InvalidStateError;
        return nullptr;
    }

    return doGet(*context, IDBKeyRangeData(range), ec);
}

RefPtr<WebCore::IDBRequest> IDBIndex::get(ScriptExecutionContext* context, const Deprecated::ScriptValue& key, ExceptionCodeWithMessage& ec)
{
    LOG(IndexedDB, "IDBIndex::get");

    if (!context) {
        ec.code = IDBDatabaseException::InvalidStateError;
        return nullptr;
    }

    DOMRequestState requestState(context);
    RefPtr<IDBKey> idbKey = scriptValueToIDBKey(&requestState, key);
    if (!idbKey || idbKey->type() == KeyType::Invalid) {
        ec.code = IDBDatabaseException::DataError;
        ec.message = ASCIILiteral("Failed to execute 'get' on 'IDBIndex': The parameter is not a valid key.");
        return nullptr;
    }

    return doGet(*context, IDBKeyRangeData(idbKey.get()), ec);
}

RefPtr<WebCore::IDBRequest> IDBIndex::doGet(ScriptExecutionContext& context, const IDBKeyRangeData& range, ExceptionCodeWithMessage& ec)
{
    if (m_deleted || m_objectStore->isDeleted()) {
        ec.code = IDBDatabaseException::InvalidStateError;
        return nullptr;
    }

    if (range.isNull) {
        ec.code = IDBDatabaseException::DataError;
        return nullptr;
    }

    auto& transaction = m_objectStore->modernTransaction();
    if (!transaction.isActive()) {
        ec.code = IDBDatabaseException::TransactionInactiveError;
        ec.message = ASCIILiteral("Failed to execute 'get' on 'IDBIndex': The transaction is inactive or finished.");
        return nullptr;
    }

    return transaction.requestGetValue(context, *this, range);
}

RefPtr<WebCore::IDBRequest> IDBIndex::getKey(ScriptExecutionContext* context, IDBKeyRange* range, ExceptionCodeWithMessage& ec)
{
    LOG(IndexedDB, "IDBIndex::getKey");

    if (!context) {
        ec.code = IDBDatabaseException::InvalidStateError;
        return nullptr;
    }

    return doGetKey(*context, IDBKeyRangeData(range), ec);
}

RefPtr<WebCore::IDBRequest> IDBIndex::getKey(ScriptExecutionContext* context, const Deprecated::ScriptValue& key, ExceptionCodeWithMessage& ec)
{
    LOG(IndexedDB, "IDBIndex::getKey");

    if (!context) {
        ec.code = IDBDatabaseException::InvalidStateError;
        return nullptr;
    }

    DOMRequestState requestState(context);
    RefPtr<IDBKey> idbKey = scriptValueToIDBKey(&requestState, key);
    if (!idbKey || idbKey->type() == KeyType::Invalid) {
        ec.code = IDBDatabaseException::DataError;
        ec.message = ASCIILiteral("Failed to execute 'getKey' on 'IDBIndex': The parameter is not a valid key.");
        return nullptr;
    }

    return doGetKey(*context, IDBKeyRangeData(idbKey.get()), ec);
}

RefPtr<WebCore::IDBRequest> IDBIndex::doGetKey(ScriptExecutionContext& context, const IDBKeyRangeData& range, ExceptionCodeWithMessage& ec)
{
    if (m_deleted || m_objectStore->isDeleted()) {
        ec.code = IDBDatabaseException::InvalidStateError;
        return nullptr;
    }

    if (range.isNull) {
        ec.code = IDBDatabaseException::DataError;
        return nullptr;
    }

    auto& transaction = m_objectStore->modernTransaction();
    if (!transaction.isActive()) {
        ec.code = IDBDatabaseException::TransactionInactiveError;
        ec.message = ASCIILiteral("Failed to execute 'getKey' on 'IDBIndex': The transaction is inactive or finished.");
        return nullptr;
    }

    return transaction.requestGetKey(context, *this, range);
}

void IDBIndex::markAsDeleted()
{
    m_deleted = true;
}

} // namespace IDBClient
} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)

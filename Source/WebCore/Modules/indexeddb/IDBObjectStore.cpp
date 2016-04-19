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
#include "IDBObjectStore.h"

#if ENABLE(INDEXED_DATABASE)

#include "DOMStringList.h"
#include "Document.h"
#include "IDBBindingUtilities.h"
#include "IDBCursor.h"
#include "IDBDatabase.h"
#include "IDBDatabaseException.h"
#include "IDBError.h"
#include "IDBIndex.h"
#include "IDBKey.h"
#include "IDBKeyRangeData.h"
#include "IDBRequest.h"
#include "IDBTransaction.h"
#include "IndexedDB.h"
#include "Logging.h"
#include "Page.h"
#include "ScriptExecutionContext.h"
#include "ScriptState.h"
#include "SerializedScriptValue.h"
#include <wtf/Locker.h>

using namespace JSC;

namespace WebCore {

Ref<IDBObjectStore> IDBObjectStore::create(ScriptExecutionContext& context, const IDBObjectStoreInfo& info, IDBTransaction& transaction)
{
    return adoptRef(*new IDBObjectStore(context, info, transaction));
}

IDBObjectStore::IDBObjectStore(ScriptExecutionContext& context, const IDBObjectStoreInfo& info, IDBTransaction& transaction)
    : ActiveDOMObject(&context)
    , m_info(info)
    , m_originalInfo(info)
    , m_transaction(transaction)
{
    suspendIfNeeded();
}

IDBObjectStore::~IDBObjectStore()
{
}

const char* IDBObjectStore::activeDOMObjectName() const
{
    return "IDBObjectStore";
}

bool IDBObjectStore::canSuspendForDocumentSuspension() const
{
    return false;
}

bool IDBObjectStore::hasPendingActivity() const
{
    return !m_transaction->isFinished();
}

const String& IDBObjectStore::name() const
{
    return m_info.name();
}

const IDBKeyPath& IDBObjectStore::keyPath() const
{
    return m_info.keyPath();
}

RefPtr<DOMStringList> IDBObjectStore::indexNames() const
{
    RefPtr<DOMStringList> indexNames = DOMStringList::create();
    for (auto& name : m_info.indexNames())
        indexNames->append(name);
    indexNames->sort();

    return indexNames;
}

RefPtr<IDBTransaction> IDBObjectStore::transaction()
{
    return &m_transaction.get();
}

bool IDBObjectStore::autoIncrement() const
{
    return m_info.autoIncrement();
}

RefPtr<IDBRequest> IDBObjectStore::openCursor(ScriptExecutionContext& context, ExceptionCodeWithMessage& ec)
{
    return openCursor(context, static_cast<IDBKeyRange*>(nullptr), ec);
}

RefPtr<IDBRequest> IDBObjectStore::openCursor(ScriptExecutionContext& context, IDBKeyRange* keyRange, ExceptionCodeWithMessage& ec)
{
    return openCursor(context, keyRange, IDBCursor::directionNext(), ec);
}

RefPtr<IDBRequest> IDBObjectStore::openCursor(ScriptExecutionContext& context, JSValue key, ExceptionCodeWithMessage& ec)
{
    return openCursor(context, key, IDBCursor::directionNext(), ec);
}

RefPtr<IDBRequest> IDBObjectStore::openCursor(ScriptExecutionContext& context, IDBKeyRange* range, const String& directionString, ExceptionCodeWithMessage& ec)
{
    LOG(IndexedDB, "IDBObjectStore::openCursor");

    if (m_deleted) {
        ec.code = IDBDatabaseException::InvalidStateError;
        ec.message = ASCIILiteral("Failed to execute 'openCursor' on 'IDBObjectStore': The object store has been deleted.");
        return nullptr;
    }

    if (!m_transaction->isActive()) {
        ec.code = IDBDatabaseException::TransactionInactiveError;
        ec.message = ASCIILiteral("Failed to execute 'openCursor' on 'IDBObjectStore': The transaction is inactive or finished.");
        return nullptr;
    }

    IndexedDB::CursorDirection direction = IDBCursor::stringToDirection(directionString, ec.code);
    if (ec.code)
        return nullptr;

    auto info = IDBCursorInfo::objectStoreCursor(m_transaction.get(), m_info.identifier(), range, direction);
    Ref<IDBRequest> request = m_transaction->requestOpenCursor(context, *this, info);
    return WTFMove(request);
}

RefPtr<IDBRequest> IDBObjectStore::openCursor(ScriptExecutionContext& context, JSValue key, const String& direction, ExceptionCodeWithMessage& ec)
{
    RefPtr<IDBKeyRange> keyRange = IDBKeyRange::only(context, key, ec.code);
    if (ec.code) {
        ec.message = ASCIILiteral("Failed to execute 'openCursor' on 'IDBObjectStore': The parameter is not a valid key.");
        return 0;
    }

    return openCursor(context, keyRange.get(), direction, ec);
}

RefPtr<IDBRequest> IDBObjectStore::get(ScriptExecutionContext& context, JSValue key, ExceptionCodeWithMessage& ec)
{
    LOG(IndexedDB, "IDBObjectStore::get");

    if (!m_transaction->isActive()) {
        ec.code = IDBDatabaseException::TransactionInactiveError;
        ec.message = ASCIILiteral("Failed to execute 'get' on 'IDBObjectStore': The transaction is inactive or finished.");
        return nullptr;
    }

    if (m_deleted) {
        ec.code = IDBDatabaseException::InvalidStateError;
        ec.message = ASCIILiteral("Failed to execute 'get' on 'IDBObjectStore': The object store has been deleted.");
        return nullptr;
    }

    RefPtr<IDBKey> idbKey = scriptValueToIDBKey(context, key);
    if (!idbKey || idbKey->type() == KeyType::Invalid) {
        ec.code = IDBDatabaseException::DataError;
        ec.message = ASCIILiteral("Failed to execute 'get' on 'IDBObjectStore': The parameter is not a valid key.");
        return nullptr;
    }

    Ref<IDBRequest> request = m_transaction->requestGetRecord(context, *this, idbKey.get());
    return WTFMove(request);
}

RefPtr<IDBRequest> IDBObjectStore::get(ScriptExecutionContext& context, IDBKeyRange* keyRange, ExceptionCodeWithMessage& ec)
{
    LOG(IndexedDB, "IDBObjectStore::get");

    if (!m_transaction->isActive()) {
        ec.code = IDBDatabaseException::TransactionInactiveError;
        return nullptr;
    }

    if (m_deleted) {
        ec.code = IDBDatabaseException::InvalidStateError;
        ec.message = ASCIILiteral("Failed to execute 'get' on 'IDBObjectStore': The object store has been deleted.");
        return nullptr;
    }

    IDBKeyRangeData keyRangeData(keyRange);
    if (!keyRangeData.isValid()) {
        ec.code = IDBDatabaseException::DataError;
        return nullptr;
    }

    Ref<IDBRequest> request = m_transaction->requestGetRecord(context, *this, keyRangeData);
    return WTFMove(request);
}

RefPtr<IDBRequest> IDBObjectStore::add(ExecState& state, JSValue value, ExceptionCodeWithMessage& ec)
{
    return putOrAdd(state, value, nullptr, IndexedDB::ObjectStoreOverwriteMode::NoOverwrite, InlineKeyCheck::Perform, ec);
}

RefPtr<IDBRequest> IDBObjectStore::add(ExecState& execState, JSValue value, JSValue key, ExceptionCodeWithMessage& ec)
{
    RefPtr<IDBKey> idbKey;
    if (!key.isUndefined())
        idbKey = scriptValueToIDBKey(execState, key);
    return putOrAdd(execState, value, idbKey, IndexedDB::ObjectStoreOverwriteMode::NoOverwrite, InlineKeyCheck::Perform, ec);
}

RefPtr<IDBRequest> IDBObjectStore::put(ExecState& execState, JSValue value, JSValue key, ExceptionCodeWithMessage& ec)
{
    RefPtr<IDBKey> idbKey;
    if (!key.isUndefined())
        idbKey = scriptValueToIDBKey(execState, key);
    return putOrAdd(execState, value, idbKey, IndexedDB::ObjectStoreOverwriteMode::Overwrite, InlineKeyCheck::Perform, ec);
}

RefPtr<IDBRequest> IDBObjectStore::put(ExecState& state, JSValue value, ExceptionCodeWithMessage& ec)
{
    return putOrAdd(state, value, nullptr, IndexedDB::ObjectStoreOverwriteMode::Overwrite, InlineKeyCheck::Perform, ec);
}

RefPtr<IDBRequest> IDBObjectStore::putForCursorUpdate(ExecState& state, JSValue value, JSValue key, ExceptionCodeWithMessage& ec)
{
    return putOrAdd(state, value, scriptValueToIDBKey(state, key), IndexedDB::ObjectStoreOverwriteMode::OverwriteForCursor, InlineKeyCheck::DoNotPerform, ec);
}

RefPtr<IDBRequest> IDBObjectStore::putOrAdd(ExecState& state, JSValue value, RefPtr<IDBKey> key, IndexedDB::ObjectStoreOverwriteMode overwriteMode, InlineKeyCheck inlineKeyCheck, ExceptionCodeWithMessage& ec)
{
    LOG(IndexedDB, "IDBObjectStore::putOrAdd");

    auto context = scriptExecutionContextFromExecState(&state);
    if (!context) {
        ec.code = IDBDatabaseException::UnknownError;
        ec.message = ASCIILiteral("Unable to store record in object store because it does not have a valid script execution context");
        return nullptr;
    }

    // The IDB spec for several IDBObjectStore methods states that transaction related exceptions should fire before
    // the exception for an object store being deleted.
    // However, a handful of W3C IDB tests expect the deleted exception even though the transaction inactive exception also applies.
    // Additionally, Chrome and Edge agree with the test, as does Legacy IDB in WebKit.
    // Until this is sorted out, we'll agree with the test and the majority share browsers.
    if (m_deleted) {
        ec.code = IDBDatabaseException::InvalidStateError;
        ec.message = ASCIILiteral("Failed to store record in an IDBObjectStore: The object store has been deleted.");
        return nullptr;
    }

    if (!m_transaction->isActive()) {
        ec.code = IDBDatabaseException::TransactionInactiveError;
        ec.message = ASCIILiteral("Failed to store record in an IDBObjectStore: The transaction is inactive or finished.");
        return nullptr;
    }

    if (m_transaction->isReadOnly()) {
        ec.code = IDBDatabaseException::ReadOnlyError;
        ec.message = ASCIILiteral("Failed to store record in an IDBObjectStore: The transaction is read-only.");
        return nullptr;
    }

    RefPtr<SerializedScriptValue> serializedValue = SerializedScriptValue::create(&state, value, nullptr, nullptr);
    if (state.hadException()) {
        // Clear the DOM exception from the serializer so we can give a more targeted exception.
        state.clearException();

        ec.code = IDBDatabaseException::DataCloneError;
        ec.message = ASCIILiteral("Failed to store record in an IDBObjectStore: An object could not be cloned.");
        return nullptr;
    }

    bool privateBrowsingEnabled = false;
    if (context->isDocument()) {
        if (auto* page = static_cast<Document*>(context)->page())
            privateBrowsingEnabled = page->sessionID().isEphemeral();
    }

    if (serializedValue->hasBlobURLs() && privateBrowsingEnabled) {
        // https://bugs.webkit.org/show_bug.cgi?id=156347 - Support Blobs in private browsing.
        ec.code = IDBDatabaseException::DataCloneError;
        ec.message = ASCIILiteral("Failed to store record in an IDBObjectStore: BlobURLs are not yet supported.");
        return nullptr;
    }

    if (key && !key->isValid()) {
        ec.code = IDBDatabaseException::DataError;
        ec.message = ASCIILiteral("Failed to store record in an IDBObjectStore: The parameter is not a valid key.");
        return nullptr;
    }

    bool usesInlineKeys = !m_info.keyPath().isNull();
    bool usesKeyGenerator = autoIncrement();
    if (usesInlineKeys && inlineKeyCheck == InlineKeyCheck::Perform) {
        if (key) {
            ec.code = IDBDatabaseException::DataError;
            ec.message = ASCIILiteral("Failed to store record in an IDBObjectStore: The object store uses in-line keys and the key parameter was provided.");
            return nullptr;
        }

        RefPtr<IDBKey> keyPathKey = maybeCreateIDBKeyFromScriptValueAndKeyPath(state, value, m_info.keyPath());
        if (keyPathKey && !keyPathKey->isValid()) {
            ec.code = IDBDatabaseException::DataError;
            ec.message = ASCIILiteral("Failed to store record in an IDBObjectStore: Evaluating the object store's key path yielded a value that is not a valid key.");
            return nullptr;
        }

        if (!keyPathKey) {
            if (usesKeyGenerator) {
                if (!canInjectIDBKeyIntoScriptValue(state, value, m_info.keyPath())) {
                    ec.code = IDBDatabaseException::DataError;
                    return nullptr;
                }
            } else {
                ec.code = IDBDatabaseException::DataError;
                ec.message = ASCIILiteral("Failed to store record in an IDBObjectStore: Evaluating the object store's key path did not yield a value.");
                return nullptr;
            }
        }

        if (keyPathKey) {
            ASSERT(!key);
            key = keyPathKey;
        }
    } else if (!usesKeyGenerator && !key) {
        ec.code = IDBDatabaseException::DataError;
        ec.message = ASCIILiteral("Failed to store record in an IDBObjectStore: The object store uses out-of-line keys and has no key generator and the key parameter was not provided.");
        return nullptr;
    }

    return m_transaction->requestPutOrAdd(*context, *this, key.get(), *serializedValue, overwriteMode);
}

RefPtr<IDBRequest> IDBObjectStore::deleteFunction(ScriptExecutionContext& context, IDBKeyRange* keyRange, ExceptionCodeWithMessage& ec)
{
    return doDelete(context, keyRange, ec);
}

RefPtr<IDBRequest> IDBObjectStore::doDelete(ScriptExecutionContext& context, IDBKeyRange* keyRange, ExceptionCodeWithMessage& ec)
{
    LOG(IndexedDB, "IDBObjectStore::deleteFunction");

    // The IDB spec for several IDBObjectStore methods states that transaction related exceptions should fire before
    // the exception for an object store being deleted.
    // However, a handful of W3C IDB tests expect the deleted exception even though the transaction inactive exception also applies.
    // Additionally, Chrome and Edge agree with the test, as does Legacy IDB in WebKit.
    // Until this is sorted out, we'll agree with the test and the majority share browsers.
    if (m_deleted) {
        ec.code = IDBDatabaseException::InvalidStateError;
        ec.message = ASCIILiteral("Failed to execute 'delete' on 'IDBObjectStore': The object store has been deleted.");
        return nullptr;
    }

    if (!m_transaction->isActive()) {
        ec.code = IDBDatabaseException::TransactionInactiveError;
        ec.message = ASCIILiteral("Failed to execute 'delete' on 'IDBObjectStore': The transaction is inactive or finished.");
        return nullptr;
    }

    if (m_transaction->isReadOnly()) {
        ec.code = IDBDatabaseException::ReadOnlyError;
        ec.message = ASCIILiteral("Failed to execute 'delete' on 'IDBObjectStore': The transaction is read-only.");
        return nullptr;
    }

    IDBKeyRangeData keyRangeData(keyRange);
    if (!keyRangeData.isValid()) {
        ec.code = IDBDatabaseException::DataError;
        ec.message = ASCIILiteral("Failed to execute 'delete' on 'IDBObjectStore': The parameter is not a valid key range.");
        return nullptr;
    }

    Ref<IDBRequest> request = m_transaction->requestDeleteRecord(context, *this, keyRangeData);
    return WTFMove(request);
}

RefPtr<IDBRequest> IDBObjectStore::deleteFunction(ScriptExecutionContext& context, JSValue key, ExceptionCodeWithMessage& ec)
{
    return modernDelete(context, key, ec);
}

RefPtr<IDBRequest> IDBObjectStore::modernDelete(ScriptExecutionContext& context, JSValue key, ExceptionCodeWithMessage& ec)
{
    RefPtr<IDBKey> idbKey = scriptValueToIDBKey(context, key);
    if (!idbKey || idbKey->type() == KeyType::Invalid) {
        ec.code = IDBDatabaseException::DataError;
        ec.message = ASCIILiteral("Failed to execute 'delete' on 'IDBObjectStore': The parameter is not a valid key.");
        return nullptr;
    }

    return doDelete(context, &IDBKeyRange::create(idbKey.releaseNonNull()).get(), ec);
}

RefPtr<IDBRequest> IDBObjectStore::clear(ScriptExecutionContext& context, ExceptionCodeWithMessage& ec)
{
    LOG(IndexedDB, "IDBObjectStore::clear");

    // The IDB spec for several IDBObjectStore methods states that transaction related exceptions should fire before
    // the exception for an object store being deleted.
    // However, a handful of W3C IDB tests expect the deleted exception even though the transaction inactive exception also applies.
    // Additionally, Chrome and Edge agree with the test, as does Legacy IDB in WebKit.
    // Until this is sorted out, we'll agree with the test and the majority share browsers.
    if (m_deleted) {
        ec.code = IDBDatabaseException::InvalidStateError;
        ec.message = ASCIILiteral("Failed to execute 'clear' on 'IDBObjectStore': The object store has been deleted.");
        return nullptr;
    }

    if (!m_transaction->isActive()) {
        ec.code = IDBDatabaseException::TransactionInactiveError;
        ec.message = ASCIILiteral("Failed to execute 'clear' on 'IDBObjectStore': The transaction is inactive or finished.");
        return nullptr;
    }

    if (m_transaction->isReadOnly()) {
        ec.code = IDBDatabaseException::ReadOnlyError;
        ec.message = ASCIILiteral("Failed to execute 'clear' on 'IDBObjectStore': The transaction is read-only.");
        return nullptr;
    }

    Ref<IDBRequest> request = m_transaction->requestClearObjectStore(context, *this);
    return adoptRef(request.leakRef());
}

RefPtr<IDBIndex> IDBObjectStore::createIndex(ScriptExecutionContext&, const String& name, const IDBKeyPath& keyPath, bool unique, bool multiEntry, ExceptionCodeWithMessage& ec)
{
    LOG(IndexedDB, "IDBObjectStore::createIndex %s", name.utf8().data());

    if (m_deleted) {
        ec.code = IDBDatabaseException::InvalidStateError;
        ec.message = ASCIILiteral("Failed to execute 'createIndex' on 'IDBObjectStore': The object store has been deleted.");
        return nullptr;
    }

    if (!m_transaction->isVersionChange()) {
        ec.code = IDBDatabaseException::InvalidStateError;
        ec.message = ASCIILiteral("Failed to execute 'createIndex' on 'IDBObjectStore': The database is not running a version change transaction.");
        return nullptr;
    }

    if (!m_transaction->isActive()) {
        ec.code = IDBDatabaseException::TransactionInactiveError;
        return nullptr;
    }

    if (!keyPath.isValid()) {
        ec.code = IDBDatabaseException::SyntaxError;
        ec.message = ASCIILiteral("Failed to execute 'createIndex' on 'IDBObjectStore': The keyPath argument contains an invalid key path.");
        return nullptr;
    }

    if (name.isNull()) {
        ec.code = TypeError;
        return nullptr;
    }

    if (m_info.hasIndex(name)) {
        ec.code = IDBDatabaseException::ConstraintError;
        ec.message = ASCIILiteral("Failed to execute 'createIndex' on 'IDBObjectStore': An index with the specified name already exists.");
        return nullptr;
    }

    if (keyPath.type() == IndexedDB::KeyPathType::Array && multiEntry) {
        ec.code = IDBDatabaseException::InvalidAccessError;
        ec.message = ASCIILiteral("Failed to execute 'createIndex' on 'IDBObjectStore': The keyPath argument was an array and the multiEntry option is true.");
        return nullptr;
    }

    // Install the new Index into the ObjectStore's info.
    IDBIndexInfo info = m_info.createNewIndex(name, keyPath, unique, multiEntry);
    m_transaction->database().didCreateIndexInfo(info);

    // Create the actual IDBObjectStore from the transaction, which also schedules the operation server side.
    auto index = m_transaction->createIndex(*this, info);
    RefPtr<IDBIndex> refIndex = index.get();

    Locker<Lock> locker(m_referencedIndexLock);
    m_referencedIndexes.set(name, WTFMove(index));

    return refIndex;
}

RefPtr<IDBIndex> IDBObjectStore::index(const String& indexName, ExceptionCodeWithMessage& ec)
{
    LOG(IndexedDB, "IDBObjectStore::index");

    if (!scriptExecutionContext())
        return nullptr;

    if (m_deleted) {
        ec.code = IDBDatabaseException::InvalidStateError;
        ec.message = ASCIILiteral("Failed to execute 'index' on 'IDBObjectStore': The object store has been deleted.");
        return nullptr;
    }

    if (m_transaction->isFinishedOrFinishing()) {
        ec.code = IDBDatabaseException::InvalidStateError;
        ec.message = ASCIILiteral("Failed to execute 'index' on 'IDBObjectStore': The transaction is finished.");
        return nullptr;
    }

    Locker<Lock> locker(m_referencedIndexLock);
    auto iterator = m_referencedIndexes.find(indexName);
    if (iterator != m_referencedIndexes.end())
        return iterator->value.get();

    auto* info = m_info.infoForExistingIndex(indexName);
    if (!info) {
        ec.code = IDBDatabaseException::NotFoundError;
        ec.message = ASCIILiteral("Failed to execute 'index' on 'IDBObjectStore': The specified index was not found.");
        return nullptr;
    }

    auto index = std::make_unique<IDBIndex>(*scriptExecutionContext(), *info, *this);
    RefPtr<IDBIndex> refIndex = index.get();
    m_referencedIndexes.set(indexName, WTFMove(index));

    return refIndex;
}

void IDBObjectStore::deleteIndex(const String& name, ExceptionCodeWithMessage& ec)
{
    LOG(IndexedDB, "IDBObjectStore::deleteIndex %s", name.utf8().data());

    if (m_deleted) {
        ec.code = IDBDatabaseException::InvalidStateError;
        ec.message = ASCIILiteral("Failed to execute 'deleteIndex' on 'IDBObjectStore': The object store has been deleted.");
        return;
    }

    if (!m_transaction->isVersionChange()) {
        ec.code = IDBDatabaseException::InvalidStateError;
        ec.message = ASCIILiteral("Failed to execute 'deleteIndex' on 'IDBObjectStore': The database is not running a version change transaction.");
        return;
    }

    if (!m_transaction->isActive()) {
        ec.code = IDBDatabaseException::TransactionInactiveError;
        ec.message = ASCIILiteral("Failed to execute 'deleteIndex' on 'IDBObjectStore': The transaction is inactive or finished.");
        return;
    }

    if (!m_info.hasIndex(name)) {
        ec.code = IDBDatabaseException::NotFoundError;
        ec.message = ASCIILiteral("Failed to execute 'deleteIndex' on 'IDBObjectStore': The specified index was not found.");
        return;
    }

    auto* info = m_info.infoForExistingIndex(name);
    ASSERT(info);
    m_transaction->database().didDeleteIndexInfo(*info);

    m_info.deleteIndex(name);

    {
        Locker<Lock> locker(m_referencedIndexLock);
        if (auto index = m_referencedIndexes.take(name)) {
            index->markAsDeleted();
            m_deletedIndexes.add(WTFMove(index));
        }

    }

    m_transaction->deleteIndex(m_info.identifier(), name);
}

RefPtr<IDBRequest> IDBObjectStore::count(ScriptExecutionContext& context, ExceptionCodeWithMessage& ec)
{
    LOG(IndexedDB, "IDBObjectStore::count");

    return doCount(context, IDBKeyRangeData::allKeys(), ec);
}

RefPtr<IDBRequest> IDBObjectStore::count(ScriptExecutionContext& context, JSValue key, ExceptionCodeWithMessage& ec)
{
    LOG(IndexedDB, "IDBObjectStore::count");

    RefPtr<IDBKey> idbKey = scriptValueToIDBKey(context, key);
    if (!idbKey || idbKey->type() == KeyType::Invalid) {
        ec.code = IDBDatabaseException::DataError;
        ec.message = ASCIILiteral("Failed to execute 'count' on 'IDBObjectStore': The parameter is not a valid key.");
        return nullptr;
    }

    return doCount(context, IDBKeyRangeData(idbKey.get()), ec);
}

RefPtr<IDBRequest> IDBObjectStore::count(ScriptExecutionContext& context, IDBKeyRange* range, ExceptionCodeWithMessage& ec)
{
    LOG(IndexedDB, "IDBObjectStore::count");

    return doCount(context, range ? IDBKeyRangeData(range) : IDBKeyRangeData::allKeys(), ec);
}

RefPtr<IDBRequest> IDBObjectStore::doCount(ScriptExecutionContext& context, const IDBKeyRangeData& range, ExceptionCodeWithMessage& ec)
{
    // The IDB spec for several IDBObjectStore methods states that transaction related exceptions should fire before
    // the exception for an object store being deleted.
    // However, a handful of W3C IDB tests expect the deleted exception even though the transaction inactive exception also applies.
    // Additionally, Chrome and Edge agree with the test, as does Legacy IDB in WebKit.
    // Until this is sorted out, we'll agree with the test and the majority share browsers.
    if (m_deleted) {
        ec.code = IDBDatabaseException::InvalidStateError;
        ec.message = ASCIILiteral("Failed to execute 'count' on 'IDBObjectStore': The object store has been deleted.");
        return nullptr;
    }

    if (!m_transaction->isActive()) {
        ec.code = IDBDatabaseException::TransactionInactiveError;
        ec.message = ASCIILiteral("Failed to execute 'count' on 'IDBObjectStore': The transaction is inactive or finished.");
        return nullptr;
    }

    if (!range.isValid()) {
        ec.code = IDBDatabaseException::DataError;
        return nullptr;
    }

    return m_transaction->requestCount(context, *this, range);
}

void IDBObjectStore::markAsDeleted()
{
    m_deleted = true;
}

void IDBObjectStore::rollbackInfoForVersionChangeAbort()
{
    m_info = m_originalInfo;
}

void IDBObjectStore::visitReferencedIndexes(SlotVisitor& visitor) const
{
    Locker<Lock> locker(m_referencedIndexLock);
    for (auto& index : m_referencedIndexes.values())
        visitor.addOpaqueRoot(index.get());
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)

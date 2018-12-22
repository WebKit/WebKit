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
#include "IDBError.h"
#include "IDBGetRecordData.h"
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
#include <JavaScriptCore/CatchScope.h>
#include <JavaScriptCore/HeapInlines.h>
#include <JavaScriptCore/JSCJSValueInlines.h>
#include <wtf/Locker.h>

namespace WebCore {
using namespace JSC;

IDBObjectStore::IDBObjectStore(ScriptExecutionContext& context, const IDBObjectStoreInfo& info, IDBTransaction& transaction)
    : ActiveDOMObject(&context)
    , m_info(info)
    , m_originalInfo(info)
    , m_transaction(transaction)
{
    ASSERT(&m_transaction.database().originThread() == &Thread::current());

    suspendIfNeeded();
}

IDBObjectStore::~IDBObjectStore()
{
    ASSERT(&m_transaction.database().originThread() == &Thread::current());
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
    return m_transaction.hasPendingActivity();
}

const String& IDBObjectStore::name() const
{
    ASSERT(&m_transaction.database().originThread() == &Thread::current());
    return m_info.name();
}

ExceptionOr<void> IDBObjectStore::setName(const String& name)
{
    ASSERT(&m_transaction.database().originThread() == &Thread::current());

    if (m_deleted)
        return Exception { InvalidStateError, "Failed set property 'name' on 'IDBObjectStore': The object store has been deleted."_s };

    if (!m_transaction.isVersionChange())
        return Exception { InvalidStateError, "Failed set property 'name' on 'IDBObjectStore': The object store's transaction is not a version change transaction."_s };

    if (!m_transaction.isActive())
        return Exception { TransactionInactiveError, "Failed set property 'name' on 'IDBObjectStore': The object store's transaction is not active."_s };

    if (m_info.name() == name)
        return { };

    if (m_transaction.database().info().hasObjectStore(name))
        return Exception { ConstraintError, makeString("Failed set property 'name' on 'IDBObjectStore': The database already has an object store named '", name, "'.") };

    m_transaction.database().renameObjectStore(*this, name);
    m_info.rename(name);

    return { };
}

const Optional<IDBKeyPath>& IDBObjectStore::keyPath() const
{
    ASSERT(&m_transaction.database().originThread() == &Thread::current());
    return m_info.keyPath();
}

Ref<DOMStringList> IDBObjectStore::indexNames() const
{
    ASSERT(&m_transaction.database().originThread() == &Thread::current());

    auto indexNames = DOMStringList::create();

    if (!m_deleted) {
        for (auto& name : m_info.indexNames())
            indexNames->append(name);
        indexNames->sort();
    }

    return indexNames;
}

IDBTransaction& IDBObjectStore::transaction()
{
    ASSERT(&m_transaction.database().originThread() == &Thread::current());
    return m_transaction;
}

bool IDBObjectStore::autoIncrement() const
{
    ASSERT(&m_transaction.database().originThread() == &Thread::current());
    return m_info.autoIncrement();
}

ExceptionOr<Ref<IDBRequest>> IDBObjectStore::openCursor(ExecState& execState, RefPtr<IDBKeyRange> range, IDBCursorDirection direction)
{
    LOG(IndexedDB, "IDBObjectStore::openCursor");
    ASSERT(&m_transaction.database().originThread() == &Thread::current());

    if (m_deleted)
        return Exception { InvalidStateError, "Failed to execute 'openCursor' on 'IDBObjectStore': The object store has been deleted."_s };

    if (!m_transaction.isActive())
        return Exception { TransactionInactiveError, "Failed to execute 'openCursor' on 'IDBObjectStore': The transaction is inactive or finished."_s };

    auto info = IDBCursorInfo::objectStoreCursor(m_transaction, m_info.identifier(), range.get(), direction, IndexedDB::CursorType::KeyAndValue);
    return m_transaction.requestOpenCursor(execState, *this, info);
}

ExceptionOr<Ref<IDBRequest>> IDBObjectStore::openCursor(ExecState& execState, JSValue key, IDBCursorDirection direction)
{
    auto onlyResult = IDBKeyRange::only(execState, key);
    if (onlyResult.hasException())
        return Exception { DataError, "Failed to execute 'openCursor' on 'IDBObjectStore': The parameter is not a valid key."_s };

    return openCursor(execState, onlyResult.releaseReturnValue(), direction);
}

ExceptionOr<Ref<IDBRequest>> IDBObjectStore::openKeyCursor(ExecState& execState, RefPtr<IDBKeyRange> range, IDBCursorDirection direction)
{
    LOG(IndexedDB, "IDBObjectStore::openCursor");
    ASSERT(&m_transaction.database().originThread() == &Thread::current());

    if (m_deleted)
        return Exception { InvalidStateError, "Failed to execute 'openKeyCursor' on 'IDBObjectStore': The object store has been deleted."_s };

    if (!m_transaction.isActive())
        return Exception { TransactionInactiveError, "Failed to execute 'openKeyCursor' on 'IDBObjectStore': The transaction is inactive or finished."_s };

    auto info = IDBCursorInfo::objectStoreCursor(m_transaction, m_info.identifier(), range.get(), direction, IndexedDB::CursorType::KeyOnly);
    return m_transaction.requestOpenCursor(execState, *this, info);
}

ExceptionOr<Ref<IDBRequest>> IDBObjectStore::openKeyCursor(ExecState& execState, JSValue key, IDBCursorDirection direction)
{
    auto onlyResult = IDBKeyRange::only(execState, key);
    if (onlyResult.hasException())
        return Exception { DataError, "Failed to execute 'openKeyCursor' on 'IDBObjectStore': The parameter is not a valid key or key range."_s };

    return openKeyCursor(execState, onlyResult.releaseReturnValue(), direction);
}

ExceptionOr<Ref<IDBRequest>> IDBObjectStore::get(ExecState& execState, JSValue key)
{
    LOG(IndexedDB, "IDBObjectStore::get");
    ASSERT(&m_transaction.database().originThread() == &Thread::current());

    if (m_deleted)
        return Exception { InvalidStateError, "Failed to execute 'get' on 'IDBObjectStore': The object store has been deleted."_s };

    if (!m_transaction.isActive())
        return Exception { TransactionInactiveError, "Failed to execute 'get' on 'IDBObjectStore': The transaction is inactive or finished."_s };

    auto idbKey = scriptValueToIDBKey(execState, key);
    if (!idbKey->isValid())
        return Exception { DataError, "Failed to execute 'get' on 'IDBObjectStore': The parameter is not a valid key."_s };

    return m_transaction.requestGetRecord(execState, *this, { idbKey.ptr(), IDBGetRecordDataType::KeyAndValue });
}

ExceptionOr<Ref<IDBRequest>> IDBObjectStore::get(ExecState& execState, IDBKeyRange* keyRange)
{
    LOG(IndexedDB, "IDBObjectStore::get");
    ASSERT(&m_transaction.database().originThread() == &Thread::current());

    if (m_deleted)
        return Exception { InvalidStateError, "Failed to execute 'get' on 'IDBObjectStore': The object store has been deleted."_s };

    if (!m_transaction.isActive())
        return Exception { TransactionInactiveError };

    IDBKeyRangeData keyRangeData(keyRange);
    if (!keyRangeData.isValid())
        return Exception { DataError };

    return m_transaction.requestGetRecord(execState, *this, { keyRangeData, IDBGetRecordDataType::KeyAndValue });
}

ExceptionOr<Ref<IDBRequest>> IDBObjectStore::getKey(ExecState& execState, JSValue key)
{
    LOG(IndexedDB, "IDBObjectStore::getKey");
    ASSERT(&m_transaction.database().originThread() == &Thread::current());

    if (m_deleted)
        return Exception { InvalidStateError, "Failed to execute 'getKey' on 'IDBObjectStore': The object store has been deleted."_s };

    if (!m_transaction.isActive())
        return Exception { TransactionInactiveError, "Failed to execute 'getKey' on 'IDBObjectStore': The transaction is inactive or finished."_s };

    auto idbKey = scriptValueToIDBKey(execState, key);
    if (!idbKey->isValid())
        return Exception { DataError, "Failed to execute 'getKey' on 'IDBObjectStore': The parameter is not a valid key."_s };

    return m_transaction.requestGetRecord(execState, *this, { idbKey.ptr(), IDBGetRecordDataType::KeyOnly });
}

ExceptionOr<Ref<IDBRequest>> IDBObjectStore::getKey(ExecState& execState, IDBKeyRange* keyRange)
{
    LOG(IndexedDB, "IDBObjectStore::getKey");
    ASSERT(&m_transaction.database().originThread() == &Thread::current());

    if (m_deleted)
        return Exception { InvalidStateError, "Failed to execute 'getKey' on 'IDBObjectStore': The object store has been deleted."_s };

    if (!m_transaction.isActive())
        return Exception { TransactionInactiveError, "Failed to execute 'getKey' on 'IDBObjectStore': The transaction is inactive or finished."_s };

    IDBKeyRangeData keyRangeData(keyRange);
    if (!keyRangeData.isValid())
        return Exception { DataError, "Failed to execute 'getKey' on 'IDBObjectStore': The parameter is not a valid key range."_s };

    return m_transaction.requestGetRecord(execState, *this, { keyRangeData, IDBGetRecordDataType::KeyOnly });
}

ExceptionOr<Ref<IDBRequest>> IDBObjectStore::add(ExecState& execState, JSValue value, JSValue key)
{
    RefPtr<IDBKey> idbKey;
    if (!key.isUndefined())
        idbKey = scriptValueToIDBKey(execState, key);
    return putOrAdd(execState, value, idbKey, IndexedDB::ObjectStoreOverwriteMode::NoOverwrite, InlineKeyCheck::Perform);
}

ExceptionOr<Ref<IDBRequest>> IDBObjectStore::put(ExecState& execState, JSValue value, JSValue key)
{
    RefPtr<IDBKey> idbKey;
    if (!key.isUndefined())
        idbKey = scriptValueToIDBKey(execState, key);
    return putOrAdd(execState, value, idbKey, IndexedDB::ObjectStoreOverwriteMode::Overwrite, InlineKeyCheck::Perform);
}

ExceptionOr<Ref<IDBRequest>> IDBObjectStore::putForCursorUpdate(ExecState& state, JSValue value, JSValue key)
{
    return putOrAdd(state, value, scriptValueToIDBKey(state, key), IndexedDB::ObjectStoreOverwriteMode::OverwriteForCursor, InlineKeyCheck::DoNotPerform);
}

ExceptionOr<Ref<IDBRequest>> IDBObjectStore::putOrAdd(ExecState& state, JSValue value, RefPtr<IDBKey> key, IndexedDB::ObjectStoreOverwriteMode overwriteMode, InlineKeyCheck inlineKeyCheck)
{
    VM& vm = state.vm();
    auto scope = DECLARE_CATCH_SCOPE(vm);

    LOG(IndexedDB, "IDBObjectStore::putOrAdd");
    ASSERT(&m_transaction.database().originThread() == &Thread::current());

    auto context = scriptExecutionContextFromExecState(&state);
    if (!context)
        return Exception { UnknownError, "Unable to store record in object store because it does not have a valid script execution context"_s };

    if (m_deleted)
        return Exception { InvalidStateError, "Failed to store record in an IDBObjectStore: The object store has been deleted."_s };

    if (!m_transaction.isActive())
        return Exception { TransactionInactiveError, "Failed to store record in an IDBObjectStore: The transaction is inactive or finished."_s };

    if (m_transaction.isReadOnly())
        return Exception { ReadonlyError, "Failed to store record in an IDBObjectStore: The transaction is read-only."_s };

    auto serializedValue = SerializedScriptValue::create(state, value);
    if (UNLIKELY(scope.exception())) {
        // Clear the DOM exception from the serializer so we can give a more targeted exception.
        scope.clearException();

        return Exception { DataCloneError, "Failed to store record in an IDBObjectStore: An object could not be cloned."_s };
    }

    bool privateBrowsingEnabled = false;
    if (is<Document>(*context)) {
        if (auto* page = downcast<Document>(*context).page())
            privateBrowsingEnabled = page->sessionID().isEphemeral();
    }

    if (serializedValue->hasBlobURLs() && privateBrowsingEnabled) {
        // https://bugs.webkit.org/show_bug.cgi?id=156347 - Support Blobs in private browsing.
        return Exception { DataCloneError, "Failed to store record in an IDBObjectStore: BlobURLs are not yet supported."_s };
    }

    if (key && !key->isValid())
        return Exception { DataError, "Failed to store record in an IDBObjectStore: The parameter is not a valid key."_s };

    bool usesInlineKeys = !!m_info.keyPath();
    bool usesKeyGenerator = autoIncrement();
    if (usesInlineKeys && inlineKeyCheck == InlineKeyCheck::Perform) {
        if (key)
            return Exception { DataError, "Failed to store record in an IDBObjectStore: The object store uses in-line keys and the key parameter was provided."_s };

        RefPtr<IDBKey> keyPathKey = maybeCreateIDBKeyFromScriptValueAndKeyPath(state, value, m_info.keyPath().value());
        if (keyPathKey && !keyPathKey->isValid())
            return Exception { DataError, "Failed to store record in an IDBObjectStore: Evaluating the object store's key path yielded a value that is not a valid key."_s };

        if (!keyPathKey) {
            if (!usesKeyGenerator)
                return Exception { DataError, "Failed to store record in an IDBObjectStore: Evaluating the object store's key path did not yield a value."_s };
            if (!canInjectIDBKeyIntoScriptValue(state, value, m_info.keyPath().value()))
                return Exception { DataError };
        }

        if (keyPathKey) {
            ASSERT(!key);
            key = keyPathKey;
        }
    } else if (!usesKeyGenerator && !key)
        return Exception { DataError, "Failed to store record in an IDBObjectStore: The object store uses out-of-line keys and has no key generator and the key parameter was not provided."_s };

    return m_transaction.requestPutOrAdd(state, *this, key.get(), *serializedValue, overwriteMode);
}

ExceptionOr<Ref<IDBRequest>> IDBObjectStore::deleteFunction(ExecState& execState, IDBKeyRange* keyRange)
{
    return doDelete(execState, keyRange);
}

ExceptionOr<Ref<IDBRequest>> IDBObjectStore::doDelete(ExecState& execState, IDBKeyRange* keyRange)
{
    LOG(IndexedDB, "IDBObjectStore::deleteFunction");
    ASSERT(&m_transaction.database().originThread() == &Thread::current());

    // The IDB spec for several IDBObjectStore methods states that transaction related exceptions should fire before
    // the exception for an object store being deleted.
    // However, a handful of W3C IDB tests expect the deleted exception even though the transaction inactive exception also applies.
    // Additionally, Chrome and Edge agree with the test, as does Legacy IDB in WebKit.
    // Until this is sorted out, we'll agree with the test and the majority share browsers.
    if (m_deleted)
        return Exception { InvalidStateError, "Failed to execute 'delete' on 'IDBObjectStore': The object store has been deleted."_s };

    if (!m_transaction.isActive())
        return Exception { TransactionInactiveError, "Failed to execute 'delete' on 'IDBObjectStore': The transaction is inactive or finished."_s };

    if (m_transaction.isReadOnly())
        return Exception { ReadonlyError, "Failed to execute 'delete' on 'IDBObjectStore': The transaction is read-only."_s };

    IDBKeyRangeData keyRangeData(keyRange);
    if (!keyRangeData.isValid())
        return Exception { DataError, "Failed to execute 'delete' on 'IDBObjectStore': The parameter is not a valid key range."_s };

    return m_transaction.requestDeleteRecord(execState, *this, keyRangeData);
}

ExceptionOr<Ref<IDBRequest>> IDBObjectStore::deleteFunction(ExecState& execState, JSValue key)
{
    Ref<IDBKey> idbKey = scriptValueToIDBKey(execState, key);
    if (!idbKey->isValid())
        return Exception { DataError, "Failed to execute 'delete' on 'IDBObjectStore': The parameter is not a valid key."_s };
    return doDelete(execState, IDBKeyRange::create(WTFMove(idbKey)).ptr());
}

ExceptionOr<Ref<IDBRequest>> IDBObjectStore::clear(ExecState& execState)
{
    LOG(IndexedDB, "IDBObjectStore::clear");
    ASSERT(&m_transaction.database().originThread() == &Thread::current());

    // The IDB spec for several IDBObjectStore methods states that transaction related exceptions should fire before
    // the exception for an object store being deleted.
    // However, a handful of W3C IDB tests expect the deleted exception even though the transaction inactive exception also applies.
    // Additionally, Chrome and Edge agree with the test, as does Legacy IDB in WebKit.
    // Until this is sorted out, we'll agree with the test and the majority share browsers.
    if (m_deleted)
        return Exception { InvalidStateError, "Failed to execute 'clear' on 'IDBObjectStore': The object store has been deleted."_s };

    if (!m_transaction.isActive())
        return Exception { TransactionInactiveError, "Failed to execute 'clear' on 'IDBObjectStore': The transaction is inactive or finished."_s };

    if (m_transaction.isReadOnly())
        return Exception { ReadonlyError, "Failed to execute 'clear' on 'IDBObjectStore': The transaction is read-only."_s };

    return m_transaction.requestClearObjectStore(execState, *this);
}

ExceptionOr<Ref<IDBIndex>> IDBObjectStore::createIndex(ExecState&, const String& name, IDBKeyPath&& keyPath, const IndexParameters& parameters)
{
    LOG(IndexedDB, "IDBObjectStore::createIndex %s (keyPath: %s, unique: %i, multiEntry: %i)", name.utf8().data(), loggingString(keyPath).utf8().data(), parameters.unique, parameters.multiEntry);
    ASSERT(&m_transaction.database().originThread() == &Thread::current());

    if (!m_transaction.isVersionChange())
        return Exception { InvalidStateError, "Failed to execute 'createIndex' on 'IDBObjectStore': The database is not running a version change transaction."_s };

    if (m_deleted)
        return Exception { InvalidStateError, "Failed to execute 'createIndex' on 'IDBObjectStore': The object store has been deleted."_s };

    if (!m_transaction.isActive())
        return Exception { TransactionInactiveError, "Failed to execute 'createIndex' on 'IDBObjectStore': The transaction is inactive."_s};

    if (m_info.hasIndex(name))
        return Exception { ConstraintError, "Failed to execute 'createIndex' on 'IDBObjectStore': An index with the specified name already exists."_s };

    if (!isIDBKeyPathValid(keyPath))
        return Exception { SyntaxError, "Failed to execute 'createIndex' on 'IDBObjectStore': The keyPath argument contains an invalid key path."_s };

    if (name.isNull())
        return Exception { TypeError };

    if (parameters.multiEntry && WTF::holds_alternative<Vector<String>>(keyPath))
        return Exception { InvalidAccessError, "Failed to execute 'createIndex' on 'IDBObjectStore': The keyPath argument was an array and the multiEntry option is true."_s };

    // Install the new Index into the ObjectStore's info.
    IDBIndexInfo info = m_info.createNewIndex(name, WTFMove(keyPath), parameters.unique, parameters.multiEntry);
    m_transaction.database().didCreateIndexInfo(info);

    // Create the actual IDBObjectStore from the transaction, which also schedules the operation server side.
    auto index = m_transaction.createIndex(*this, info);

    Ref<IDBIndex> referencedIndex { *index };

    Locker<Lock> locker(m_referencedIndexLock);
    m_referencedIndexes.set(name, WTFMove(index));

    return WTFMove(referencedIndex);
}

ExceptionOr<Ref<IDBIndex>> IDBObjectStore::index(const String& indexName)
{
    LOG(IndexedDB, "IDBObjectStore::index");
    ASSERT(&m_transaction.database().originThread() == &Thread::current());

    if (!scriptExecutionContext())
        return Exception { InvalidStateError }; // FIXME: Is this code tested? Is iteven reachable?

    if (m_deleted)
        return Exception { InvalidStateError, "Failed to execute 'index' on 'IDBObjectStore': The object store has been deleted."_s };

    if (m_transaction.isFinishedOrFinishing())
        return Exception { InvalidStateError, "Failed to execute 'index' on 'IDBObjectStore': The transaction is finished."_s };

    Locker<Lock> locker(m_referencedIndexLock);
    auto iterator = m_referencedIndexes.find(indexName);
    if (iterator != m_referencedIndexes.end())
        return Ref<IDBIndex> { *iterator->value };

    auto* info = m_info.infoForExistingIndex(indexName);
    if (!info)
        return Exception { NotFoundError, "Failed to execute 'index' on 'IDBObjectStore': The specified index was not found."_s };

    auto index = std::make_unique<IDBIndex>(*scriptExecutionContext(), *info, *this);

    Ref<IDBIndex> referencedIndex { *index };

    m_referencedIndexes.set(indexName, WTFMove(index));

    return WTFMove(referencedIndex);
}

ExceptionOr<void> IDBObjectStore::deleteIndex(const String& name)
{
    LOG(IndexedDB, "IDBObjectStore::deleteIndex %s", name.utf8().data());
    ASSERT(&m_transaction.database().originThread() == &Thread::current());

    if (m_deleted)
        return Exception { InvalidStateError, "Failed to execute 'deleteIndex' on 'IDBObjectStore': The object store has been deleted."_s };

    if (!m_transaction.isVersionChange())
        return Exception { InvalidStateError, "Failed to execute 'deleteIndex' on 'IDBObjectStore': The database is not running a version change transaction."_s };

    if (!m_transaction.isActive())
        return Exception { TransactionInactiveError,  "Failed to execute 'deleteIndex' on 'IDBObjectStore': The transaction is inactive or finished."_s };

    if (!m_info.hasIndex(name))
        return Exception { NotFoundError, "Failed to execute 'deleteIndex' on 'IDBObjectStore': The specified index was not found."_s };

    auto* info = m_info.infoForExistingIndex(name);
    ASSERT(info);
    m_transaction.database().didDeleteIndexInfo(*info);

    m_info.deleteIndex(name);

    {
        Locker<Lock> locker(m_referencedIndexLock);
        if (auto index = m_referencedIndexes.take(name)) {
            index->markAsDeleted();
            auto identifier = index->info().identifier();
            m_deletedIndexes.add(identifier, WTFMove(index));
        }
    }

    m_transaction.deleteIndex(m_info.identifier(), name);

    return { };
}

ExceptionOr<Ref<IDBRequest>> IDBObjectStore::count(ExecState& execState, JSValue key)
{
    LOG(IndexedDB, "IDBObjectStore::count");

    Ref<IDBKey> idbKey = scriptValueToIDBKey(execState, key);
    if (!idbKey->isValid())
        return Exception { DataError, "Failed to execute 'count' on 'IDBObjectStore': The parameter is not a valid key."_s };

    return doCount(execState, IDBKeyRangeData(idbKey.ptr()));
}

ExceptionOr<Ref<IDBRequest>> IDBObjectStore::count(ExecState& execState, IDBKeyRange* range)
{
    LOG(IndexedDB, "IDBObjectStore::count");

    return doCount(execState, range ? IDBKeyRangeData(range) : IDBKeyRangeData::allKeys());
}

ExceptionOr<Ref<IDBRequest>> IDBObjectStore::doCount(ExecState& execState, const IDBKeyRangeData& range)
{
    ASSERT(&m_transaction.database().originThread() == &Thread::current());

    // The IDB spec for several IDBObjectStore methods states that transaction related exceptions should fire before
    // the exception for an object store being deleted.
    // However, a handful of W3C IDB tests expect the deleted exception even though the transaction inactive exception also applies.
    // Additionally, Chrome and Edge agree with the test, as does Legacy IDB in WebKit.
    // Until this is sorted out, we'll agree with the test and the majority share browsers.
    if (m_deleted)
        return Exception { InvalidStateError, "Failed to execute 'count' on 'IDBObjectStore': The object store has been deleted."_s };

    if (!m_transaction.isActive())
        return Exception { TransactionInactiveError, "Failed to execute 'count' on 'IDBObjectStore': The transaction is inactive or finished."_s };

    if (!range.isValid())
        return Exception { DataError };

    return m_transaction.requestCount(execState, *this, range);
}

ExceptionOr<Ref<IDBRequest>> IDBObjectStore::getAll(ExecState& execState, RefPtr<IDBKeyRange> range, Optional<uint32_t> count)
{
    LOG(IndexedDB, "IDBObjectStore::getAll");
    ASSERT(&m_transaction.database().originThread() == &Thread::current());

    if (m_deleted)
        return Exception { InvalidStateError, "Failed to execute 'getAll' on 'IDBObjectStore': The object store has been deleted."_s };

    if (!m_transaction.isActive())
        return Exception { TransactionInactiveError, "Failed to execute 'getAll' on 'IDBObjectStore': The transaction is inactive or finished."_s };

    return m_transaction.requestGetAllObjectStoreRecords(execState, *this, range.get(), IndexedDB::GetAllType::Values, count);
}

ExceptionOr<Ref<IDBRequest>> IDBObjectStore::getAll(ExecState& execState, JSValue key, Optional<uint32_t> count)
{
    auto onlyResult = IDBKeyRange::only(execState, key);
    if (onlyResult.hasException())
        return Exception { DataError, "Failed to execute 'getAll' on 'IDBObjectStore': The parameter is not a valid key."_s };

    return getAll(execState, onlyResult.releaseReturnValue(), count);
}

ExceptionOr<Ref<IDBRequest>> IDBObjectStore::getAllKeys(ExecState& execState, RefPtr<IDBKeyRange> range, Optional<uint32_t> count)
{
    LOG(IndexedDB, "IDBObjectStore::getAllKeys");
    ASSERT(&m_transaction.database().originThread() == &Thread::current());

    if (m_deleted)
        return Exception { InvalidStateError, "Failed to execute 'getAllKeys' on 'IDBObjectStore': The object store has been deleted."_s };

    if (!m_transaction.isActive())
        return Exception { TransactionInactiveError, "Failed to execute 'getAllKeys' on 'IDBObjectStore': The transaction is inactive or finished."_s };

    return m_transaction.requestGetAllObjectStoreRecords(execState, *this, range.get(), IndexedDB::GetAllType::Keys, count);
}

ExceptionOr<Ref<IDBRequest>> IDBObjectStore::getAllKeys(ExecState& execState, JSValue key, Optional<uint32_t> count)
{
    auto onlyResult = IDBKeyRange::only(execState, key);
    if (onlyResult.hasException())
        return Exception { DataError, "Failed to execute 'getAllKeys' on 'IDBObjectStore': The parameter is not a valid key."_s };

    return getAllKeys(execState, onlyResult.releaseReturnValue(), count);
}

void IDBObjectStore::markAsDeleted()
{
    ASSERT(&m_transaction.database().originThread() == &Thread::current());
    m_deleted = true;
}

void IDBObjectStore::rollbackForVersionChangeAbort()
{
    ASSERT(&m_transaction.database().originThread() == &Thread::current());

    String currentName = m_info.name();
    m_info = m_originalInfo;

    auto& databaseInfo = transaction().database().info();
    auto* objectStoreInfo = databaseInfo.infoForExistingObjectStore(m_info.identifier());
    if (!objectStoreInfo) {
        m_info.rename(currentName);
        m_deleted = true;
    } else {
        m_deleted = false;
        
        HashSet<uint64_t> indexesToRemove;
        for (auto indexIdentifier : objectStoreInfo->indexMap().keys()) {
            if (!objectStoreInfo->hasIndex(indexIdentifier))
                indexesToRemove.add(indexIdentifier);
        }

        for (auto indexIdentifier : indexesToRemove)
            m_info.deleteIndex(indexIdentifier);
    }

    Locker<Lock> locker(m_referencedIndexLock);

    Vector<uint64_t> identifiersToRemove;
    for (auto& iterator : m_deletedIndexes) {
        if (m_info.hasIndex(iterator.key)) {
            auto name = iterator.value->info().name();
            m_referencedIndexes.set(name, WTFMove(iterator.value));
            identifiersToRemove.append(iterator.key);
        }
    }

    for (auto identifier : identifiersToRemove)
        m_deletedIndexes.remove(identifier);

    for (auto& index : m_referencedIndexes.values())
        index->rollbackInfoForVersionChangeAbort();
}

void IDBObjectStore::visitReferencedIndexes(SlotVisitor& visitor) const
{
    Locker<Lock> locker(m_referencedIndexLock);
    for (auto& index : m_referencedIndexes.values())
        visitor.addOpaqueRoot(index.get());
    for (auto& index : m_deletedIndexes.values())
        visitor.addOpaqueRoot(index.get());
}

void IDBObjectStore::renameReferencedIndex(IDBIndex& index, const String& newName)
{
    LOG(IndexedDB, "IDBObjectStore::renameReferencedIndex");

    auto* indexInfo = m_info.infoForExistingIndex(index.info().identifier());
    ASSERT(indexInfo);
    indexInfo->rename(newName);

    ASSERT(m_referencedIndexes.contains(index.info().name()));
    ASSERT(!m_referencedIndexes.contains(newName));
    ASSERT(m_referencedIndexes.get(index.info().name()) == &index);

    m_referencedIndexes.set(newName, m_referencedIndexes.take(index.info().name()));
}

void IDBObjectStore::ref()
{
    m_transaction.ref();
}

void IDBObjectStore::deref()
{
    m_transaction.deref();
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)

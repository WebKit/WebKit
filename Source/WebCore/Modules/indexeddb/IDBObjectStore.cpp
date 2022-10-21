/*
 * Copyright (C) 2015-2021 Apple Inc. All rights reserved.
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
#include "SerializedScriptValue.h"
#include "WebCoreOpaqueRoot.h"
#include <JavaScriptCore/CatchScope.h>
#include <JavaScriptCore/HeapInlines.h>
#include <JavaScriptCore/JSCJSValueInlines.h>
#include <wtf/IsoMallocInlines.h>
#include <wtf/Locker.h>

namespace WebCore {
using namespace JSC;

WTF_MAKE_ISO_ALLOCATED_IMPL(IDBObjectStore);

UniqueRef<IDBObjectStore> IDBObjectStore::create(ScriptExecutionContext& context, const IDBObjectStoreInfo& info, IDBTransaction& transaction)
{
    auto result = UniqueRef(*new IDBObjectStore(context, info, transaction));
    result->suspendIfNeeded();
    return result;
}

IDBObjectStore::IDBObjectStore(ScriptExecutionContext& context, const IDBObjectStoreInfo& info, IDBTransaction& transaction)
    : ActiveDOMObject(&context)
    , m_info(info)
    , m_originalInfo(info)
    , m_transaction(transaction)
{
    ASSERT(canCurrentThreadAccessThreadLocalData(m_transaction.database().originThread()));
}

IDBObjectStore::~IDBObjectStore()
{
    ASSERT(canCurrentThreadAccessThreadLocalData(m_transaction.database().originThread()));
}

const char* IDBObjectStore::activeDOMObjectName() const
{
    return "IDBObjectStore";
}

bool IDBObjectStore::virtualHasPendingActivity() const
{
    return m_transaction.hasPendingActivity();
}

const String& IDBObjectStore::name() const
{
    ASSERT(canCurrentThreadAccessThreadLocalData(m_transaction.database().originThread()));
    return m_info.name();
}

ExceptionOr<void> IDBObjectStore::setName(const String& name)
{
    ASSERT(canCurrentThreadAccessThreadLocalData(m_transaction.database().originThread()));

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

const std::optional<IDBKeyPath>& IDBObjectStore::keyPath() const
{
    ASSERT(canCurrentThreadAccessThreadLocalData(m_transaction.database().originThread()));
    return m_info.keyPath();
}

Ref<DOMStringList> IDBObjectStore::indexNames() const
{
    ASSERT(canCurrentThreadAccessThreadLocalData(m_transaction.database().originThread()));

    if (m_deleted)
        return DOMStringList::create();

    auto indexNames = DOMStringList::create(m_info.indexNames());
    indexNames->sort();
    return indexNames;
}

IDBTransaction& IDBObjectStore::transaction()
{
    ASSERT(canCurrentThreadAccessThreadLocalData(m_transaction.database().originThread()));
    return m_transaction;
}

bool IDBObjectStore::autoIncrement() const
{
    ASSERT(canCurrentThreadAccessThreadLocalData(m_transaction.database().originThread()));
    return m_info.autoIncrement();
}

ExceptionOr<Ref<IDBRequest>> IDBObjectStore::doOpenCursor(IDBCursorDirection direction, Function<ExceptionOr<RefPtr<IDBKeyRange>>()>&& function)
{
    LOG(IndexedDB, "IDBObjectStore::openCursor");
    ASSERT(canCurrentThreadAccessThreadLocalData(m_transaction.database().originThread()));

    if (m_deleted)
        return Exception { InvalidStateError, "Failed to execute 'openCursor' on 'IDBObjectStore': The object store has been deleted."_s };

    if (!m_transaction.isActive())
        return Exception { TransactionInactiveError, "Failed to execute 'openCursor' on 'IDBObjectStore': The transaction is inactive or finished."_s };

    auto keyRange = function();
    if (keyRange.hasException())
        return keyRange.releaseException();
    auto* keyRangePointer = keyRange.returnValue().get();

    auto info = IDBCursorInfo::objectStoreCursor(m_transaction, m_info.identifier(), keyRangePointer, direction, IndexedDB::CursorType::KeyAndValue);
    return m_transaction.requestOpenCursor(*this, info);
}

ExceptionOr<Ref<IDBRequest>> IDBObjectStore::openCursor(RefPtr<IDBKeyRange>&& range, IDBCursorDirection direction)
{
    return doOpenCursor(direction, [range = WTFMove(range)]() {
        return range;
    });
}

ExceptionOr<Ref<IDBRequest>> IDBObjectStore::openCursor(JSGlobalObject& execState, JSValue key, IDBCursorDirection direction)
{
    return doOpenCursor(direction, [state = &execState, key]() {
        auto onlyResult = IDBKeyRange::only(*state, key);
        if (onlyResult.hasException())
            return ExceptionOr<RefPtr<IDBKeyRange>>{ Exception(DataError, "Failed to execute 'openCursor' on 'IDBObjectStore': The parameter is not a valid key."_s) };

        return ExceptionOr<RefPtr<IDBKeyRange>> { onlyResult.releaseReturnValue() };
    });
}

ExceptionOr<Ref<IDBRequest>> IDBObjectStore::doOpenKeyCursor(IDBCursorDirection direction, Function<ExceptionOr<RefPtr<IDBKeyRange>>()>&& function)
{
    LOG(IndexedDB, "IDBObjectStore::openKeyCursor");
    ASSERT(canCurrentThreadAccessThreadLocalData(m_transaction.database().originThread()));

    if (m_deleted)
        return Exception { InvalidStateError, "Failed to execute 'openKeyCursor' on 'IDBObjectStore': The object store has been deleted."_s };

    if (!m_transaction.isActive())
        return Exception { TransactionInactiveError, "Failed to execute 'openKeyCursor' on 'IDBObjectStore': The transaction is inactive or finished."_s };

    auto keyRange = function();
    if (keyRange.hasException())
        return keyRange.releaseException();

    auto* keyRangePointer = keyRange.returnValue().get();
    auto info = IDBCursorInfo::objectStoreCursor(m_transaction, m_info.identifier(), keyRangePointer, direction, IndexedDB::CursorType::KeyOnly);
    return m_transaction.requestOpenCursor(*this, info);
}

ExceptionOr<Ref<IDBRequest>> IDBObjectStore::openKeyCursor(RefPtr<IDBKeyRange>&& range, IDBCursorDirection direction)
{
    return doOpenKeyCursor(direction, [range = WTFMove(range)]() {
        return range;
    });
}

ExceptionOr<Ref<IDBRequest>> IDBObjectStore::openKeyCursor(JSGlobalObject& execState, JSValue key, IDBCursorDirection direction)
{
    return doOpenCursor(direction, [state = &execState, key]() {
        auto onlyResult = IDBKeyRange::only(*state, key);
        if (onlyResult.hasException())
            return ExceptionOr<RefPtr<IDBKeyRange>>{ Exception(DataError, "Failed to execute 'openKeyCursor' on 'IDBObjectStore': The parameter is not a valid key."_s) };

        return ExceptionOr<RefPtr<IDBKeyRange>> { onlyResult.releaseReturnValue() };
    });
}

ExceptionOr<Ref<IDBRequest>> IDBObjectStore::get(JSGlobalObject& execState, JSValue key)
{
    LOG(IndexedDB, "IDBObjectStore::get");
    ASSERT(canCurrentThreadAccessThreadLocalData(m_transaction.database().originThread()));

    if (m_deleted)
        return Exception { InvalidStateError, "Failed to execute 'get' on 'IDBObjectStore': The object store has been deleted."_s };

    if (!m_transaction.isActive())
        return Exception { TransactionInactiveError, "Failed to execute 'get' on 'IDBObjectStore': The transaction is inactive or finished."_s };

    auto idbKey = scriptValueToIDBKey(execState, key);
    if (!idbKey->isValid())
        return Exception { DataError, "Failed to execute 'get' on 'IDBObjectStore': The parameter is not a valid key."_s };

    return m_transaction.requestGetRecord(*this, { idbKey.ptr(), IDBGetRecordDataType::KeyAndValue });
}

ExceptionOr<Ref<IDBRequest>> IDBObjectStore::get(IDBKeyRange* keyRange)
{
    LOG(IndexedDB, "IDBObjectStore::get");
    ASSERT(canCurrentThreadAccessThreadLocalData(m_transaction.database().originThread()));

    if (m_deleted)
        return Exception { InvalidStateError, "Failed to execute 'get' on 'IDBObjectStore': The object store has been deleted."_s };

    if (!m_transaction.isActive())
        return Exception { TransactionInactiveError };

    IDBKeyRangeData keyRangeData(keyRange);
    if (!keyRangeData.isValid())
        return Exception { DataError };

    return m_transaction.requestGetRecord(*this, { keyRangeData, IDBGetRecordDataType::KeyAndValue });
}

ExceptionOr<Ref<IDBRequest>> IDBObjectStore::getKey(JSGlobalObject& execState, JSValue key)
{
    LOG(IndexedDB, "IDBObjectStore::getKey");
    ASSERT(canCurrentThreadAccessThreadLocalData(m_transaction.database().originThread()));

    if (m_deleted)
        return Exception { InvalidStateError, "Failed to execute 'getKey' on 'IDBObjectStore': The object store has been deleted."_s };

    if (!m_transaction.isActive())
        return Exception { TransactionInactiveError, "Failed to execute 'getKey' on 'IDBObjectStore': The transaction is inactive or finished."_s };

    auto idbKey = scriptValueToIDBKey(execState, key);
    if (!idbKey->isValid())
        return Exception { DataError, "Failed to execute 'getKey' on 'IDBObjectStore': The parameter is not a valid key."_s };

    return m_transaction.requestGetRecord(*this, { idbKey.ptr(), IDBGetRecordDataType::KeyOnly });
}

ExceptionOr<Ref<IDBRequest>> IDBObjectStore::getKey(IDBKeyRange* keyRange)
{
    LOG(IndexedDB, "IDBObjectStore::getKey");
    ASSERT(canCurrentThreadAccessThreadLocalData(m_transaction.database().originThread()));

    if (m_deleted)
        return Exception { InvalidStateError, "Failed to execute 'getKey' on 'IDBObjectStore': The object store has been deleted."_s };

    if (!m_transaction.isActive())
        return Exception { TransactionInactiveError, "Failed to execute 'getKey' on 'IDBObjectStore': The transaction is inactive or finished."_s };

    IDBKeyRangeData keyRangeData(keyRange);
    if (!keyRangeData.isValid())
        return Exception { DataError, "Failed to execute 'getKey' on 'IDBObjectStore': The parameter is not a valid key range."_s };

    return m_transaction.requestGetRecord(*this, { keyRangeData, IDBGetRecordDataType::KeyOnly });
}

ExceptionOr<Ref<IDBRequest>> IDBObjectStore::add(JSGlobalObject& execState, JSValue value, JSValue key)
{
    RefPtr<IDBKey> idbKey;
    if (!key.isUndefined())
        idbKey = scriptValueToIDBKey(execState, key);
    return putOrAdd(execState, value, idbKey, IndexedDB::ObjectStoreOverwriteMode::NoOverwrite, InlineKeyCheck::Perform);
}

ExceptionOr<Ref<IDBRequest>> IDBObjectStore::put(JSGlobalObject& execState, JSValue value, JSValue key)
{
    RefPtr<IDBKey> idbKey;
    if (!key.isUndefined())
        idbKey = scriptValueToIDBKey(execState, key);
    return putOrAdd(execState, value, idbKey, IndexedDB::ObjectStoreOverwriteMode::Overwrite, InlineKeyCheck::Perform);
}

ExceptionOr<Ref<IDBRequest>> IDBObjectStore::putForCursorUpdate(JSGlobalObject& state, JSValue value, RefPtr<IDBKey>&& key, RefPtr<SerializedScriptValue>&& serializedValue)
{
    return putOrAdd(state, value, WTFMove(key), IndexedDB::ObjectStoreOverwriteMode::OverwriteForCursor, InlineKeyCheck::DoNotPerform, WTFMove(serializedValue));
}

ExceptionOr<Ref<IDBRequest>> IDBObjectStore::putOrAdd(JSGlobalObject& state, JSValue value, RefPtr<IDBKey> key, IndexedDB::ObjectStoreOverwriteMode overwriteMode, InlineKeyCheck inlineKeyCheck, RefPtr<SerializedScriptValue>&& serializedValue)
{
    VM& vm = state.vm();
    auto scope = DECLARE_CATCH_SCOPE(vm);

    LOG(IndexedDB, "IDBObjectStore::putOrAdd");
    ASSERT(canCurrentThreadAccessThreadLocalData(m_transaction.database().originThread()));

    auto context = scriptExecutionContext();
    if (!context)
        return Exception { UnknownError, "Unable to store record in object store because it does not have a valid script execution context"_s };

    if (m_deleted)
        return Exception { InvalidStateError, "Failed to store record in an IDBObjectStore: The object store has been deleted."_s };

    if (!m_transaction.isActive())
        return Exception { TransactionInactiveError, "Failed to store record in an IDBObjectStore: The transaction is inactive or finished."_s };

    if (m_transaction.isReadOnly())
        return Exception { ReadonlyError, "Failed to store record in an IDBObjectStore: The transaction is read-only."_s };

    if (!serializedValue) {
        // Transaction should be inactive during structured clone.
        m_transaction.deactivate();
        serializedValue = SerializedScriptValue::create(state, value, SerializationForStorage::Yes);
        m_transaction.activate();
    }

    if (UNLIKELY(scope.exception()))
        return Exception { DataCloneError, "Failed to store record in an IDBObjectStore: An object could not be cloned."_s };

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

        auto clonedValue = serializedValue->deserialize(state, &state, SerializationErrorMode::NonThrowing);
        RefPtr<IDBKey> keyPathKey = maybeCreateIDBKeyFromScriptValueAndKeyPath(state, clonedValue, m_info.keyPath().value());
        if (keyPathKey && !keyPathKey->isValid())
            return Exception { DataError, "Failed to store record in an IDBObjectStore: Evaluating the object store's key path yielded a value that is not a valid key."_s };

        if (!keyPathKey) {
            if (!usesKeyGenerator)
                return Exception { DataError, "Failed to store record in an IDBObjectStore: Evaluating the object store's key path did not yield a value."_s };
            if (!canInjectIDBKeyIntoScriptValue(state, clonedValue, m_info.keyPath().value()))
                return Exception { DataError };
        }

        if (keyPathKey) {
            ASSERT(!key);
            key = keyPathKey;
        }
    } else if (!usesKeyGenerator && !key)
        return Exception { DataError, "Failed to store record in an IDBObjectStore: The object store uses out-of-line keys and has no key generator and the key parameter was not provided."_s };

    return m_transaction.requestPutOrAdd(*this, WTFMove(key), *serializedValue, overwriteMode);
}

ExceptionOr<Ref<IDBRequest>> IDBObjectStore::deleteFunction(IDBKeyRange* keyRange)
{
    return doDelete([keyRange]() {
        return RefPtr { keyRange };
    });
}

ExceptionOr<Ref<IDBRequest>> IDBObjectStore::doDelete(Function<ExceptionOr<RefPtr<IDBKeyRange>>()>&& function)
{
    LOG(IndexedDB, "IDBObjectStore::deleteFunction");
    ASSERT(canCurrentThreadAccessThreadLocalData(m_transaction.database().originThread()));

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

    auto keyRange = function();
    if (keyRange.hasException())
        return keyRange.releaseException();

    IDBKeyRangeData keyRangeData = keyRange.returnValue().get();
    if (!keyRangeData.isValid())
        return Exception { DataError, "Failed to execute 'delete' on 'IDBObjectStore': The parameter is not a valid key range."_s };

    return m_transaction.requestDeleteRecord(*this, keyRangeData);
}

ExceptionOr<Ref<IDBRequest>> IDBObjectStore::deleteFunction(JSGlobalObject& execState, JSValue key)
{
    return doDelete([state = &execState, key]() {
        auto idbKey = scriptValueToIDBKey(*state, key);
        if (!idbKey->isValid())
            return ExceptionOr<RefPtr<IDBKeyRange>>{ Exception(DataError, "Failed to execute 'delete' on 'IDBObjectStore': The parameter is not a valid key."_s) };
        return ExceptionOr<RefPtr<IDBKeyRange>> { (IDBKeyRange::create(WTFMove(idbKey))).ptr() };
    });
}

ExceptionOr<Ref<IDBRequest>> IDBObjectStore::clear()
{
    LOG(IndexedDB, "IDBObjectStore::clear");
    ASSERT(canCurrentThreadAccessThreadLocalData(m_transaction.database().originThread()));

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

    return m_transaction.requestClearObjectStore(*this);
}

ExceptionOr<Ref<IDBIndex>> IDBObjectStore::createIndex(const String& name, IDBKeyPath&& keyPath, const IndexParameters& parameters)
{
    LOG(IndexedDB, "IDBObjectStore::createIndex %s (keyPath: %s, unique: %i, multiEntry: %i)", name.utf8().data(), loggingString(keyPath).utf8().data(), parameters.unique, parameters.multiEntry);
    ASSERT(canCurrentThreadAccessThreadLocalData(m_transaction.database().originThread()));

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

    if (parameters.multiEntry && std::holds_alternative<Vector<String>>(keyPath))
        return Exception { InvalidAccessError, "Failed to execute 'createIndex' on 'IDBObjectStore': The keyPath argument was an array and the multiEntry option is true."_s };

    // Install the new Index into the ObjectStore's info.
    IDBIndexInfo info = m_info.createNewIndex(m_transaction.database().info().generateNextIndexID(), name, WTFMove(keyPath), parameters.unique, parameters.multiEntry);
    m_transaction.database().didCreateIndexInfo(info);

    // Create the actual IDBObjectStore from the transaction, which also schedules the operation server side.
    auto index = m_transaction.createIndex(*this, info);

    Ref<IDBIndex> referencedIndex { *index };

    Locker locker { m_referencedIndexLock };
    m_referencedIndexes.set(name, WTFMove(index));

    return referencedIndex;
}

ExceptionOr<Ref<IDBIndex>> IDBObjectStore::index(const String& indexName)
{
    LOG(IndexedDB, "IDBObjectStore::index");
    ASSERT(canCurrentThreadAccessThreadLocalData(m_transaction.database().originThread()));

    if (!scriptExecutionContext())
        return Exception { InvalidStateError }; // FIXME: Is this code tested? Is iteven reachable?

    if (m_deleted)
        return Exception { InvalidStateError, "Failed to execute 'index' on 'IDBObjectStore': The object store has been deleted."_s };

    if (m_transaction.isFinishedOrFinishing())
        return Exception { InvalidStateError, "Failed to execute 'index' on 'IDBObjectStore': The transaction is finished."_s };

    Locker locker { m_referencedIndexLock };
    auto iterator = m_referencedIndexes.find(indexName);
    if (iterator != m_referencedIndexes.end())
        return Ref<IDBIndex> { *iterator->value };

    auto* info = m_info.infoForExistingIndex(indexName);
    if (!info)
        return Exception { NotFoundError, "Failed to execute 'index' on 'IDBObjectStore': The specified index was not found."_s };

    auto index = IDBIndex::create(*scriptExecutionContext(), *info, *this);

    Ref referencedIndex { index.get() };

    m_referencedIndexes.set(indexName, index.moveToUniquePtr());

    return referencedIndex;
}

ExceptionOr<void> IDBObjectStore::deleteIndex(const String& name)
{
    LOG(IndexedDB, "IDBObjectStore::deleteIndex %s", name.utf8().data());
    ASSERT(canCurrentThreadAccessThreadLocalData(m_transaction.database().originThread()));

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
        Locker locker { m_referencedIndexLock };
        if (auto index = m_referencedIndexes.take(name)) {
            index->markAsDeleted();
            auto identifier = index->info().identifier();
            m_deletedIndexes.add(identifier, WTFMove(index));
        }
    }

    m_transaction.deleteIndex(m_info.identifier(), name);

    return { };
}

ExceptionOr<Ref<IDBRequest>> IDBObjectStore::count(JSGlobalObject& execState, JSValue key)
{
    LOG(IndexedDB, "IDBObjectStore::count");

    auto idbKey = scriptValueToIDBKey(execState, key);

    return doCount(IDBKeyRangeData(idbKey->isValid() ? idbKey.ptr() : nullptr));
}

ExceptionOr<Ref<IDBRequest>> IDBObjectStore::count(IDBKeyRange* range)
{
    LOG(IndexedDB, "IDBObjectStore::count");

    return doCount(range ? IDBKeyRangeData(range) : IDBKeyRangeData::allKeys());
}

ExceptionOr<Ref<IDBRequest>> IDBObjectStore::doCount(const IDBKeyRangeData& range)
{
    ASSERT(canCurrentThreadAccessThreadLocalData(m_transaction.database().originThread()));

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
        return Exception { DataError, "Failed to execute 'count' on 'IDBObjectStore': The parameter is not a valid key."_s };

    return m_transaction.requestCount(*this, range);
}

ExceptionOr<Ref<IDBRequest>> IDBObjectStore::doGetAll(std::optional<uint32_t> count, Function<ExceptionOr<RefPtr<IDBKeyRange>>()>&& function)
{
    LOG(IndexedDB, "IDBObjectStore::getAll");
    ASSERT(canCurrentThreadAccessThreadLocalData(m_transaction.database().originThread()));

    if (m_deleted)
        return Exception { InvalidStateError, "Failed to execute 'getAll' on 'IDBObjectStore': The object store has been deleted."_s };

    if (!m_transaction.isActive())
        return Exception { TransactionInactiveError, "Failed to execute 'getAll' on 'IDBObjectStore': The transaction is inactive or finished."_s };

    auto keyRange = function();
    if (keyRange.hasException())
        return keyRange.releaseException();

    auto* keyRangePointer = keyRange.returnValue().get();
    return m_transaction.requestGetAllObjectStoreRecords(*this, keyRangePointer, IndexedDB::GetAllType::Values, count);
}

ExceptionOr<Ref<IDBRequest>> IDBObjectStore::getAll(RefPtr<IDBKeyRange>&& range, std::optional<uint32_t> count)
{
    return doGetAll(count, [range = WTFMove(range)]() {
        return range;
    });
}

ExceptionOr<Ref<IDBRequest>> IDBObjectStore::getAll(JSGlobalObject& execState, JSValue key, std::optional<uint32_t> count)
{
    return doGetAll(count, [state = &execState, key]() {
        auto onlyResult = IDBKeyRange::only(*state, key);
        if (onlyResult.hasException())
            return ExceptionOr<RefPtr<IDBKeyRange>>{ Exception(DataError, "Failed to execute 'getAll' on 'IDBObjectStore': The parameter is not a valid key."_s) };

        return ExceptionOr<RefPtr<IDBKeyRange>> { onlyResult.releaseReturnValue() };
    });
}

ExceptionOr<Ref<IDBRequest>> IDBObjectStore::doGetAllKeys(std::optional<uint32_t> count, Function<ExceptionOr<RefPtr<IDBKeyRange>>()>&& function)
{
    LOG(IndexedDB, "IDBObjectStore::getAllKeys");
    ASSERT(canCurrentThreadAccessThreadLocalData(m_transaction.database().originThread()));

    if (m_deleted)
        return Exception { InvalidStateError, "Failed to execute 'getAllKeys' on 'IDBObjectStore': The object store has been deleted."_s };

    if (!m_transaction.isActive())
        return Exception { TransactionInactiveError, "Failed to execute 'getAllKeys' on 'IDBObjectStore': The transaction is inactive or finished."_s };

    auto keyRange = function();
    if (keyRange.hasException())
        return keyRange.releaseException();

    auto* keyRangePointer = keyRange.returnValue().get();
    return m_transaction.requestGetAllObjectStoreRecords(*this, keyRangePointer, IndexedDB::GetAllType::Keys, count);
}

ExceptionOr<Ref<IDBRequest>> IDBObjectStore::getAllKeys(RefPtr<IDBKeyRange>&& range, std::optional<uint32_t> count)
{
    return doGetAllKeys(count, [range = WTFMove(range)]() {
        return range;
    });
}

ExceptionOr<Ref<IDBRequest>> IDBObjectStore::getAllKeys(JSGlobalObject& execState, JSValue key, std::optional<uint32_t> count)
{
    return doGetAllKeys(count, [state = &execState, key]() {
        auto onlyResult = IDBKeyRange::only(*state, key);
        if (onlyResult.hasException())
            return ExceptionOr<RefPtr<IDBKeyRange>>{ Exception(DataError, "Failed to execute 'getAllKeys' on 'IDBObjectStore': The parameter is not a valid key."_s) };

        return ExceptionOr<RefPtr<IDBKeyRange>> { onlyResult.releaseReturnValue() };
    });
}

void IDBObjectStore::markAsDeleted()
{
    ASSERT(canCurrentThreadAccessThreadLocalData(m_transaction.database().originThread()));
    m_deleted = true;
}

void IDBObjectStore::rollbackForVersionChangeAbort()
{
    ASSERT(canCurrentThreadAccessThreadLocalData(m_transaction.database().originThread()));

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

    Locker locker { m_referencedIndexLock };

    Vector<uint64_t> identifiersToRemove;
    Vector<std::unique_ptr<IDBIndex>> indexesToDelete;
    for (auto& iterator : m_deletedIndexes) {
        if (m_info.hasIndex(iterator.key)) {
            auto name = iterator.value->info().name();
            auto result = m_referencedIndexes.add(name, nullptr);
            if (!result.isNewEntry)
                indexesToDelete.append(std::exchange(result.iterator->value, nullptr));
            result.iterator->value = std::exchange(iterator.value, nullptr);
            identifiersToRemove.append(iterator.key);
        }
    }

    for (auto identifier : identifiersToRemove)
        m_deletedIndexes.remove(identifier);

    for (auto& index : m_referencedIndexes.values())
        index->rollbackInfoForVersionChangeAbort();

    for (auto& index : indexesToDelete) {
        index->rollbackInfoForVersionChangeAbort();
        auto indexIdentifier = index->info().identifier();
        m_deletedIndexes.set(indexIdentifier, std::exchange(index, nullptr));
    }
}

template<typename Visitor>
void IDBObjectStore::visitReferencedIndexes(Visitor& visitor) const
{
    Locker locker { m_referencedIndexLock };
    for (auto& index : m_referencedIndexes.values())
        addWebCoreOpaqueRoot(visitor, index.get());
    for (auto& index : m_deletedIndexes.values())
        addWebCoreOpaqueRoot(visitor, index.get());
}

template void IDBObjectStore::visitReferencedIndexes(AbstractSlotVisitor&) const;
template void IDBObjectStore::visitReferencedIndexes(SlotVisitor&) const;

void IDBObjectStore::renameReferencedIndex(IDBIndex& index, const String& newName)
{
    Locker locker { m_referencedIndexLock };
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

WebCoreOpaqueRoot root(IDBObjectStore* store)
{
    return WebCoreOpaqueRoot { store };
}

} // namespace WebCore

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
#include "LegacyObjectStore.h"

#if ENABLE(INDEXED_DATABASE)

#include "DOMStringList.h"
#include "IDBAny.h"
#include "IDBBindingUtilities.h"
#include "IDBCursorWithValue.h"
#include "IDBDatabase.h"
#include "IDBDatabaseException.h"
#include "IDBKey.h"
#include "IDBKeyData.h"
#include "IDBKeyPath.h"
#include "IDBKeyRange.h"
#include "IDBTransaction.h"
#include "LegacyCursorWithValue.h"
#include "LegacyIndex.h"
#include "Logging.h"
#include "ScriptExecutionContext.h"
#include "SerializedScriptValue.h"
#include "SharedBuffer.h"

namespace WebCore {

LegacyObjectStore::LegacyObjectStore(const IDBObjectStoreMetadata& metadata, LegacyTransaction* transaction)
    : m_metadata(metadata)
    , m_transaction(transaction)
    , m_deleted(false)
{
    ASSERT(m_transaction);
    // We pass a reference to this object before it can be adopted.
    relaxAdoptionRequirement();
}

RefPtr<DOMStringList> LegacyObjectStore::indexNames() const
{
    LOG(StorageAPI, "LegacyObjectStore::indexNames");
    RefPtr<DOMStringList> indexNames = DOMStringList::create();
    for (auto& index : m_metadata.indexes.values())
        indexNames->append(index.name);
    indexNames->sort();
    return indexNames.release();
}

RefPtr<IDBRequest> LegacyObjectStore::get(ScriptExecutionContext* context, IDBKeyRange* keyRange, ExceptionCodeWithMessage& ec)
{
    LOG(StorageAPI, "LegacyObjectStore::get");
    if (m_deleted) {
        ec.code = IDBDatabaseException::InvalidStateError;
        return 0;
    }
    if (!keyRange) {
        ec.code = IDBDatabaseException::DataError;
        return 0;
    }
    if (!m_transaction->isActive()) {
        ec.code = IDBDatabaseException::TransactionInactiveError;
        return 0;
    }
    RefPtr<LegacyRequest> request = LegacyRequest::create(context, LegacyAny::create(this), m_transaction.get());
    backendDB()->get(m_transaction->id(), id(), IDBIndexMetadata::InvalidId, keyRange, false, request);
    return request.release();
}

RefPtr<IDBRequest> LegacyObjectStore::get(ScriptExecutionContext* context, const Deprecated::ScriptValue& key, ExceptionCodeWithMessage& ec)
{
    RefPtr<IDBKeyRange> keyRange = IDBKeyRange::only(context, key, ec.code);
    if (ec.code)
        return 0;
    return get(context, keyRange.get(), ec);
}

RefPtr<IDBRequest> LegacyObjectStore::add(JSC::ExecState& state, JSC::JSValue value, JSC::JSValue key, ExceptionCodeWithMessage& ec)
{
    LOG(StorageAPI, "LegacyObjectStore::add");
    auto deprecatedValue = Deprecated::ScriptValue(state.vm(), value);
    auto dKey = Deprecated::ScriptValue(state.vm(), key);
    return put(IDBDatabaseBackend::AddOnly, LegacyAny::create(this), state, deprecatedValue, dKey, ec);
}

RefPtr<IDBRequest> LegacyObjectStore::add(JSC::ExecState& state, JSC::JSValue value, ExceptionCodeWithMessage& ec)
{
    LOG(StorageAPI, "LegacyObjectStore::add");
    auto deprecatedValue = Deprecated::ScriptValue(state.vm(), value);
    return put(IDBDatabaseBackend::AddOnly, LegacyAny::create(this), state, deprecatedValue, static_cast<IDBKey*>(nullptr), ec);
}

RefPtr<IDBRequest> LegacyObjectStore::put(JSC::ExecState& state, JSC::JSValue value, JSC::JSValue key, ExceptionCodeWithMessage& ec)
{
    LOG(StorageAPI, "LegacyObjectStore::put");
    auto deprecatedValue = Deprecated::ScriptValue(state.vm(), value);
    return put(IDBDatabaseBackend::AddOrUpdate, LegacyAny::create(this), state, deprecatedValue, Deprecated::ScriptValue(state.vm(), key), ec);
}

RefPtr<IDBRequest> LegacyObjectStore::put(JSC::ExecState& state, JSC::JSValue value, ExceptionCodeWithMessage& ec)
{
    LOG(StorageAPI, "LegacyObjectStore::put");
    auto deprecatedValue = Deprecated::ScriptValue(state.vm(), value);
    return put(IDBDatabaseBackend::AddOrUpdate, LegacyAny::create(this), state, deprecatedValue, static_cast<IDBKey*>(nullptr), ec);
}

RefPtr<IDBRequest> LegacyObjectStore::put(IDBDatabaseBackend::PutMode putMode, RefPtr<LegacyAny> source, JSC::ExecState& state, Deprecated::ScriptValue& value, const Deprecated::ScriptValue& keyValue, ExceptionCodeWithMessage& ec)
{
    ScriptExecutionContext* context = scriptExecutionContextFromExecState(&state);
    DOMRequestState requestState(context);
    RefPtr<IDBKey> key = scriptValueToIDBKey(&requestState, keyValue);
    return put(putMode, source, state, value, key.release(), ec);
}

RefPtr<IDBRequest> LegacyObjectStore::put(IDBDatabaseBackend::PutMode putMode, RefPtr<LegacyAny> source, JSC::ExecState& state, Deprecated::ScriptValue& value, RefPtr<IDBKey> prpKey, ExceptionCodeWithMessage& ec)
{
    RefPtr<IDBKey> key = prpKey;
    if (m_deleted) {
        ec.code = IDBDatabaseException::InvalidStateError;
        return nullptr;
    }
    if (!m_transaction->isActive()) {
        ec.code = IDBDatabaseException::TransactionInactiveError;
        return nullptr;
    }
    if (m_transaction->isReadOnly()) {
        ec.code = IDBDatabaseException::ReadOnlyError;
        return nullptr;
    }

    RefPtr<SerializedScriptValue> serializedValue = SerializedScriptValue::create(&state, value.jsValue(), nullptr, nullptr);
    if (state.hadException())
        return nullptr;

    if (serializedValue->hasBlobURLs()) {
        // FIXME: Add Blob/File/FileList support
        ec.code = IDBDatabaseException::DataCloneError;
        return nullptr;
    }

    const IDBKeyPath& keyPath = m_metadata.keyPath;
    const bool usesInLineKeys = !keyPath.isNull();
    const bool hasKeyGenerator = autoIncrement();

    ScriptExecutionContext* context = scriptExecutionContextFromExecState(&state);
    DOMRequestState requestState(context);

    if (putMode != IDBDatabaseBackend::CursorUpdate && usesInLineKeys && key) {
        ec.code = IDBDatabaseException::DataError;
        return nullptr;
    }
    if (!usesInLineKeys && !hasKeyGenerator && !key) {
        ec.code = IDBDatabaseException::DataError;
        return nullptr;
    }
    if (usesInLineKeys) {
        RefPtr<IDBKey> keyPathKey = createIDBKeyFromScriptValueAndKeyPath(requestState.exec(), value, keyPath);
        if (keyPathKey && !keyPathKey->isValid()) {
            ec.code = IDBDatabaseException::DataError;
            return nullptr;
        }
        if (!hasKeyGenerator && !keyPathKey) {
            ec.code = IDBDatabaseException::DataError;
            return nullptr;
        }
        if (hasKeyGenerator && !keyPathKey) {
            if (!canInjectIDBKeyIntoScriptValue(&requestState, value, keyPath)) {
                ec.code = IDBDatabaseException::DataError;
                return nullptr;
            }
        }
        if (keyPathKey)
            key = keyPathKey;
    }
    if (key && !key->isValid()) {
        ec.code = IDBDatabaseException::DataError;
        return nullptr;
    }

    Vector<int64_t> indexIds;
    Vector<IndexKeys> indexKeys;
    for (auto& index : m_metadata.indexes) {
        Vector<IDBKeyData> keyDatas;
        generateIndexKeysForValue(requestState.exec(), index.value, value, keyDatas);
        indexIds.append(index.key);

        // FIXME: Much of the Indexed DB code needs to use IDBKeyData directly to avoid wasteful conversions like this.
        Vector<RefPtr<IDBKey>> keys;
        for (auto& keyData : keyDatas) {
            RefPtr<IDBKey> key = keyData.maybeCreateIDBKey();
            if (key)
                keys.append(key.release());
        }
        indexKeys.append(keys);
    }

    RefPtr<LegacyRequest> request = LegacyRequest::create(context, source, m_transaction.get());
    Vector<uint8_t> valueBytes = serializedValue->toWireBytes();
    // This is a hack to account for disagreements about whether SerializedScriptValue should deal in Vector<uint8_t> or Vector<char>.
    // See https://lists.webkit.org/pipermail/webkit-dev/2013-February/023682.html
    Vector<char>* valueBytesSigned = reinterpret_cast<Vector<char>*>(&valueBytes);
    RefPtr<SharedBuffer> valueBuffer = SharedBuffer::adoptVector(*valueBytesSigned);
    backendDB()->put(m_transaction->id(), id(), valueBuffer, key.release(), static_cast<IDBDatabaseBackend::PutMode>(putMode), request, indexIds, indexKeys);
    return request.release();
}

RefPtr<IDBRequest> LegacyObjectStore::deleteFunction(ScriptExecutionContext* context, IDBKeyRange* keyRange, ExceptionCodeWithMessage& ec)
{
    LOG(StorageAPI, "LegacyObjectStore::delete");
    if (m_deleted) {
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
    if (!keyRange) {
        ec.code = IDBDatabaseException::DataError;
        return 0;
    }

    RefPtr<LegacyRequest> request = LegacyRequest::create(context, LegacyAny::create(this), m_transaction.get());
    backendDB()->deleteRange(m_transaction->id(), id(), keyRange, request);
    return request.release();
}

RefPtr<IDBRequest> LegacyObjectStore::deleteFunction(ScriptExecutionContext* context, const Deprecated::ScriptValue& key, ExceptionCodeWithMessage& ec)
{
    RefPtr<IDBKeyRange> keyRange = IDBKeyRange::only(context, key, ec.code);
    if (ec.code)
        return 0;
    return deleteFunction(context, keyRange.get(), ec);
}

RefPtr<IDBRequest> LegacyObjectStore::clear(ScriptExecutionContext* context, ExceptionCodeWithMessage& ec)
{
    LOG(StorageAPI, "LegacyObjectStore::clear");
    if (m_deleted) {
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

    RefPtr<LegacyRequest> request = LegacyRequest::create(context, LegacyAny::create(this), m_transaction.get());
    backendDB()->clearObjectStore(m_transaction->id(), id(), request);
    return request.release();
}

namespace {
// This class creates the index keys for a given index by extracting
// them from the SerializedScriptValue, for all the existing values in
// the objectStore. It only needs to be kept alive by virtue of being
// a listener on an IDBRequest object, in the same way that JavaScript
// cursor success handlers are kept alive.
class IndexPopulator : public EventListener {
public:
    static Ref<IndexPopulator> create(RefPtr<IDBDatabaseBackend> backend, int64_t transactionId, int64_t objectStoreId, const IDBIndexMetadata& indexMetadata)
    {
        return adoptRef(*new IndexPopulator(backend, transactionId, objectStoreId, indexMetadata));
    }

    virtual bool operator==(const EventListener& other)
    {
        return this == &other;
    }

private:
    IndexPopulator(RefPtr<IDBDatabaseBackend> backend, int64_t transactionId, int64_t objectStoreId, const IDBIndexMetadata& indexMetadata)
        : EventListener(CPPEventListenerType)
        , m_databaseBackend(backend)
        , m_transactionId(transactionId)
        , m_objectStoreId(objectStoreId)
        , m_indexMetadata(indexMetadata)
    {
    }

    virtual void handleEvent(ScriptExecutionContext*, Event* event)
    {
        ASSERT(event->type() == eventNames().successEvent);
        EventTarget* target = event->target();
        LegacyRequest* request = static_cast<LegacyRequest*>(target);

        RefPtr<LegacyAny> cursorAny = request->legacyResult(ASSERT_NO_EXCEPTION);
        RefPtr<LegacyCursorWithValue> cursor;
        if (cursorAny->type() == IDBAny::Type::IDBCursorWithValue)
            cursor = cursorAny->legacyCursorWithValue();

        Vector<int64_t, 1> indexIds;
        indexIds.append(m_indexMetadata.id);
        if (cursor) {
            ExceptionCodeWithMessage ec;
            cursor->continueFunction(static_cast<IDBKey*>(nullptr), ec);
            ASSERT(!ec.code);

            RefPtr<IDBKey> primaryKey = cursor->idbPrimaryKey();
            Deprecated::ScriptValue value = cursor->value();

            Vector<IDBKeyData> indexKeyDatas;
            generateIndexKeysForValue(request->requestState()->exec(), m_indexMetadata, value, indexKeyDatas);

            Vector<RefPtr<IDBKey>> indexKeys;
            for (auto& indexKeyData : indexKeyDatas) {
                RefPtr<IDBKey> key = indexKeyData.maybeCreateIDBKey();
                if (key)
                    indexKeys.append(key.release());
            }
            Vector<LegacyObjectStore::IndexKeys, 1> indexKeysList;
            indexKeysList.append(indexKeys);

            m_databaseBackend->setIndexKeys(m_transactionId, m_objectStoreId, primaryKey, indexIds, indexKeysList);
        } else {
            // Now that we are done indexing, tell the backend to go
            // back to processing tasks of type NormalTask.
            m_databaseBackend->setIndexesReady(m_transactionId, m_objectStoreId, indexIds);
            m_databaseBackend = nullptr;
        }

    }

    RefPtr<IDBDatabaseBackend> m_databaseBackend;
    const int64_t m_transactionId;
    const int64_t m_objectStoreId;
    const IDBIndexMetadata m_indexMetadata;
};
}

RefPtr<IDBIndex> LegacyObjectStore::createIndex(ScriptExecutionContext* context, const String& name, const IDBKeyPath& keyPath, const Dictionary& options, ExceptionCodeWithMessage& ec)
{
    bool unique = false;
    options.get("unique", unique);

    bool multiEntry = false;
    options.get("multiEntry", multiEntry);

    return createIndex(context, name, keyPath, unique, multiEntry, ec);
}

RefPtr<IDBIndex> LegacyObjectStore::createIndex(ScriptExecutionContext* context, const String& name, const IDBKeyPath& keyPath, bool unique, bool multiEntry, ExceptionCodeWithMessage& ec)
{
    LOG(StorageAPI, "LegacyObjectStore::createIndex");
    if (!m_transaction->isVersionChange() || m_deleted) {
        ec.code = IDBDatabaseException::InvalidStateError;
        return 0;
    }
    if (!m_transaction->isActive()) {
        ec.code = IDBDatabaseException::TransactionInactiveError;
        return 0;
    }
    if (!keyPath.isValid()) {
        ec.code = IDBDatabaseException::SyntaxError;
        return 0;
    }
    if (name.isNull()) {
        ec.code = TypeError;
        return 0;
    }
    if (containsIndex(name)) {
        ec.code = IDBDatabaseException::ConstraintError;
        return 0;
    }

    if (keyPath.type() == IndexedDB::KeyPathType::Array && multiEntry) {
        ec.code = IDBDatabaseException::InvalidAccessError;
        return 0;
    }

    int64_t indexId = m_metadata.maxIndexId + 1;
    backendDB()->createIndex(m_transaction->id(), id(), indexId, name, keyPath, unique, multiEntry);

    ++m_metadata.maxIndexId;

    IDBIndexMetadata metadata(name, indexId, keyPath, unique, multiEntry);
    RefPtr<LegacyIndex> index = LegacyIndex::create(metadata, this, m_transaction.get());
    m_indexMap.set(name, index);
    m_metadata.indexes.set(indexId, metadata);

    ASSERT(!ec.code);
    if (ec.code)
        return 0;

    ASSERT_UNUSED(context, context);
    return index.release();
}

RefPtr<IDBIndex> LegacyObjectStore::index(const String& name, ExceptionCodeWithMessage& ec)
{
    LOG(StorageAPI, "LegacyObjectStore::index");
    if (m_deleted) {
        ec.code = IDBDatabaseException::InvalidStateError;
        return 0;
    }
    if (m_transaction->isFinished()) {
        ec.code = IDBDatabaseException::InvalidStateError;
        return 0;
    }

    IDBIndexMap::iterator it = m_indexMap.find(name);
    if (it != m_indexMap.end())
        return it->value;

    int64_t indexId = findIndexId(name);
    if (indexId == IDBIndexMetadata::InvalidId) {
        ec.code = IDBDatabaseException::NotFoundError;
        return 0;
    }

    const IDBIndexMetadata* indexMetadata(nullptr);
    for (auto& index : m_metadata.indexes.values()) {
        if (index.name == name) {
            indexMetadata = &index;
            break;
        }
    }
    ASSERT(indexMetadata);
    ASSERT(indexMetadata->id != IDBIndexMetadata::InvalidId);

    RefPtr<LegacyIndex> index = LegacyIndex::create(*indexMetadata, this, m_transaction.get());
    m_indexMap.set(name, index);
    return index.release();
}

void LegacyObjectStore::deleteIndex(const String& name, ExceptionCodeWithMessage& ec)
{
    LOG(StorageAPI, "LegacyObjectStore::deleteIndex");
    if (!m_transaction->isVersionChange() || m_deleted) {
        ec.code = IDBDatabaseException::InvalidStateError;
        return;
    }
    if (!m_transaction->isActive()) {
        ec.code = IDBDatabaseException::TransactionInactiveError;
        return;
    }
    int64_t indexId = findIndexId(name);
    if (indexId == IDBIndexMetadata::InvalidId) {
        ec.code = IDBDatabaseException::NotFoundError;
        return;
    }

    backendDB()->deleteIndex(m_transaction->id(), id(), indexId);

    m_metadata.indexes.remove(indexId);
    IDBIndexMap::iterator it = m_indexMap.find(name);
    if (it != m_indexMap.end()) {
        it->value->markDeleted();
        m_indexMap.remove(name);
    }
}

RefPtr<IDBRequest> LegacyObjectStore::openCursor(ScriptExecutionContext* context, ExceptionCodeWithMessage& ec)
{
    return openCursor(context, static_cast<IDBKeyRange*>(nullptr), ec);
}

RefPtr<IDBRequest> LegacyObjectStore::openCursor(ScriptExecutionContext* context, IDBKeyRange* keyRange, ExceptionCodeWithMessage& ec)
{
    return openCursor(context, keyRange, IDBCursor::directionNext(), ec);
}

RefPtr<IDBRequest> LegacyObjectStore::openCursor(ScriptExecutionContext* context, const Deprecated::ScriptValue& key, ExceptionCodeWithMessage& ec)
{
    return openCursor(context, key, IDBCursor::directionNext(), ec);
}

RefPtr<IDBRequest> LegacyObjectStore::openCursor(ScriptExecutionContext* context, IDBKeyRange* range, const String& direction, ExceptionCodeWithMessage& ec)
{
    return openCursor(context, range, direction, IDBDatabaseBackend::NormalTask, ec);
}

RefPtr<IDBRequest> LegacyObjectStore::openCursor(ScriptExecutionContext* context, IDBKeyRange* range, const String& directionString, IDBDatabaseBackend::TaskType taskType, ExceptionCodeWithMessage& ec)
{
    LOG(StorageAPI, "LegacyObjectStore::openCursor");
    if (m_deleted) {
        ec.code = IDBDatabaseException::InvalidStateError;
        return 0;
    }
    if (!m_transaction->isActive()) {
        ec.code = IDBDatabaseException::TransactionInactiveError;
        return 0;
    }
    IndexedDB::CursorDirection direction = IDBCursor::stringToDirection(directionString, ec.code);
    if (ec.code)
        return 0;

    RefPtr<LegacyRequest> request = LegacyRequest::create(context, LegacyAny::create(this), m_transaction.get());
    request->setCursorDetails(IndexedDB::CursorType::KeyAndValue, direction);

    backendDB()->openCursor(m_transaction->id(), id(), IDBIndexMetadata::InvalidId, range, direction, false, static_cast<IDBDatabaseBackend::TaskType>(taskType), request);
    return request.release();
}

RefPtr<IDBRequest> LegacyObjectStore::openCursor(ScriptExecutionContext* context, const Deprecated::ScriptValue& key, const String& direction, ExceptionCodeWithMessage& ec)
{
    RefPtr<IDBKeyRange> keyRange = IDBKeyRange::only(context, key, ec.code);
    if (ec.code)
        return 0;
    return openCursor(context, keyRange.get(), direction, ec);
}

RefPtr<IDBRequest> LegacyObjectStore::count(ScriptExecutionContext* context, IDBKeyRange* range, ExceptionCodeWithMessage& ec)
{
    LOG(StorageAPI, "LegacyObjectStore::count");
    if (m_deleted) {
        ec.code = IDBDatabaseException::InvalidStateError;
        return 0;
    }
    if (!m_transaction->isActive()) {
        ec.code = IDBDatabaseException::TransactionInactiveError;
        return 0;
    }
    RefPtr<LegacyRequest> request = LegacyRequest::create(context, LegacyAny::create(this), m_transaction.get());
    backendDB()->count(m_transaction->id(), id(), IDBIndexMetadata::InvalidId, range, request);
    return request.release();
}

RefPtr<IDBRequest> LegacyObjectStore::count(ScriptExecutionContext* context, const Deprecated::ScriptValue& key, ExceptionCodeWithMessage& ec)
{
    RefPtr<IDBKeyRange> keyRange = IDBKeyRange::only(context, key, ec.code);
    if (ec.code)
        return 0;
    return count(context, keyRange.get(), ec);
}

void LegacyObjectStore::transactionFinished()
{
    ASSERT(m_transaction->isFinished());

    // Break reference cycles.
    m_indexMap.clear();
}

int64_t LegacyObjectStore::findIndexId(const String& name) const
{
    for (auto& index : m_metadata.indexes) {
        if (index.value.name == name) {
            ASSERT(index.key != IDBIndexMetadata::InvalidId);
            return index.key;
        }
    }
    return IDBIndexMetadata::InvalidId;
}

IDBDatabaseBackend* LegacyObjectStore::backendDB() const
{
    return m_transaction->backendDB();
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)

/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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
#include "IDBObjectStoreBackendImpl.h"

#if ENABLE(INDEXED_DATABASE)

#include "IDBBackingStore.h"
#include "IDBBindingUtilities.h"
#include "IDBCallbacks.h"
#include "IDBCursorBackendImpl.h"
#include "IDBDatabaseBackendImpl.h"
#include "IDBDatabaseException.h"
#include "IDBIndexBackendImpl.h"
#include "IDBKey.h"
#include "IDBKeyPath.h"
#include "IDBKeyRange.h"
#include "IDBTracing.h"
#include "IDBTransactionBackendImpl.h"
#include <wtf/MathExtras.h>

namespace WebCore {

class IDBObjectStoreBackendImpl::CreateIndexOperation : public IDBTransactionBackendImpl::Operation {
public:
    static PassOwnPtr<IDBTransactionBackendImpl::Operation> create(PassRefPtr<IDBObjectStoreBackendImpl> objectStore, PassRefPtr<IDBIndexBackendImpl> index)
    {
        return adoptPtr(new CreateIndexOperation(objectStore, index));
    }
    virtual void perform(IDBTransactionBackendImpl*);
private:
    CreateIndexOperation(PassRefPtr<IDBObjectStoreBackendImpl> objectStore, PassRefPtr<IDBIndexBackendImpl> index)
        : m_objectStore(objectStore)
        , m_index(index)
    {
    }

    RefPtr<IDBObjectStoreBackendImpl> m_objectStore;
    RefPtr<IDBIndexBackendImpl> m_index;
};

class IDBObjectStoreBackendImpl::DeleteIndexOperation : public IDBTransactionBackendImpl::Operation {
public:
    static PassOwnPtr<IDBTransactionBackendImpl::Operation> create(PassRefPtr<IDBObjectStoreBackendImpl> objectStore, PassRefPtr<IDBIndexBackendImpl> index)
    {
        return adoptPtr(new DeleteIndexOperation(objectStore, index));
    }
    virtual void perform(IDBTransactionBackendImpl*);
private:
    DeleteIndexOperation(PassRefPtr<IDBObjectStoreBackendImpl> objectStore, PassRefPtr<IDBIndexBackendImpl> index)
        : m_objectStore(objectStore)
        , m_index(index)
    {
    }

    RefPtr<IDBObjectStoreBackendImpl> m_objectStore;
    RefPtr<IDBIndexBackendImpl> m_index;
};

class IDBObjectStoreBackendImpl::CreateIndexAbortOperation : public IDBTransactionBackendImpl::Operation {
public:
    static PassOwnPtr<IDBTransactionBackendImpl::Operation> create(PassRefPtr<IDBObjectStoreBackendImpl> objectStore, PassRefPtr<IDBIndexBackendImpl> index)
    {
        return adoptPtr(new CreateIndexAbortOperation(objectStore, index));
    }
    virtual void perform(IDBTransactionBackendImpl*);
private:
    CreateIndexAbortOperation(PassRefPtr<IDBObjectStoreBackendImpl> objectStore, PassRefPtr<IDBIndexBackendImpl> index)
        : m_objectStore(objectStore)
        , m_index(index)
    {
    }

    RefPtr<IDBObjectStoreBackendImpl> m_objectStore;
    RefPtr<IDBIndexBackendImpl> m_index;
};

class IDBObjectStoreBackendImpl::DeleteIndexAbortOperation : public IDBTransactionBackendImpl::Operation {
public:
    static PassOwnPtr<IDBTransactionBackendImpl::Operation> create(PassRefPtr<IDBObjectStoreBackendImpl> objectStore, PassRefPtr<IDBIndexBackendImpl> index)
    {
        return adoptPtr(new DeleteIndexAbortOperation(objectStore, index));
    }
    virtual void perform(IDBTransactionBackendImpl*);
private:
    DeleteIndexAbortOperation(PassRefPtr<IDBObjectStoreBackendImpl> objectStore, PassRefPtr<IDBIndexBackendImpl> index)
        : m_objectStore(objectStore)
        , m_index(index)
    {
    }

    RefPtr<IDBObjectStoreBackendImpl> m_objectStore;
    RefPtr<IDBIndexBackendImpl> m_index;
};


IDBObjectStoreBackendImpl::~IDBObjectStoreBackendImpl()
{
}

IDBObjectStoreBackendImpl::IDBObjectStoreBackendImpl(const IDBDatabaseBackendImpl* database, const IDBObjectStoreMetadata& metadata)
    : m_database(database)
    , m_metadata(metadata)
{
    loadIndexes();
}

IDBObjectStoreMetadata IDBObjectStoreBackendImpl::metadata() const
{
    IDBObjectStoreMetadata metadata(m_metadata);
    for (IndexMap::const_iterator it = m_indexes.begin(); it != m_indexes.end(); ++it)
        metadata.indexes.set(it->key, it->value->metadata());
    return metadata;
}

bool IDBObjectStoreBackendImpl::IndexWriter::verifyIndexKeys(IDBBackingStore& backingStore, IDBBackingStore::Transaction* transaction, int64_t databaseId, int64_t objectStoreId, int64_t indexId, bool& canAddKeys, const IDBKey* primaryKey, String* errorMessage) const
{
    canAddKeys = false;
    for (size_t i = 0; i < m_indexKeys.size(); ++i) {
        bool ok = addingKeyAllowed(backingStore, transaction, databaseId, objectStoreId, indexId, (m_indexKeys)[i].get(), primaryKey, canAddKeys);
        if (!ok)
            return false;
        if (!canAddKeys) {
            if (errorMessage)
                *errorMessage = String::format("Unable to add key to index '%s': at least one key does not satisfy the uniqueness requirements.", m_indexMetadata.name.utf8().data());
            return true;
        }
    }
    canAddKeys = true;
    return true;
}

void IDBObjectStoreBackendImpl::IndexWriter::writeIndexKeys(const IDBBackingStore::RecordIdentifier& recordIdentifier, IDBBackingStore& backingStore, IDBBackingStore::Transaction* transaction, int64_t databaseId, int64_t objectStoreId) const
{
    int64_t indexId = m_indexMetadata.id;
    for (size_t i = 0; i < m_indexKeys.size(); ++i) {
        backingStore.putIndexDataForRecord(transaction, databaseId, objectStoreId, indexId, *(m_indexKeys)[i].get(), recordIdentifier);
    }
}

bool IDBObjectStoreBackendImpl::IndexWriter::addingKeyAllowed(IDBBackingStore& backingStore, IDBBackingStore::Transaction* transaction, int64_t databaseId, int64_t objectStoreId, int64_t indexId, const IDBKey* indexKey, const IDBKey* primaryKey, bool& allowed) const
{
    allowed = false;
    if (!m_indexMetadata.unique) {
        allowed = true;
        return true;
    }

    RefPtr<IDBKey> foundPrimaryKey;
    bool found = false;
    bool ok = backingStore.keyExistsInIndex(transaction, databaseId, objectStoreId, indexId, *indexKey, foundPrimaryKey, found);
    if (!ok)
        return false;
    if (!found || (primaryKey && foundPrimaryKey->isEqual(primaryKey)))
        allowed = true;
    return true;
}

bool IDBObjectStoreBackendImpl::makeIndexWriters(PassRefPtr<IDBTransactionBackendImpl> transaction, IDBBackingStore* backingStore, int64_t databaseId, const IDBObjectStoreMetadata& objectStore, PassRefPtr<IDBKey> primaryKey, bool keyWasGenerated, const Vector<int64_t>& indexIds, const Vector<IDBObjectStoreBackendInterface::IndexKeys>& indexKeys, Vector<OwnPtr<IndexWriter> >* indexWriters, String* errorMessage, bool& completed)
{
    ASSERT(indexIds.size() == indexKeys.size());
    completed = false;

    HashMap<int64_t, IDBObjectStoreBackendInterface::IndexKeys> indexKeyMap;
    for (size_t i = 0; i < indexIds.size(); ++i)
        indexKeyMap.add(indexIds[i], indexKeys[i]);

    for (IDBObjectStoreMetadata::IndexMap::const_iterator it = objectStore.indexes.begin(); it != objectStore.indexes.end(); ++it) {

        const IDBIndexMetadata& index = it->value;

        IDBObjectStoreBackendInterface::IndexKeys keys = indexKeyMap.get(it->key);
        // If the objectStore is using autoIncrement, then any indexes with an identical keyPath need to also use the primary (generated) key as a key.
        if (keyWasGenerated && (index.keyPath == objectStore.keyPath))
            keys.append(primaryKey);

        OwnPtr<IndexWriter> indexWriter(adoptPtr(new IndexWriter(index, keys)));
        bool canAddKeys = false;
        bool backingStoreSuccess = indexWriter->verifyIndexKeys(*backingStore, transaction->backingStoreTransaction(), databaseId, objectStore.id, index.id, canAddKeys, primaryKey.get(), errorMessage);
        if (!backingStoreSuccess)
            return false;
        if (!canAddKeys)
            return true;

        indexWriters->append(indexWriter.release());
    }

    completed = true;
    return true;
}

PassRefPtr<IDBIndexBackendInterface> IDBObjectStoreBackendImpl::createIndex(int64_t id, const String& name, const IDBKeyPath& keyPath, bool unique, bool multiEntry, IDBTransactionBackendInterface* transactionPtr, ExceptionCode& ec)
{
    ASSERT_WITH_MESSAGE(!m_indexes.contains(id), "Indexes already contain '%s'", name.utf8().data());

    RefPtr<IDBIndexBackendImpl> index = IDBIndexBackendImpl::create(m_database, IDBIndexMetadata(name, id, keyPath, unique, multiEntry));
    ASSERT(index->name() == name);

    RefPtr<IDBTransactionBackendImpl> transaction = IDBTransactionBackendImpl::from(transactionPtr);
    ASSERT(transaction->mode() == IDBTransaction::VERSION_CHANGE);
    ASSERT(id > m_metadata.maxIndexId);
    m_metadata.maxIndexId = id;

    if (!transaction->scheduleTask(CreateIndexOperation::create(this, index), CreateIndexAbortOperation::create(this, index))) {
        ec = IDBDatabaseException::TransactionInactiveError;
        return 0;
    }

    m_indexes.set(id, index);
    return index.release();
}

void IDBObjectStoreBackendImpl::CreateIndexOperation::perform(IDBTransactionBackendImpl* transaction)
{
    IDB_TRACE("CreateIndexOperation");
    if (!m_objectStore->backingStore()->createIndex(transaction->backingStoreTransaction(), m_objectStore->databaseId(), m_objectStore->id(), m_index->id(), m_index->name(), m_index->keyPath(), m_index->unique(), m_index->multiEntry())) {
        transaction->abort(IDBDatabaseError::create(IDBDatabaseException::UnknownError, String::format("Internal error when trying to create index '%s'.", m_index->name().utf8().data())));
        return;
    }
}

PassRefPtr<IDBIndexBackendInterface> IDBObjectStoreBackendImpl::index(int64_t indexId)
{
    RefPtr<IDBIndexBackendInterface> index = m_indexes.get(indexId);
    ASSERT(index);
    return index.release();
}

void IDBObjectStoreBackendImpl::deleteIndex(int64_t indexId, IDBTransactionBackendInterface* transactionPtr, ExceptionCode& ec)
{
    ASSERT(m_indexes.contains(indexId));

    RefPtr<IDBIndexBackendImpl> index = m_indexes.get(indexId);
    RefPtr<IDBTransactionBackendImpl> transaction = IDBTransactionBackendImpl::from(transactionPtr);
    ASSERT(transaction->mode() == IDBTransaction::VERSION_CHANGE);

    if (!transaction->scheduleTask(DeleteIndexOperation::create(this, index), DeleteIndexAbortOperation::create(this, index))) {
        ec = IDBDatabaseException::TransactionInactiveError;
        return;
    }
    m_indexes.remove(indexId);
}

void IDBObjectStoreBackendImpl::DeleteIndexOperation::perform(IDBTransactionBackendImpl* transaction)
{
    IDB_TRACE("DeleteIndexOperation");
    m_objectStore->backingStore()->deleteIndex(transaction->backingStoreTransaction(), m_objectStore->databaseId(), m_objectStore->id(), m_index->id());
}

void IDBObjectStoreBackendImpl::loadIndexes()
{
    Vector<IDBIndexMetadata> indexes = backingStore()->getIndexes(databaseId(), m_metadata.id);

    for (size_t i = 0; i < indexes.size(); ++i)
        m_indexes.set(indexes[i].id, IDBIndexBackendImpl::create(m_database, indexes[i]));
}

void IDBObjectStoreBackendImpl::CreateIndexAbortOperation::perform(IDBTransactionBackendImpl* transaction)
{
    ASSERT(!transaction);
    ASSERT(m_objectStore->m_indexes.contains(m_index->id()));
    m_objectStore->m_indexes.remove(m_index->id());
}

void IDBObjectStoreBackendImpl::DeleteIndexAbortOperation::perform(IDBTransactionBackendImpl* transaction)
{
    ASSERT(!transaction);
    ASSERT(!m_objectStore->m_indexes.contains(m_index->id()));
    m_objectStore->m_indexes.set(m_index->id(), m_index);
}

PassRefPtr<IDBKey> IDBObjectStoreBackendImpl::generateKey(PassRefPtr<IDBBackingStore> backingStore, PassRefPtr<IDBTransactionBackendImpl> transaction, int64_t databaseId, int64_t objectStoreId)
{
    const int64_t maxGeneratorValue = 9007199254740992LL; // Maximum integer storable as ECMAScript number.
    int64_t currentNumber;
    bool ok = backingStore->getKeyGeneratorCurrentNumber(transaction->backingStoreTransaction(), databaseId, objectStoreId, currentNumber);
    if (!ok) {
        LOG_ERROR("Failed to getKeyGeneratorCurrentNumber");
        return IDBKey::createInvalid();
    }
    if (currentNumber < 0 || currentNumber > maxGeneratorValue)
        return IDBKey::createInvalid();

    return IDBKey::createNumber(currentNumber);
}

bool IDBObjectStoreBackendImpl::updateKeyGenerator(PassRefPtr<IDBBackingStore> backingStore, PassRefPtr<IDBTransactionBackendImpl> transaction, int64_t databaseId, int64_t objectStoreId, const IDBKey* key, bool checkCurrent)
{
    ASSERT(key && key->type() == IDBKey::NumberType);
    return backingStore->maybeUpdateKeyGeneratorCurrentNumber(transaction->backingStoreTransaction(), databaseId, objectStoreId, static_cast<int64_t>(floor(key->number())) + 1, checkCurrent);
}

} // namespace WebCore

#endif

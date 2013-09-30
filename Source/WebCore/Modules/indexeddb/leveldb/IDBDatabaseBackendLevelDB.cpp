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
#include "IDBDatabaseBackendLevelDB.h"

#if ENABLE(INDEXED_DATABASE) && USE(LEVELDB)

#include "IDBBackingStoreLevelDB.h"
#include "IDBCursorBackendLevelDB.h"
#include "IDBDatabaseException.h"
#include "IDBFactoryBackendLevelDB.h"
#include "IDBKeyRange.h"
#include "IDBObjectStoreBackendLevelDB.h"
#include "IDBTransactionBackendLevelDB.h"
#include "IDBTransactionCoordinatorLevelDB.h"
#include "Logging.h"
#include "SharedBuffer.h"
#include <wtf/TemporaryChange.h>

namespace WebCore {

class CreateObjectStoreOperation : public IDBTransactionBackendLevelDB::Operation {
public:
    static PassOwnPtr<IDBTransactionBackendLevelDB::Operation> create(PassRefPtr<IDBBackingStore> backingStore, const IDBObjectStoreMetadata& objectStoreMetadata)
    {
        return adoptPtr(new CreateObjectStoreOperation(backingStore, objectStoreMetadata));
    }
    virtual void perform(IDBTransactionBackendLevelDB*);
private:
    CreateObjectStoreOperation(PassRefPtr<IDBBackingStore> backingStore, const IDBObjectStoreMetadata& objectStoreMetadata)
        : m_backingStore(backingStore)
        , m_objectStoreMetadata(objectStoreMetadata)
    {
    }

    const RefPtr<IDBBackingStore> m_backingStore;
    const IDBObjectStoreMetadata m_objectStoreMetadata;
};

class DeleteObjectStoreOperation : public IDBTransactionBackendLevelDB::Operation {
public:
    static PassOwnPtr<IDBTransactionBackendLevelDB::Operation> create(PassRefPtr<IDBBackingStore> backingStore, const IDBObjectStoreMetadata& objectStoreMetadata)
    {
        return adoptPtr(new DeleteObjectStoreOperation(backingStore, objectStoreMetadata));
    }
    virtual void perform(IDBTransactionBackendLevelDB*);
private:
    DeleteObjectStoreOperation(PassRefPtr<IDBBackingStore> backingStore, const IDBObjectStoreMetadata& objectStoreMetadata)
        : m_backingStore(backingStore)
        , m_objectStoreMetadata(objectStoreMetadata)
    {
    }

    const RefPtr<IDBBackingStore> m_backingStore;
    const IDBObjectStoreMetadata m_objectStoreMetadata;
};

class IDBDatabaseBackendLevelDB::VersionChangeOperation : public IDBTransactionBackendLevelDB::Operation {
public:
    static PassOwnPtr<IDBTransactionBackendLevelDB::Operation> create(PassRefPtr<IDBDatabaseBackendLevelDB> database, int64_t transactionId, int64_t version, PassRefPtr<IDBCallbacks> callbacks, PassRefPtr<IDBDatabaseCallbacks> databaseCallbacks)
    {
        return adoptPtr(new VersionChangeOperation(database, transactionId, version, callbacks, databaseCallbacks));
    }
    virtual void perform(IDBTransactionBackendLevelDB*);
private:
    VersionChangeOperation(PassRefPtr<IDBDatabaseBackendLevelDB> database, int64_t transactionId, int64_t version, PassRefPtr<IDBCallbacks> callbacks, PassRefPtr<IDBDatabaseCallbacks> databaseCallbacks)
        : m_database(database)
        , m_transactionId(transactionId)
        , m_version(version)
        , m_callbacks(callbacks)
        , m_databaseCallbacks(databaseCallbacks)
    {
    }

    RefPtr<IDBDatabaseBackendLevelDB> m_database;
    int64_t m_transactionId;
    int64_t m_version;
    RefPtr<IDBCallbacks> m_callbacks;
    RefPtr<IDBDatabaseCallbacks> m_databaseCallbacks;
};

class CreateObjectStoreAbortOperation : public IDBTransactionBackendLevelDB::Operation {
public:
    static PassOwnPtr<IDBTransactionBackendLevelDB::Operation> create(PassRefPtr<IDBDatabaseBackendLevelDB> database, int64_t objectStoreId)
    {
        return adoptPtr(new CreateObjectStoreAbortOperation(database, objectStoreId));
    }
    virtual void perform(IDBTransactionBackendLevelDB*);
private:
    CreateObjectStoreAbortOperation(PassRefPtr<IDBDatabaseBackendLevelDB> database, int64_t objectStoreId)
        : m_database(database)
        , m_objectStoreId(objectStoreId)
    {
    }

    const RefPtr<IDBDatabaseBackendLevelDB> m_database;
    const int64_t m_objectStoreId;
};

class DeleteObjectStoreAbortOperation : public IDBTransactionBackendLevelDB::Operation {
public:
    static PassOwnPtr<IDBTransactionBackendLevelDB::Operation> create(PassRefPtr<IDBDatabaseBackendLevelDB> database, const IDBObjectStoreMetadata& objectStore)
    {
        return adoptPtr(new DeleteObjectStoreAbortOperation(database, objectStore));
    }
    virtual void perform(IDBTransactionBackendLevelDB*);
private:
    DeleteObjectStoreAbortOperation(PassRefPtr<IDBDatabaseBackendLevelDB> database, const IDBObjectStoreMetadata& objectStoreMetadata)
        : m_database(database)
        , m_objectStoreMetadata(objectStoreMetadata)
    {
    }

    RefPtr<IDBDatabaseBackendLevelDB> m_database;
    IDBObjectStoreMetadata m_objectStoreMetadata;
};

class IDBDatabaseBackendLevelDB::VersionChangeAbortOperation : public IDBTransactionBackendLevelDB::Operation {
public:
    static PassOwnPtr<IDBTransactionBackendLevelDB::Operation> create(PassRefPtr<IDBDatabaseBackendLevelDB> database, const String& previousVersion, int64_t previousIntVersion)
    {
        return adoptPtr(new VersionChangeAbortOperation(database, previousVersion, previousIntVersion));
    }
    virtual void perform(IDBTransactionBackendLevelDB*);
private:
    VersionChangeAbortOperation(PassRefPtr<IDBDatabaseBackendLevelDB> database, const String& previousVersion, int64_t previousIntVersion)
        : m_database(database)
        , m_previousVersion(previousVersion)
        , m_previousIntVersion(previousIntVersion)
    {
    }

    RefPtr<IDBDatabaseBackendLevelDB> m_database;
    String m_previousVersion;
    int64_t m_previousIntVersion;
};

class CreateIndexOperation : public IDBTransactionBackendLevelDB::Operation {
public:
    static PassOwnPtr<IDBTransactionBackendLevelDB::Operation> create(PassRefPtr<IDBBackingStore> backingStore, int64_t objectStoreId, const IDBIndexMetadata& indexMetadata)
    {
        return adoptPtr(new CreateIndexOperation(backingStore, objectStoreId, indexMetadata));
    }
    virtual void perform(IDBTransactionBackendLevelDB*);
private:
    CreateIndexOperation(PassRefPtr<IDBBackingStore> backingStore, int64_t objectStoreId, const IDBIndexMetadata& indexMetadata)
        : m_backingStore(backingStore)
        , m_objectStoreId(objectStoreId)
        , m_indexMetadata(indexMetadata)
    {
    }

    const RefPtr<IDBBackingStore> m_backingStore;
    const int64_t m_objectStoreId;
    const IDBIndexMetadata m_indexMetadata;
};

class DeleteIndexOperation : public IDBTransactionBackendLevelDB::Operation {
public:
    static PassOwnPtr<IDBTransactionBackendLevelDB::Operation> create(PassRefPtr<IDBBackingStore> backingStore, int64_t objectStoreId, const IDBIndexMetadata& indexMetadata)
    {
        return adoptPtr(new DeleteIndexOperation(backingStore, objectStoreId, indexMetadata));
    }
    virtual void perform(IDBTransactionBackendLevelDB*);
private:
    DeleteIndexOperation(PassRefPtr<IDBBackingStore> backingStore, int64_t objectStoreId, const IDBIndexMetadata& indexMetadata)
        : m_backingStore(backingStore)
        , m_objectStoreId(objectStoreId)
        , m_indexMetadata(indexMetadata)
    {
    }

    const RefPtr<IDBBackingStore> m_backingStore;
    const int64_t m_objectStoreId;
    const IDBIndexMetadata m_indexMetadata;
};

class CreateIndexAbortOperation : public IDBTransactionBackendLevelDB::Operation {
public:
    static PassOwnPtr<IDBTransactionBackendLevelDB::Operation> create(PassRefPtr<IDBDatabaseBackendLevelDB> database, int64_t objectStoreId, int64_t indexId)
    {
        return adoptPtr(new CreateIndexAbortOperation(database, objectStoreId, indexId));
    }
    virtual void perform(IDBTransactionBackendLevelDB*);
private:
    CreateIndexAbortOperation(PassRefPtr<IDBDatabaseBackendLevelDB> database, int64_t objectStoreId, int64_t indexId)
        : m_database(database)
        , m_objectStoreId(objectStoreId)
        , m_indexId(indexId)
    {
    }

    const RefPtr<IDBDatabaseBackendLevelDB> m_database;
    const int64_t m_objectStoreId;
    const int64_t m_indexId;
};

class DeleteIndexAbortOperation : public IDBTransactionBackendLevelDB::Operation {
public:
    static PassOwnPtr<IDBTransactionBackendLevelDB::Operation> create(PassRefPtr<IDBDatabaseBackendLevelDB> database, int64_t objectStoreId, const IDBIndexMetadata& indexMetadata)
    {
        return adoptPtr(new DeleteIndexAbortOperation(database, objectStoreId, indexMetadata));
    }
    virtual void perform(IDBTransactionBackendLevelDB*);
private:
    DeleteIndexAbortOperation(PassRefPtr<IDBDatabaseBackendLevelDB> database, int64_t objectStoreId, const IDBIndexMetadata& indexMetadata)
        : m_database(database)
        , m_objectStoreId(objectStoreId)
        , m_indexMetadata(indexMetadata)
    {
    }

    const RefPtr<IDBDatabaseBackendLevelDB> m_database;
    const int64_t m_objectStoreId;
    const IDBIndexMetadata m_indexMetadata;
};

class GetOperation : public IDBTransactionBackendLevelDB::Operation {
public:
    static PassOwnPtr<IDBTransactionBackendLevelDB::Operation> create(PassRefPtr<IDBBackingStore> backingStore, const IDBDatabaseMetadata& metadata, int64_t objectStoreId, int64_t indexId, PassRefPtr<IDBKeyRange> keyRange, IndexedDB::CursorType cursorType, PassRefPtr<IDBCallbacks> callbacks)
    {
        return adoptPtr(new GetOperation(backingStore, metadata, objectStoreId, indexId, keyRange, cursorType, callbacks));
    }
    virtual void perform(IDBTransactionBackendLevelDB*);
private:
    GetOperation(PassRefPtr<IDBBackingStore> backingStore, const IDBDatabaseMetadata& metadata, int64_t objectStoreId, int64_t indexId, PassRefPtr<IDBKeyRange> keyRange, IndexedDB::CursorType cursorType, PassRefPtr<IDBCallbacks> callbacks)
        : m_backingStore(backingStore)
        , m_databaseId(metadata.id)
        , m_objectStoreId(objectStoreId)
        , m_indexId(indexId)
        , m_keyPath(metadata.objectStores.get(objectStoreId).keyPath)
        , m_autoIncrement(metadata.objectStores.get(objectStoreId).autoIncrement)
        , m_keyRange(keyRange)
        , m_cursorType(cursorType)
        , m_callbacks(callbacks)
    {
        ASSERT(metadata.objectStores.contains(objectStoreId));
        ASSERT(metadata.objectStores.get(objectStoreId).id == objectStoreId);
    }

    const RefPtr<IDBBackingStore> m_backingStore;
    const int64_t m_databaseId;
    const int64_t m_objectStoreId;
    const int64_t m_indexId;
    const IDBKeyPath m_keyPath;
    const bool m_autoIncrement;
    const RefPtr<IDBKeyRange> m_keyRange;
    const IndexedDB::CursorType m_cursorType;
    const RefPtr<IDBCallbacks> m_callbacks;
};

class PutOperation : public IDBTransactionBackendLevelDB::Operation {
public:
    static PassOwnPtr<IDBTransactionBackendLevelDB::Operation> create(PassRefPtr<IDBBackingStore> backingStore, int64_t databaseId, const IDBObjectStoreMetadata& objectStore, PassRefPtr<SharedBuffer> value, PassRefPtr<IDBKey> key, IDBDatabaseBackendInterface::PutMode putMode, PassRefPtr<IDBCallbacks> callbacks, const Vector<int64_t>& indexIds, const Vector<IDBDatabaseBackendInterface::IndexKeys>& indexKeys)
    {
        return adoptPtr(new PutOperation(backingStore, databaseId, objectStore, value, key, putMode, callbacks, indexIds, indexKeys));
    }
    virtual void perform(IDBTransactionBackendLevelDB*);
private:
    PutOperation(PassRefPtr<IDBBackingStore> backingStore, int64_t databaseId, const IDBObjectStoreMetadata& objectStore, PassRefPtr<SharedBuffer>& value, PassRefPtr<IDBKey> key, IDBDatabaseBackendInterface::PutMode putMode, PassRefPtr<IDBCallbacks> callbacks, const Vector<int64_t>& indexIds, const Vector<IDBDatabaseBackendInterface::IndexKeys>& indexKeys)
        : m_backingStore(backingStore)
        , m_databaseId(databaseId)
        , m_objectStore(objectStore)
        , m_value(value)
        , m_key(key)
        , m_putMode(putMode)
        , m_callbacks(callbacks)
        , m_indexIds(indexIds)
        , m_indexKeys(indexKeys)
    {
    }

    const RefPtr<IDBBackingStore> m_backingStore;
    const int64_t m_databaseId;
    const IDBObjectStoreMetadata m_objectStore;
    const RefPtr<SharedBuffer> m_value;
    const RefPtr<IDBKey> m_key;
    const IDBDatabaseBackendInterface::PutMode m_putMode;
    const RefPtr<IDBCallbacks> m_callbacks;
    const Vector<int64_t> m_indexIds;
    const Vector<IDBDatabaseBackendInterface::IndexKeys> m_indexKeys;
};

class SetIndexesReadyOperation : public IDBTransactionBackendLevelDB::Operation {
public:
    static PassOwnPtr<IDBTransactionBackendLevelDB::Operation> create(size_t indexCount)
    {
        return adoptPtr(new SetIndexesReadyOperation(indexCount));
    }
    virtual void perform(IDBTransactionBackendLevelDB*);
private:
    SetIndexesReadyOperation(size_t indexCount)
        : m_indexCount(indexCount)
    {
    }

    const size_t m_indexCount;
};

class OpenCursorOperation : public IDBTransactionBackendLevelDB::Operation {
public:
    static PassOwnPtr<IDBTransactionBackendLevelDB::Operation> create(PassRefPtr<IDBBackingStore> backingStore, int64_t databaseId, int64_t objectStoreId, int64_t indexId, PassRefPtr<IDBKeyRange> keyRange, IndexedDB::CursorDirection direction, IndexedDB::CursorType cursorType, IDBDatabaseBackendInterface::TaskType taskType, PassRefPtr<IDBCallbacks> callbacks)
    {
        return adoptPtr(new OpenCursorOperation(backingStore, databaseId, objectStoreId, indexId, keyRange, direction, cursorType, taskType, callbacks));
    }
    virtual void perform(IDBTransactionBackendLevelDB*);
private:
    OpenCursorOperation(PassRefPtr<IDBBackingStore> backingStore, int64_t databaseId, int64_t objectStoreId, int64_t indexId, PassRefPtr<IDBKeyRange> keyRange, IndexedDB::CursorDirection direction, IndexedDB::CursorType cursorType, IDBDatabaseBackendInterface::TaskType taskType, PassRefPtr<IDBCallbacks> callbacks)
        : m_backingStore(backingStore)
        , m_databaseId(databaseId)
        , m_objectStoreId(objectStoreId)
        , m_indexId(indexId)
        , m_keyRange(keyRange)
        , m_direction(direction)
        , m_cursorType(cursorType)
        , m_taskType(taskType)
        , m_callbacks(callbacks)
    {
    }

    const RefPtr<IDBBackingStore> m_backingStore;
    const int64_t m_databaseId;
    const int64_t m_objectStoreId;
    const int64_t m_indexId;
    const PassRefPtr<IDBKeyRange> m_keyRange;
    const IndexedDB::CursorDirection m_direction;
    const IndexedDB::CursorType m_cursorType;
    const IDBDatabaseBackendInterface::TaskType m_taskType;
    const RefPtr<IDBCallbacks> m_callbacks;
};

class CountOperation : public IDBTransactionBackendLevelDB::Operation {
public:
    static PassOwnPtr<IDBTransactionBackendLevelDB::Operation> create(PassRefPtr<IDBBackingStore> backingStore, int64_t databaseId, int64_t objectStoreId, int64_t indexId, PassRefPtr<IDBKeyRange> keyRange, PassRefPtr<IDBCallbacks> callbacks)
    {
        return adoptPtr(new CountOperation(backingStore, databaseId, objectStoreId, indexId, keyRange, callbacks));
    }
    virtual void perform(IDBTransactionBackendLevelDB*);
private:
    CountOperation(PassRefPtr<IDBBackingStore> backingStore, int64_t databaseId, int64_t objectStoreId, int64_t indexId, PassRefPtr<IDBKeyRange> keyRange, PassRefPtr<IDBCallbacks> callbacks)
        : m_backingStore(backingStore)
        , m_databaseId(databaseId)
        , m_objectStoreId(objectStoreId)
        , m_indexId(indexId)
        , m_keyRange(keyRange)
        , m_callbacks(callbacks)
    {
    }

    const RefPtr<IDBBackingStore> m_backingStore;
    const int64_t m_databaseId;
    const int64_t m_objectStoreId;
    const int64_t m_indexId;
    const RefPtr<IDBKeyRange> m_keyRange;
    const RefPtr<IDBCallbacks> m_callbacks;
};

class DeleteRangeOperation : public IDBTransactionBackendLevelDB::Operation {
public:
    static PassOwnPtr<IDBTransactionBackendLevelDB::Operation> create(PassRefPtr<IDBBackingStore> backingStore, int64_t databaseId, int64_t objectStoreId, PassRefPtr<IDBKeyRange> keyRange, PassRefPtr<IDBCallbacks> callbacks)
    {
        return adoptPtr(new DeleteRangeOperation(backingStore, databaseId, objectStoreId, keyRange, callbacks));
    }
    virtual void perform(IDBTransactionBackendLevelDB*);
private:
    DeleteRangeOperation(PassRefPtr<IDBBackingStore> backingStore, int64_t databaseId, int64_t objectStoreId, PassRefPtr<IDBKeyRange> keyRange, PassRefPtr<IDBCallbacks> callbacks)
        : m_backingStore(backingStore)
        , m_databaseId(databaseId)
        , m_objectStoreId(objectStoreId)
        , m_keyRange(keyRange)
        , m_callbacks(callbacks)
    {
    }

    const RefPtr<IDBBackingStore> m_backingStore;
    const int64_t m_databaseId;
    const int64_t m_objectStoreId;
    const RefPtr<IDBKeyRange> m_keyRange;
    const RefPtr<IDBCallbacks> m_callbacks;
};

class ClearOperation : public IDBTransactionBackendLevelDB::Operation {
public:
    static PassOwnPtr<IDBTransactionBackendLevelDB::Operation> create(PassRefPtr<IDBBackingStore> backingStore, int64_t databaseId, int64_t objectStoreId, PassRefPtr<IDBCallbacks> callbacks)
    {
        return adoptPtr(new ClearOperation(backingStore, databaseId, objectStoreId, callbacks));
    }
    virtual void perform(IDBTransactionBackendLevelDB*);
private:
    ClearOperation(PassRefPtr<IDBBackingStore> backingStore, int64_t databaseId, int64_t objectStoreId, PassRefPtr<IDBCallbacks> callbacks)
        : m_backingStore(backingStore)
        , m_databaseId(databaseId)
        , m_objectStoreId(objectStoreId)
        , m_callbacks(callbacks)
    {
    }

    const RefPtr<IDBBackingStore> m_backingStore;
    const int64_t m_databaseId;
    const int64_t m_objectStoreId;
    const RefPtr<IDBCallbacks> m_callbacks;
};

class IDBDatabaseBackendLevelDB::PendingOpenCall {
public:
    static PassOwnPtr<PendingOpenCall> create(PassRefPtr<IDBCallbacks> callbacks, PassRefPtr<IDBDatabaseCallbacks> databaseCallbacks, int64_t transactionId, int64_t version)
    {
        return adoptPtr(new PendingOpenCall(callbacks, databaseCallbacks, transactionId, version));
    }
    PassRefPtr<IDBCallbacks> callbacks() { return m_callbacks; }
    PassRefPtr<IDBDatabaseCallbacks> databaseCallbacks() { return m_databaseCallbacks; }
    int64_t version() { return m_version; }
    int64_t transactionId() const { return m_transactionId; }

private:
    PendingOpenCall(PassRefPtr<IDBCallbacks> callbacks, PassRefPtr<IDBDatabaseCallbacks> databaseCallbacks, int64_t transactionId, int64_t version)
        : m_callbacks(callbacks)
        , m_databaseCallbacks(databaseCallbacks)
        , m_version(version)
        , m_transactionId(transactionId)
    {
    }
    RefPtr<IDBCallbacks> m_callbacks;
    RefPtr<IDBDatabaseCallbacks> m_databaseCallbacks;
    int64_t m_version;
    const int64_t m_transactionId;
};

class IDBDatabaseBackendLevelDB::PendingDeleteCall {
public:
    static PassOwnPtr<PendingDeleteCall> create(PassRefPtr<IDBCallbacks> callbacks)
    {
        return adoptPtr(new PendingDeleteCall(callbacks));
    }
    PassRefPtr<IDBCallbacks> callbacks() { return m_callbacks; }

private:
    PendingDeleteCall(PassRefPtr<IDBCallbacks> callbacks)
        : m_callbacks(callbacks)
    {
    }
    RefPtr<IDBCallbacks> m_callbacks;
};

PassRefPtr<IDBDatabaseBackendLevelDB> IDBDatabaseBackendLevelDB::create(const String& name, IDBBackingStore* database, IDBFactoryBackendLevelDB* factory, const String& uniqueIdentifier)
{
    RefPtr<IDBDatabaseBackendLevelDB> backend = adoptRef(new IDBDatabaseBackendLevelDB(name, database, factory, uniqueIdentifier));
    if (!backend->openInternal())
        return 0;
    return backend.release();
}

IDBDatabaseBackendLevelDB::IDBDatabaseBackendLevelDB(const String& name, IDBBackingStore* backingStore, IDBFactoryBackendLevelDB* factory, const String& uniqueIdentifier)
    : m_backingStore(backingStore)
    , m_metadata(name, InvalidId, 0, InvalidId)
    , m_identifier(uniqueIdentifier)
    , m_factory(factory)
    , m_transactionCoordinator(IDBTransactionCoordinatorLevelDB::create())
    , m_closingConnection(false)
{
    ASSERT(!m_metadata.name.isNull());
}

void IDBDatabaseBackendLevelDB::addObjectStore(const IDBObjectStoreMetadata& objectStore, int64_t newMaxObjectStoreId)
{
    ASSERT(!m_metadata.objectStores.contains(objectStore.id));
    if (newMaxObjectStoreId != IDBObjectStoreMetadata::InvalidId) {
        ASSERT(m_metadata.maxObjectStoreId < newMaxObjectStoreId);
        m_metadata.maxObjectStoreId = newMaxObjectStoreId;
    }
    m_metadata.objectStores.set(objectStore.id, objectStore);
}

void IDBDatabaseBackendLevelDB::removeObjectStore(int64_t objectStoreId)
{
    ASSERT(m_metadata.objectStores.contains(objectStoreId));
    m_metadata.objectStores.remove(objectStoreId);
}

void IDBDatabaseBackendLevelDB::addIndex(int64_t objectStoreId, const IDBIndexMetadata& index, int64_t newMaxIndexId)
{
    ASSERT(m_metadata.objectStores.contains(objectStoreId));
    IDBObjectStoreMetadata objectStore = m_metadata.objectStores.get(objectStoreId);

    ASSERT(!objectStore.indexes.contains(index.id));
    objectStore.indexes.set(index.id, index);
    if (newMaxIndexId != IDBIndexMetadata::InvalidId) {
        ASSERT(objectStore.maxIndexId < newMaxIndexId);
        objectStore.maxIndexId = newMaxIndexId;
    }
    m_metadata.objectStores.set(objectStoreId, objectStore);
}

void IDBDatabaseBackendLevelDB::removeIndex(int64_t objectStoreId, int64_t indexId)
{
    ASSERT(m_metadata.objectStores.contains(objectStoreId));
    IDBObjectStoreMetadata objectStore = m_metadata.objectStores.get(objectStoreId);

    ASSERT(objectStore.indexes.contains(indexId));
    objectStore.indexes.remove(indexId);
    m_metadata.objectStores.set(objectStoreId, objectStore);
}

bool IDBDatabaseBackendLevelDB::openInternal()
{
    bool success = false;
    bool ok = m_backingStore->getIDBDatabaseMetaData(m_metadata.name, &m_metadata, success);
    ASSERT_WITH_MESSAGE(success == (m_metadata.id != InvalidId), "success = %s, m_id = %lld", success ? "true" : "false", static_cast<long long>(m_metadata.id));
    if (!ok)
        return false;
    if (success)
        return m_backingStore->getObjectStores(m_metadata.id, &m_metadata.objectStores);

    return m_backingStore->createIDBDatabaseMetaData(m_metadata.name, String::number(m_metadata.version), m_metadata.version, m_metadata.id);
}

IDBDatabaseBackendLevelDB::~IDBDatabaseBackendLevelDB()
{
}

PassRefPtr<IDBBackingStore> IDBDatabaseBackendLevelDB::backingStore() const
{
    return m_backingStore;
}

void IDBDatabaseBackendLevelDB::createObjectStore(int64_t transactionId, int64_t objectStoreId, const String& name, const IDBKeyPath& keyPath, bool autoIncrement)
{
    LOG(StorageAPI, "IDBDatabaseBackendLevelDB::createObjectStore");
    IDBTransactionBackendLevelDB* transaction = m_transactions.get(transactionId);
    if (!transaction)
        return;
    ASSERT(transaction->mode() == IndexedDB::TransactionVersionChange);

    ASSERT(!m_metadata.objectStores.contains(objectStoreId));
    IDBObjectStoreMetadata objectStoreMetadata(name, objectStoreId, keyPath, autoIncrement, IDBDatabaseBackendInterface::MinimumIndexId);

    transaction->scheduleTask(CreateObjectStoreOperation::create(m_backingStore, objectStoreMetadata), CreateObjectStoreAbortOperation::create(this, objectStoreId));

    addObjectStore(objectStoreMetadata, objectStoreId);
}

void CreateObjectStoreOperation::perform(IDBTransactionBackendLevelDB* transaction)
{
    LOG(StorageAPI, "CreateObjectStoreOperation");
    if (!m_backingStore->createObjectStore(transaction->backingStoreTransaction(), transaction->database()->id(), m_objectStoreMetadata.id, m_objectStoreMetadata.name, m_objectStoreMetadata.keyPath, m_objectStoreMetadata.autoIncrement)) {
        RefPtr<IDBDatabaseError> error = IDBDatabaseError::create(IDBDatabaseException::UnknownError, String::format("Internal error creating object store '%s'.", m_objectStoreMetadata.name.utf8().data()));
        transaction->abort(error.release());
        return;
    }
}

void IDBDatabaseBackendLevelDB::deleteObjectStore(int64_t transactionId, int64_t objectStoreId)
{
    LOG(StorageAPI, "IDBDatabaseBackendLevelDB::deleteObjectStore");
    IDBTransactionBackendLevelDB* transaction = m_transactions.get(transactionId);
    if (!transaction)
        return;
    ASSERT(transaction->mode() == IndexedDB::TransactionVersionChange);

    ASSERT(m_metadata.objectStores.contains(objectStoreId));
    const IDBObjectStoreMetadata& objectStoreMetadata = m_metadata.objectStores.get(objectStoreId);

    transaction->scheduleTask(DeleteObjectStoreOperation::create(m_backingStore, objectStoreMetadata),  DeleteObjectStoreAbortOperation::create(this, objectStoreMetadata));
    removeObjectStore(objectStoreId);
}

void IDBDatabaseBackendLevelDB::createIndex(int64_t transactionId, int64_t objectStoreId, int64_t indexId, const String& name, const IDBKeyPath& keyPath, bool unique, bool multiEntry)
{
    LOG(StorageAPI, "IDBDatabaseBackendLevelDB::createIndex");
    IDBTransactionBackendLevelDB* transaction = m_transactions.get(transactionId);
    if (!transaction)
        return;
    ASSERT(transaction->mode() == IndexedDB::TransactionVersionChange);

    ASSERT(m_metadata.objectStores.contains(objectStoreId));
    const IDBObjectStoreMetadata objectStore = m_metadata.objectStores.get(objectStoreId);

    ASSERT(!objectStore.indexes.contains(indexId));
    const IDBIndexMetadata indexMetadata(name, indexId, keyPath, unique, multiEntry);

    transaction->scheduleTask(CreateIndexOperation::create(m_backingStore, objectStoreId, indexMetadata), CreateIndexAbortOperation::create(this, objectStoreId, indexId));

    addIndex(objectStoreId, indexMetadata, indexId);
}

void CreateIndexOperation::perform(IDBTransactionBackendLevelDB* transaction)
{
    LOG(StorageAPI, "CreateIndexOperation");
    if (!m_backingStore->createIndex(transaction->backingStoreTransaction(), transaction->database()->id(), m_objectStoreId, m_indexMetadata.id, m_indexMetadata.name, m_indexMetadata.keyPath, m_indexMetadata.unique, m_indexMetadata.multiEntry)) {
        transaction->abort(IDBDatabaseError::create(IDBDatabaseException::UnknownError, String::format("Internal error when trying to create index '%s'.", m_indexMetadata.name.utf8().data())));
        return;
    }
}

void CreateIndexAbortOperation::perform(IDBTransactionBackendLevelDB* transaction)
{
    LOG(StorageAPI, "CreateIndexAbortOperation");
    ASSERT_UNUSED(transaction, !transaction);
    m_database->removeIndex(m_objectStoreId, m_indexId);
}

void IDBDatabaseBackendLevelDB::deleteIndex(int64_t transactionId, int64_t objectStoreId, int64_t indexId)
{
    LOG(StorageAPI, "IDBDatabaseBackendLevelDB::deleteIndex");
    IDBTransactionBackendLevelDB* transaction = m_transactions.get(transactionId);
    if (!transaction)
        return;
    ASSERT(transaction->mode() == IndexedDB::TransactionVersionChange);

    ASSERT(m_metadata.objectStores.contains(objectStoreId));
    const IDBObjectStoreMetadata objectStore = m_metadata.objectStores.get(objectStoreId);

    ASSERT(objectStore.indexes.contains(indexId));
    const IDBIndexMetadata& indexMetadata = objectStore.indexes.get(indexId);

    transaction->scheduleTask(DeleteIndexOperation::create(m_backingStore, objectStoreId, indexMetadata), DeleteIndexAbortOperation::create(this, objectStoreId, indexMetadata));

    removeIndex(objectStoreId, indexId);
}

void DeleteIndexOperation::perform(IDBTransactionBackendLevelDB* transaction)
{
    LOG(StorageAPI, "DeleteIndexOperation");
    bool ok = m_backingStore->deleteIndex(transaction->backingStoreTransaction(), transaction->database()->id(), m_objectStoreId, m_indexMetadata.id);
    if (!ok) {
        RefPtr<IDBDatabaseError> error = IDBDatabaseError::create(IDBDatabaseException::UnknownError, String::format("Internal error deleting index '%s'.", m_indexMetadata.name.utf8().data()));
        transaction->abort(error);
    }
}

void DeleteIndexAbortOperation::perform(IDBTransactionBackendLevelDB* transaction)
{
    LOG(StorageAPI, "DeleteIndexAbortOperation");
    ASSERT_UNUSED(transaction, !transaction);
    m_database->addIndex(m_objectStoreId, m_indexMetadata, IDBIndexMetadata::InvalidId);
}

void IDBDatabaseBackendLevelDB::commit(int64_t transactionId)
{
    // The frontend suggests that we commit, but we may have previously initiated an abort, and so have disposed of the transaction. onAbort has already been dispatched to the frontend, so it will find out about that asynchronously.
    if (m_transactions.contains(transactionId))
        m_transactions.get(transactionId)->commit();
}

void IDBDatabaseBackendLevelDB::abort(int64_t transactionId)
{
    // If the transaction is unknown, then it has already been aborted by the backend before this call so it is safe to ignore it.
    if (m_transactions.contains(transactionId))
        m_transactions.get(transactionId)->abort();
}

void IDBDatabaseBackendLevelDB::abort(int64_t transactionId, PassRefPtr<IDBDatabaseError> error)
{
    // If the transaction is unknown, then it has already been aborted by the backend before this call so it is safe to ignore it.
    if (m_transactions.contains(transactionId))
        m_transactions.get(transactionId)->abort(error);
}

void IDBDatabaseBackendLevelDB::get(int64_t transactionId, int64_t objectStoreId, int64_t indexId, PassRefPtr<IDBKeyRange> keyRange, bool keyOnly, PassRefPtr<IDBCallbacks> callbacks)
{
    LOG(StorageAPI, "IDBDatabaseBackendLevelDB::get");
    IDBTransactionBackendLevelDB* transaction = m_transactions.get(transactionId);
    if (!transaction)
        return;

    transaction->scheduleTask(GetOperation::create(m_backingStore, m_metadata, objectStoreId, indexId, keyRange, keyOnly ? IndexedDB::CursorKeyOnly : IndexedDB::CursorKeyAndValue, callbacks));
}

void GetOperation::perform(IDBTransactionBackendLevelDB* transaction)
{
    LOG(StorageAPI, "GetOperation");

    RefPtr<IDBKey> key;

    if (m_keyRange->isOnlyKey())
        key = m_keyRange->lower();
    else {
        RefPtr<IDBBackingStore::Cursor> backingStoreCursor;
        if (m_indexId == IDBIndexMetadata::InvalidId) {
            ASSERT(m_cursorType != IndexedDB::CursorKeyOnly);
            // ObjectStore Retrieval Operation
            backingStoreCursor = m_backingStore->openObjectStoreCursor(transaction->backingStoreTransaction(), m_databaseId, m_objectStoreId, m_keyRange.get(), IndexedDB::CursorNext);
        } else {
            if (m_cursorType == IndexedDB::CursorKeyOnly)
                // Index Value Retrieval Operation
                backingStoreCursor = m_backingStore->openIndexKeyCursor(transaction->backingStoreTransaction(), m_databaseId, m_objectStoreId, m_indexId, m_keyRange.get(), IndexedDB::CursorNext);
            else
                // Index Referenced Value Retrieval Operation
                backingStoreCursor = m_backingStore->openIndexCursor(transaction->backingStoreTransaction(), m_databaseId, m_objectStoreId, m_indexId, m_keyRange.get(), IndexedDB::CursorNext);
        }

        if (!backingStoreCursor) {
            m_callbacks->onSuccess();
            return;
        }

        key = backingStoreCursor->key();
    }

    RefPtr<IDBKey> primaryKey;
    bool ok;
    if (m_indexId == IDBIndexMetadata::InvalidId) {
        // Object Store Retrieval Operation
        Vector<char> value;
        ok = m_backingStore->getRecord(transaction->backingStoreTransaction(), m_databaseId, m_objectStoreId, *key, value);
        if (!ok) {
            m_callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::UnknownError, "Internal error in getRecord."));
            return;
        }

        if (value.isEmpty()) {
            m_callbacks->onSuccess();
            return;
        }

        if (m_autoIncrement && !m_keyPath.isNull()) {
            m_callbacks->onSuccess(SharedBuffer::adoptVector(value), key, m_keyPath);
            return;
        }

        m_callbacks->onSuccess(SharedBuffer::adoptVector(value));
        return;

    }

    // From here we are dealing only with indexes.
    ok = m_backingStore->getPrimaryKeyViaIndex(transaction->backingStoreTransaction(), m_databaseId, m_objectStoreId, m_indexId, *key, primaryKey);
    if (!ok) {
        m_callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::UnknownError, "Internal error in getPrimaryKeyViaIndex."));
        return;
    }
    if (!primaryKey) {
        m_callbacks->onSuccess();
        return;
    }
    if (m_cursorType == IndexedDB::CursorKeyOnly) {
        // Index Value Retrieval Operation
        m_callbacks->onSuccess(primaryKey.get());
        return;
    }

    // Index Referenced Value Retrieval Operation
    Vector<char> value;
    ok = m_backingStore->getRecord(transaction->backingStoreTransaction(), m_databaseId, m_objectStoreId, *primaryKey, value);
    if (!ok) {
        m_callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::UnknownError, "Internal error in getRecord."));
        return;
    }

    if (value.isEmpty()) {
        m_callbacks->onSuccess();
        return;
    }
    if (m_autoIncrement && !m_keyPath.isNull()) {
        m_callbacks->onSuccess(SharedBuffer::adoptVector(value), primaryKey, m_keyPath);
        return;
    }
    m_callbacks->onSuccess(SharedBuffer::adoptVector(value));
}

void IDBDatabaseBackendLevelDB::put(int64_t transactionId, int64_t objectStoreId, PassRefPtr<SharedBuffer> value, PassRefPtr<IDBKey> key, PutMode putMode, PassRefPtr<IDBCallbacks> callbacks, const Vector<int64_t>& indexIds, const Vector<IndexKeys>& indexKeys)
{
    LOG(StorageAPI, "IDBDatabaseBackendLevelDB::put");
    IDBTransactionBackendLevelDB* transaction = m_transactions.get(transactionId);
    if (!transaction)
        return;
    ASSERT(transaction->mode() != IndexedDB::TransactionReadOnly);

    const IDBObjectStoreMetadata objectStoreMetadata = m_metadata.objectStores.get(objectStoreId);

    ASSERT(objectStoreMetadata.autoIncrement || key.get());

    transaction->scheduleTask(PutOperation::create(m_backingStore, id(), objectStoreMetadata, value, key, putMode, callbacks, indexIds, indexKeys));
}

void PutOperation::perform(IDBTransactionBackendLevelDB* transaction)
{
    LOG(StorageAPI, "PutOperation");
    ASSERT(transaction->mode() != IndexedDB::TransactionReadOnly);
    ASSERT(m_indexIds.size() == m_indexKeys.size());
    bool keyWasGenerated = false;

    RefPtr<IDBKey> key;
    if (m_putMode != IDBDatabaseBackendInterface::CursorUpdate && m_objectStore.autoIncrement && !m_key) {
        RefPtr<IDBKey> autoIncKey = IDBObjectStoreBackendLevelDB::generateKey(m_backingStore, transaction, m_databaseId, m_objectStore.id);
        keyWasGenerated = true;
        if (!autoIncKey->isValid()) {
            m_callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::ConstraintError, "Maximum key generator value reached."));
            return;
        }
        key = autoIncKey;
    } else
        key = m_key;

    ASSERT(key && key->isValid());

    IDBBackingStore::RecordIdentifier recordIdentifier;
    if (m_putMode == IDBDatabaseBackendInterface::AddOnly) {
        bool found = false;
        bool ok = m_backingStore->keyExistsInObjectStore(transaction->backingStoreTransaction(), m_databaseId, m_objectStore.id, *key, &recordIdentifier, found);
        if (!ok) {
            m_callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::UnknownError, "Internal error checking key existence."));
            return;
        }
        if (found) {
            m_callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::ConstraintError, "Key already exists in the object store."));
            return;
        }
    }

    Vector<OwnPtr<IDBObjectStoreBackendLevelDB::IndexWriter> > indexWriters;
    String errorMessage;
    bool obeysConstraints = false;
    bool backingStoreSuccess = IDBObjectStoreBackendLevelDB::makeIndexWriters(transaction, m_backingStore.get(), m_databaseId, m_objectStore, key, keyWasGenerated, m_indexIds, m_indexKeys, &indexWriters, &errorMessage, obeysConstraints);
    if (!backingStoreSuccess) {
        m_callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::UnknownError, "Internal error: backing store error updating index keys."));
        return;
    }
    if (!obeysConstraints) {
        m_callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::ConstraintError, errorMessage));
        return;
    }

    // Before this point, don't do any mutation. After this point, rollback the transaction in case of error.
    backingStoreSuccess = m_backingStore->putRecord(transaction->backingStoreTransaction(), m_databaseId, m_objectStore.id, *key, m_value, &recordIdentifier);
    if (!backingStoreSuccess) {
        m_callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::UnknownError, "Internal error: backing store error performing put/add."));
        return;
    }

    for (size_t i = 0; i < indexWriters.size(); ++i) {
        IDBObjectStoreBackendLevelDB::IndexWriter* indexWriter = indexWriters[i].get();
        indexWriter->writeIndexKeys(recordIdentifier, *m_backingStore, transaction->backingStoreTransaction(), m_databaseId, m_objectStore.id);
    }

    if (m_objectStore.autoIncrement && m_putMode != IDBDatabaseBackendInterface::CursorUpdate && key->type() == IDBKey::NumberType) {
        bool ok = IDBObjectStoreBackendLevelDB::updateKeyGenerator(m_backingStore, transaction, m_databaseId, m_objectStore.id, key.get(), !keyWasGenerated);
        if (!ok) {
            m_callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::UnknownError, "Internal error updating key generator."));
            return;
        }
    }

    m_callbacks->onSuccess(key.release());
}

void IDBDatabaseBackendLevelDB::setIndexKeys(int64_t transactionId, int64_t objectStoreId, PassRefPtr<IDBKey> prpPrimaryKey, const Vector<int64_t>& indexIds, const Vector<IndexKeys>& indexKeys)
{
    LOG(StorageAPI, "IDBDatabaseBackendLevelDB::setIndexKeys");
    IDBTransactionBackendLevelDB* transaction = m_transactions.get(transactionId);
    if (!transaction)
        return;
    ASSERT(transaction->mode() == IndexedDB::TransactionVersionChange);

    RefPtr<IDBKey> primaryKey = prpPrimaryKey;
    RefPtr<IDBBackingStore> store = backingStore();
    // FIXME: This method could be asynchronous, but we need to evaluate if it's worth the extra complexity.
    IDBBackingStore::RecordIdentifier recordIdentifier;
    bool found = false;
    bool ok = store->keyExistsInObjectStore(transaction->backingStoreTransaction(), m_metadata.id, objectStoreId, *primaryKey, &recordIdentifier, found);
    if (!ok) {
        transaction->abort(IDBDatabaseError::create(IDBDatabaseException::UnknownError, "Internal error setting index keys."));
        return;
    }
    if (!found) {
        RefPtr<IDBDatabaseError> error = IDBDatabaseError::create(IDBDatabaseException::UnknownError, String::format("Internal error setting index keys for object store."));
        transaction->abort(error.release());
        return;
    }

    Vector<OwnPtr<IDBObjectStoreBackendLevelDB::IndexWriter> > indexWriters;
    String errorMessage;
    bool obeysConstraints = false;
    ASSERT(m_metadata.objectStores.contains(objectStoreId));
    const IDBObjectStoreMetadata& objectStoreMetadata = m_metadata.objectStores.get(objectStoreId);
    bool backingStoreSuccess = IDBObjectStoreBackendLevelDB::makeIndexWriters(transaction, store.get(), id(), objectStoreMetadata, primaryKey, false, indexIds, indexKeys, &indexWriters, &errorMessage, obeysConstraints);
    if (!backingStoreSuccess) {
        transaction->abort(IDBDatabaseError::create(IDBDatabaseException::UnknownError, "Internal error: backing store error updating index keys."));
        return;
    }
    if (!obeysConstraints) {
        transaction->abort(IDBDatabaseError::create(IDBDatabaseException::ConstraintError, errorMessage));
        return;
    }

    for (size_t i = 0; i < indexWriters.size(); ++i) {
        IDBObjectStoreBackendLevelDB::IndexWriter* indexWriter = indexWriters[i].get();
        indexWriter->writeIndexKeys(recordIdentifier, *store.get(), transaction->backingStoreTransaction(), id(), objectStoreId);
    }
}

void IDBDatabaseBackendLevelDB::setIndexesReady(int64_t transactionId, int64_t, const Vector<int64_t>& indexIds)
{
    LOG(StorageAPI, "IDBObjectStoreBackendLevelDB::setIndexesReady");

    IDBTransactionBackendLevelDB* transaction = m_transactions.get(transactionId);
    if (!transaction)
        return;

    transaction->scheduleTask(IDBDatabaseBackendInterface::PreemptiveTask, SetIndexesReadyOperation::create(indexIds.size()));
}

void SetIndexesReadyOperation::perform(IDBTransactionBackendLevelDB* transaction)
{
    LOG(StorageAPI, "SetIndexesReadyOperation");
    for (size_t i = 0; i < m_indexCount; ++i)
        transaction->didCompletePreemptiveEvent();
}

void IDBDatabaseBackendLevelDB::openCursor(int64_t transactionId, int64_t objectStoreId, int64_t indexId, PassRefPtr<IDBKeyRange> keyRange, IndexedDB::CursorDirection direction, bool keyOnly, TaskType taskType, PassRefPtr<IDBCallbacks> callbacks)
{
    LOG(StorageAPI, "IDBDatabaseBackendLevelDB::openCursor");
    IDBTransactionBackendLevelDB* transaction = m_transactions.get(transactionId);
    if (!transaction)
        return;

    transaction->scheduleTask(OpenCursorOperation::create(m_backingStore, id(), objectStoreId, indexId, keyRange, direction, keyOnly ? IndexedDB::CursorKeyOnly : IndexedDB::CursorKeyAndValue, taskType, callbacks));
}

void OpenCursorOperation::perform(IDBTransactionBackendLevelDB* transaction)
{
    LOG(StorageAPI, "OpenCursorOperation");

    // The frontend has begun indexing, so this pauses the transaction
    // until the indexing is complete. This can't happen any earlier
    // because we don't want to switch to early mode in case multiple
    // indexes are being created in a row, with put()'s in between.
    if (m_taskType == IDBDatabaseBackendInterface::PreemptiveTask)
        transaction->addPreemptiveEvent();

    RefPtr<IDBBackingStore::Cursor> backingStoreCursor;
    if (m_indexId == IDBIndexMetadata::InvalidId) {
        ASSERT(m_cursorType != IndexedDB::CursorKeyOnly);
        backingStoreCursor = m_backingStore->openObjectStoreCursor(transaction->backingStoreTransaction(), m_databaseId, m_objectStoreId, m_keyRange.get(), m_direction);
    } else {
        ASSERT(m_taskType == IDBDatabaseBackendInterface::NormalTask);
        if (m_cursorType == IndexedDB::CursorKeyOnly)
            backingStoreCursor = m_backingStore->openIndexKeyCursor(transaction->backingStoreTransaction(), m_databaseId, m_objectStoreId, m_indexId, m_keyRange.get(), m_direction);
        else
            backingStoreCursor = m_backingStore->openIndexCursor(transaction->backingStoreTransaction(), m_databaseId, m_objectStoreId, m_indexId, m_keyRange.get(), m_direction);
    }

    if (!backingStoreCursor) {
        m_callbacks->onSuccess(static_cast<SharedBuffer*>(0));
        return;
    }

    IDBDatabaseBackendInterface::TaskType taskType(static_cast<IDBDatabaseBackendInterface::TaskType>(m_taskType));
    RefPtr<IDBCursorBackendLevelDB> cursor = IDBCursorBackendLevelDB::create(backingStoreCursor.get(), m_cursorType, taskType, transaction, m_objectStoreId);
    m_callbacks->onSuccess(cursor, cursor->key(), cursor->primaryKey(), cursor->value());
}

void IDBDatabaseBackendLevelDB::count(int64_t transactionId, int64_t objectStoreId, int64_t indexId, PassRefPtr<IDBKeyRange> keyRange, PassRefPtr<IDBCallbacks> callbacks)
{
    LOG(StorageAPI, "IDBDatabaseBackendLevelDB::count");
    IDBTransactionBackendLevelDB* transaction = m_transactions.get(transactionId);
    if (!transaction)
        return;

    ASSERT(m_metadata.objectStores.contains(objectStoreId));
    transaction->scheduleTask(CountOperation::create(m_backingStore, id(), objectStoreId, indexId, keyRange, callbacks));
}

void CountOperation::perform(IDBTransactionBackendLevelDB* transaction)
{
    LOG(StorageAPI, "CountOperation");
    uint32_t count = 0;
    RefPtr<IDBBackingStore::Cursor> backingStoreCursor;

    if (m_indexId == IDBIndexMetadata::InvalidId)
        backingStoreCursor = m_backingStore->openObjectStoreKeyCursor(transaction->backingStoreTransaction(), m_databaseId, m_objectStoreId, m_keyRange.get(), IndexedDB::CursorNext);
    else
        backingStoreCursor = m_backingStore->openIndexKeyCursor(transaction->backingStoreTransaction(), m_databaseId, m_objectStoreId, m_indexId, m_keyRange.get(), IndexedDB::CursorNext);
    if (!backingStoreCursor) {
        m_callbacks->onSuccess(count);
        return;
    }

    do {
        ++count;
    } while (backingStoreCursor->continueFunction(0));

    m_callbacks->onSuccess(count);
}

void IDBDatabaseBackendLevelDB::deleteRange(int64_t transactionId, int64_t objectStoreId, PassRefPtr<IDBKeyRange> keyRange, PassRefPtr<IDBCallbacks> callbacks)
{
    LOG(StorageAPI, "IDBDatabaseBackendLevelDB::deleteRange");
    IDBTransactionBackendLevelDB* transaction = m_transactions.get(transactionId);
    if (!transaction)
        return;
    ASSERT(transaction->mode() != IndexedDB::TransactionReadOnly);

    transaction->scheduleTask(DeleteRangeOperation::create(m_backingStore, id(), objectStoreId, keyRange, callbacks));
}

void DeleteRangeOperation::perform(IDBTransactionBackendLevelDB* transaction)
{
    LOG(StorageAPI, "DeleteRangeOperation");
    RefPtr<IDBBackingStore::Cursor> backingStoreCursor = m_backingStore->openObjectStoreCursor(transaction->backingStoreTransaction(), m_databaseId, m_objectStoreId, m_keyRange.get(), IndexedDB::CursorNext);
    if (backingStoreCursor) {
        do {
            if (!m_backingStore->deleteRecord(transaction->backingStoreTransaction(), m_databaseId, m_objectStoreId, backingStoreCursor->recordIdentifier())) {
                m_callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::UnknownError, "Error deleting data in range"));
                return;
            }
        } while (backingStoreCursor->continueFunction(0));
    }

    m_callbacks->onSuccess();
}

void IDBDatabaseBackendLevelDB::clear(int64_t transactionId, int64_t objectStoreId, PassRefPtr<IDBCallbacks> callbacks)
{
    LOG(StorageAPI, "IDBDatabaseBackendLevelDB::clear");
    IDBTransactionBackendLevelDB* transaction = m_transactions.get(transactionId);
    if (!transaction)
        return;
    ASSERT(transaction->mode() != IndexedDB::TransactionReadOnly);

    transaction->scheduleTask(ClearOperation::create(m_backingStore, id(), objectStoreId, callbacks));
}

void ClearOperation::perform(IDBTransactionBackendLevelDB* transaction)
{
    LOG(StorageAPI, "ObjectStoreClearOperation");
    if (!m_backingStore->clearObjectStore(transaction->backingStoreTransaction(), m_databaseId, m_objectStoreId)) {
        m_callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::UnknownError, "Error clearing object store"));
        return;
    }
    m_callbacks->onSuccess();
}

void DeleteObjectStoreOperation::perform(IDBTransactionBackendLevelDB* transaction)
{
    LOG(StorageAPI, "DeleteObjectStoreOperation");
    bool ok = m_backingStore->deleteObjectStore(transaction->backingStoreTransaction(), transaction->database()->id(), m_objectStoreMetadata.id);
    if (!ok) {
        RefPtr<IDBDatabaseError> error = IDBDatabaseError::create(IDBDatabaseException::UnknownError, String::format("Internal error deleting object store '%s'.", m_objectStoreMetadata.name.utf8().data()));
        transaction->abort(error);
    }
}

void IDBDatabaseBackendLevelDB::VersionChangeOperation::perform(IDBTransactionBackendLevelDB* transaction)
{
    LOG(StorageAPI, "VersionChangeOperation");
    int64_t databaseId = m_database->id();
    uint64_t oldVersion = m_database->m_metadata.version;

    // FIXME: Database versions are now of type uint64_t, but this code expected int64_t.
    ASSERT(m_version > (int64_t)oldVersion);
    m_database->m_metadata.version = m_version;
    if (!m_database->m_backingStore->updateIDBDatabaseIntVersion(transaction->backingStoreTransaction(), databaseId, m_database->m_metadata.version)) {
        RefPtr<IDBDatabaseError> error = IDBDatabaseError::create(IDBDatabaseException::UnknownError, "Error writing data to stable storage when updating version.");
        m_callbacks->onError(error);
        transaction->abort(error);
        return;
    }
    ASSERT(!m_database->m_pendingSecondHalfOpen);
    m_database->m_pendingSecondHalfOpen = PendingOpenCall::create(m_callbacks, m_databaseCallbacks, m_transactionId, m_version);
    m_callbacks->onUpgradeNeeded(oldVersion, m_database, m_database->metadata());
}

void IDBDatabaseBackendLevelDB::transactionStarted(PassRefPtr<IDBTransactionBackendLevelDB> prpTransaction)
{
    RefPtr<IDBTransactionBackendLevelDB> transaction = prpTransaction;
    if (transaction->mode() == IndexedDB::TransactionVersionChange) {
        ASSERT(!m_runningVersionChangeTransaction);
        m_runningVersionChangeTransaction = transaction;
    }
}

void IDBDatabaseBackendLevelDB::transactionFinished(PassRefPtr<IDBTransactionBackendLevelDB> prpTransaction)
{
    RefPtr<IDBTransactionBackendLevelDB> transaction = prpTransaction;
    ASSERT(m_transactions.contains(transaction->id()));
    ASSERT(m_transactions.get(transaction->id()) == transaction.get());
    m_transactions.remove(transaction->id());
    if (transaction->mode() == IndexedDB::TransactionVersionChange) {
        ASSERT(transaction.get() == m_runningVersionChangeTransaction.get());
        m_runningVersionChangeTransaction.clear();
    }
}

void IDBDatabaseBackendLevelDB::transactionFinishedAndAbortFired(PassRefPtr<IDBTransactionBackendLevelDB> prpTransaction)
{
    RefPtr<IDBTransactionBackendLevelDB> transaction = prpTransaction;
    if (transaction->mode() == IndexedDB::TransactionVersionChange) {
        // If this was an open-with-version call, there will be a "second
        // half" open call waiting for us in processPendingCalls.
        // FIXME: When we no longer support setVersion, assert such a thing.
        if (m_pendingSecondHalfOpen) {
            m_pendingSecondHalfOpen->callbacks()->onError(IDBDatabaseError::create(IDBDatabaseException::AbortError, "Version change transaction was aborted in upgradeneeded event handler."));
            m_pendingSecondHalfOpen.release();
        }
        processPendingCalls();
    }
}

void IDBDatabaseBackendLevelDB::transactionFinishedAndCompleteFired(PassRefPtr<IDBTransactionBackendLevelDB> prpTransaction)
{
    RefPtr<IDBTransactionBackendLevelDB> transaction = prpTransaction;
    if (transaction->mode() == IndexedDB::TransactionVersionChange)
        processPendingCalls();
}

size_t IDBDatabaseBackendLevelDB::connectionCount()
{
    // This does not include pending open calls, as those should not block version changes and deletes.
    return m_databaseCallbacksSet.size();
}

void IDBDatabaseBackendLevelDB::processPendingCalls()
{
    if (m_pendingSecondHalfOpen) {
        // FIXME: Database versions are now of type uint64_t, but this code expected int64_t.
        ASSERT(m_pendingSecondHalfOpen->version() == (int64_t)m_metadata.version);
        ASSERT(m_metadata.id != InvalidId);
        m_pendingSecondHalfOpen->callbacks()->onSuccess(this, this->metadata());
        m_pendingSecondHalfOpen.release();
        // Fall through when complete, as pending deletes may be (partially) unblocked.
    }

    // Note that this check is only an optimization to reduce queue-churn and
    // not necessary for correctness; deleteDatabase and openConnection will
    // requeue their calls if this condition is true.
    if (m_runningVersionChangeTransaction)
        return;

    if (!m_pendingDeleteCalls.isEmpty() && isDeleteDatabaseBlocked())
        return;
    while (!m_pendingDeleteCalls.isEmpty()) {
        OwnPtr<PendingDeleteCall> pendingDeleteCall = m_pendingDeleteCalls.takeFirst();
        deleteDatabaseFinal(pendingDeleteCall->callbacks());
    }
    // deleteDatabaseFinal should never re-queue calls.
    ASSERT(m_pendingDeleteCalls.isEmpty());

    // This check is also not really needed, openConnection would just requeue its calls.
    if (m_runningVersionChangeTransaction)
        return;

    // Open calls can be requeued if an open call started a version change transaction.
    Deque<OwnPtr<PendingOpenCall> > pendingOpenCalls;
    m_pendingOpenCalls.swap(pendingOpenCalls);
    while (!pendingOpenCalls.isEmpty()) {
        OwnPtr<PendingOpenCall> pendingOpenCall = pendingOpenCalls.takeFirst();
        openConnection(pendingOpenCall->callbacks(), pendingOpenCall->databaseCallbacks(), pendingOpenCall->transactionId(), pendingOpenCall->version());
    }
}

void IDBDatabaseBackendLevelDB::createTransaction(int64_t transactionId, PassRefPtr<IDBDatabaseCallbacks> callbacks, const Vector<int64_t>& objectStoreIds, unsigned short mode)
{
    RefPtr<IDBTransactionBackendLevelDB> transaction = IDBTransactionBackendLevelDB::create(transactionId, callbacks, objectStoreIds, static_cast<IndexedDB::TransactionMode>(mode), this);
    ASSERT(!m_transactions.contains(transactionId));
    m_transactions.add(transactionId, transaction.get());
}

void IDBDatabaseBackendLevelDB::openConnection(PassRefPtr<IDBCallbacks> prpCallbacks, PassRefPtr<IDBDatabaseCallbacks> prpDatabaseCallbacks, int64_t transactionId, int64_t version)
{
    ASSERT(m_backingStore.get());
    RefPtr<IDBCallbacks> callbacks = prpCallbacks;
    RefPtr<IDBDatabaseCallbacks> databaseCallbacks = prpDatabaseCallbacks;

    if (!m_pendingDeleteCalls.isEmpty() || m_runningVersionChangeTransaction) {
        m_pendingOpenCalls.append(PendingOpenCall::create(callbacks, databaseCallbacks, transactionId, version));
        return;
    }

    if (m_metadata.id == InvalidId) {
        // The database was deleted then immediately re-opened; openInternal() recreates it in the backing store.
        if (openInternal())
            ASSERT(m_metadata.version == IDBDatabaseMetadata::NoIntVersion);
        else {
            String message;
            RefPtr<IDBDatabaseError> error;
            if (version == IDBDatabaseMetadata::NoIntVersion)
                message = "Internal error opening database with no version specified.";
            else
                message = String::format("Internal error opening database with version %lld", static_cast<long long>(version));
            callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::UnknownError, message));
            return;
        }
    }

    // We infer that the database didn't exist from its lack of either type of version.
    bool isNewDatabase = m_metadata.version == IDBDatabaseMetadata::NoIntVersion;

    if (version == IDBDatabaseMetadata::DefaultIntVersion) {
        // FIXME: this comments was related to Chromium code. It may be incorrect
        // For unit tests only - skip upgrade steps. Calling from script with DefaultIntVersion throws exception.
        ASSERT(isNewDatabase);
        m_databaseCallbacksSet.add(databaseCallbacks);
        callbacks->onSuccess(this, this->metadata());
        return;
    }

    if (version == IDBDatabaseMetadata::NoIntVersion) {
        if (!isNewDatabase) {
            m_databaseCallbacksSet.add(RefPtr<IDBDatabaseCallbacks>(databaseCallbacks));
            callbacks->onSuccess(this, this->metadata());
            return;
        }
        // Spec says: If no version is specified and no database exists, set database version to 1.
        version = 1;
    }

    // FIXME: Database versions are now of type uint64_t, but this code expected int64_t.
    if (version > (int64_t)m_metadata.version) {
        runIntVersionChangeTransaction(callbacks, databaseCallbacks, transactionId, version);
        return;
    }

    // FIXME: Database versions are now of type uint64_t, but this code expected int64_t.
    if (version < (int64_t)m_metadata.version) {
        callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::VersionError, String::format("The requested version (%lld) is less than the existing version (%lld).", static_cast<long long>(version), static_cast<long long>(m_metadata.version))));
        return;
    }

    // FIXME: Database versions are now of type uint64_t, but this code expected int64_t.
    ASSERT(version == (int64_t)m_metadata.version);
    m_databaseCallbacksSet.add(databaseCallbacks);
    callbacks->onSuccess(this, this->metadata());
}

void IDBDatabaseBackendLevelDB::runIntVersionChangeTransaction(PassRefPtr<IDBCallbacks> prpCallbacks, PassRefPtr<IDBDatabaseCallbacks> prpDatabaseCallbacks, int64_t transactionId, int64_t requestedVersion)
{
    RefPtr<IDBCallbacks> callbacks = prpCallbacks;
    RefPtr<IDBDatabaseCallbacks> databaseCallbacks = prpDatabaseCallbacks;
    ASSERT(callbacks);
    for (DatabaseCallbacksSet::const_iterator it = m_databaseCallbacksSet.begin(); it != m_databaseCallbacksSet.end(); ++it) {
        // Front end ensures the event is not fired at connections that have closePending set.
        if (*it != databaseCallbacks)
            (*it)->onVersionChange(m_metadata.version, requestedVersion, IndexedDB::NullVersion);
    }
    // The spec dictates we wait until all the version change events are
    // delivered and then check m_databaseCallbacks.empty() before proceeding
    // or firing a blocked event, but instead we should be consistent with how
    // the old setVersion (incorrectly) did it.
    // FIXME: Remove the call to onBlocked and instead wait until the frontend
    // tells us that all the blocked events have been delivered. See
    // https://bugs.webkit.org/show_bug.cgi?id=71130
    if (connectionCount())
        callbacks->onBlocked(m_metadata.version);
    // FIXME: Add test for m_runningVersionChangeTransaction.
    if (m_runningVersionChangeTransaction || connectionCount()) {
        m_pendingOpenCalls.append(PendingOpenCall::create(callbacks, databaseCallbacks, transactionId, requestedVersion));
        return;
    }

    Vector<int64_t> objectStoreIds;
    createTransaction(transactionId, databaseCallbacks, objectStoreIds, IndexedDB::TransactionVersionChange);
    RefPtr<IDBTransactionBackendLevelDB> transaction = m_transactions.get(transactionId);

    transaction->scheduleTask(VersionChangeOperation::create(this, transactionId, requestedVersion, callbacks, databaseCallbacks), VersionChangeAbortOperation::create(this, String::number(m_metadata.version), m_metadata.version));

    ASSERT(!m_pendingSecondHalfOpen);
    m_databaseCallbacksSet.add(databaseCallbacks);
}

void IDBDatabaseBackendLevelDB::deleteDatabase(PassRefPtr<IDBCallbacks> prpCallbacks)
{
    RefPtr<IDBCallbacks> callbacks = prpCallbacks;
    if (isDeleteDatabaseBlocked()) {
        for (DatabaseCallbacksSet::const_iterator it = m_databaseCallbacksSet.begin(); it != m_databaseCallbacksSet.end(); ++it) {
            // Front end ensures the event is not fired at connections that have closePending set.
            (*it)->onVersionChange(m_metadata.version, 0, IndexedDB::NullVersion);
        }
        // FIXME: Only fire onBlocked if there are open connections after the
        // VersionChangeEvents are received, not just set up to fire.
        // https://bugs.webkit.org/show_bug.cgi?id=71130
        callbacks->onBlocked(m_metadata.version);
        m_pendingDeleteCalls.append(PendingDeleteCall::create(callbacks.release()));
        return;
    }
    deleteDatabaseFinal(callbacks.release());
}

bool IDBDatabaseBackendLevelDB::isDeleteDatabaseBlocked()
{
    return connectionCount();
}

void IDBDatabaseBackendLevelDB::deleteDatabaseFinal(PassRefPtr<IDBCallbacks> callbacks)
{
    ASSERT(!isDeleteDatabaseBlocked());
    ASSERT(m_backingStore);
    if (!m_backingStore->deleteDatabase(m_metadata.name)) {
        callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::UnknownError, "Internal error deleting database."));
        return;
    }
    m_metadata.id = InvalidId;
    m_metadata.version = IDBDatabaseMetadata::NoIntVersion;
    m_metadata.objectStores.clear();
    callbacks->onSuccess();
}

void IDBDatabaseBackendLevelDB::close(PassRefPtr<IDBDatabaseCallbacks> prpCallbacks)
{
    RefPtr<IDBDatabaseCallbacks> callbacks = prpCallbacks;
    ASSERT(m_databaseCallbacksSet.contains(callbacks));

    m_databaseCallbacksSet.remove(callbacks);
    if (m_pendingSecondHalfOpen && m_pendingSecondHalfOpen->databaseCallbacks() == callbacks) {
        m_pendingSecondHalfOpen->callbacks()->onError(IDBDatabaseError::create(IDBDatabaseException::AbortError, "The connection was closed."));
        m_pendingSecondHalfOpen.release();
    }

    if (connectionCount() > 1)
        return;

    // processPendingCalls allows the inspector to process a pending open call
    // and call close, reentering IDBDatabaseBackendLevelDB::close. Then the
    // backend would be removed both by the inspector closing its connection, and
    // by the connection that first called close.
    // To avoid that situation, don't proceed in case of reentrancy.
    if (m_closingConnection)
        return;
    TemporaryChange<bool> closingConnection(m_closingConnection, true);
    processPendingCalls();

    // FIXME: Add a test for the m_pendingOpenCalls cases below.
    if (!connectionCount() && !m_pendingOpenCalls.size() && !m_pendingDeleteCalls.size()) {
        TransactionMap transactions(m_transactions);
        RefPtr<IDBDatabaseError> error = IDBDatabaseError::create(IDBDatabaseException::UnknownError, "Connection is closing.");
        for (TransactionMap::const_iterator::Values it = transactions.values().begin(), end = transactions.values().end(); it != end; ++it)
            (*it)->abort(error);

        ASSERT(m_transactions.isEmpty());

        m_backingStore.clear();

        // This check should only be false in unit tests.
        ASSERT(m_factory);
        if (m_factory)
            m_factory->removeIDBDatabaseBackend(m_identifier);
    }
}

void CreateObjectStoreAbortOperation::perform(IDBTransactionBackendLevelDB* transaction)
{
    LOG(StorageAPI, "CreateObjectStoreAbortOperation");
    ASSERT_UNUSED(transaction, !transaction);
    m_database->removeObjectStore(m_objectStoreId);
}

void DeleteObjectStoreAbortOperation::perform(IDBTransactionBackendLevelDB* transaction)
{
    LOG(StorageAPI, "DeleteObjectStoreAbortOperation");
    ASSERT_UNUSED(transaction, !transaction);
    m_database->addObjectStore(m_objectStoreMetadata, IDBObjectStoreMetadata::InvalidId);
}

void IDBDatabaseBackendLevelDB::VersionChangeAbortOperation::perform(IDBTransactionBackendLevelDB* transaction)
{
    LOG(StorageAPI, "VersionChangeAbortOperation");
    ASSERT_UNUSED(transaction, !transaction);
    m_database->m_metadata.version = m_previousIntVersion;
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE) && USE(LEVELDB)

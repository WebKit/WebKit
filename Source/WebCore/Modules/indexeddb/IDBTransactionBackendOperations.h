/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#ifndef IDBTransactionBackendOperations_h
#define IDBTransactionBackendOperations_h

#include "IDBDatabaseBackend.h"
#include "IDBOperation.h"
#include "IDBTransactionBackend.h"

#if ENABLE(INDEXED_DATABASE)

namespace WebCore {

class IDBServerConnection;

class CreateObjectStoreOperation : public IDBOperation {
public:
    static PassRefPtr<IDBOperation> create(IDBTransactionBackend* transaction, const IDBObjectStoreMetadata& objectStoreMetadata)
    {
        return adoptRef(new CreateObjectStoreOperation(transaction, objectStoreMetadata));
    }
    virtual void perform(std::function<void()> successCallback) override final;

    const IDBObjectStoreMetadata& objectStoreMetadata() const { return m_objectStoreMetadata; }

private:
    CreateObjectStoreOperation(IDBTransactionBackend* transaction, const IDBObjectStoreMetadata& objectStoreMetadata)
        : m_transaction(transaction)
        , m_objectStoreMetadata(objectStoreMetadata)
    {
    }
    
    RefPtr<IDBTransactionBackend> m_transaction;
    const IDBObjectStoreMetadata m_objectStoreMetadata;
};

class DeleteObjectStoreOperation : public IDBOperation {
public:
    static PassRefPtr<IDBOperation> create(IDBTransactionBackend* transaction, const IDBObjectStoreMetadata& objectStoreMetadata)
    {
        return adoptRef(new DeleteObjectStoreOperation(transaction, objectStoreMetadata));
    }
    virtual void perform(std::function<void()> successCallback) override final;

    IDBTransactionBackend* transaction() const { return m_transaction.get(); }
    const IDBObjectStoreMetadata& objectStoreMetadata() const { return m_objectStoreMetadata; }

private:
    DeleteObjectStoreOperation(IDBTransactionBackend* transaction, const IDBObjectStoreMetadata& objectStoreMetadata)
        : m_transaction(transaction)
        , m_objectStoreMetadata(objectStoreMetadata)
    {
    }

    RefPtr<IDBTransactionBackend> m_transaction;
    const IDBObjectStoreMetadata m_objectStoreMetadata;
};

class IDBDatabaseBackend::VersionChangeOperation : public IDBOperation {
public:
    static PassRefPtr<IDBOperation> create(IDBTransactionBackend* transaction, int64_t version, PassRefPtr<IDBCallbacks> callbacks, PassRefPtr<IDBDatabaseCallbacks> databaseCallbacks)
    {
        return adoptRef(new VersionChangeOperation(transaction, version, callbacks, databaseCallbacks));
    }
    virtual void perform(std::function<void()> successCallback) override final;

    IDBTransactionBackend* transaction() const { return m_transaction.get(); }
    int64_t version() const { return m_version; }
    IDBDatabaseCallbacks* databaseCallbacks() const { return m_databaseCallbacks.get(); }

private:
    VersionChangeOperation(IDBTransactionBackend* transaction, int64_t version, PassRefPtr<IDBCallbacks> callbacks, PassRefPtr<IDBDatabaseCallbacks> databaseCallbacks)
        : m_transaction(transaction)
        , m_version(version)
        , m_callbacks(callbacks)
        , m_databaseCallbacks(databaseCallbacks)
    {
    }

    RefPtr<IDBTransactionBackend> m_transaction;
    int64_t m_version;
    RefPtr<IDBCallbacks> m_callbacks;
    RefPtr<IDBDatabaseCallbacks> m_databaseCallbacks;
};

class CreateObjectStoreAbortOperation : public IDBSynchronousOperation {
public:
    static PassRefPtr<IDBSynchronousOperation> create(IDBTransactionBackend* transaction, int64_t objectStoreId)
    {
        return adoptRef(new CreateObjectStoreAbortOperation(transaction, objectStoreId));
    }
    virtual void perform() override final;
private:
    CreateObjectStoreAbortOperation(IDBTransactionBackend* transaction, int64_t objectStoreId)
        : m_transaction(transaction)
        , m_objectStoreID(objectStoreId)
    {
    }

    RefPtr<IDBTransactionBackend> m_transaction;
    const int64_t m_objectStoreID;
};

class DeleteObjectStoreAbortOperation : public IDBSynchronousOperation {
public:
    static PassRefPtr<IDBSynchronousOperation> create(IDBTransactionBackend* transaction, const IDBObjectStoreMetadata& objectStore)
    {
        return adoptRef(new DeleteObjectStoreAbortOperation(transaction, objectStore));
    }
    virtual void perform() override final;
private:
    DeleteObjectStoreAbortOperation(IDBTransactionBackend* transaction, const IDBObjectStoreMetadata& objectStoreMetadata)
        : m_transaction(transaction)
        , m_objectStoreMetadata(objectStoreMetadata)
    {
    }

    RefPtr<IDBTransactionBackend> m_transaction;
    IDBObjectStoreMetadata m_objectStoreMetadata;
};

class IDBDatabaseBackend::VersionChangeAbortOperation : public IDBSynchronousOperation {
public:
    static PassRefPtr<IDBSynchronousOperation> create(IDBTransactionBackend* transaction, const String& previousVersion, int64_t previousIntVersion)
    {
        return adoptRef(new VersionChangeAbortOperation(transaction, previousVersion, previousIntVersion));
    }
    virtual void perform() override final;
private:
    VersionChangeAbortOperation(IDBTransactionBackend* transaction, const String& previousVersion, int64_t previousIntVersion)
        : m_transaction(transaction)
        , m_previousVersion(previousVersion)
        , m_previousIntVersion(previousIntVersion)
    {
    }

    RefPtr<IDBTransactionBackend> m_transaction;
    String m_previousVersion;
    int64_t m_previousIntVersion;
};

class CreateIndexOperation : public IDBOperation {
public:
    static PassRefPtr<IDBOperation> create(IDBTransactionBackend* transaction, int64_t objectStoreId, const IDBIndexMetadata& indexMetadata)
    {
        return adoptRef(new CreateIndexOperation(transaction, objectStoreId, indexMetadata));
    }
    virtual void perform(std::function<void()> successCallback) override final;

    int64_t objectStoreID() const { return m_objectStoreID; }
    const IDBIndexMetadata& idbIndexMetadata() const { return m_indexMetadata; }

private:
    CreateIndexOperation(IDBTransactionBackend* transaction, int64_t objectStoreId, const IDBIndexMetadata& indexMetadata)
        : m_transaction(transaction)
        , m_objectStoreID(objectStoreId)
        , m_indexMetadata(indexMetadata)
    {
    }

    RefPtr<IDBTransactionBackend> m_transaction;
    const int64_t m_objectStoreID;
    const IDBIndexMetadata m_indexMetadata;
};

class CreateIndexAbortOperation : public IDBSynchronousOperation {
public:
    static PassRefPtr<IDBSynchronousOperation> create(IDBTransactionBackend* transaction, int64_t objectStoreId, int64_t indexId)
    {
        return adoptRef(new CreateIndexAbortOperation(transaction, objectStoreId, indexId));
    }
    virtual void perform() override final;
private:
    CreateIndexAbortOperation(IDBTransactionBackend* transaction, int64_t objectStoreId, int64_t indexId)
        : m_transaction(transaction)
        , m_objectStoreID(objectStoreId)
        , m_indexID(indexId)
    {
    }

    const RefPtr<IDBTransactionBackend> m_transaction;
    const int64_t m_objectStoreID;
    const int64_t m_indexID;
};

class DeleteIndexOperation : public IDBOperation {
public:
    static PassRefPtr<IDBOperation> create(IDBTransactionBackend* transaction, int64_t objectStoreId, const IDBIndexMetadata& indexMetadata)
    {
        return adoptRef(new DeleteIndexOperation(transaction, objectStoreId, indexMetadata));
    }
    virtual void perform(std::function<void()> successCallback) override final;

    int64_t objectStoreID() const { return m_objectStoreID; }
    const IDBIndexMetadata& idbIndexMetadata() const { return m_indexMetadata; }

private:
    DeleteIndexOperation(IDBTransactionBackend* transaction, int64_t objectStoreId, const IDBIndexMetadata& indexMetadata)
        : m_transaction(transaction)
        , m_objectStoreID(objectStoreId)
        , m_indexMetadata(indexMetadata)
    {
    }

    RefPtr<IDBTransactionBackend> m_transaction;
    const int64_t m_objectStoreID;
    const IDBIndexMetadata m_indexMetadata;
};

class DeleteIndexAbortOperation : public IDBSynchronousOperation {
public:
    static PassRefPtr<IDBSynchronousOperation> create(IDBTransactionBackend* transaction, int64_t objectStoreId, const IDBIndexMetadata& indexMetadata)
    {
        return adoptRef(new DeleteIndexAbortOperation(transaction, objectStoreId, indexMetadata));
    }
    virtual void perform() override final;
private:
    DeleteIndexAbortOperation(IDBTransactionBackend* transaction, int64_t objectStoreId, const IDBIndexMetadata& indexMetadata)
        : m_transaction(transaction)
        , m_objectStoreID(objectStoreId)
        , m_indexMetadata(indexMetadata)
    {
    }

    RefPtr<IDBTransactionBackend> m_transaction;
    const int64_t m_objectStoreID;
    const IDBIndexMetadata m_indexMetadata;
};

class GetOperation : public IDBOperation {
public:
    static PassRefPtr<IDBOperation> create(IDBTransactionBackend* transaction, const IDBDatabaseMetadata& metadata, int64_t objectStoreId, int64_t indexId, PassRefPtr<IDBKeyRange> keyRange, IndexedDB::CursorType cursorType, PassRefPtr<IDBCallbacks> callbacks)
    {
        return adoptRef(new GetOperation(transaction, metadata, objectStoreId, indexId, keyRange, cursorType, callbacks));
    }
    virtual void perform(std::function<void()> successCallback) override final;

    int64_t objectStoreID() const { return m_objectStoreID; }
    int64_t indexID() const { return m_indexID; }
    IndexedDB::CursorType cursorType() const { return m_cursorType; }
    IDBKeyRange* keyRange() const { return m_keyRange.get(); }
    bool autoIncrement() const { return m_autoIncrement; }
    IDBKeyPath keyPath() const { return m_keyPath; }

private:
    GetOperation(IDBTransactionBackend* transaction, const IDBDatabaseMetadata& metadata, int64_t objectStoreId, int64_t indexId, PassRefPtr<IDBKeyRange> keyRange, IndexedDB::CursorType cursorType, PassRefPtr<IDBCallbacks> callbacks)
        : m_transaction(transaction)
        , m_objectStoreID(objectStoreId)
        , m_indexID(indexId)
        , m_keyPath(metadata.objectStores.get(objectStoreId).keyPath)
        , m_autoIncrement(metadata.objectStores.get(objectStoreId).autoIncrement)
        , m_keyRange(keyRange)
        , m_cursorType(cursorType)
        , m_callbacks(callbacks)
    {
        ASSERT(metadata.objectStores.contains(objectStoreId));
        ASSERT(metadata.objectStores.get(objectStoreId).id == objectStoreId);
    }

    RefPtr<IDBTransactionBackend> m_transaction;
    const int64_t m_objectStoreID;
    const int64_t m_indexID;
    const IDBKeyPath m_keyPath;
    const bool m_autoIncrement;
    const RefPtr<IDBKeyRange> m_keyRange;
    const IndexedDB::CursorType m_cursorType;
    const RefPtr<IDBCallbacks> m_callbacks;
};

class PutOperation : public IDBOperation {
public:
    static PassRefPtr<IDBOperation> create(IDBTransactionBackend* transaction, const IDBObjectStoreMetadata& objectStore, PassRefPtr<SharedBuffer> value, PassRefPtr<IDBKey> key, IDBDatabaseBackend::PutMode putMode, PassRefPtr<IDBCallbacks> callbacks, const Vector<int64_t>& indexIds, const Vector<IndexKeys>& indexKeys)
    {
        return adoptRef(new PutOperation(transaction, objectStore, value, key, putMode, callbacks, indexIds, indexKeys));
    }
    virtual void perform(std::function<void()> successCallback) override final;

    IDBDatabaseBackend::PutMode putMode() const { return m_putMode; }
    const IDBObjectStoreMetadata& objectStore() const { return m_objectStore; }
    IDBKey* key() const { return m_key.get(); }
    const Vector<int64_t>& indexIDs() const { return m_indexIDs; }
    const Vector<IndexKeys>& indexKeys() const { return m_indexKeys; }
    SharedBuffer* value() const { return m_value.get(); }

private:
    PutOperation(IDBTransactionBackend* transaction, const IDBObjectStoreMetadata& objectStore, PassRefPtr<SharedBuffer>& value, PassRefPtr<IDBKey> key, IDBDatabaseBackend::PutMode putMode, PassRefPtr<IDBCallbacks> callbacks, const Vector<int64_t>& indexIds, const Vector<IndexKeys>& indexKeys)
        : m_transaction(transaction)
        , m_objectStore(objectStore)
        , m_value(value)
        , m_key(key)
        , m_putMode(putMode)
        , m_callbacks(callbacks)
        , m_indexIDs(indexIds)
        , m_indexKeys(indexKeys)
    {
    }

    RefPtr<IDBTransactionBackend> m_transaction;
    const IDBObjectStoreMetadata m_objectStore;
    const RefPtr<SharedBuffer> m_value;
    const RefPtr<IDBKey> m_key;
    const IDBDatabaseBackend::PutMode m_putMode;
    const RefPtr<IDBCallbacks> m_callbacks;
    const Vector<int64_t> m_indexIDs;
    const Vector<IndexKeys> m_indexKeys;
};

class SetIndexesReadyOperation : public IDBOperation {
public:
    static PassRefPtr<IDBOperation> create(IDBTransactionBackend* transaction, size_t indexCount)
    {
        return adoptRef(new SetIndexesReadyOperation(transaction, indexCount));
    }
    virtual void perform(std::function<void()> successCallback) override final;
private:
    SetIndexesReadyOperation(IDBTransactionBackend* transaction, size_t indexCount)
        : m_transaction(transaction)
        , m_indexCount(indexCount)
    {
    }

    RefPtr<IDBTransactionBackend> m_transaction;
    const size_t m_indexCount;
};

class OpenCursorOperation : public IDBOperation {
public:
    static PassRefPtr<IDBOperation> create(IDBTransactionBackend* transaction, int64_t objectStoreId, int64_t indexId, PassRefPtr<IDBKeyRange> keyRange, IndexedDB::CursorDirection direction, IndexedDB::CursorType cursorType, IDBDatabaseBackend::TaskType taskType, PassRefPtr<IDBCallbacks> callbacks)
    {
        return adoptRef(new OpenCursorOperation(transaction, objectStoreId, indexId, keyRange, direction, cursorType, taskType, callbacks));
    }
    virtual void perform(std::function<void()> successCallback) override final;

    int64_t objectStoreID() const { return m_objectStoreID; }
    int64_t indexID() const { return m_indexID; }
    IndexedDB::CursorDirection direction() const { return m_direction; }
    IndexedDB::CursorType cursorType() const { return m_cursorType; }
    IDBDatabaseBackend::TaskType taskType() const { return m_taskType; }
    IDBKeyRange* keyRange() const { return m_keyRange.get(); }
    IndexedDB::CursorDirection cursorDirection() const { return m_direction; }

private:
    OpenCursorOperation(IDBTransactionBackend* transaction, int64_t objectStoreId, int64_t indexId, PassRefPtr<IDBKeyRange> keyRange, IndexedDB::CursorDirection direction, IndexedDB::CursorType cursorType, IDBDatabaseBackend::TaskType taskType, PassRefPtr<IDBCallbacks> callbacks)
        : m_transaction(transaction)
        , m_objectStoreID(objectStoreId)
        , m_indexID(indexId)
        , m_keyRange(keyRange)
        , m_direction(direction)
        , m_cursorType(cursorType)
        , m_taskType(taskType)
        , m_callbacks(callbacks)
    {
    }

    RefPtr<IDBTransactionBackend> m_transaction;
    const int64_t m_objectStoreID;
    const int64_t m_indexID;
    const PassRefPtr<IDBKeyRange> m_keyRange;
    const IndexedDB::CursorDirection m_direction;
    const IndexedDB::CursorType m_cursorType;
    const IDBDatabaseBackend::TaskType m_taskType;
    const RefPtr<IDBCallbacks> m_callbacks;
};

class CountOperation : public IDBOperation {
public:
    static PassRefPtr<IDBOperation> create(IDBTransactionBackend* transaction, int64_t objectStoreId, int64_t indexId, PassRefPtr<IDBKeyRange> keyRange, PassRefPtr<IDBCallbacks> callbacks)
    {
        return adoptRef(new CountOperation(transaction, objectStoreId, indexId, keyRange, callbacks));
    }
    virtual void perform(std::function<void()> successCallback) override final;

    int64_t objectStoreID() const { return m_objectStoreID; }
    int64_t indexID() const { return m_indexID; }
    IDBKeyRange* keyRange() const { return m_keyRange.get(); }

private:
    CountOperation(IDBTransactionBackend* transaction, int64_t objectStoreId, int64_t indexId, PassRefPtr<IDBKeyRange> keyRange, PassRefPtr<IDBCallbacks> callbacks)
        : m_transaction(transaction)
        , m_objectStoreID(objectStoreId)
        , m_indexID(indexId)
        , m_keyRange(keyRange)
        , m_callbacks(callbacks)
    {
    }

    RefPtr<IDBTransactionBackend> m_transaction;
    const int64_t m_objectStoreID;
    const int64_t m_indexID;
    const RefPtr<IDBKeyRange> m_keyRange;
    const RefPtr<IDBCallbacks> m_callbacks;
};

class DeleteRangeOperation : public IDBOperation {
public:
    static PassRefPtr<IDBOperation> create(IDBTransactionBackend* transaction, int64_t objectStoreId, PassRefPtr<IDBKeyRange> keyRange, PassRefPtr<IDBCallbacks> callbacks)
    {
        return adoptRef(new DeleteRangeOperation(transaction, objectStoreId, keyRange, callbacks));
    }
    virtual void perform(std::function<void()> successCallback) override final;

    int64_t objectStoreID() const { return m_objectStoreID; }
    IDBKeyRange* keyRange() const { return m_keyRange.get(); }

private:
    DeleteRangeOperation(IDBTransactionBackend* transaction, int64_t objectStoreId, PassRefPtr<IDBKeyRange> keyRange, PassRefPtr<IDBCallbacks> callbacks)
        : m_transaction(transaction)
        , m_objectStoreID(objectStoreId)
        , m_keyRange(keyRange)
        , m_callbacks(callbacks)
    {
    }

    RefPtr<IDBTransactionBackend> m_transaction;
    const int64_t m_objectStoreID;
    const RefPtr<IDBKeyRange> m_keyRange;
    const RefPtr<IDBCallbacks> m_callbacks;
};

class ClearObjectStoreOperation : public IDBOperation {
public:
    static PassRefPtr<IDBOperation> create(IDBTransactionBackend* transaction, int64_t objectStoreId, PassRefPtr<IDBCallbacks> callbacks)
    {
        return adoptRef(new ClearObjectStoreOperation(transaction, objectStoreId, callbacks));
    }
    virtual void perform(std::function<void()> successCallback) override final;

    IDBTransactionBackend* transaction() const { return m_transaction.get(); }
    int64_t objectStoreID() const { return m_objectStoreID; }

private:
    ClearObjectStoreOperation(IDBTransactionBackend* transaction, int64_t objectStoreId, PassRefPtr<IDBCallbacks> callbacks)
        : m_transaction(transaction)
        , m_objectStoreID(objectStoreId)
        , m_callbacks(callbacks)
    {
    }

    RefPtr<IDBTransactionBackend> m_transaction;
    const int64_t m_objectStoreID;
    const RefPtr<IDBCallbacks> m_callbacks;
};

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
#endif // IDBTransactionBackendOperations_h

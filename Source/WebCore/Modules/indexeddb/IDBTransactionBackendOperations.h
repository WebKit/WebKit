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
#include "IDBServerConnection.h"
#include "IDBTransactionBackend.h"

#if ENABLE(INDEXED_DATABASE)

namespace WebCore {

class CreateObjectStoreOperation : public IDBOperation {
public:
    static PassRefPtr<IDBOperation> create(IDBTransactionBackend* transaction, const IDBObjectStoreMetadata& objectStoreMetadata)
    {
        return adoptRef(new CreateObjectStoreOperation(transaction, objectStoreMetadata));
    }
    virtual void perform(std::function<void()> successCallback) OVERRIDE FINAL;
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
    virtual void perform(std::function<void()> successCallback) OVERRIDE FINAL;
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
    static PassRefPtr<IDBOperation> create(IDBTransactionBackend* transaction, int64_t transactionId, int64_t version, PassRefPtr<IDBCallbacks> callbacks, PassRefPtr<IDBDatabaseCallbacks> databaseCallbacks)
    {
        return adoptRef(new VersionChangeOperation(transaction, transactionId, version, callbacks, databaseCallbacks));
    }
    virtual void perform(std::function<void()> successCallback) OVERRIDE FINAL;
private:
    VersionChangeOperation(IDBTransactionBackend* transaction, int64_t transactionId, int64_t version, PassRefPtr<IDBCallbacks> callbacks, PassRefPtr<IDBDatabaseCallbacks> databaseCallbacks)
        : m_transaction(transaction)
        , m_transactionId(transactionId)
        , m_version(version)
        , m_callbacks(callbacks)
        , m_databaseCallbacks(databaseCallbacks)
    {
    }

    RefPtr<IDBTransactionBackend> m_transaction;
    int64_t m_transactionId;
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
    virtual void perform() OVERRIDE FINAL;
private:
    CreateObjectStoreAbortOperation(IDBTransactionBackend* transaction, int64_t objectStoreId)
        : m_transaction(transaction)
        , m_objectStoreId(objectStoreId)
    {
    }

    RefPtr<IDBTransactionBackend> m_transaction;
    const int64_t m_objectStoreId;
};

class DeleteObjectStoreAbortOperation : public IDBSynchronousOperation {
public:
    static PassRefPtr<IDBSynchronousOperation> create(IDBTransactionBackend* transaction, const IDBObjectStoreMetadata& objectStore)
    {
        return adoptRef(new DeleteObjectStoreAbortOperation(transaction, objectStore));
    }
    virtual void perform() OVERRIDE FINAL;
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
    virtual void perform() OVERRIDE FINAL;
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
    virtual void perform(std::function<void()> successCallback) OVERRIDE FINAL;
private:
    CreateIndexOperation(IDBTransactionBackend* transaction, int64_t objectStoreId, const IDBIndexMetadata& indexMetadata)
        : m_transaction(transaction)
        , m_objectStoreId(objectStoreId)
        , m_indexMetadata(indexMetadata)
    {
    }

    RefPtr<IDBTransactionBackend> m_transaction;
    const int64_t m_objectStoreId;
    const IDBIndexMetadata m_indexMetadata;
};

class CreateIndexAbortOperation : public IDBSynchronousOperation {
public:
    static PassRefPtr<IDBSynchronousOperation> create(IDBTransactionBackend* transaction, int64_t objectStoreId, int64_t indexId)
    {
        return adoptRef(new CreateIndexAbortOperation(transaction, objectStoreId, indexId));
    }
    virtual void perform() OVERRIDE FINAL;
private:
    CreateIndexAbortOperation(IDBTransactionBackend* transaction, int64_t objectStoreId, int64_t indexId)
        : m_transaction(transaction)
        , m_objectStoreId(objectStoreId)
        , m_indexId(indexId)
    {
    }

    const RefPtr<IDBTransactionBackend> m_transaction;
    const int64_t m_objectStoreId;
    const int64_t m_indexId;
};

class DeleteIndexOperation : public IDBOperation {
public:
    static PassRefPtr<IDBOperation> create(IDBTransactionBackend* transaction, int64_t objectStoreId, const IDBIndexMetadata& indexMetadata)
    {
        return adoptRef(new DeleteIndexOperation(transaction, objectStoreId, indexMetadata));
    }
    virtual void perform(std::function<void()> successCallback) OVERRIDE FINAL;
private:
    DeleteIndexOperation(IDBTransactionBackend* transaction, int64_t objectStoreId, const IDBIndexMetadata& indexMetadata)
        : m_transaction(transaction)
        , m_objectStoreId(objectStoreId)
        , m_indexMetadata(indexMetadata)
    {
    }

    RefPtr<IDBTransactionBackend> m_transaction;
    const int64_t m_objectStoreId;
    const IDBIndexMetadata m_indexMetadata;
};

class DeleteIndexAbortOperation : public IDBSynchronousOperation {
public:
    static PassRefPtr<IDBSynchronousOperation> create(IDBTransactionBackend* transaction, int64_t objectStoreId, const IDBIndexMetadata& indexMetadata)
    {
        return adoptRef(new DeleteIndexAbortOperation(transaction, objectStoreId, indexMetadata));
    }
    virtual void perform() OVERRIDE FINAL;
private:
    DeleteIndexAbortOperation(IDBTransactionBackend* transaction, int64_t objectStoreId, const IDBIndexMetadata& indexMetadata)
        : m_transaction(transaction)
        , m_objectStoreId(objectStoreId)
        , m_indexMetadata(indexMetadata)
    {
    }

    RefPtr<IDBTransactionBackend> m_transaction;
    const int64_t m_objectStoreId;
    const IDBIndexMetadata m_indexMetadata;
};

class GetOperation : public IDBOperation {
public:
    static PassRefPtr<IDBOperation> create(IDBTransactionBackend* transaction, const IDBDatabaseMetadata& metadata, int64_t objectStoreId, int64_t indexId, PassRefPtr<IDBKeyRange> keyRange, IndexedDB::CursorType cursorType, PassRefPtr<IDBCallbacks> callbacks)
    {
        return adoptRef(new GetOperation(transaction, metadata, objectStoreId, indexId, keyRange, cursorType, callbacks));
    }
    virtual void perform(std::function<void()> successCallback) OVERRIDE FINAL;
private:
    GetOperation(IDBTransactionBackend* transaction, const IDBDatabaseMetadata& metadata, int64_t objectStoreId, int64_t indexId, PassRefPtr<IDBKeyRange> keyRange, IndexedDB::CursorType cursorType, PassRefPtr<IDBCallbacks> callbacks)
        : m_transaction(transaction)
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

    RefPtr<IDBTransactionBackend> m_transaction;
    const int64_t m_databaseId;
    const int64_t m_objectStoreId;
    const int64_t m_indexId;
    const IDBKeyPath m_keyPath;
    const bool m_autoIncrement;
    const RefPtr<IDBKeyRange> m_keyRange;
    const IndexedDB::CursorType m_cursorType;
    const RefPtr<IDBCallbacks> m_callbacks;
};

class PutOperation : public IDBOperation {
public:
    static PassRefPtr<IDBOperation> create(IDBTransactionBackend* transaction, int64_t databaseId, const IDBObjectStoreMetadata& objectStore, PassRefPtr<SharedBuffer> value, PassRefPtr<IDBKey> key, IDBDatabaseBackend::PutMode putMode, PassRefPtr<IDBCallbacks> callbacks, const Vector<int64_t>& indexIds, const Vector<IndexKeys>& indexKeys)
    {
        return adoptRef(new PutOperation(transaction, databaseId, objectStore, value, key, putMode, callbacks, indexIds, indexKeys));
    }
    virtual void perform(std::function<void()> successCallback) OVERRIDE FINAL;
private:
    PutOperation(IDBTransactionBackend* transaction, int64_t databaseId, const IDBObjectStoreMetadata& objectStore, PassRefPtr<SharedBuffer>& value, PassRefPtr<IDBKey> key, IDBDatabaseBackend::PutMode putMode, PassRefPtr<IDBCallbacks> callbacks, const Vector<int64_t>& indexIds, const Vector<IndexKeys>& indexKeys)
        : m_transaction(transaction)
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

    RefPtr<IDBTransactionBackend> m_transaction;
    const int64_t m_databaseId;
    const IDBObjectStoreMetadata m_objectStore;
    const RefPtr<SharedBuffer> m_value;
    const RefPtr<IDBKey> m_key;
    const IDBDatabaseBackend::PutMode m_putMode;
    const RefPtr<IDBCallbacks> m_callbacks;
    const Vector<int64_t> m_indexIds;
    const Vector<IndexKeys> m_indexKeys;
};

class SetIndexesReadyOperation : public IDBOperation {
public:
    static PassRefPtr<IDBOperation> create(IDBTransactionBackend* transaction, size_t indexCount)
    {
        return adoptRef(new SetIndexesReadyOperation(transaction, indexCount));
    }
    virtual void perform(std::function<void()> successCallback) OVERRIDE FINAL;
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
    static PassRefPtr<IDBOperation> create(IDBTransactionBackend* transaction, int64_t databaseId, int64_t objectStoreId, int64_t indexId, PassRefPtr<IDBKeyRange> keyRange, IndexedDB::CursorDirection direction, IndexedDB::CursorType cursorType, IDBDatabaseBackend::TaskType taskType, PassRefPtr<IDBCallbacks> callbacks)
    {
        return adoptRef(new OpenCursorOperation(transaction, databaseId, objectStoreId, indexId, keyRange, direction, cursorType, taskType, callbacks));
    }
    virtual void perform(std::function<void()> successCallback) OVERRIDE FINAL;
private:
    OpenCursorOperation(IDBTransactionBackend* transaction, int64_t databaseId, int64_t objectStoreId, int64_t indexId, PassRefPtr<IDBKeyRange> keyRange, IndexedDB::CursorDirection direction, IndexedDB::CursorType cursorType, IDBDatabaseBackend::TaskType taskType, PassRefPtr<IDBCallbacks> callbacks)
        : m_transaction(transaction)
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

    RefPtr<IDBTransactionBackend> m_transaction;
    const int64_t m_databaseId;
    const int64_t m_objectStoreId;
    const int64_t m_indexId;
    const PassRefPtr<IDBKeyRange> m_keyRange;
    const IndexedDB::CursorDirection m_direction;
    const IndexedDB::CursorType m_cursorType;
    const IDBDatabaseBackend::TaskType m_taskType;
    const RefPtr<IDBCallbacks> m_callbacks;
};

class CountOperation : public IDBOperation {
public:
    static PassRefPtr<IDBOperation> create(IDBTransactionBackend* transaction, int64_t databaseId, int64_t objectStoreId, int64_t indexId, PassRefPtr<IDBKeyRange> keyRange, PassRefPtr<IDBCallbacks> callbacks)
    {
        return adoptRef(new CountOperation(transaction, databaseId, objectStoreId, indexId, keyRange, callbacks));
    }
    virtual void perform(std::function<void()> successCallback) OVERRIDE FINAL;
private:
    CountOperation(IDBTransactionBackend* transaction, int64_t databaseId, int64_t objectStoreId, int64_t indexId, PassRefPtr<IDBKeyRange> keyRange, PassRefPtr<IDBCallbacks> callbacks)
        : m_transaction(transaction)
        , m_databaseId(databaseId)
        , m_objectStoreId(objectStoreId)
        , m_indexId(indexId)
        , m_keyRange(keyRange)
        , m_callbacks(callbacks)
    {
    }

    RefPtr<IDBTransactionBackend> m_transaction;
    const int64_t m_databaseId;
    const int64_t m_objectStoreId;
    const int64_t m_indexId;
    const RefPtr<IDBKeyRange> m_keyRange;
    const RefPtr<IDBCallbacks> m_callbacks;
};

class DeleteRangeOperation : public IDBOperation {
public:
    static PassRefPtr<IDBOperation> create(IDBTransactionBackend* transaction, int64_t databaseId, int64_t objectStoreId, PassRefPtr<IDBKeyRange> keyRange, PassRefPtr<IDBCallbacks> callbacks)
    {
        return adoptRef(new DeleteRangeOperation(transaction, databaseId, objectStoreId, keyRange, callbacks));
    }
    virtual void perform(std::function<void()> successCallback) OVERRIDE FINAL;
private:
    DeleteRangeOperation(IDBTransactionBackend* transaction, int64_t databaseId, int64_t objectStoreId, PassRefPtr<IDBKeyRange> keyRange, PassRefPtr<IDBCallbacks> callbacks)
        : m_transaction(transaction)
        , m_databaseId(databaseId)
        , m_objectStoreId(objectStoreId)
        , m_keyRange(keyRange)
        , m_callbacks(callbacks)
    {
    }

    RefPtr<IDBTransactionBackend> m_transaction;
    const int64_t m_databaseId;
    const int64_t m_objectStoreId;
    const RefPtr<IDBKeyRange> m_keyRange;
    const RefPtr<IDBCallbacks> m_callbacks;
};

class ClearOperation : public IDBOperation {
public:
    static PassRefPtr<IDBOperation> create(IDBTransactionBackend* transaction, int64_t databaseId, int64_t objectStoreId, PassRefPtr<IDBCallbacks> callbacks)
    {
        return adoptRef(new ClearOperation(transaction, databaseId, objectStoreId, callbacks));
    }
    virtual void perform(std::function<void()> successCallback) OVERRIDE FINAL;
private:
    ClearOperation(IDBTransactionBackend* transaction, int64_t databaseId, int64_t objectStoreId, PassRefPtr<IDBCallbacks> callbacks)
        : m_transaction(transaction)
        , m_databaseId(databaseId)
        , m_objectStoreId(objectStoreId)
        , m_callbacks(callbacks)
    {
    }

    RefPtr<IDBTransactionBackend> m_transaction;
    const int64_t m_databaseId;
    const int64_t m_objectStoreId;
    const RefPtr<IDBCallbacks> m_callbacks;
};

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
#endif // IDBTransactionBackendOperations_h

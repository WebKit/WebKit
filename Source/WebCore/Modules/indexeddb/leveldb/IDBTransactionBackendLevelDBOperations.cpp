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

#include "config.h"
#include "IDBTransactionBackendLevelDBOperations.h"

#include "IDBCursorBackendLevelDB.h"
#include "IDBDatabaseCallbacks.h"
#include "IDBIndexWriter.h"
#include "IDBKeyRange.h"
#include "Logging.h"
#include <wtf/text/CString.h>

#if ENABLE(INDEXED_DATABASE) && USE(LEVELDB)

namespace WebCore {

void CreateObjectStoreOperation::perform()
{
    LOG(StorageAPI, "CreateObjectStoreOperation");
    if (!m_backingStore->createObjectStore(m_transaction->backingStoreTransaction(), m_transaction->database()->id(), m_objectStoreMetadata.id, m_objectStoreMetadata.name, m_objectStoreMetadata.keyPath, m_objectStoreMetadata.autoIncrement)) {
        RefPtr<IDBDatabaseError> error = IDBDatabaseError::create(IDBDatabaseException::UnknownError, String::format("Internal error creating object store '%s'.", m_objectStoreMetadata.name.utf8().data()));
        m_transaction->abort(error.release());
        return;
    }
}

void CreateIndexOperation::perform()
{
    LOG(StorageAPI, "CreateIndexOperation");
    if (!m_backingStore->createIndex(m_transaction->backingStoreTransaction(), m_transaction->database()->id(), m_objectStoreId, m_indexMetadata.id, m_indexMetadata.name, m_indexMetadata.keyPath, m_indexMetadata.unique, m_indexMetadata.multiEntry)) {
        m_transaction->abort(IDBDatabaseError::create(IDBDatabaseException::UnknownError, String::format("Internal error when trying to create index '%s'.", m_indexMetadata.name.utf8().data())));
        return;
    }
}

void CreateIndexAbortOperation::perform()
{
    LOG(StorageAPI, "CreateIndexAbortOperation");
    m_transaction->database()->removeIndex(m_objectStoreId, m_indexId);
}

void DeleteIndexOperation::perform()
{
    LOG(StorageAPI, "DeleteIndexOperation");
    bool ok = m_backingStore->deleteIndex(m_transaction->backingStoreTransaction(), m_transaction->database()->id(), m_objectStoreId, m_indexMetadata.id);
    if (!ok) {
        RefPtr<IDBDatabaseError> error = IDBDatabaseError::create(IDBDatabaseException::UnknownError, String::format("Internal error deleting index '%s'.", m_indexMetadata.name.utf8().data()));
        m_transaction->abort(error);
    }
}

void DeleteIndexAbortOperation::perform()
{
    LOG(StorageAPI, "DeleteIndexAbortOperation");
    m_transaction->database()->addIndex(m_objectStoreId, m_indexMetadata, IDBIndexMetadata::InvalidId);
}

void GetOperation::perform()
{
    LOG(StorageAPI, "GetOperation");

    RefPtr<IDBKey> key;

    if (m_keyRange->isOnlyKey())
        key = m_keyRange->lower();
    else {
        RefPtr<IDBBackingStoreInterface::Cursor> backingStoreCursor;
        if (m_indexId == IDBIndexMetadata::InvalidId) {
            ASSERT(m_cursorType != IndexedDB::CursorKeyOnly);
            // ObjectStore Retrieval Operation
            backingStoreCursor = m_backingStore->openObjectStoreCursor(m_transaction->backingStoreTransaction(), m_databaseId, m_objectStoreId, m_keyRange.get(), IndexedDB::CursorNext);
        } else {
            if (m_cursorType == IndexedDB::CursorKeyOnly) {
                // Index Value Retrieval Operation
                backingStoreCursor = m_backingStore->openIndexKeyCursor(m_transaction->backingStoreTransaction(), m_databaseId, m_objectStoreId, m_indexId, m_keyRange.get(), IndexedDB::CursorNext);
            } else {
                // Index Referenced Value Retrieval Operation
                backingStoreCursor = m_backingStore->openIndexCursor(m_transaction->backingStoreTransaction(), m_databaseId, m_objectStoreId, m_indexId, m_keyRange.get(), IndexedDB::CursorNext);
            }
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
        ok = m_backingStore->getRecord(m_transaction->backingStoreTransaction(), m_databaseId, m_objectStoreId, *key, value);
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
    ok = m_backingStore->getPrimaryKeyViaIndex(m_transaction->backingStoreTransaction(), m_databaseId, m_objectStoreId, m_indexId, *key, primaryKey);
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
    ok = m_backingStore->getRecord(m_transaction->backingStoreTransaction(), m_databaseId, m_objectStoreId, *primaryKey, value);
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

void PutOperation::perform()
{
    LOG(StorageAPI, "PutOperation");
    ASSERT(m_transaction->mode() != IndexedDB::TransactionReadOnly);
    ASSERT(m_indexIds.size() == m_indexKeys.size());
    bool keyWasGenerated = false;

    RefPtr<IDBKey> key;
    if (m_putMode != IDBDatabaseBackendInterface::CursorUpdate && m_objectStore.autoIncrement && !m_key) {
        RefPtr<IDBKey> autoIncKey = m_backingStore->generateKey(*m_transaction, m_databaseId, m_objectStore.id);
        keyWasGenerated = true;
        if (!autoIncKey->isValid()) {
            m_callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::ConstraintError, "Maximum key generator value reached."));
            return;
        }
        key = autoIncKey;
    } else
        key = m_key;

    ASSERT(key);
    ASSERT(key->isValid());

    RefPtr<IDBRecordIdentifier> recordIdentifier;
    if (m_putMode == IDBDatabaseBackendInterface::AddOnly) {
        bool ok = m_backingStore->keyExistsInObjectStore(m_transaction->backingStoreTransaction(), m_databaseId, m_objectStore.id, *key, recordIdentifier);
        if (!ok) {
            m_callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::UnknownError, "Internal error checking key existence."));
            return;
        }
        if (recordIdentifier) {
            m_callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::ConstraintError, "Key already exists in the object store."));
            return;
        }
    }

    Vector<RefPtr<IDBIndexWriter>> indexWriters;
    String errorMessage;
    bool obeysConstraints = false;
    bool backingStoreSuccess = m_backingStore->makeIndexWriters(*m_transaction, m_databaseId, m_objectStore, *key, keyWasGenerated, m_indexIds, m_indexKeys, indexWriters, &errorMessage, obeysConstraints);
    if (!backingStoreSuccess) {
        m_callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::UnknownError, "Internal error: backing store error updating index keys."));
        return;
    }
    if (!obeysConstraints) {
        m_callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::ConstraintError, errorMessage));
        return;
    }

    // Before this point, don't do any mutation. After this point, rollback the transaction in case of error.
    backingStoreSuccess = m_backingStore->putRecord(m_transaction->backingStoreTransaction(), m_databaseId, m_objectStore.id, *key, m_value, recordIdentifier.get());
    if (!backingStoreSuccess) {
        m_callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::UnknownError, "Internal error: backing store error performing put/add."));
        return;
    }

    for (size_t i = 0; i < indexWriters.size(); ++i) {
        IDBIndexWriter* indexWriter = indexWriters[i].get();
        indexWriter->writeIndexKeys(recordIdentifier.get(), *m_backingStore, m_transaction->backingStoreTransaction(), m_databaseId, m_objectStore.id);
    }

    if (m_objectStore.autoIncrement && m_putMode != IDBDatabaseBackendInterface::CursorUpdate && key->type() == IDBKey::NumberType) {
        bool ok = m_backingStore->updateKeyGenerator(*m_transaction, m_databaseId, m_objectStore.id, *key, !keyWasGenerated);
        if (!ok) {
            m_callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::UnknownError, "Internal error updating key generator."));
            return;
        }
    }

    m_callbacks->onSuccess(key.release());
}

void SetIndexesReadyOperation::perform()
{
    LOG(StorageAPI, "SetIndexesReadyOperation");
    for (size_t i = 0; i < m_indexCount; ++i)
        m_transaction->didCompletePreemptiveEvent();
}

void OpenCursorOperation::perform()
{
    LOG(StorageAPI, "OpenCursorOperation");

    // The frontend has begun indexing, so this pauses the transaction
    // until the indexing is complete. This can't happen any earlier
    // because we don't want to switch to early mode in case multiple
    // indexes are being created in a row, with put()'s in between.
    if (m_taskType == IDBDatabaseBackendInterface::PreemptiveTask)
        m_transaction->addPreemptiveEvent();

    RefPtr<IDBBackingStoreInterface::Cursor> backingStoreCursor;
    if (m_indexId == IDBIndexMetadata::InvalidId) {
        ASSERT(m_cursorType != IndexedDB::CursorKeyOnly);
        backingStoreCursor = m_backingStore->openObjectStoreCursor(m_transaction->backingStoreTransaction(), m_databaseId, m_objectStoreId, m_keyRange.get(), m_direction);
    } else {
        ASSERT(m_taskType == IDBDatabaseBackendInterface::NormalTask);
        if (m_cursorType == IndexedDB::CursorKeyOnly)
            backingStoreCursor = m_backingStore->openIndexKeyCursor(m_transaction->backingStoreTransaction(), m_databaseId, m_objectStoreId, m_indexId, m_keyRange.get(), m_direction);
        else
            backingStoreCursor = m_backingStore->openIndexCursor(m_transaction->backingStoreTransaction(), m_databaseId, m_objectStoreId, m_indexId, m_keyRange.get(), m_direction);
    }

    if (!backingStoreCursor) {
        m_callbacks->onSuccess(static_cast<SharedBuffer*>(0));
        return;
    }

    IDBDatabaseBackendInterface::TaskType taskType(static_cast<IDBDatabaseBackendInterface::TaskType>(m_taskType));
    RefPtr<IDBCursorBackendLevelDB> cursor = IDBCursorBackendLevelDB::create(backingStoreCursor.get(), m_cursorType, taskType, m_transaction.get(), m_objectStoreId);
    m_callbacks->onSuccess(cursor, cursor->key(), cursor->primaryKey(), cursor->value());
}

void CountOperation::perform()
{
    LOG(StorageAPI, "CountOperation");
    uint32_t count = 0;
    RefPtr<IDBBackingStoreInterface::Cursor> backingStoreCursor;

    if (m_indexId == IDBIndexMetadata::InvalidId)
        backingStoreCursor = m_backingStore->openObjectStoreKeyCursor(m_transaction->backingStoreTransaction(), m_databaseId, m_objectStoreId, m_keyRange.get(), IndexedDB::CursorNext);
    else
        backingStoreCursor = m_backingStore->openIndexKeyCursor(m_transaction->backingStoreTransaction(), m_databaseId, m_objectStoreId, m_indexId, m_keyRange.get(), IndexedDB::CursorNext);
    if (!backingStoreCursor) {
        m_callbacks->onSuccess(count);
        return;
    }

    do {
        ++count;
    } while (backingStoreCursor->continueFunction(0));

    m_callbacks->onSuccess(count);
}

void DeleteRangeOperation::perform()
{
    LOG(StorageAPI, "DeleteRangeOperation");
    RefPtr<IDBBackingStoreInterface::Cursor> backingStoreCursor = m_backingStore->openObjectStoreCursor(m_transaction->backingStoreTransaction(), m_databaseId, m_objectStoreId, m_keyRange.get(), IndexedDB::CursorNext);
    if (backingStoreCursor) {
        do {
            if (!m_backingStore->deleteRecord(m_transaction->backingStoreTransaction(), m_databaseId, m_objectStoreId, backingStoreCursor->recordIdentifier())) {
                m_callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::UnknownError, "Error deleting data in range"));
                return;
            }
        } while (backingStoreCursor->continueFunction(0));
    }

    m_callbacks->onSuccess();
}

void ClearOperation::perform()
{
    LOG(StorageAPI, "ObjectStoreClearOperation");
    if (!m_backingStore->clearObjectStore(m_transaction->backingStoreTransaction(), m_databaseId, m_objectStoreId)) {
        m_callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::UnknownError, "Error clearing object store"));
        return;
    }
    m_callbacks->onSuccess();
}

void DeleteObjectStoreOperation::perform()
{
    LOG(StorageAPI, "DeleteObjectStoreOperation");
    bool ok = m_backingStore->deleteObjectStore(m_transaction->backingStoreTransaction(), m_transaction->database()->id(), m_objectStoreMetadata.id);
    if (!ok) {
        RefPtr<IDBDatabaseError> error = IDBDatabaseError::create(IDBDatabaseException::UnknownError, String::format("Internal error deleting object store '%s'.", m_objectStoreMetadata.name.utf8().data()));
        m_transaction->abort(error);
    }
}

void IDBDatabaseBackendImpl::VersionChangeOperation::perform()
{
    LOG(StorageAPI, "VersionChangeOperation");
    IDBDatabaseBackendImpl* database = m_transaction->database();
    int64_t databaseId = database->id();
    uint64_t oldVersion = database->m_metadata.version;

    // FIXME: Database versions are now of type uint64_t, but this code expected int64_t.
    ASSERT(m_version > (int64_t)oldVersion);
    database->m_metadata.version = m_version;
    if (!database->m_backingStore->updateIDBDatabaseIntVersion(m_transaction->backingStoreTransaction(), databaseId, database->m_metadata.version)) {
        RefPtr<IDBDatabaseError> error = IDBDatabaseError::create(IDBDatabaseException::UnknownError, "Error writing data to stable storage when updating version.");
        m_callbacks->onError(error);
        m_transaction->abort(error);
        return;
    }
    ASSERT(!database->m_pendingSecondHalfOpen);
    database->m_pendingSecondHalfOpen = PendingOpenCall::create(m_callbacks, m_databaseCallbacks, m_transactionId, m_version);
    m_callbacks->onUpgradeNeeded(oldVersion, database, database->metadata());
}

void CreateObjectStoreAbortOperation::perform()
{
    LOG(StorageAPI, "CreateObjectStoreAbortOperation");
    m_transaction->database()->removeObjectStore(m_objectStoreId);
}

void DeleteObjectStoreAbortOperation::perform()
{
    LOG(StorageAPI, "DeleteObjectStoreAbortOperation");
    m_transaction->database()->addObjectStore(m_objectStoreMetadata, IDBObjectStoreMetadata::InvalidId);
}

void IDBDatabaseBackendImpl::VersionChangeAbortOperation::perform()
{
    LOG(StorageAPI, "VersionChangeAbortOperation");
    m_transaction->database()->m_metadata.version = m_previousIntVersion;
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE) && USE(LEVELDB)

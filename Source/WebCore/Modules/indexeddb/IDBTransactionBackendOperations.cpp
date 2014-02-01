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
#include "IDBTransactionBackendOperations.h"

#include "IDBCursorBackend.h"
#include "IDBDatabaseCallbacks.h"
#include "IDBKeyRange.h"
#include "IDBRecordIdentifier.h"
#include "IDBServerConnection.h"
#include "Logging.h"
#include <wtf/text/CString.h>

#if ENABLE(INDEXED_DATABASE)

#define STANDARD_DATABASE_ERROR_CALLBACK std::function<void(PassRefPtr<IDBDatabaseError>)> operationCallback = \
    [operation, completionCallback](PassRefPtr<IDBDatabaseError> error) { \
        if (error) \
            operation->m_transaction->abort(error); \
        completionCallback(); \
    };

namespace WebCore {

void CreateObjectStoreOperation::perform(std::function<void()> completionCallback)
{
    LOG(StorageAPI, "CreateObjectStoreOperation");

    RefPtr<CreateObjectStoreOperation> operation(this);
    STANDARD_DATABASE_ERROR_CALLBACK;

    m_transaction->database().serverConnection().createObjectStore(*m_transaction, *this, operationCallback);
}

void CreateIndexOperation::perform(std::function<void()> completionCallback)
{
    LOG(StorageAPI, "CreateIndexOperation");

    RefPtr<CreateIndexOperation> operation(this);
    STANDARD_DATABASE_ERROR_CALLBACK;

    m_transaction->database().serverConnection().createIndex(*m_transaction, *this, operationCallback);
}

void CreateIndexAbortOperation::perform()
{
    LOG(StorageAPI, "CreateIndexAbortOperation");
    m_transaction->database().removeIndex(m_objectStoreID, m_indexID);
}

void DeleteIndexOperation::perform(std::function<void()> completionCallback)
{
    LOG(StorageAPI, "DeleteIndexOperation");

    RefPtr<DeleteIndexOperation> operation(this);
    STANDARD_DATABASE_ERROR_CALLBACK;

    m_transaction->database().serverConnection().deleteIndex(*m_transaction, *this, operationCallback);
}

void DeleteIndexAbortOperation::perform()
{
    LOG(StorageAPI, "DeleteIndexAbortOperation");
    m_transaction->database().addIndex(m_objectStoreID, m_indexMetadata, IDBIndexMetadata::InvalidId);
}

void GetOperation::perform(std::function<void()> completionCallback)
{
    LOG(StorageAPI, "GetOperation");

    RefPtr<GetOperation> operation(this);
    STANDARD_DATABASE_ERROR_CALLBACK;

    m_transaction->database().serverConnection().get(*m_transaction, *this, [this, operation, operationCallback](const IDBGetResult& result, PassRefPtr<IDBDatabaseError> prpError) {
        RefPtr<IDBDatabaseError> error = prpError;

        if (error)
            m_callbacks->onError(error);
        else {
            if (!result.valueBuffer) {
                if (result.keyData.isNull)
                    m_callbacks->onSuccess();
                else
                    m_callbacks->onSuccess(result.keyData.maybeCreateIDBKey());
            } else {
                if (!result.keyData.isNull)
                    m_callbacks->onSuccess(result.valueBuffer, result.keyData.maybeCreateIDBKey(), result.keyPath);
                else
                    m_callbacks->onSuccess(result.valueBuffer.get());
            }
        }

        operationCallback(error.release());
    });
}

void PutOperation::perform(std::function<void()> completionCallback)
{
    LOG(StorageAPI, "PutOperation");
    ASSERT(m_transaction->mode() != IndexedDB::TransactionMode::ReadOnly);
    ASSERT(m_indexIDs.size() == m_indexKeys.size());

    RefPtr<PutOperation> operation(this);
    STANDARD_DATABASE_ERROR_CALLBACK;

    m_transaction->database().serverConnection().put(*m_transaction, *this, [this, operation, operationCallback](PassRefPtr<IDBKey> key, PassRefPtr<IDBDatabaseError> prpError) {
        RefPtr<IDBDatabaseError> error = prpError;
        if (key) {
            ASSERT(!error);
            m_callbacks->onSuccess(key);
        } else {
            ASSERT(error);
            m_callbacks->onError(error);
        }
        operationCallback(error.release());
    });
}

void SetIndexesReadyOperation::perform(std::function<void()> completionCallback)
{
    LOG(StorageAPI, "SetIndexesReadyOperation");

    for (size_t i = 0; i < m_indexCount; ++i)
        m_transaction->didCompletePreemptiveEvent();

    callOnMainThread(completionCallback);
}

void OpenCursorOperation::perform(std::function<void()> completionCallback)
{
    LOG(StorageAPI, "OpenCursorOperation");

    RefPtr<OpenCursorOperation> operation(this);
    auto callback = [this, operation, completionCallback](int64_t cursorID, PassRefPtr<IDBKey> key, PassRefPtr<IDBKey> primaryKey, PassRefPtr<SharedBuffer> valueBuffer, PassRefPtr<IDBKey> valueKey, PassRefPtr<IDBDatabaseError>) {
        // FIXME: When the LevelDB port fails to open a backing store cursor it calls onSuccess(nullptr);
        // This seems nonsensical and might have to change soon, breaking them.
        if (!cursorID)
            m_callbacks->onSuccess(static_cast<SharedBuffer*>(0));
        else {
            RefPtr<IDBCursorBackend> cursor = IDBCursorBackend::create(cursorID, m_cursorType, m_taskType, *m_transaction, m_objectStoreID);
            if (key || primaryKey || valueBuffer || valueKey)
                cursor->updateCursorData(key.get(), primaryKey.get(), valueBuffer.get(), valueKey.get());

            m_callbacks->onSuccess(cursor.release());
        }

        completionCallback();
    };

    m_transaction->database().serverConnection().openCursor(*m_transaction, *this, callback);
}

void CountOperation::perform(std::function<void()> completionCallback)
{
    LOG(StorageAPI, "CountOperation");

    RefPtr<CountOperation> operation(this);
    auto callback = [this, operation, completionCallback](int64_t count, PassRefPtr<IDBDatabaseError>) {
        // FIXME: The LevelDB port never had an error condition for the count operation.
        // We probably need to support an error for the count operation, breaking the LevelDB port.
        m_callbacks->onSuccess(count);

        completionCallback();
    };

    m_transaction->database().serverConnection().count(*m_transaction, *this, callback);
}

void DeleteRangeOperation::perform(std::function<void()> completionCallback)
{
    LOG(StorageAPI, "DeleteRangeOperation");

    RefPtr<DeleteRangeOperation> operation(this);
    auto callback = [this, operation, completionCallback](PassRefPtr<IDBDatabaseError> error) {
        if (error)
            m_callbacks->onError(error);
        else
            m_callbacks->onSuccess();

        completionCallback();
    };

    m_transaction->database().serverConnection().deleteRange(*m_transaction, *this, callback);
}

void ClearObjectStoreOperation::perform(std::function<void()> completionCallback)
{
    LOG(StorageAPI, "ClearObjectStoreOperation");

    RefPtr<ClearObjectStoreOperation> operation(this);

    auto clearCallback = [this, operation, completionCallback](PassRefPtr<IDBDatabaseError> prpError) {
        RefPtr<IDBDatabaseError> error = prpError;

        if (error) {
            m_callbacks->onError(error);
            m_transaction->abort(error.release());
        } else
            m_callbacks->onSuccess();

        completionCallback();
    };

    m_transaction->database().serverConnection().clearObjectStore(*m_transaction, *this, clearCallback);
}

void DeleteObjectStoreOperation::perform(std::function<void()> completionCallback)
{
    LOG(StorageAPI, "DeleteObjectStoreOperation");

    RefPtr<DeleteObjectStoreOperation> operation(this);
    STANDARD_DATABASE_ERROR_CALLBACK;

    m_transaction->database().serverConnection().deleteObjectStore(*m_transaction, *this, operationCallback);
}

void IDBDatabaseBackend::VersionChangeOperation::perform(std::function<void()> completionCallback)
{
    LOG(StorageAPI, "VersionChangeOperation");

    uint64_t oldVersion = m_transaction->database().metadata().version;
    RefPtr<IDBDatabaseBackend::VersionChangeOperation> operation(this);
    ASSERT(static_cast<uint64_t>(m_version) > oldVersion);

    std::function<void(PassRefPtr<IDBDatabaseError>)> operationCallback = [oldVersion, operation, this, completionCallback](PassRefPtr<IDBDatabaseError> prpError) {
        RefPtr<IDBDatabaseError> error = prpError;
        if (error) {
            m_callbacks->onError(error);
            m_transaction->abort(error);
        } else {
            ASSERT(!m_transaction->database().hasPendingSecondHalfOpen());
            m_transaction->database().setCurrentVersion(m_version);
            m_transaction->database().setPendingSecondHalfOpen(IDBPendingOpenCall::create(*m_callbacks, *m_databaseCallbacks, m_transaction->id(), m_version));
            m_callbacks->onUpgradeNeeded(oldVersion, &m_transaction->database(), m_transaction->database().metadata());
        }
        completionCallback();
    };

    m_transaction->database().serverConnection().changeDatabaseVersion(*m_transaction, *this, operationCallback);
}

void CreateObjectStoreAbortOperation::perform()
{
    LOG(StorageAPI, "CreateObjectStoreAbortOperation");
    m_transaction->database().removeObjectStore(m_objectStoreID);
}

void DeleteObjectStoreAbortOperation::perform()
{
    LOG(StorageAPI, "DeleteObjectStoreAbortOperation");
    m_transaction->database().addObjectStore(m_objectStoreMetadata, IDBObjectStoreMetadata::InvalidId);
}

void IDBDatabaseBackend::VersionChangeAbortOperation::perform()
{
    LOG(StorageAPI, "VersionChangeAbortOperation");
    m_transaction->database().setCurrentVersion(m_previousIntVersion);
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)

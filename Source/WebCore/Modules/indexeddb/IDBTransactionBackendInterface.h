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

#ifndef IDBTransactionBackendInterface_h
#define IDBTransactionBackendInterface_h

#include "IDBBackingStoreInterface.h"
#include "IDBDatabaseBackendInterface.h"
#include "IndexedDB.h"
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>

#if ENABLE(INDEXED_DATABASE)

namespace WebCore {

class IDBCallbacks;
class IDBDatabaseCallbacks;
class IDBDatabaseError;
class IDBKey;
class IDBKeyRange;

struct IDBDatabaseMetadata;
struct IDBIndexMetadata;
struct IDBObjectStoreMetadata;

class IDBTransactionBackendInterface : public RefCounted<IDBTransactionBackendInterface> {
public:
    virtual ~IDBTransactionBackendInterface() { }

    virtual IndexedDB::TransactionMode mode() const = 0;

    virtual void run() = 0;
    virtual void commit() = 0;
    virtual void abort() = 0;
    virtual void abort(PassRefPtr<IDBDatabaseError>) = 0;
    virtual const HashSet<int64_t>& scope() const = 0;

    virtual void scheduleCreateObjectStoreOperation(const IDBObjectStoreMetadata&) = 0;
    virtual void scheduleDeleteObjectStoreOperation(const IDBObjectStoreMetadata&) = 0;
    virtual void scheduleVersionChangeOperation(int64_t transactionId, int64_t requestedVersion, PassRefPtr<IDBCallbacks>, PassRefPtr<IDBDatabaseCallbacks>, const IDBDatabaseMetadata&) = 0;
    virtual void scheduleCreateIndexOperation(int64_t objectStoreId, const IDBIndexMetadata&) = 0;
    virtual void scheduleDeleteIndexOperation(int64_t objectStoreId, const IDBIndexMetadata&) = 0;
    virtual void scheduleGetOperation(const IDBDatabaseMetadata&, int64_t objectStoreId, int64_t indexId, PassRefPtr<IDBKeyRange>, IndexedDB::CursorType, PassRefPtr<IDBCallbacks>) = 0;
    virtual void schedulePutOperation(const IDBObjectStoreMetadata&, PassRefPtr<SharedBuffer> value, PassRefPtr<IDBKey>, IDBDatabaseBackendInterface::PutMode, PassRefPtr<IDBCallbacks>, const Vector<int64_t>& indexIds, const Vector<IndexKeys>&) = 0;
    virtual void scheduleSetIndexesReadyOperation(size_t indexCount) = 0;
    virtual void scheduleOpenCursorOperation(int64_t objectStoreId, int64_t indexId, PassRefPtr<IDBKeyRange>, IndexedDB::CursorDirection, IndexedDB::CursorType, IDBDatabaseBackendInterface::TaskType, PassRefPtr<IDBCallbacks>) = 0;
    virtual void scheduleCountOperation(int64_t objectStoreId, int64_t indexId, PassRefPtr<IDBKeyRange>, PassRefPtr<IDBCallbacks>) = 0;
    virtual void scheduleDeleteRangeOperation(int64_t objectStoreId, PassRefPtr<IDBKeyRange>, PassRefPtr<IDBCallbacks>) = 0;
    virtual void scheduleClearOperation(int64_t objectStoreId, PassRefPtr<IDBCallbacks>) = 0;

    virtual IDBBackingStoreInterface::Transaction* backingStoreTransaction() = 0;

    int64_t id() const { return m_id; }

protected:
    IDBTransactionBackendInterface(int64_t id)
        : m_id(id)
    {
    }

private:
    int64_t m_id;
};

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
#endif // IDBTransactionBackendInterface_h

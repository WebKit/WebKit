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

#ifndef IDBIndexBackendImpl_h
#define IDBIndexBackendImpl_h

#if ENABLE(INDEXED_DATABASE)

#include "IDBCursorBackendInterface.h"
#include "IDBDatabaseBackendImpl.h"
#include "IDBIndexBackendInterface.h"
#include "IDBKeyPath.h"
#include "IDBMetadata.h"

namespace WebCore {

class IDBBackingStore;
class IDBKey;
class IDBObjectStoreBackendImpl;

class IDBIndexBackendImpl : public IDBIndexBackendInterface {
public:
    static PassRefPtr<IDBIndexBackendImpl> create(const IDBDatabaseBackendImpl* database, IDBObjectStoreBackendImpl* objectStoreBackend, const IDBIndexMetadata& metadata)
    {
        return adoptRef(new IDBIndexBackendImpl(database, objectStoreBackend, metadata));
    }
    virtual ~IDBIndexBackendImpl();

    int64_t id() const
    {
        ASSERT(m_metadata.id != InvalidId);
        return m_metadata.id;
    }

    // IDBIndexBackendInterface
    virtual void openCursor(PassRefPtr<IDBKeyRange>, unsigned short direction, PassRefPtr<IDBCallbacks>, IDBTransactionBackendInterface*, ExceptionCode&);
    virtual void count(PassRefPtr<IDBKeyRange>, PassRefPtr<IDBCallbacks>, IDBTransactionBackendInterface*, ExceptionCode&);
    virtual void openKeyCursor(PassRefPtr<IDBKeyRange>, unsigned short direction, PassRefPtr<IDBCallbacks>, IDBTransactionBackendInterface*, ExceptionCode&);
    virtual void get(PassRefPtr<IDBKeyRange>, PassRefPtr<IDBCallbacks>, IDBTransactionBackendInterface*, ExceptionCode&);
    virtual void getKey(PassRefPtr<IDBKeyRange>, PassRefPtr<IDBCallbacks>, IDBTransactionBackendInterface*, ExceptionCode&);

    IDBIndexMetadata metadata() const { return m_metadata; }
    const String& name() { return m_metadata.name; }
    const IDBKeyPath& keyPath() { return m_metadata.keyPath; }
    const bool& unique() { return m_metadata.unique; }
    const bool& multiEntry() { return m_metadata.multiEntry; }

private:
    IDBIndexBackendImpl(const IDBDatabaseBackendImpl*, IDBObjectStoreBackendImpl*, const IDBIndexMetadata&);

    class OpenIndexCursorOperation;
    class IndexCountOperation;
    class IndexReferencedValueRetrievalOperation;
    class IndexValueRetrievalOperation;

    static const int64_t InvalidId = 0;

    PassRefPtr<IDBBackingStore> backingStore() const { return m_database->backingStore(); }
    int64_t databaseId() const { return m_database->id(); }

    const IDBDatabaseBackendImpl* m_database;

    IDBObjectStoreBackendImpl* m_objectStoreBackend;

    IDBIndexMetadata m_metadata;
};

} // namespace WebCore

#endif

#endif // IDBIndexBackendImpl_h

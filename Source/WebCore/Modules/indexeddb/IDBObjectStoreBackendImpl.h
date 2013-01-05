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

#ifndef IDBObjectStoreBackendImpl_h
#define IDBObjectStoreBackendImpl_h

#include "IDBBackingStore.h"
#include "IDBDatabaseBackendImpl.h"
#include "IDBKeyPath.h"
#include "IDBMetadata.h"
#include "IDBObjectStoreBackendInterface.h"
#include <wtf/HashMap.h>
#include <wtf/text/StringHash.h>

#if ENABLE(INDEXED_DATABASE)

namespace WebCore {

class IDBDatabaseBackendImpl;
class IDBIndexBackendImpl;
class IDBTransactionBackendImpl;
class IDBTransactionBackendInterface;
struct IDBObjectStoreMetadata;

class IDBObjectStoreBackendImpl : public IDBObjectStoreBackendInterface {
public:
    static PassRefPtr<IDBObjectStoreBackendImpl> create(const IDBDatabaseBackendImpl* database, const IDBObjectStoreMetadata& metadata)
    {
        return adoptRef(new IDBObjectStoreBackendImpl(database, metadata));
    }
    virtual ~IDBObjectStoreBackendImpl();

    typedef HashMap<int64_t, RefPtr<IDBIndexBackendImpl> > IndexMap;

    static const int64_t InvalidId = 0;
    int64_t id() const
    {
        ASSERT(m_metadata.id != InvalidId);
        return m_metadata.id;
    }

    // IDBObjectStoreBackendInterface
    virtual void get(PassRefPtr<IDBKeyRange>, PassRefPtr<IDBCallbacks>, IDBTransactionBackendInterface*, ExceptionCode&) { ASSERT_NOT_REACHED(); }
    virtual void put(PassRefPtr<SerializedScriptValue>, PassRefPtr<IDBKey>, PutMode, PassRefPtr<IDBCallbacks>, IDBTransactionBackendInterface*, const Vector<int64_t>&, const Vector<IndexKeys>&) { ASSERT_NOT_REACHED(); }

    virtual void deleteFunction(PassRefPtr<IDBKeyRange>, PassRefPtr<IDBCallbacks>, IDBTransactionBackendInterface*, ExceptionCode&) { ASSERT_NOT_REACHED(); }
    virtual void clear(PassRefPtr<IDBCallbacks>, IDBTransactionBackendInterface*, ExceptionCode&) { ASSERT_NOT_REACHED(); }

    virtual PassRefPtr<IDBIndexBackendInterface> createIndex(int64_t, const String& name, const IDBKeyPath&, bool unique, bool multiEntry, IDBTransactionBackendInterface*, ExceptionCode&);
    virtual void setIndexKeys(PassRefPtr<IDBKey> prpPrimaryKey, const Vector<int64_t>&, const Vector<IndexKeys>&, IDBTransactionBackendInterface*) { ASSERT_NOT_REACHED(); }
    virtual void setIndexesReady(const Vector<int64_t>&, IDBTransactionBackendInterface*) { ASSERT_NOT_REACHED(); }
    virtual PassRefPtr<IDBIndexBackendInterface> index(int64_t);
    virtual void deleteIndex(int64_t, IDBTransactionBackendInterface*, ExceptionCode&);

    virtual void openCursor(PassRefPtr<IDBKeyRange>, IDBCursor::Direction, PassRefPtr<IDBCallbacks>, IDBTransactionBackendInterface::TaskType, IDBTransactionBackendInterface*, ExceptionCode&) { ASSERT_NOT_REACHED(); }
    virtual void count(PassRefPtr<IDBKeyRange>, PassRefPtr<IDBCallbacks>, IDBTransactionBackendInterface*, ExceptionCode&) { ASSERT_NOT_REACHED(); }

    static bool populateIndex(IDBBackingStore&, int64_t databaseId, int64_t objectStoreId, PassRefPtr<IDBIndexBackendImpl>);

    IDBObjectStoreMetadata metadata() const;
    const String& name() { return m_metadata.name; }
    const IDBKeyPath& keyPath() const { return m_metadata.keyPath; }
    const bool& autoIncrement() const { return m_metadata.autoIncrement; }

    PassRefPtr<IDBBackingStore> backingStore() const { return m_database->backingStore(); }
    int64_t databaseId() const { return m_database->id(); }

    class IndexWriter {
    public:
        explicit IndexWriter(const IDBIndexMetadata& indexMetadata)
            : m_indexMetadata(indexMetadata)
        { }

        IndexWriter(const IDBIndexMetadata& indexMetadata, const IDBObjectStoreBackendInterface::IndexKeys& indexKeys)
            : m_indexMetadata(indexMetadata)
            , m_indexKeys(indexKeys)
        { }

        bool verifyIndexKeys(IDBBackingStore&, IDBBackingStore::Transaction*, int64_t databaseId, int64_t objectStoreId, int64_t indexId, bool& canAddKeys, const IDBKey* primaryKey = 0, String* errorMessage = 0) const WARN_UNUSED_RETURN;

        void writeIndexKeys(const IDBBackingStore::RecordIdentifier&, IDBBackingStore&, IDBBackingStore::Transaction*, int64_t databaseId, int64_t objectStoreId) const;

    private:
        bool addingKeyAllowed(IDBBackingStore&, IDBBackingStore::Transaction*, int64_t databaseId, int64_t objectStoreId, int64_t indexId, const IDBKey* indexKey, const IDBKey* primaryKey, bool& allowed) const WARN_UNUSED_RETURN;

        const IDBIndexMetadata m_indexMetadata;
        IDBObjectStoreBackendInterface::IndexKeys m_indexKeys;
    };

    static bool makeIndexWriters(PassRefPtr<IDBTransactionBackendImpl>, IDBBackingStore*, int64_t databaseId, const IDBObjectStoreMetadata&, PassRefPtr<IDBKey> primaryKey, bool keyWasGenerated, const Vector<int64_t>& indexIds, const Vector<IDBObjectStoreBackendInterface::IndexKeys>&, Vector<OwnPtr<IndexWriter> >* indexWriters, String* errorMessage, bool& completed) WARN_UNUSED_RETURN;

    static PassRefPtr<IDBKey> generateKey(PassRefPtr<IDBBackingStore>, PassRefPtr<IDBTransactionBackendImpl>, int64_t databaseId, int64_t objectStoreId);
    static bool updateKeyGenerator(PassRefPtr<IDBBackingStore>, PassRefPtr<IDBTransactionBackendImpl>, int64_t databaseId, int64_t objectStoreId, const IDBKey*, bool checkCurrent);

private:
    IDBObjectStoreBackendImpl(const IDBDatabaseBackendImpl*, const IDBObjectStoreMetadata&);

    void loadIndexes();

    class CreateIndexOperation;
    class DeleteIndexOperation;

    // When a "versionchange" transaction aborts, these restore the back-end object hierarchy.
    class CreateIndexAbortOperation;
    class DeleteIndexAbortOperation;

    const IDBDatabaseBackendImpl* m_database;

    IDBObjectStoreMetadata m_metadata;
    IndexMap m_indexes;
};

} // namespace WebCore

#endif

#endif // IDBObjectStoreBackendImpl_h

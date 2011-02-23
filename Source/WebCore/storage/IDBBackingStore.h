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

#ifndef IDBBackingStore_h
#define IDBBackingStore_h

#if ENABLE(INDEXED_DATABASE)

#include "SQLiteDatabase.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

class IDBFactoryBackendImpl;
class IDBKey;
class SecurityOrigin;

class IDBBackingStore : public RefCounted<IDBBackingStore> {
public:
    static PassRefPtr<IDBBackingStore> open(SecurityOrigin*, const String& pathBase, int64_t maximumSize, const String& fileIdentifier, IDBFactoryBackendImpl*);
    ~IDBBackingStore();

    bool extractIDBDatabaseMetaData(const String& name, String& foundVersion, int64_t& foundId);
    bool setIDBDatabaseMetaData(const String& name, const String& version, int64_t& rowId, bool invalidRowId);

    void getObjectStores(int64_t databaseId, Vector<int64_t>& foundIds, Vector<String>& foundNames, Vector<String>& foundKeyPaths, Vector<bool>& foundAutoIncrementFlags);
    bool createObjectStore(const String& name, const String& keyPath, bool autoIncrement, int64_t databaseId, int64_t& assignedObjectStoreId);
    void deleteObjectStore(int64_t objectStoreId);
    String getObjectStoreRecord(int64_t objectStoreId, const IDBKey&);
    bool putObjectStoreRecord(int64_t objectStoreId, const IDBKey&, const String& value, int64_t& rowId, bool invalidRowId);
    void clearObjectStore(int64_t objectStoreId);
    double nextAutoIncrementNumber(int64_t objectStoreId);

    class ObjectStoreRecordCallback {
    public:
         virtual bool callback(int64_t objectStoreDataId, const String& value) = 0;
         virtual ~ObjectStoreRecordCallback() {};
    };
    bool forEachObjectStoreRecord(int64_t objectStoreId, ObjectStoreRecordCallback&);

    void getIndexes(int64_t objectStoreId, Vector<int64_t>& foundIds, Vector<String>& foundNames, Vector<String>& foundKeyPaths, Vector<bool>& foundUniqueFlags);
    bool createIndex(int64_t objectStoreId, const String& name, const String& keyPath, bool isUnique, int64_t& indexId);
    void deleteIndex(int64_t indexId);
    bool putIndexDataForRecord(int64_t indexId, const IDBKey&, int64_t objectStoreDataId);
    bool deleteIndexDataForRecord(int64_t objectStoreDataId);

    SQLiteDatabase& db() { return m_db; }

private:
    IDBBackingStore(String identifier, IDBFactoryBackendImpl* factory);

    SQLiteDatabase m_db;
    String m_identifier;
    RefPtr<IDBFactoryBackendImpl> m_factory;
};

} // namespace WebCore

#endif

#endif // IDBBackingStore_h

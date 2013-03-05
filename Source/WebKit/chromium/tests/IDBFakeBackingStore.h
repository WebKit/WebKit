/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#ifndef IDBFakeBackingStore_h
#define IDBFakeBackingStore_h

#include "IDBBackingStore.h"

namespace WebCore {

class IDBFakeBackingStore : public IDBBackingStore {
public:
    virtual Vector<String> getDatabaseNames() OVERRIDE { return Vector<String>(); }
    virtual bool getIDBDatabaseMetaData(const String& name, IDBDatabaseMetadata*, bool& found) OVERRIDE { return true; }
    virtual bool createIDBDatabaseMetaData(const String& name, const String& version, int64_t intVersion, int64_t& rowId) OVERRIDE { return true; }
    virtual bool updateIDBDatabaseMetaData(Transaction*, int64_t rowId, const String& version) OVERRIDE { return false; }
    virtual bool updateIDBDatabaseIntVersion(Transaction*, int64_t rowId, int64_t version) OVERRIDE { return false; }
    virtual bool deleteDatabase(const String& name) OVERRIDE { return false; }

    virtual bool createObjectStore(Transaction*, int64_t databaseId, int64_t objectStoreId, const String& name, const IDBKeyPath&, bool autoIncrement) OVERRIDE { return false; };

    virtual void clearObjectStore(Transaction*, int64_t databaseId, int64_t objectStoreId) OVERRIDE { }
    virtual void deleteRecord(Transaction*, int64_t databaseId, int64_t objectStoreId, const RecordIdentifier&) OVERRIDE { }
    virtual bool getKeyGeneratorCurrentNumber(Transaction*, int64_t databaseId, int64_t objectStoreId, int64_t& currentNumber) OVERRIDE { return true; }
    virtual bool maybeUpdateKeyGeneratorCurrentNumber(Transaction*, int64_t databaseId, int64_t objectStoreId, int64_t newNumber, bool checkCurrent) OVERRIDE { return true; }
    virtual bool keyExistsInObjectStore(Transaction*, int64_t databaseId, int64_t objectStoreId, const IDBKey&, RecordIdentifier* foundRecordIdentifier, bool& found) OVERRIDE { return true; }

    virtual bool createIndex(Transaction*, int64_t databaseId, int64_t objectStoreId, int64_t indexId, const String& name, const IDBKeyPath&, bool isUnique, bool isMultiEntry) OVERRIDE { return false; };
    virtual void deleteIndex(Transaction*, int64_t databaseId, int64_t objectStoreId, int64_t indexId) OVERRIDE { }
    virtual void putIndexDataForRecord(Transaction*, int64_t databaseId, int64_t objectStoreId, int64_t indexId, const IDBKey&, const RecordIdentifier&) OVERRIDE { }

    virtual PassRefPtr<Cursor> openObjectStoreKeyCursor(Transaction*, int64_t databaseId, int64_t objectStoreId, const IDBKeyRange*, IDBCursor::Direction) OVERRIDE { return PassRefPtr<Cursor>(); }
    virtual PassRefPtr<Cursor> openObjectStoreCursor(Transaction*, int64_t databaseId, int64_t objectStoreId, const IDBKeyRange*, IDBCursor::Direction) OVERRIDE { return PassRefPtr<Cursor>(); }
    virtual PassRefPtr<Cursor> openIndexKeyCursor(Transaction*, int64_t databaseId, int64_t objectStoreId, int64_t indexId, const IDBKeyRange*, IDBCursor::Direction) OVERRIDE { return PassRefPtr<Cursor>(); }
    virtual PassRefPtr<Cursor> openIndexCursor(Transaction*, int64_t databaseId, int64_t objectStoreId, int64_t indexId, const IDBKeyRange*, IDBCursor::Direction) OVERRIDE { return PassRefPtr<Cursor>(); }
};

} // namespace WebCore

#endif // IDBFakeBackingStore_h

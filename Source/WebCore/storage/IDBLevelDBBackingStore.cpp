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
#include "IDBLevelDBBackingStore.h"

#if ENABLE(INDEXED_DATABASE)
#if ENABLE(LEVELDB)

#include "IDBFactoryBackendImpl.h"
#include <leveldb/db.h>

namespace WebCore {

IDBLevelDBBackingStore::IDBLevelDBBackingStore(String identifier, IDBFactoryBackendImpl* factory, leveldb::DB* db)
    : m_identifier(identifier)
    , m_factory(factory)
    , m_db(db)
{
    m_factory->addIDBBackingStore(identifier, this);
}

IDBLevelDBBackingStore::~IDBLevelDBBackingStore()
{
    m_factory->removeIDBBackingStore(m_identifier);
}

PassRefPtr<IDBBackingStore> IDBLevelDBBackingStore::open(SecurityOrigin*, const String&, int64_t, const String&, IDBFactoryBackendImpl*)
{
    ASSERT_NOT_REACHED(); // FIXME: Implement.
    return adoptRef(new IDBLevelDBBackingStore("", 0, 0));
}

bool IDBLevelDBBackingStore::extractIDBDatabaseMetaData(const String&, String&, int64_t&)
{
    ASSERT_NOT_REACHED(); // FIXME: Implement.
    return false;
}


bool IDBLevelDBBackingStore::setIDBDatabaseMetaData(const String&, const String&, int64_t&, bool)
{
    ASSERT_NOT_REACHED(); // FIXME: Implement.
    return false;
}

void IDBLevelDBBackingStore::getObjectStores(int64_t, Vector<int64_t>&, Vector<String>&, Vector<String>&, Vector<bool>&)
{
    ASSERT_NOT_REACHED(); // FIXME: Implement.
}

bool IDBLevelDBBackingStore::createObjectStore(int64_t, const String&, const String&, bool, int64_t&)
{
    ASSERT_NOT_REACHED(); // FIXME: Implement.
    return false;
}

void IDBLevelDBBackingStore::deleteObjectStore(int64_t, int64_t)
{
    ASSERT_NOT_REACHED(); // FIXME: Implement.
}

String IDBLevelDBBackingStore::getObjectStoreRecord(int64_t, int64_t, const IDBKey&)
{
    ASSERT_NOT_REACHED(); // FIXME: Implement.
    return String();
}

bool IDBLevelDBBackingStore::putObjectStoreRecord(int64_t, int64_t, const IDBKey&, const String&, ObjectStoreRecordIdentifier*)
{
    ASSERT_NOT_REACHED(); // FIXME: Implement.
    return false;
}

void IDBLevelDBBackingStore::clearObjectStore(int64_t, int64_t)
{
    ASSERT_NOT_REACHED(); // FIXME: Implement.
}

PassRefPtr<IDBBackingStore::ObjectStoreRecordIdentifier> IDBLevelDBBackingStore::createInvalidRecordIdentifier()
{
    ASSERT_NOT_REACHED(); // FIXME: Implement.
    return 0;
}

void IDBLevelDBBackingStore::deleteObjectStoreRecord(int64_t, int64_t, const ObjectStoreRecordIdentifier*)
{
    ASSERT_NOT_REACHED(); // FIXME: Implement.
}

double IDBLevelDBBackingStore::nextAutoIncrementNumber(int64_t, int64_t)
{
    ASSERT_NOT_REACHED(); // FIXME: Implement.
    return 0;
}

bool IDBLevelDBBackingStore::keyExistsInObjectStore(int64_t, int64_t, const IDBKey&, ObjectStoreRecordIdentifier*)
{
    ASSERT_NOT_REACHED(); // FIXME: Implement.
    return false;
}

bool IDBLevelDBBackingStore::forEachObjectStoreRecord(int64_t, int64_t, ObjectStoreRecordCallback&)
{
    ASSERT_NOT_REACHED(); // FIXME: Implement.
    return false;
}

void IDBLevelDBBackingStore::getIndexes(int64_t, int64_t, Vector<int64_t>&, Vector<String>&, Vector<String>&, Vector<bool>&)
{
    ASSERT_NOT_REACHED(); // FIXME: Implement.
}

bool IDBLevelDBBackingStore::createIndex(int64_t, int64_t, const String&, const String&, bool, int64_t&)
{
    ASSERT_NOT_REACHED(); // FIXME: Implement.
    return false;
}

void IDBLevelDBBackingStore::deleteIndex(int64_t, int64_t, int64_t)
{
    ASSERT_NOT_REACHED(); // FIXME: Implement.
}

bool IDBLevelDBBackingStore::putIndexDataForRecord(int64_t, int64_t, int64_t, const IDBKey&, const ObjectStoreRecordIdentifier*)
{
    ASSERT_NOT_REACHED(); // FIXME: Implement.
    return false;
}

bool IDBLevelDBBackingStore::deleteIndexDataForRecord(int64_t, int64_t, int64_t, const ObjectStoreRecordIdentifier*)
{
    ASSERT_NOT_REACHED(); // FIXME: Implement.
    return false;
}

String IDBLevelDBBackingStore::getObjectViaIndex(int64_t, int64_t, int64_t, const IDBKey&)
{
    ASSERT_NOT_REACHED(); // FIXME: Implement.
    return String();
}

PassRefPtr<IDBKey> IDBLevelDBBackingStore::getPrimaryKeyViaIndex(int64_t, int64_t, int64_t, const IDBKey&)
{
    ASSERT_NOT_REACHED(); // FIXME: Implement.
    return 0;
}

bool IDBLevelDBBackingStore::keyExistsInIndex(int64_t, int64_t, int64_t, const IDBKey&)
{
    ASSERT_NOT_REACHED(); // FIXME: Implement.
    return false;
}

PassRefPtr<IDBBackingStore::Cursor> IDBLevelDBBackingStore::openObjectStoreCursor(int64_t, int64_t, const IDBKeyRange*, IDBCursor::Direction)
{
    ASSERT_NOT_REACHED(); // FIXME: Implement.
    return 0;
}

PassRefPtr<IDBBackingStore::Cursor> IDBLevelDBBackingStore::openIndexKeyCursor(int64_t, int64_t, int64_t, const IDBKeyRange*, IDBCursor::Direction)
{
    ASSERT_NOT_REACHED(); // FIXME: Implement.
    return 0;
}

PassRefPtr<IDBBackingStore::Cursor> IDBLevelDBBackingStore::openIndexCursor(int64_t, int64_t, int64_t, const IDBKeyRange*, IDBCursor::Direction)
{
    ASSERT_NOT_REACHED(); // FIXME: Implement.
    return 0;
}

PassRefPtr<IDBBackingStore::Transaction> IDBLevelDBBackingStore::createTransaction()
{
    ASSERT_NOT_REACHED(); // FIXME: Implement.
    return 0;
}

} // namespace WebCore

#endif // ENABLE(LEVELDB)
#endif // ENABLE(INDEXED_DATABASE)

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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
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
#include "IDBFactoryBackendImpl.h"

#include "DOMStringList.h"
#include "IDBDatabaseBackendImpl.h"
#include "IDBDatabaseException.h"
#include "IDBLevelDBBackingStore.h"
#include "IDBObjectStoreBackendImpl.h"
#include "IDBSQLiteBackingStore.h"
#include "IDBTransactionCoordinator.h"
#include "SecurityOrigin.h"
#include <wtf/Threading.h>
#include <wtf/UnusedParam.h>

#if ENABLE(INDEXED_DATABASE)

namespace WebCore {

static String computeFileIdentifier(SecurityOrigin* securityOrigin, IDBFactoryBackendInterface::BackingStoreType type)
{
    return securityOrigin->databaseIdentifier() + String::format("@%d", type);
}

static String computeUniqueIdentifier(const String& name, SecurityOrigin* securityOrigin, IDBFactoryBackendInterface::BackingStoreType type)
{
    return computeFileIdentifier(securityOrigin, type) + name;
}

IDBFactoryBackendImpl::IDBFactoryBackendImpl()
    : m_transactionCoordinator(IDBTransactionCoordinator::create())
{
}

IDBFactoryBackendImpl::~IDBFactoryBackendImpl()
{
}

void IDBFactoryBackendImpl::removeIDBDatabaseBackend(const String& uniqueIdentifier)
{
    ASSERT(m_databaseBackendMap.contains(uniqueIdentifier));
    m_databaseBackendMap.remove(uniqueIdentifier);
}

void IDBFactoryBackendImpl::addIDBBackingStore(const String& fileIdentifier, IDBBackingStore* backingStore)
{
    ASSERT(!m_backingStoreMap.contains(fileIdentifier));
    m_backingStoreMap.set(fileIdentifier, backingStore);
}

void IDBFactoryBackendImpl::removeIDBBackingStore(const String& fileIdentifier)
{
    ASSERT(m_backingStoreMap.contains(fileIdentifier));
    m_backingStoreMap.remove(fileIdentifier);
}

void IDBFactoryBackendImpl::getDatabaseNames(PassRefPtr<IDBCallbacks> callbacks, PassRefPtr<SecurityOrigin> securityOrigin, Frame*, const String& dataDir, int64_t maximumSize, BackingStoreType backingStoreType)
{
    ASSERT(backingStoreType != DefaultBackingStore);

    RefPtr<IDBBackingStore> backingStore = openBackingStore(securityOrigin, dataDir, maximumSize, backingStoreType);
    if (!backingStore) {
        callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::UNKNOWN_ERR, "Internal error."));
        return;
    }

    RefPtr<DOMStringList> databaseNames = DOMStringList::create();

    Vector<String> foundNames;
    backingStore->getDatabaseNames(foundNames);
    for (Vector<String>::const_iterator it = foundNames.begin(); it != foundNames.end(); ++it)
        databaseNames->append(*it);

    callbacks->onSuccess(databaseNames.release());
}

void IDBFactoryBackendImpl::open(const String& name, PassRefPtr<IDBCallbacks> callbacks, PassRefPtr<SecurityOrigin> securityOrigin, Frame*, const String& dataDir, int64_t maximumSize, BackingStoreType backingStoreType)
{
    ASSERT(backingStoreType != DefaultBackingStore);

    const String fileIdentifier = computeFileIdentifier(securityOrigin.get(), backingStoreType);
    const String uniqueIdentifier = computeUniqueIdentifier(name, securityOrigin.get(), backingStoreType);

    IDBDatabaseBackendMap::iterator it = m_databaseBackendMap.find(uniqueIdentifier);
    if (it != m_databaseBackendMap.end()) {
        callbacks->onSuccess(it->second);
        return;
    }

    // FIXME: Everything from now on should be done on another thread.

#if USE(LEVELDB)
    if (backingStoreType == LevelDBBackingStore) {
        bool hasSQLBackingStore = IDBSQLiteBackingStore::backingStoreExists(securityOrigin.get(), name, dataDir);

        // LayoutTests: SQLite backing store may not exist on disk but may exist in cache.
        String cachedSqliteBackingStoreIdentifier = computeFileIdentifier(securityOrigin.get(), SQLiteBackingStore);
        if (!hasSQLBackingStore && (m_backingStoreMap.end() != m_backingStoreMap.find(cachedSqliteBackingStoreIdentifier)))
            hasSQLBackingStore = true;

        if (hasSQLBackingStore) {
            bool migrationSucceeded = migrateFromSQLiteToLevelDB(name, securityOrigin.get(), dataDir, maximumSize);
            UNUSED_PARAM(migrationSucceeded); // FIXME: When migration is actually implemented, we need error handling here.
        }
    }
#endif

    RefPtr<IDBBackingStore> backingStore = openBackingStore(securityOrigin, dataDir, maximumSize, backingStoreType);
    if (!backingStore) {
        callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::UNKNOWN_ERR, "Internal error."));
        return;
    }

    RefPtr<IDBDatabaseBackendImpl> databaseBackend = IDBDatabaseBackendImpl::create(name, backingStore.get(), m_transactionCoordinator.get(), this, uniqueIdentifier);
    callbacks->onSuccess(databaseBackend.get());
    m_databaseBackendMap.set(uniqueIdentifier, databaseBackend.get());
}

PassRefPtr<IDBBackingStore> IDBFactoryBackendImpl::openBackingStore(PassRefPtr<SecurityOrigin> securityOrigin, const String& dataDir, int64_t maximumSize, BackingStoreType backingStoreType)
{
    ASSERT(backingStoreType != DefaultBackingStore);

    const String fileIdentifier = computeFileIdentifier(securityOrigin.get(), backingStoreType);

    RefPtr<IDBBackingStore> backingStore;
    IDBBackingStoreMap::iterator it2 = m_backingStoreMap.find(fileIdentifier);
    if (it2 != m_backingStoreMap.end() && (backingStoreType == it2->second->backingStoreType()))
        backingStore = it2->second;
    else {
        if (backingStoreType == SQLiteBackingStore)
            backingStore = IDBSQLiteBackingStore::open(securityOrigin.get(), dataDir, maximumSize, fileIdentifier, this);
#if USE(LEVELDB)
        else if (backingStoreType == LevelDBBackingStore)
            backingStore = IDBLevelDBBackingStore::open(securityOrigin.get(), dataDir, maximumSize, fileIdentifier, this);
#endif
    }

    if (backingStore)
        return backingStore.release();

    return 0;
}

#if USE(LEVELDB)

static bool migrateObjectStores(PassRefPtr<IDBBackingStore> fromBackingStore, int64_t fromDatabaseId, PassRefPtr<IDBBackingStore> toBackingStore, int64_t toDatabaseId)
{
    Vector<int64_t> fromObjStoreIds, toObjStoreIds;
    Vector<String> fromObjStoreNames, toObjStoreNames;
    Vector<String> fromKeyPaths, toKeyPaths;
    Vector<bool> fromAutoIncrement, toAutoIncrement;

    // Migrate objectStores. Check to see if the object store already exists in the target.
    fromBackingStore->getObjectStores(fromDatabaseId, fromObjStoreIds, fromObjStoreNames, fromKeyPaths, fromAutoIncrement);

    toBackingStore->getObjectStores(toDatabaseId, toObjStoreIds, toObjStoreNames, toKeyPaths, toAutoIncrement);

    for (unsigned i = 0; i < fromObjStoreIds.size(); i++) {
        if (toObjStoreNames.contains(fromObjStoreNames[i]))
            continue;

        int64_t assignedObjectStoreId = -1;

        RefPtr<IDBBackingStore::Transaction> trans = toBackingStore->createTransaction();
        trans->begin();

        if (!toBackingStore->createObjectStore(toDatabaseId, fromObjStoreNames[i], fromKeyPaths[i], fromAutoIncrement[i], assignedObjectStoreId))
            return false;

        RefPtr<IDBBackingStore::Cursor> cursor = fromBackingStore->openObjectStoreCursor(fromDatabaseId, fromObjStoreIds[i], 0, IDBCursor::NEXT);
        if (cursor) {
            do {
                RefPtr<IDBKey> key = cursor->key();
                RefPtr<IDBBackingStore::ObjectStoreRecordIdentifier> recordIdentifier = toBackingStore->createInvalidRecordIdentifier();

                if (!toBackingStore->putObjectStoreRecord(toDatabaseId, assignedObjectStoreId, *(key.get()), cursor->value(), recordIdentifier.get()))
                    return false;

            } while (cursor->continueFunction());
        }

        // Populate any/all indexes for this objectstore.
        Vector<int64_t> idxIds;
        Vector<String> idxNames;
        Vector<String> idxKeyPaths;
        Vector<bool> idxUnique;
        fromBackingStore->getIndexes(fromDatabaseId, fromObjStoreIds[i], idxIds, idxNames, idxKeyPaths, idxUnique);
        for (unsigned j = 0; j < idxIds.size(); j++) {
            int64_t indexId = -1;

            if (!toBackingStore->createIndex(toDatabaseId, assignedObjectStoreId, idxNames[j], idxKeyPaths[j], idxUnique[j], indexId))
                return false;

            if (!IDBObjectStoreBackendImpl::populateIndex(*toBackingStore, toDatabaseId, assignedObjectStoreId, indexId, fromKeyPaths[i]))
                return false;
        }

        trans->commit();
    }

    return true;
}
#endif // #if USE(LEVELDB)

bool IDBFactoryBackendImpl::migrateFromSQLiteToLevelDB(const String& name, SecurityOrigin* securityOrigin, const String& dataDir, int64_t maximumSize)
{
#if USE(LEVELDB)
    String fromUniqueIdentifier = computeUniqueIdentifier(name, securityOrigin, SQLiteBackingStore);
    String fromFileIdentifier = computeFileIdentifier(securityOrigin, SQLiteBackingStore);
    String toUniqueIdentifier = computeUniqueIdentifier(name, securityOrigin, LevelDBBackingStore);
    String toFileIdentifier = computeFileIdentifier(securityOrigin, LevelDBBackingStore);
    RefPtr<IDBTransactionCoordinator> transactionCoordinator = IDBTransactionCoordinator::create();
    RefPtr<IDBBackingStore> fromBackingStore;
    RefPtr<IDBDatabaseBackendImpl> fromDatabaseBackend;
    RefPtr<IDBBackingStore> toBackingStore;


    // Open "From" backing store and backend. When running LayoutTests, the
    // "from" database may be cached in this class instance, so look for it there first.
    IDBBackingStoreMap::iterator it = m_backingStoreMap.find(fromFileIdentifier);
    if (it != m_backingStoreMap.end())
        fromBackingStore = it->second;
    else
        fromBackingStore = IDBSQLiteBackingStore::open(securityOrigin, dataDir, maximumSize, fromFileIdentifier, this);

    if (!fromBackingStore)
        return false;

    IDBDatabaseBackendMap::iterator it2 = m_databaseBackendMap.find(fromUniqueIdentifier);
    if (it2 != m_databaseBackendMap.end())
        fromDatabaseBackend = it2->second;
    else {
        fromDatabaseBackend = IDBDatabaseBackendImpl::create(name, fromBackingStore.get(), transactionCoordinator.get(), this, fromUniqueIdentifier);
        m_databaseBackendMap.set(fromUniqueIdentifier, fromDatabaseBackend.get());
    }

    if (!fromDatabaseBackend)
        return false;

    // Open "To" database. First find out if it already exists -- this will determine if
    // it is safe to call IDBLevelDBBackingStore::extractIDBDatabaseMetaData.
    it = m_backingStoreMap.find(toFileIdentifier);
    if (it != m_backingStoreMap.end())
        toBackingStore = it->second;
    else
        toBackingStore = IDBLevelDBBackingStore::open(securityOrigin, dataDir, maximumSize, toFileIdentifier, this);

    if (!toBackingStore)
        return false;


    String toDatabaseName = fromDatabaseBackend->name();
    String toDatabaseVersion = fromDatabaseBackend->version();
    int64_t toDatabaseId = -1;

    if (!toBackingStore->extractIDBDatabaseMetaData(toDatabaseName, toDatabaseVersion, toDatabaseId)) {
        if (!toBackingStore->setIDBDatabaseMetaData(toDatabaseName, toDatabaseVersion, toDatabaseId, true))
            return false;
    }

    return migrateObjectStores(fromBackingStore, fromDatabaseBackend->id(), toBackingStore, toDatabaseId);

#endif // USE(LEVELDB)
    return false;
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)

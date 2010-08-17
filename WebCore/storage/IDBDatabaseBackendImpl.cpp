/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
#include "IDBDatabaseBackendImpl.h"

#include "DOMStringList.h"
#include "IDBDatabaseException.h"
#include "IDBObjectStoreBackendImpl.h"
#include "SQLiteDatabase.h"
#include "SQLiteStatement.h"

#if ENABLE(INDEXED_DATABASE)

namespace WebCore {

static bool extractMetaData(SQLiteDatabase* sqliteDatabase, const String& expectedName, String& foundDescription, String& foundVersion)
{
    SQLiteStatement metaDataQuery(*sqliteDatabase, "SELECT name, description, version FROM MetaData");
    if (metaDataQuery.prepare() != SQLResultOk || metaDataQuery.step() != SQLResultRow)
        return false;

    if (metaDataQuery.getColumnText(0) != expectedName) {
        LOG_ERROR("Name in MetaData (%s) doesn't match expected (%s) for IndexedDB", metaDataQuery.getColumnText(0).utf8().data(), expectedName.utf8().data());
        ASSERT_NOT_REACHED();
    }
    foundDescription = metaDataQuery.getColumnText(1);
    foundVersion = metaDataQuery.getColumnText(2);

    if (metaDataQuery.step() == SQLResultRow) {
        LOG_ERROR("More than one row found in MetaData table");
        ASSERT_NOT_REACHED();
    }

    return true;
}

static bool setMetaData(SQLiteDatabase* sqliteDatabase, const String& name, const String& description, const String& version)
{
    ASSERT(!name.isNull() && !description.isNull() && !version.isNull());

    sqliteDatabase->executeCommand("DELETE FROM MetaData");

    SQLiteStatement insert(*sqliteDatabase, "INSERT INTO MetaData (name, description, version) VALUES (?, ?, ?)");
    if (insert.prepare() != SQLResultOk) {
        LOG_ERROR("Failed to prepare MetaData insert statement for IndexedDB");
        return false;
    }

    insert.bindText(1, name);
    insert.bindText(2, description);
    insert.bindText(3, version);

    if (insert.step() != SQLResultDone) {
        LOG_ERROR("Failed to insert row into MetaData for IndexedDB");
        return false;
    }

    // FIXME: Should we assert there's only one row?

    return true;
}

IDBDatabaseBackendImpl::IDBDatabaseBackendImpl(const String& name, const String& description, PassOwnPtr<SQLiteDatabase> sqliteDatabase)
    : m_sqliteDatabase(sqliteDatabase)
    , m_name(name)
    , m_version("")
{
    ASSERT(!m_name.isNull());

    // FIXME: The spec is in flux about how to handle description. Sync it up once a final decision is made.
    String foundDescription = "";
    bool result = extractMetaData(m_sqliteDatabase.get(), m_name, foundDescription, m_version);
    m_description = description.isNull() ? foundDescription : description;

    if (!result || m_description != foundDescription)
        setMetaData(m_sqliteDatabase.get(), m_name, m_description, m_version);
}

IDBDatabaseBackendImpl::~IDBDatabaseBackendImpl()
{
}

void IDBDatabaseBackendImpl::setDescription(const String& description)
{
    if (description == m_description)
        return;

    m_description = description;
    setMetaData(m_sqliteDatabase.get(), m_name, m_description, m_version);
}

PassRefPtr<DOMStringList> IDBDatabaseBackendImpl::objectStores() const
{
    RefPtr<DOMStringList> objectStoreNames = DOMStringList::create();
    for (ObjectStoreMap::const_iterator it = m_objectStores.begin(); it != m_objectStores.end(); ++it)
        objectStoreNames->append(it->first);
    return objectStoreNames.release();
}

void IDBDatabaseBackendImpl::createObjectStore(const String& name, const String& keyPath, bool autoIncrement, PassRefPtr<IDBCallbacks> callbacks)
{
    if (m_objectStores.contains(name)) {
        callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::CONSTRAINT_ERR, "An objectStore with that name already exists."));
        return;
    }

    RefPtr<IDBObjectStoreBackendInterface> objectStore = IDBObjectStoreBackendImpl::create(name, keyPath, autoIncrement);
    m_objectStores.set(name, objectStore);
    callbacks->onSuccess(objectStore.release());
}

PassRefPtr<IDBObjectStoreBackendInterface> IDBDatabaseBackendImpl::objectStore(const String& name, unsigned short mode)
{
    // FIXME: If no transaction is running, this should implicitly start one.
    ASSERT_UNUSED(mode, !mode); // FIXME: Handle non-standard modes.
    return m_objectStores.get(name);
}

void IDBDatabaseBackendImpl::removeObjectStore(const String& name, PassRefPtr<IDBCallbacks> callbacks)
{
    if (!m_objectStores.contains(name)) {
        callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::NOT_FOUND_ERR, "No objectStore with that name exists."));
        return;
    }

    m_objectStores.remove(name);
    callbacks->onSuccess();
}

PassRefPtr<IDBTransactionBackendInterface> IDBDatabaseBackendImpl::transaction(DOMStringList*, unsigned short, unsigned long)
{
    // FIXME: Ask the transaction manager for a new IDBTransactionBackendImpl.
    ASSERT_NOT_REACHED();
    return 0;
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)

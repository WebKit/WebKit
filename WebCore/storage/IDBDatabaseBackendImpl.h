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

#ifndef IDBDatabaseBackendImpl_h
#define IDBDatabaseBackendImpl_h

#include "IDBCallbacks.h"
#include "IDBDatabase.h"
#include <wtf/HashMap.h>
#include <wtf/text/StringHash.h>

#if ENABLE(INDEXED_DATABASE)

namespace WebCore {

class IDBObjectStoreBackendImpl;
class IDBTransactionCoordinator;
class SQLiteDatabase;

class IDBDatabaseBackendImpl : public IDBDatabaseBackendInterface {
public:
    static PassRefPtr<IDBDatabaseBackendImpl> create(const String& name, const String& description, PassOwnPtr<SQLiteDatabase> database, IDBTransactionCoordinator* coordinator)
    {
        return adoptRef(new IDBDatabaseBackendImpl(name, description, database, coordinator));
    }
    virtual ~IDBDatabaseBackendImpl();

    void setDescription(const String& description);
    SQLiteDatabase& sqliteDatabase() const { return *m_sqliteDatabase.get(); }

    virtual String name() const { return m_name; }
    virtual String description() const { return m_description; }
    virtual String version() const { return m_version; }
    virtual PassRefPtr<DOMStringList> objectStores() const;

    virtual PassRefPtr<IDBObjectStoreBackendInterface> createObjectStore(const String& name, const String& keyPath, bool autoIncrement, IDBTransactionBackendInterface*);
    virtual PassRefPtr<IDBObjectStoreBackendInterface> objectStore(const String& name, unsigned short mode);
    virtual void removeObjectStore(const String& name, IDBTransactionBackendInterface*);
    virtual void setVersion(const String& version, PassRefPtr<IDBCallbacks>);
    virtual PassRefPtr<IDBTransactionBackendInterface> transaction(DOMStringList* storeNames, unsigned short mode, unsigned long timeout);
    virtual void close();

    IDBTransactionCoordinator* transactionCoordinator() const { return m_transactionCoordinator.get(); }

private:
    IDBDatabaseBackendImpl(const String& name, const String& description, PassOwnPtr<SQLiteDatabase> database, IDBTransactionCoordinator*);

    void loadObjectStores();

    static void createObjectStoreInternal(ScriptExecutionContext*, PassRefPtr<IDBDatabaseBackendImpl>, PassRefPtr<IDBObjectStoreBackendImpl>, PassRefPtr<IDBTransactionBackendInterface>);
    static void removeObjectStoreInternal(ScriptExecutionContext*, PassRefPtr<IDBDatabaseBackendImpl>, PassRefPtr<IDBObjectStoreBackendImpl>, PassRefPtr<IDBTransactionBackendInterface>);
    static void setVersionInternal(ScriptExecutionContext*, PassRefPtr<IDBDatabaseBackendImpl>, const String& version, PassRefPtr<IDBCallbacks>, PassRefPtr<IDBTransactionBackendInterface>);

    OwnPtr<SQLiteDatabase> m_sqliteDatabase;
    String m_name;
    String m_description;
    String m_version;

    typedef HashMap<String, RefPtr<IDBObjectStoreBackendImpl> > ObjectStoreMap;
    ObjectStoreMap m_objectStores;

    RefPtr<IDBTransactionCoordinator> m_transactionCoordinator;
};

} // namespace WebCore

#endif

#endif // IDBDatabaseBackendImpl_h

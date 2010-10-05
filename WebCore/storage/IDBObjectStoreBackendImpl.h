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

#ifndef IDBObjectStoreBackendImpl_h
#define IDBObjectStoreBackendImpl_h

#include "IDBObjectStoreBackendInterface.h"
#include <wtf/HashMap.h>
#include <wtf/text/StringHash.h>

#if ENABLE(INDEXED_DATABASE)

namespace WebCore {

class IDBDatabaseBackendImpl;
class IDBIndexBackendImpl;
class IDBTransactionBackendInterface;
class SQLiteDatabase;
class ScriptExecutionContext;

class IDBObjectStoreBackendImpl : public IDBObjectStoreBackendInterface {
public:
    static PassRefPtr<IDBObjectStoreBackendImpl> create(IDBDatabaseBackendImpl* database, int64_t id, const String& name, const String& keyPath, bool autoIncrement)
    {
        return adoptRef(new IDBObjectStoreBackendImpl(database, id, name, keyPath, autoIncrement));
    }
    static PassRefPtr<IDBObjectStoreBackendImpl> create(IDBDatabaseBackendImpl* database, const String& name, const String& keyPath, bool autoIncrement)
    {
        return adoptRef(new IDBObjectStoreBackendImpl(database, name, keyPath, autoIncrement));
    }
    ~IDBObjectStoreBackendImpl();

    int64_t id() const
    {
        ASSERT(m_id != InvalidId);
        return m_id;
    }
    void setId(int64_t id) { m_id = id; }
    String name() const { return m_name; }
    String keyPath() const { return m_keyPath; }
    PassRefPtr<DOMStringList> indexNames() const;
    bool autoIncrement() const { return m_autoIncrement; }

    void get(PassRefPtr<IDBKey> key, PassRefPtr<IDBCallbacks>, IDBTransactionBackendInterface*);
    void put(PassRefPtr<SerializedScriptValue> value, PassRefPtr<IDBKey> key, bool addOnly, PassRefPtr<IDBCallbacks>, IDBTransactionBackendInterface*);
    void remove(PassRefPtr<IDBKey> key, PassRefPtr<IDBCallbacks>, IDBTransactionBackendInterface*);

    PassRefPtr<IDBIndexBackendInterface> createIndex(const String& name, const String& keyPath, bool unique, IDBTransactionBackendInterface*);
    PassRefPtr<IDBIndexBackendInterface> index(const String& name);
    void removeIndex(const String& name, IDBTransactionBackendInterface*);

    void openCursor(PassRefPtr<IDBKeyRange> range, unsigned short direction, PassRefPtr<IDBCallbacks>, IDBTransactionBackendInterface*);

    IDBDatabaseBackendImpl* database() const { return m_database.get(); }

private:
    IDBObjectStoreBackendImpl(IDBDatabaseBackendImpl*, int64_t id, const String& name, const String& keyPath, bool autoIncrement);
    IDBObjectStoreBackendImpl(IDBDatabaseBackendImpl*, const String& name, const String& keyPath, bool autoIncrement);

    void loadIndexes();
    SQLiteDatabase& sqliteDatabase() const;

    static void getInternal(ScriptExecutionContext*, PassRefPtr<IDBObjectStoreBackendImpl>, PassRefPtr<IDBKey> key, PassRefPtr<IDBCallbacks>);
    static void putInternal(ScriptExecutionContext*, PassRefPtr<IDBObjectStoreBackendImpl>, PassRefPtr<SerializedScriptValue> value, PassRefPtr<IDBKey> key, bool addOnly, PassRefPtr<IDBCallbacks>);
    static void removeInternal(ScriptExecutionContext*, PassRefPtr<IDBObjectStoreBackendImpl>, PassRefPtr<IDBKey> key, PassRefPtr<IDBCallbacks>);
    static void createIndexInternal(ScriptExecutionContext*, PassRefPtr<IDBObjectStoreBackendImpl>, PassRefPtr<IDBIndexBackendImpl>, PassRefPtr<IDBTransactionBackendInterface>);
    static void removeIndexInternal(ScriptExecutionContext*, PassRefPtr<IDBObjectStoreBackendImpl>, PassRefPtr<IDBIndexBackendImpl>, PassRefPtr<IDBTransactionBackendInterface>);
    static void openCursorInternal(ScriptExecutionContext*, PassRefPtr<IDBObjectStoreBackendImpl>, PassRefPtr<IDBKeyRange> range, unsigned short direction, PassRefPtr<IDBCallbacks>, PassRefPtr<IDBTransactionBackendInterface>);

    static const int64_t InvalidId = 0;

    RefPtr<IDBDatabaseBackendImpl> m_database;

    int64_t m_id;
    String m_name;
    String m_keyPath;
    bool m_autoIncrement;

    typedef HashMap<String, RefPtr<IDBIndexBackendImpl> > IndexMap;
    IndexMap m_indexes;
};

} // namespace WebCore

#endif

#endif // IDBObjectStoreBackendImpl_h

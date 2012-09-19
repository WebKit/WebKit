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
class ScriptExecutionContext;

class IDBIndexBackendImpl : public IDBIndexBackendInterface {
public:
    static PassRefPtr<IDBIndexBackendImpl> create(const IDBDatabaseBackendImpl* database, IDBObjectStoreBackendImpl* objectStoreBackend, int64_t id, const String& name, const IDBKeyPath& keyPath, bool unique, bool multiEntry)
    {
        return adoptRef(new IDBIndexBackendImpl(database, objectStoreBackend, id, name, keyPath, unique, multiEntry));
    }
    static PassRefPtr<IDBIndexBackendImpl> create(const IDBDatabaseBackendImpl* database, IDBObjectStoreBackendImpl* objectStoreBackend, const String& name, const IDBKeyPath& keyPath, bool unique, bool multiEntry)
    {
        return adoptRef(new IDBIndexBackendImpl(database, objectStoreBackend, name, keyPath, unique, multiEntry));
    }
    virtual ~IDBIndexBackendImpl();

    int64_t id() const
    {
        ASSERT(m_id != InvalidId);
        return m_id;
    }
    void setId(int64_t id) { m_id = id; }
    bool hasValidId() const { return m_id != InvalidId; };

    // IDBIndexBackendInterface
    virtual void openCursor(PassRefPtr<IDBKeyRange>, unsigned short direction, PassRefPtr<IDBCallbacks>, IDBTransactionBackendInterface*, ExceptionCode&);
    virtual void count(PassRefPtr<IDBKeyRange>, PassRefPtr<IDBCallbacks>, IDBTransactionBackendInterface*, ExceptionCode&);
    virtual void openKeyCursor(PassRefPtr<IDBKeyRange>, unsigned short direction, PassRefPtr<IDBCallbacks>, IDBTransactionBackendInterface*, ExceptionCode&);
    virtual void get(PassRefPtr<IDBKeyRange>, PassRefPtr<IDBCallbacks>, IDBTransactionBackendInterface*, ExceptionCode&);
    virtual void getKey(PassRefPtr<IDBKeyRange>, PassRefPtr<IDBCallbacks>, IDBTransactionBackendInterface*, ExceptionCode&);

    IDBIndexMetadata metadata() const;
    const String& name() { return m_name; }
    const IDBKeyPath& keyPath() { return m_keyPath; }
    const bool& unique() { return m_unique; }
    const bool& multiEntry() { return m_multiEntry; }

private:
    IDBIndexBackendImpl(const IDBDatabaseBackendImpl*, IDBObjectStoreBackendImpl*, int64_t id, const String& name, const IDBKeyPath&, bool unique, bool multiEntry);
    IDBIndexBackendImpl(const IDBDatabaseBackendImpl*, IDBObjectStoreBackendImpl*, const String& name, const IDBKeyPath&, bool unique, bool multiEntry);

    static void openCursorInternal(ScriptExecutionContext*, PassRefPtr<IDBIndexBackendImpl>, PassRefPtr<IDBKeyRange>, unsigned short direction, IDBCursorBackendInterface::CursorType, PassRefPtr<IDBCallbacks>, PassRefPtr<IDBTransactionBackendImpl>);
    static void countInternal(ScriptExecutionContext*, PassRefPtr<IDBIndexBackendImpl>, PassRefPtr<IDBKeyRange>, PassRefPtr<IDBCallbacks>, PassRefPtr<IDBTransactionBackendImpl>);
    static void getInternal(ScriptExecutionContext*, PassRefPtr<IDBIndexBackendImpl>, PassRefPtr<IDBKeyRange>, PassRefPtr<IDBCallbacks>, PassRefPtr<IDBTransactionBackendImpl>);
    static void getKeyInternal(ScriptExecutionContext*, PassRefPtr<IDBIndexBackendImpl>, PassRefPtr<IDBKeyRange>, PassRefPtr<IDBCallbacks>, PassRefPtr<IDBTransactionBackendImpl>);

    static const int64_t InvalidId = 0;

    PassRefPtr<IDBBackingStore> backingStore() const { return m_database->backingStore(); }
    int64_t databaseId() const { return m_database->id(); }

    const IDBDatabaseBackendImpl* m_database;

    IDBObjectStoreBackendImpl* m_objectStoreBackend;
    int64_t m_id;
    String m_name;
    IDBKeyPath m_keyPath;
    bool m_unique;
    bool m_multiEntry;
};

} // namespace WebCore

#endif

#endif // IDBIndexBackendImpl_h

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

#ifndef IDBDatabase_h
#define IDBDatabase_h

#include "DOMStringList.h"
#include "IDBDatabaseBackendInterface.h"
#include "IDBTransaction.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

#if ENABLE(INDEXED_DATABASE)

namespace WebCore {

class IDBAny;
class IDBObjectStore;
class IDBRequest;
class ScriptExecutionContext;

class IDBDatabase : public RefCounted<IDBDatabase> {
public:
    static PassRefPtr<IDBDatabase> create(PassRefPtr<IDBDatabaseBackendInterface> database)
    {
        return adoptRef(new IDBDatabase(database));
    }
    ~IDBDatabase();

    void setSetVersionTransaction(IDBTransactionBackendInterface*);

    // Implement the IDL
    String name() const { return m_backend->name(); }
    String version() const { return m_backend->version(); }
    PassRefPtr<DOMStringList> objectStores() const { return m_backend->objectStores(); }

    PassRefPtr<IDBObjectStore> createObjectStore(const String& name, const String& keyPath = String(), bool autoIncrement = false);
    void removeObjectStore(const String& name);
    PassRefPtr<IDBRequest> setVersion(ScriptExecutionContext*, const String& version);
    PassRefPtr<IDBTransaction> transaction(ScriptExecutionContext*, DOMStringList* storeNames = 0, unsigned short mode = IDBTransaction::READ_ONLY,
                                           unsigned long timeout = 0); // FIXME: what should the default timeout be?
    void close();

private:
    IDBDatabase(PassRefPtr<IDBDatabaseBackendInterface>);

    RefPtr<IDBDatabaseBackendInterface> m_backend;
    RefPtr<IDBTransactionBackendInterface> m_setVersionTransaction;
};

} // namespace WebCore

#endif

#endif // IDBDatabase_h

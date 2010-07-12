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

#ifndef IDBDatabaseRequest_h
#define IDBDatabaseRequest_h

#include "DOMStringList.h"
#include "IDBDatabase.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

#if ENABLE(INDEXED_DATABASE)

namespace WebCore {

class IDBAny;
class IDBObjectStoreRequest;
class IDBRequest;
class ScriptExecutionContext;

class IDBDatabaseRequest : public RefCounted<IDBDatabaseRequest> {
public:
    static PassRefPtr<IDBDatabaseRequest> create(PassRefPtr<IDBDatabase> database)
    {
        return adoptRef(new IDBDatabaseRequest(database));
    }
    ~IDBDatabaseRequest();

    // Implement the IDL
    String name() const { return m_database->name(); }
    String description() const { return m_database->description(); }
    String version() const { return m_database->version(); }
    PassRefPtr<DOMStringList> objectStores() const { return m_database->objectStores(); }

    PassRefPtr<IDBRequest> createObjectStore(ScriptExecutionContext*, const String& name, const String& keyPath = String(), bool autoIncrement = false);
    PassRefPtr<IDBObjectStoreRequest> objectStore(const String& name, unsigned short mode = 0); // FIXME: Use constant rather than 0.
    PassRefPtr<IDBRequest> removeObjectStore(ScriptExecutionContext*, const String& name);

private:
    IDBDatabaseRequest(PassRefPtr<IDBDatabase>);

    RefPtr<IDBDatabase> m_database;
    RefPtr<IDBAny> m_this;
};

} // namespace WebCore

#endif

#endif // IDBDatabaseRequest_h

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

#ifndef IDBDatabaseProxy_h
#define IDBDatabaseProxy_h

#include "IDBDatabaseBackendInterface.h"
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/PassRefPtr.h>

#if ENABLE(INDEXED_DATABASE)

namespace WebKit { class WebIDBDatabase; }

namespace WebCore {

class IDBDatabaseProxy : public IDBDatabaseBackendInterface {
public:
    static PassRefPtr<IDBDatabaseBackendInterface> create(PassOwnPtr<WebKit::WebIDBDatabase>);
    virtual ~IDBDatabaseProxy();

    virtual String name() const;
    virtual String description() const;
    virtual String version() const;
    virtual PassRefPtr<DOMStringList> objectStores() const;

    virtual PassRefPtr<IDBObjectStoreBackendInterface> createObjectStore(const String& name, const String& keyPath, bool autoIncrement, IDBTransactionBackendInterface*);
    virtual PassRefPtr<IDBObjectStoreBackendInterface> objectStore(const String& name, unsigned short mode);
    virtual void removeObjectStore(const String& name, IDBTransactionBackendInterface*);
    virtual void setVersion(const String& version, PassRefPtr<IDBCallbacks>);
    virtual PassRefPtr<IDBTransactionBackendInterface> transaction(DOMStringList* storeNames, unsigned short mode, unsigned long timeout);
    virtual void close();

private:
    IDBDatabaseProxy(PassOwnPtr<WebKit::WebIDBDatabase>);

    OwnPtr<WebKit::WebIDBDatabase> m_webIDBDatabase;
};

} // namespace WebCore

#endif

#endif // IDBDatabaseProxy_h

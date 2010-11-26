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

#ifndef IDBObjectStoreProxy_h
#define IDBObjectStoreProxy_h

#include "IDBObjectStoreBackendInterface.h"
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/PassRefPtr.h>

#if ENABLE(INDEXED_DATABASE)

namespace WebKit { class WebIDBObjectStore; }

namespace WebCore {

class IDBObjectStoreProxy : public IDBObjectStoreBackendInterface {
public:
    static PassRefPtr<IDBObjectStoreBackendInterface> create(PassOwnPtr<WebKit::WebIDBObjectStore>);
    virtual ~IDBObjectStoreProxy();

    virtual String name() const;
    virtual String keyPath() const;
    virtual PassRefPtr<DOMStringList> indexNames() const;

    virtual void get(PassRefPtr<IDBKey>, PassRefPtr<IDBCallbacks>, IDBTransactionBackendInterface*, ExceptionCode&);
    virtual void put(PassRefPtr<SerializedScriptValue>, PassRefPtr<IDBKey>, bool addOnly, PassRefPtr<IDBCallbacks>, IDBTransactionBackendInterface*, ExceptionCode&);
    virtual void deleteFunction(PassRefPtr<IDBKey>, PassRefPtr<IDBCallbacks>, IDBTransactionBackendInterface*, ExceptionCode&);

    PassRefPtr<IDBIndexBackendInterface> createIndex(const String& name, const String& keyPath, bool unique, IDBTransactionBackendInterface*, ExceptionCode&);
    PassRefPtr<IDBIndexBackendInterface> index(const String& name, ExceptionCode&);
    void deleteIndex(const String& name, IDBTransactionBackendInterface*, ExceptionCode&);

    virtual void openCursor(PassRefPtr<IDBKeyRange>, unsigned short direction, PassRefPtr<IDBCallbacks>, IDBTransactionBackendInterface*, ExceptionCode&);

private:
    IDBObjectStoreProxy(PassOwnPtr<WebKit::WebIDBObjectStore>);

    OwnPtr<WebKit::WebIDBObjectStore> m_webIDBObjectStore;
};

} // namespace WebCore

#endif

#endif // IDBObjectStoreProxy_h


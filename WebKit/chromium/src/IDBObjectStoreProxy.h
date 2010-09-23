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
    ~IDBObjectStoreProxy();

    String name() const;
    String keyPath() const;
    PassRefPtr<DOMStringList> indexNames() const;

    void get(PassRefPtr<IDBKey> key, PassRefPtr<IDBCallbacks>, IDBTransactionBackendInterface*);
    void put(PassRefPtr<SerializedScriptValue> value, PassRefPtr<IDBKey> key, bool addOnly, PassRefPtr<IDBCallbacks>);
    void remove(PassRefPtr<IDBKey> key, PassRefPtr<IDBCallbacks>);

    void createIndex(const String& name, const String& keyPath, bool unique, PassRefPtr<IDBCallbacks>);
    PassRefPtr<IDBIndexBackendInterface> index(const String& name);
    void removeIndex(const String& name, PassRefPtr<IDBCallbacks>);

    virtual void openCursor(PassRefPtr<IDBKeyRange> range, unsigned short direction, PassRefPtr<IDBCallbacks>);

private:
    IDBObjectStoreProxy(PassOwnPtr<WebKit::WebIDBObjectStore>);

    OwnPtr<WebKit::WebIDBObjectStore> m_webIDBObjectStore;
};

} // namespace WebCore

#endif

#endif // IDBObjectStoreProxy_h


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

#ifndef IDBObjectStoreBackendProxy_h
#define IDBObjectStoreBackendProxy_h

#if ENABLE(INDEXED_DATABASE)

#include "IDBObjectStoreBackendInterface.h"
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/PassRefPtr.h>

namespace WebKit {

class WebIDBObjectStore;

class IDBObjectStoreBackendProxy : public WebCore::IDBObjectStoreBackendInterface {
public:
    static PassRefPtr<WebCore::IDBObjectStoreBackendInterface> create(PassOwnPtr<WebIDBObjectStore>);
    virtual ~IDBObjectStoreBackendProxy();

    virtual String name() const;
    virtual String keyPath() const;
    virtual PassRefPtr<WebCore::DOMStringList> indexNames() const;
    virtual bool autoIncrement() const;

    virtual void get(PassRefPtr<WebCore::IDBKeyRange>, PassRefPtr<WebCore::IDBCallbacks>, WebCore::IDBTransactionBackendInterface*, WebCore::ExceptionCode&);
    virtual void put(PassRefPtr<WebCore::SerializedScriptValue>, PassRefPtr<WebCore::IDBKey>, PutMode, PassRefPtr<WebCore::IDBCallbacks>, WebCore::IDBTransactionBackendInterface*, WebCore::ExceptionCode&);
    virtual void deleteFunction(PassRefPtr<WebCore::IDBKey>, PassRefPtr<WebCore::IDBCallbacks>, WebCore::IDBTransactionBackendInterface*, WebCore::ExceptionCode&);
    virtual void deleteFunction(PassRefPtr<WebCore::IDBKeyRange>, PassRefPtr<WebCore::IDBCallbacks>, WebCore::IDBTransactionBackendInterface*, WebCore::ExceptionCode&);
    virtual void clear(PassRefPtr<WebCore::IDBCallbacks>, WebCore::IDBTransactionBackendInterface*, WebCore::ExceptionCode&);

    PassRefPtr<WebCore::IDBIndexBackendInterface> createIndex(const String& name, const String& keyPath, bool unique, bool multiEntry, WebCore::IDBTransactionBackendInterface*, WebCore::ExceptionCode&);
    PassRefPtr<WebCore::IDBIndexBackendInterface> index(const String& name, WebCore::ExceptionCode&);
    void deleteIndex(const String& name, WebCore::IDBTransactionBackendInterface*, WebCore::ExceptionCode&);

    virtual void openCursor(PassRefPtr<WebCore::IDBKeyRange>, unsigned short direction, PassRefPtr<WebCore::IDBCallbacks>, WebCore::IDBTransactionBackendInterface*, WebCore::ExceptionCode&);
    virtual void count(PassRefPtr<WebCore::IDBKeyRange>, PassRefPtr<WebCore::IDBCallbacks>, WebCore::IDBTransactionBackendInterface*, WebCore::ExceptionCode&);

private:
    IDBObjectStoreBackendProxy(PassOwnPtr<WebIDBObjectStore>);

    OwnPtr<WebIDBObjectStore> m_webIDBObjectStore;
};

} // namespace WebKit

#endif

#endif // IDBObjectStoreBackendProxy_h

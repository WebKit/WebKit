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

#ifndef WebIDBDatabaseImpl_h
#define WebIDBDatabaseImpl_h

#if ENABLE(INDEXED_DATABASE)

#include "IDBDatabaseCallbacksProxy.h"
#include "platform/WebCommon.h"
#include "WebExceptionCode.h"
#include "WebIDBDatabase.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>

namespace WebCore { class IDBDatabaseBackendInterface; }

namespace WebKit {

class WebIDBDatabaseCallbacks;
class WebIDBDatabaseMetadata;
class WebIDBObjectStore;
class WebIDBTransaction;

// See comment in WebIDBFactory for a high level overview these classes.
class WebIDBDatabaseImpl : public WebIDBDatabase {
public:
    WebIDBDatabaseImpl(WTF::PassRefPtr<WebCore::IDBDatabaseBackendInterface>, WTF::PassRefPtr<IDBDatabaseCallbacksProxy>);
    virtual ~WebIDBDatabaseImpl();

    virtual WebIDBMetadata metadata() const;

    virtual WebIDBObjectStore* createObjectStore(long long, const WebString& name, const WebIDBKeyPath&, bool autoIncrement, const WebIDBTransaction&, WebExceptionCode&);
    virtual void deleteObjectStore(long long objectStoreId, const WebIDBTransaction&, WebExceptionCode&);
    // FIXME: Remove this method in https://bugs.webkit.org/show_bug.cgi?id=103923.
    virtual WebIDBTransaction* createTransaction(long long id, const WebVector<long long>&, unsigned short mode);
    virtual void createTransaction(long long id, WebIDBDatabaseCallbacks*, const WebVector<long long>&, unsigned short mode);
    virtual void forceClose();
    virtual void close();
    virtual void abort(long long transactionId);
    virtual void commit(long long transactionId);

private:
    WTF::RefPtr<WebCore::IDBDatabaseBackendInterface> m_databaseBackend;
    WTF::RefPtr<IDBDatabaseCallbacksProxy> m_databaseCallbacks;
};

} // namespace WebKit

#endif // WebIDBDatabaseImpl_h

#endif // ENABLE(INDEXED_DATABASE)

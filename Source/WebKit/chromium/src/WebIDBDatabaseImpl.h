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

#include "platform/WebCommon.h"
#include "WebExceptionCode.h"
#include "WebIDBDatabase.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>

namespace WebCore { class IDBDatabaseBackendInterface; }

namespace WebKit {

class IDBDatabaseCallbacksProxy;
class WebIDBDatabaseCallbacks;
class WebIDBObjectStore;
class WebIDBTransaction;

// See comment in WebIDBFactory for a high level overview these classes.
class WebIDBDatabaseImpl : public WebIDBDatabase {
public:
    WebIDBDatabaseImpl(WTF::PassRefPtr<WebCore::IDBDatabaseBackendInterface>);
    virtual ~WebIDBDatabaseImpl();

    virtual WebString name() const;
    virtual WebString version() const;
    virtual WebDOMStringList objectStoreNames() const;

    // FIXME: Remove WebString keyPath overload once callers are updated.
    // http://webkit.org/b/84207
    virtual WebIDBObjectStore* createObjectStore(const WebString&, const WebString&, bool, const WebIDBTransaction&, WebExceptionCode&);
    virtual WebIDBObjectStore* createObjectStore(const WebString& name, const WebIDBKeyPath& keyPath, bool autoIncrement, const WebIDBTransaction& transaction, WebExceptionCode& ec) { return createObjectStore(name, keyPath.string(), autoIncrement, transaction, ec); }
    virtual void deleteObjectStore(const WebString& name, const WebIDBTransaction&, WebExceptionCode&);
    virtual void setVersion(const WebString& version, WebIDBCallbacks*, WebExceptionCode&);
    virtual WebIDBTransaction* transaction(const WebDOMStringList& names, unsigned short mode, WebExceptionCode&);
    virtual void close();

    virtual void open(WebIDBDatabaseCallbacks*);

private:
    WTF::RefPtr<WebCore::IDBDatabaseBackendInterface> m_databaseBackend;
    WTF::RefPtr<IDBDatabaseCallbacksProxy> m_databaseCallbacks;
};

} // namespace WebKit

#endif // WebIDBDatabaseImpl_h

#endif // ENABLE(INDEXED_DATABASE)

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

#ifndef WebIDBDatabase_h
#define WebIDBDatabase_h

#include "WebCommon.h"
#include "WebDOMStringList.h"
#include "WebExceptionCode.h"

namespace WebKit {

class WebFrame;
class WebIDBCallbacks;
class WebIDBDatabaseCallbacks;
class WebIDBObjectStore;
class WebIDBTransaction;

// See comment in WebIDBFactory for a high level overview of these classes.
class WebIDBDatabase {
public:
    virtual ~WebIDBDatabase() { }

    virtual WebString name() const
    {
        WEBKIT_ASSERT_NOT_REACHED();
        return WebString();
    }
    virtual WebString version() const
    {
        WEBKIT_ASSERT_NOT_REACHED();
        return WebString();
    }
    virtual WebDOMStringList objectStoreNames() const
    {
        WEBKIT_ASSERT_NOT_REACHED();
        return WebDOMStringList();
    }
    virtual WebIDBObjectStore* createObjectStore(const WebString& name, const WebString& keyPath, bool autoIncrement, const WebIDBTransaction&, WebExceptionCode&)
    {
        WEBKIT_ASSERT_NOT_REACHED();
        return 0;
    }
    virtual void deleteObjectStore(const WebString& name, const WebIDBTransaction& transaction, WebExceptionCode& ec) { WEBKIT_ASSERT_NOT_REACHED(); }
    virtual void setVersion(const WebString& version, WebIDBCallbacks* callbacks, WebExceptionCode&) { WEBKIT_ASSERT_NOT_REACHED(); }
    // Transfers ownership of the WebIDBTransaction to the caller.
    virtual WebIDBTransaction* transaction(const WebDOMStringList& names, unsigned short mode, WebExceptionCode& ec)
    {
        return transaction(names, mode, 0, ec);
    }
    // FIXME: Remove.
    virtual WebIDBTransaction* transaction(const WebDOMStringList& names, unsigned short mode, unsigned long, WebExceptionCode& ec)
    {
        return transaction(names, mode, ec);
    }
    virtual void close() { WEBKIT_ASSERT_NOT_REACHED(); }

    virtual void open(WebIDBDatabaseCallbacks*) { WEBKIT_ASSERT_NOT_REACHED(); }

protected:
    WebIDBDatabase() { }
};

} // namespace WebKit

#endif // WebIDBDatabase_h

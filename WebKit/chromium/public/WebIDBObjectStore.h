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

#ifndef WebIDBObjectStore_h
#define WebIDBObjectStore_h

#include "WebCommon.h"
#include "WebExceptionCode.h"
#include "WebDOMStringList.h"
#include "WebIDBCallbacks.h"
#include "WebIDBTransaction.h"
#include "WebString.h"

namespace WebKit {

class WebIDBKeyRange;
class WebIDBTransaction;

// See comment in WebIDBFactory for a high level overview these classes.
class WebIDBObjectStore {
public:
    virtual ~WebIDBObjectStore() { }

    virtual WebString name() const
    {
        WEBKIT_ASSERT_NOT_REACHED();
        return WebString();
    }
    virtual WebString keyPath() const
    {
        WEBKIT_ASSERT_NOT_REACHED();
        return WebString();
    }
    virtual WebDOMStringList indexNames() const
    {
        WEBKIT_ASSERT_NOT_REACHED();
        return WebDOMStringList();
    }

    virtual void get(const WebIDBKey&, WebIDBCallbacks*, const WebIDBTransaction&, WebExceptionCode&) { WEBKIT_ASSERT_NOT_REACHED(); }
    virtual void put(const WebSerializedScriptValue&, const WebIDBKey&, bool addOnly, WebIDBCallbacks*, const WebIDBTransaction&, WebExceptionCode&) { WEBKIT_ASSERT_NOT_REACHED(); }
    // FIXME: Remove after roll.
    virtual void remove(const WebIDBKey& key, WebIDBCallbacks* callbacks, const WebIDBTransaction& transaction, WebExceptionCode& ec) { deleteFunction(key, callbacks, transaction, ec); }
    virtual void deleteFunction(const WebIDBKey& key, WebIDBCallbacks* callbacks, const WebIDBTransaction& transaction, WebExceptionCode& ec) { remove(key, callbacks, transaction, ec); }
    virtual WebIDBIndex* createIndex(const WebString& name, const WebString& keyPath, bool unique, const WebIDBTransaction&, WebExceptionCode&)
    {
        WEBKIT_ASSERT_NOT_REACHED();
        return 0;
    }
    // Transfers ownership of the WebIDBIndex to the caller.
    virtual WebIDBIndex* index(const WebString& name, WebExceptionCode&)
    {
        WEBKIT_ASSERT_NOT_REACHED();
        return 0;
    }
    virtual void deleteIndex(const WebString& name, const WebIDBTransaction&, WebExceptionCode&) { WEBKIT_ASSERT_NOT_REACHED(); }
    virtual void openCursor(const WebIDBKeyRange&, unsigned short direction, WebIDBCallbacks*, const WebIDBTransaction&, WebExceptionCode&) { WEBKIT_ASSERT_NOT_REACHED(); }

protected:
    WebIDBObjectStore() {}
};

} // namespace WebKit

#endif // WebIDBObjectStore_h

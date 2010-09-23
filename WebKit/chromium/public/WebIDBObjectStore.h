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
#include "WebDOMStringList.h"
#include "WebIDBCallbacks.h"
#include "WebIDBTransaction.h"
#include "WebString.h"

namespace WebKit {

class WebIDBKeyRange;
class WebIDBTransaction;

// See comment in WebIndexedDatabase for a high level overview these classes.
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

    // FIXME: Remove the default parameters for transactionIds.
    virtual void get(const WebIDBKey& key, WebIDBCallbacks* callbacks, const WebIDBTransaction&)
    {
        get(key, callbacks);
    }
    virtual void get(const WebIDBKey& key, WebIDBCallbacks* callbacks)
    {
        WebIDBTransaction transaction;
        get(key, callbacks, transaction);
    }
    virtual void put(const WebSerializedScriptValue& value, const WebIDBKey& key, bool addOnly, WebIDBCallbacks* callbacks, const WebIDBTransaction&)
    {
        put(value, key, addOnly, callbacks);
    }
    virtual void put(const WebSerializedScriptValue& value, const WebIDBKey& key, bool addOnly, WebIDBCallbacks* callbacks)
    {
        WebIDBTransaction transaction;
        put(value, key, addOnly, callbacks, transaction);
    }
    virtual void remove(const WebIDBKey& key, WebIDBCallbacks* callbacks, const WebIDBTransaction&)
    {
        remove(key, callbacks);
    }
    virtual void remove(const WebIDBKey& key, WebIDBCallbacks* callbacks)
    {
        WebIDBTransaction transaction;
        remove(key, callbacks, transaction);
    }
    virtual void createIndex(const WebString& name, const WebString& keyPath, bool unique, WebIDBCallbacks*) { WEBKIT_ASSERT_NOT_REACHED(); }
    // Transfers ownership of the WebIDBIndex to the caller.
    virtual WebIDBIndex* index(const WebString& name)
    {
        WEBKIT_ASSERT_NOT_REACHED();
        return 0;
    }
    virtual void removeIndex(const WebString& name, WebIDBCallbacks*) { WEBKIT_ASSERT_NOT_REACHED(); }
    virtual void openCursor(const WebIDBKeyRange& range, unsigned short direction, WebIDBCallbacks* callbacks, const WebIDBTransaction&)
    {
        openCursor(range, direction, callbacks);
    }
    virtual void openCursor(const WebIDBKeyRange& range, unsigned short direction, WebIDBCallbacks* callbacks)
    {
        WebIDBTransaction transaction;
        openCursor(range, direction, callbacks, transaction);
    }
    // FIXME: finish.
};

} // namespace WebKit

#endif // WebIDBObjectStore_h

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

#ifndef WebIDBIndex_h
#define WebIDBIndex_h

#include "WebIDBTransaction.h"
#include "WebString.h"

namespace WebKit {

class WebIDBCallbacks;
class WebIDBKey;
class WebIDBKeyRange;

// See comment in WebIndexedDatabase for a high level overview of these classes.
class WebIDBIndex {
public:
    virtual ~WebIDBIndex() { }

    virtual WebString name() const
    {
        WEBKIT_ASSERT_NOT_REACHED();
        return WebString();
    }
    virtual WebString storeName() const
    {
        WEBKIT_ASSERT_NOT_REACHED();
        return WebString();
    }
    virtual WebString keyPath() const
    {
        WEBKIT_ASSERT_NOT_REACHED();
        return WebString();
    }
    virtual bool unique() const
    {
        WEBKIT_ASSERT_NOT_REACHED();
        return false;
    }

    // FIXME: Remove the versions without transaction parameters.
    virtual void openObjectCursor(const WebIDBKeyRange& range, unsigned short direction, WebIDBCallbacks* callbacks, const WebIDBTransaction&)
    {
        openObjectCursor(range, direction, callbacks);
    }
    virtual void openObjectCursor(const WebIDBKeyRange& range, unsigned short direction, WebIDBCallbacks* callbacks)
    {
        WebIDBTransaction transaction;
        openObjectCursor(range, direction, callbacks, transaction);
    }
    virtual void openCursor(const WebIDBKeyRange& range, unsigned short direction, WebIDBCallbacks* callbacks, const WebIDBTransaction&)
    {
        openCursor(range, direction, callbacks);
    }
    virtual void openCursor(const WebIDBKeyRange& range, unsigned short direction, WebIDBCallbacks* callbacks)
    {
        WebIDBTransaction transaction;
        openCursor(range, direction, callbacks, transaction);
    }
    virtual void getObject(const WebIDBKey& key, WebIDBCallbacks* callbacks, const WebIDBTransaction&)
    {
        getObject(key, callbacks);
    }
    virtual void getObject(const WebIDBKey& key, WebIDBCallbacks* callbacks)
    {
        WebIDBTransaction transaction;
        getObject(key, callbacks, transaction);
    }
    virtual void get(const WebIDBKey& key, WebIDBCallbacks* callbacks, const WebIDBTransaction&)
    {
        get(key, callbacks);
    }
    virtual void get(const WebIDBKey& key, WebIDBCallbacks* callbacks)
    {
        WebIDBTransaction transaction;
        get(key, callbacks, transaction);
    }
};

} // namespace WebKit

#endif // WebIDBIndex_h

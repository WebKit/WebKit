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

#include "WebDOMStringList.h"
#include "WebExceptionCode.h"
#include "WebIDBCallbacks.h"
#include "WebIDBCursor.h"
#include "WebIDBKeyPath.h"
#include "WebIDBTransaction.h"
#include "platform/WebCommon.h"
#include "platform/WebString.h"

namespace WebKit {

class WebIDBKeyRange;
class WebIDBTransaction;

// See comment in WebIDBFactory for a high level overview these classes.
class WebIDBObjectStore {
public:
    virtual ~WebIDBObjectStore() { }

    virtual void get(const WebIDBKeyRange&, WebIDBCallbacks*, const WebIDBTransaction&, WebExceptionCode&) { WEBKIT_ASSERT_NOT_REACHED(); }

    enum PutMode {
        AddOrUpdate,
        AddOnly,
        CursorUpdate
    };

    typedef WebVector<WebIDBKey> WebIndexKeys;

    virtual void put(const WebSerializedScriptValue&, const WebIDBKey&, PutMode, WebIDBCallbacks*, const WebIDBTransaction&, const WebVector<long long>& indexIds, const WebVector<WebIndexKeys>&) { WEBKIT_ASSERT_NOT_REACHED(); }
    virtual void deleteFunction(const WebIDBKeyRange&, WebIDBCallbacks*, const WebIDBTransaction&, WebExceptionCode&) { WEBKIT_ASSERT_NOT_REACHED(); }
    virtual void clear(WebIDBCallbacks*, const WebIDBTransaction&, WebExceptionCode&) { WEBKIT_ASSERT_NOT_REACHED(); }
    virtual WebIDBIndex* createIndex(const WebString&, const WebIDBKeyPath&, bool, bool, const WebIDBTransaction&, WebExceptionCode&)
    {
        WEBKIT_ASSERT_NOT_REACHED();
        return 0;
    }
    virtual WebIDBIndex* createIndex(long long, const WebString&, const WebIDBKeyPath&, bool, bool, const WebIDBTransaction&, WebExceptionCode&)
    {
        WEBKIT_ASSERT_NOT_REACHED();
        return 0;
    }
    virtual void setIndexKeys(const WebIDBKey&, const WebVector<long long>&, const WebVector<WebIndexKeys>&, const WebIDBTransaction&) { WEBKIT_ASSERT_NOT_REACHED(); }
    virtual void setIndexesReady(const WebVector<long long>&, const WebIDBTransaction&) { WEBKIT_ASSERT_NOT_REACHED(); };
    // Transfers ownership of the WebIDBIndex to the caller.
    virtual WebIDBIndex* index(long long)
    {
        WEBKIT_ASSERT_NOT_REACHED();
        return 0;
    }
    virtual void deleteIndex(long long indexId, const WebIDBTransaction&, WebExceptionCode&) { WEBKIT_ASSERT_NOT_REACHED(); }
    virtual void openCursor(const WebIDBKeyRange&, WebIDBCursor::Direction direction, WebIDBCallbacks*, WebIDBTransaction::TaskType, const WebIDBTransaction&, WebExceptionCode&) { WEBKIT_ASSERT_NOT_REACHED(); }
    virtual void count(const WebIDBKeyRange&, WebIDBCallbacks*, const WebIDBTransaction&, WebExceptionCode&) { WEBKIT_ASSERT_NOT_REACHED(); }

protected:
    WebIDBObjectStore() {}
};

} // namespace WebKit

#endif // WebIDBObjectStore_h

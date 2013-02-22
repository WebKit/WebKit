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

#include "../../../Platform/chromium/public/WebCommon.h"
#include "WebDOMStringList.h"
#include "WebExceptionCode.h"
#include "WebIDBMetadata.h"

namespace WebKit {

class WebData;
class WebFrame;
class WebIDBCallbacks;
class WebIDBDatabaseCallbacks;
class WebIDBDatabaseError;
class WebIDBKey;
class WebIDBKeyPath;
class WebIDBKeyRange;

// See comment in WebIDBFactory for a high level overview of these classes.
class WebIDBDatabase {
public:
    virtual ~WebIDBDatabase() { }

    virtual void createObjectStore(long long transactionId, long long objectStoreId, const WebString& name, const WebIDBKeyPath&, bool autoIncrement) { WEBKIT_ASSERT_NOT_REACHED(); }
    virtual void deleteObjectStore(long long transactionId, long long objectStoreId) { WEBKIT_ASSERT_NOT_REACHED(); }
    virtual void createTransaction(long long id, WebIDBDatabaseCallbacks* callbacks, const WebVector<long long>&, unsigned short mode) { WEBKIT_ASSERT_NOT_REACHED(); }
    virtual void close() { WEBKIT_ASSERT_NOT_REACHED(); }
    virtual void forceClose() { WEBKIT_ASSERT_NOT_REACHED(); }

    virtual void abort(long long transactionId) { WEBKIT_ASSERT_NOT_REACHED(); }
    virtual void abort(long long transactionId, const WebIDBDatabaseError&) { WEBKIT_ASSERT_NOT_REACHED(); }
    virtual void commit(long long transactionId) { WEBKIT_ASSERT_NOT_REACHED(); }

    virtual void createIndex(long long transactionId, long long objectStoreId, long long indexId, const WebString& name, const WebIDBKeyPath&, bool unique, bool multiEntry) { WEBKIT_ASSERT_NOT_REACHED(); }
    virtual void deleteIndex(long long transactionId, long long objectStoreId, long long indexId) { WEBKIT_ASSERT_NOT_REACHED(); }

    enum TaskType {
        NormalTask = 0,
        PreemptiveTask
    };

    enum PutMode {
        AddOrUpdate,
        AddOnly,
        CursorUpdate
    };

    typedef WebVector<WebIDBKey> WebIndexKeys;

    virtual void get(long long transactionId, long long objectStoreId, long long indexId, const WebIDBKeyRange&, bool keyOnly, WebIDBCallbacks*) { WEBKIT_ASSERT_NOT_REACHED(); }
    // Note that 'value' may be consumed/adopted by this call.
    virtual void put(long long transactionId, long long objectStoreId, const WebData& value, const WebIDBKey&, PutMode, WebIDBCallbacks*, const WebVector<long long>& indexIds, const WebVector<WebIndexKeys>&) { WEBKIT_ASSERT_NOT_REACHED(); }
    virtual void setIndexKeys(long long transactionId, long long objectStoreId, const WebIDBKey&, const WebVector<long long>& indexIds, const WebVector<WebIndexKeys>&) { WEBKIT_ASSERT_NOT_REACHED(); }
    virtual void setIndexesReady(long long transactionId, long long objectStoreId, const WebVector<long long>& indexIds) { WEBKIT_ASSERT_NOT_REACHED(); }
    virtual void openCursor(long long transactionId, long long objectStoreId, long long indexId, const WebIDBKeyRange&, unsigned short direction, bool keyOnly, TaskType, WebIDBCallbacks*) { WEBKIT_ASSERT_NOT_REACHED(); }
    virtual void count(long long transactionId, long long objectStoreId, long long indexId, const WebIDBKeyRange&, WebIDBCallbacks*) { WEBKIT_ASSERT_NOT_REACHED(); }
    virtual void deleteRange(long long transactionId, long long objectStoreId, const WebIDBKeyRange&, WebIDBCallbacks*) { WEBKIT_ASSERT_NOT_REACHED(); }
    virtual void clear(long long transactionId, long long objectStoreId, WebIDBCallbacks*) { WEBKIT_ASSERT_NOT_REACHED(); }

protected:
    WebIDBDatabase() { }
};

} // namespace WebKit

#endif // WebIDBDatabase_h

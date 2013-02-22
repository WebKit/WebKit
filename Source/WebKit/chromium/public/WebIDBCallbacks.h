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

#ifndef WebIDBCallbacks_h
#define WebIDBCallbacks_h

#include "../../../Platform/chromium/public/WebCommon.h"
#include "../../../Platform/chromium/public/WebVector.h"

namespace WebKit {

class WebDOMStringList;
class WebData;
class WebIDBCursor;
class WebIDBDatabase;
class WebIDBDatabaseError;
class WebIDBIndex;
class WebIDBKey;
class WebIDBKeyPath;
struct WebIDBMetadata;

class WebIDBCallbacks {
public:
    virtual ~WebIDBCallbacks() { }

    // For classes that follow the PImpl pattern, pass a const reference.
    // For the rest, pass ownership to the callee via a pointer.
    virtual void onError(const WebIDBDatabaseError&) { WEBKIT_ASSERT_NOT_REACHED(); }
    virtual void onSuccess(const WebDOMStringList&) { WEBKIT_ASSERT_NOT_REACHED(); }
    virtual void onSuccess(WebIDBCursor*, const WebIDBKey& key, const WebIDBKey& primaryKey, const WebData&) { WEBKIT_ASSERT_NOT_REACHED(); }
    virtual void onSuccess(WebIDBDatabase*) { WEBKIT_ASSERT_NOT_REACHED(); }
    virtual void onSuccess(WebIDBDatabase*, const WebIDBMetadata&) { WEBKIT_ASSERT_NOT_REACHED(); }
    virtual void onSuccess(const WebIDBKey&) { WEBKIT_ASSERT_NOT_REACHED(); }
    virtual void onSuccess(const WebData&) { WEBKIT_ASSERT_NOT_REACHED(); }
    virtual void onSuccess(const WebData&, const WebIDBKey&, const WebIDBKeyPath&) { WEBKIT_ASSERT_NOT_REACHED(); }
    virtual void onSuccess(long long) { WEBKIT_ASSERT_NOT_REACHED(); }
    virtual void onSuccess() { WEBKIT_ASSERT_NOT_REACHED(); }
    virtual void onSuccess(const WebIDBKey& key, const WebIDBKey& primaryKey, const WebData&) { WEBKIT_ASSERT_NOT_REACHED(); }
    // FIXME: Remove the following overload once callers are updated:
    virtual void onBlocked() { WEBKIT_ASSERT_NOT_REACHED(); }
    virtual void onBlocked(long long oldVersion) { WEBKIT_ASSERT_NOT_REACHED(); }
    virtual void onSuccessWithPrefetch(const WebVector<WebIDBKey>& keys, const WebVector<WebIDBKey>& primaryKeys, const WebVector<WebData>& values) { WEBKIT_ASSERT_NOT_REACHED(); }
    virtual void onUpgradeNeeded(long long oldVersion, WebIDBDatabase*, const WebIDBMetadata&) { WEBKIT_ASSERT_NOT_REACHED(); }
};

} // namespace WebKit

#endif // WebIDBCallbacks_h

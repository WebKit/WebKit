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

#include "WebCommon.h"

namespace WebKit {

class WebIDBCursor;
class WebIDBDatabase;
class WebIDBDatabaseError;
class WebIDBKey;
class WebIDBIndex;
class WebIDBObjectStore;
class WebIDBTransaction;
class WebSerializedScriptValue;

class WebIDBCallbacks {
public:
    virtual ~WebIDBCallbacks() { }

    // For classes that follow the PImpl pattern, pass a const reference.
    // For the rest, pass ownership to the callee via a pointer.
    virtual void onError(const WebIDBDatabaseError&) { WEBKIT_ASSERT_NOT_REACHED(); }
    virtual void onSuccess() { WEBKIT_ASSERT_NOT_REACHED(); } // For "null".
    virtual void onSuccess(WebIDBCursor*) { WEBKIT_ASSERT_NOT_REACHED(); }
    virtual void onSuccess(WebIDBDatabase*) { WEBKIT_ASSERT_NOT_REACHED(); }
    virtual void onSuccess(const WebIDBKey&) { WEBKIT_ASSERT_NOT_REACHED(); }
    virtual void onSuccess(WebIDBIndex*) { WEBKIT_ASSERT_NOT_REACHED(); }
    virtual void onSuccess(WebIDBObjectStore*) { WEBKIT_ASSERT_NOT_REACHED(); }
    virtual void onSuccess(WebIDBTransaction*) { WEBKIT_ASSERT_NOT_REACHED(); }
    virtual void onSuccess(const WebSerializedScriptValue&) { WEBKIT_ASSERT_NOT_REACHED(); }
};

} // namespace WebKit

#endif // WebIDBCallbacks_h

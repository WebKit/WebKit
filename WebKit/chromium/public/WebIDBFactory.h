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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
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

#ifndef WebIDBFactory_h
#define WebIDBFactory_h

#include "WebCommon.h"
#include "WebDOMStringList.h"
#include "WebIDBCallbacks.h"
#include "WebSecurityOrigin.h"
#include "WebString.h"
#include "WebVector.h"

namespace WebKit {

class WebFrame;
class WebIDBDatabase;
class WebSecurityOrigin;

// The entry point into the IndexedDatabase API.  These classes match their Foo and
// FooSync counterparts in the spec, but operate only in an async manner.
// http://dev.w3.org/2006/webapi/WebSimpleDB/
class WebIDBFactory {
public:
    WEBKIT_API static WebIDBFactory* create();

    virtual ~WebIDBFactory() { }

    // The WebKit implementation of open ignores the WebFrame* parameter.
    virtual void open(const WebString& name, const WebString& description, WebIDBCallbacks* callbacks, const WebSecurityOrigin& origin, WebFrame* webFrame, const WebString& dataDir)
    {
        open(name, description, callbacks, origin, webFrame);
    }
    // FIXME: Delete soon.  Compatability hack.
    virtual void open(const WebString& name, const WebString& description, WebIDBCallbacks* callbacks, const WebSecurityOrigin& origin, WebFrame* webFrame)
    {
        open(name, description, callbacks, origin, webFrame, "/tmp/temporary-indexed-db-files");
    }

    virtual void abortPendingTransactions(const WebVector<int>& pendingIDs) { WEBKIT_ASSERT_NOT_REACHED(); }

    // The file name that would be used for persisting a given indexed database on the file system.
    WEBKIT_API static WebString databaseFileName(const WebString& name, const WebSecurityOrigin&);
};

} // namespace WebKit

#endif // WebIDBFactory_h

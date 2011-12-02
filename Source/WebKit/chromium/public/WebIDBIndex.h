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

#include "WebExceptionCode.h"
#include "WebIDBTransaction.h"
#include "platform/WebString.h"

namespace WebKit {

class WebIDBCallbacks;
class WebIDBKey;
class WebIDBKeyRange;

// See comment in WebIDBFactory for a high level overview of these classes.
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
    virtual bool multiEntry() const
    {
        WEBKIT_ASSERT_NOT_REACHED();
        return false;
    }

    virtual void openObjectCursor(const WebIDBKeyRange&, unsigned short direction, WebIDBCallbacks*, const WebIDBTransaction&, WebExceptionCode&) { WEBKIT_ASSERT_NOT_REACHED(); }
    virtual void openKeyCursor(const WebIDBKeyRange&, unsigned short direction, WebIDBCallbacks*, const WebIDBTransaction&, WebExceptionCode&) { WEBKIT_ASSERT_NOT_REACHED(); }
    virtual void getObject(const WebIDBKey&, WebIDBCallbacks*, const WebIDBTransaction&, WebExceptionCode&) { WEBKIT_ASSERT_NOT_REACHED(); }
    virtual void getKey(const WebIDBKey&, WebIDBCallbacks*, const WebIDBTransaction&, WebExceptionCode&) { WEBKIT_ASSERT_NOT_REACHED(); }

protected:
    WebIDBIndex() {}
};

} // namespace WebKit

#endif // WebIDBIndex_h

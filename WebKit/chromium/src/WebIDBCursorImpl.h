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

#ifndef WebIDBCursorImpl_h
#define WebIDBCursorImpl_h

#include "WebCommon.h"
#include "WebIDBCursor.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>

namespace WebCore { class IDBCursorBackendInterface; }

namespace WebKit {

// See comment in WebIndexedObjectStore for a high level overview these classes.
class WebIDBCursorImpl : public WebIDBCursor {
public:
    WebIDBCursorImpl(WTF::PassRefPtr<WebCore::IDBCursorBackendInterface>);
    virtual ~WebIDBCursorImpl();

    virtual unsigned short direction() const;
    virtual WebIDBKey key() const;
    virtual void value(WebSerializedScriptValue&, WebIDBKey&) const;
    virtual void update(const WebSerializedScriptValue&, WebIDBCallbacks*);
    virtual void continueFunction(const WebIDBKey&, WebIDBCallbacks*);
    virtual void remove(WebIDBCallbacks*);

 private:
    WTF::RefPtr<WebCore::IDBCursorBackendInterface> m_idbCursorBackend;
};

} // namespace WebKit

#endif // WebIDBCursorImpl_h

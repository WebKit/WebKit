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

#ifndef WebIDBIndexImpl_h
#define WebIDBIndexImpl_h

#include "WebCommon.h"
#include "WebIDBIndex.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>

namespace WebCore { class IDBIndexBackendInterface; }

namespace WebKit {

// See comment in WebIndexedDatabase for a high level overview these classes.
class WebIDBIndexImpl : public WebIDBIndex {
public:
    WebIDBIndexImpl(WTF::PassRefPtr<WebCore::IDBIndexBackendInterface>);
    virtual ~WebIDBIndexImpl();

    virtual WebString name() const;
    virtual WebString storeName() const;
    virtual WebString keyPath() const;
    virtual bool unique() const;

    virtual void openObjectCursor(const WebIDBKeyRange&, unsigned short direction, WebIDBCallbacks*, const WebIDBTransaction&); 
    virtual void openCursor(const WebIDBKeyRange&, unsigned short direction, WebIDBCallbacks*, const WebIDBTransaction&);
    virtual void getObject(const WebIDBKey&, WebIDBCallbacks*, const WebIDBTransaction&);
    virtual void get(const WebIDBKey&, WebIDBCallbacks*, const WebIDBTransaction&);

private:
    WTF::RefPtr<WebCore::IDBIndexBackendInterface> m_backend;
};

} // namespace WebKit

#endif // WebIDBIndexImpl_h

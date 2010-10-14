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

#ifndef IDBCursorBackendProxy_h
#define IDBCursorBackendProxy_h

#if ENABLE(INDEXED_DATABASE)

#include "IDBCursorBackendInterface.h"
#include "WebIDBCursor.h"
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class IDBCursorBackendProxy : public IDBCursorBackendInterface {
public:
    static PassRefPtr<IDBCursorBackendInterface> create(PassOwnPtr<WebKit::WebIDBCursor>);
    virtual ~IDBCursorBackendProxy();

    virtual unsigned short direction() const;
    virtual PassRefPtr<IDBKey> key() const;
    virtual PassRefPtr<IDBAny> value() const;
    virtual void update(PassRefPtr<SerializedScriptValue>, PassRefPtr<IDBCallbacks>, ExceptionCode&);
    virtual void continueFunction(PassRefPtr<IDBKey>, PassRefPtr<IDBCallbacks>, ExceptionCode&);
    virtual void remove(PassRefPtr<IDBCallbacks>, ExceptionCode&);

private:
    IDBCursorBackendProxy(PassOwnPtr<WebKit::WebIDBCursor>);

    OwnPtr<WebKit::WebIDBCursor> m_idbCursor;
};

} // namespace WebCore

#endif

#endif // IDBCursorBackendProxy_h

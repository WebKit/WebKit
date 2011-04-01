/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

namespace WebKit {

class IDBCursorBackendProxy : public WebCore::IDBCursorBackendInterface {
public:
    static PassRefPtr<WebCore::IDBCursorBackendInterface> create(PassOwnPtr<WebIDBCursor>);
    virtual ~IDBCursorBackendProxy();

    virtual unsigned short direction() const;
    virtual PassRefPtr<WebCore::IDBKey> key() const;
    virtual PassRefPtr<WebCore::IDBKey> primaryKey() const;
    virtual PassRefPtr<WebCore::SerializedScriptValue> value() const;
    virtual void update(PassRefPtr<WebCore::SerializedScriptValue>, PassRefPtr<WebCore::IDBCallbacks>, WebCore::ExceptionCode&);
    virtual void continueFunction(PassRefPtr<WebCore::IDBKey>, PassRefPtr<WebCore::IDBCallbacks>, WebCore::ExceptionCode&);
    virtual void deleteFunction(PassRefPtr<WebCore::IDBCallbacks>, WebCore::ExceptionCode&);

private:
    IDBCursorBackendProxy(PassOwnPtr<WebIDBCursor>);

    OwnPtr<WebIDBCursor> m_idbCursor;
};

} // namespace WebKit

#endif

#endif // IDBCursorBackendProxy_h

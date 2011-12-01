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

#ifndef IDBTransactionBackendProxy_h
#define IDBTransactionBackendProxy_h

#if ENABLE(INDEXED_DATABASE)

#include "IDBTransactionBackendInterface.h"
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>

namespace WebKit {

class WebIDBTransaction;

class IDBTransactionBackendProxy : public WebCore::IDBTransactionBackendInterface {
public:
    static PassRefPtr<IDBTransactionBackendInterface> create(PassOwnPtr<WebIDBTransaction>);
    virtual ~IDBTransactionBackendProxy();

    virtual PassRefPtr<WebCore::IDBObjectStoreBackendInterface> objectStore(const String& name, WebCore::ExceptionCode&);
    virtual unsigned short mode() const;
    virtual void abort();
    virtual bool scheduleTask(PassOwnPtr<WebCore::ScriptExecutionContext::Task>, PassOwnPtr<WebCore::ScriptExecutionContext::Task>);
    virtual void didCompleteTaskEvents();
    virtual void setCallbacks(WebCore::IDBTransactionCallbacks*);
    virtual void registerOpenCursor(WebCore::IDBCursorBackendImpl*);
    virtual void unregisterOpenCursor(WebCore::IDBCursorBackendImpl*);
    virtual void addPendingEvents(int) { ASSERT_NOT_REACHED(); }

    WebIDBTransaction* getWebIDBTransaction() const { return m_webIDBTransaction.get(); }

private:
    IDBTransactionBackendProxy(PassOwnPtr<WebIDBTransaction>);

    OwnPtr<WebIDBTransaction> m_webIDBTransaction;
};

} // namespace WebKit

#endif

#endif // IDBTransactionBackendProxy_h

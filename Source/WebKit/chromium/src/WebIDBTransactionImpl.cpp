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

#include "config.h"
#include "WebIDBTransactionImpl.h"

#if ENABLE(INDEXED_DATABASE)

#include "IDBTransaction.h"
#include "IDBTransactionCallbacksProxy.h"
#include "WebIDBObjectStoreImpl.h"
#include "WebIDBTransactionCallbacks.h"

using namespace WebCore;

namespace WebKit {

WebIDBTransactionImpl::WebIDBTransactionImpl(PassRefPtr<IDBTransactionBackendInterface> backend)
    : m_backend(backend)
{
}

WebIDBTransactionImpl::~WebIDBTransactionImpl()
{
}

int WebIDBTransactionImpl::mode() const
{
    return m_backend->mode();
}

WebIDBObjectStore* WebIDBTransactionImpl::objectStore(const WebString& name, ExceptionCode& ec)
{
    RefPtr<IDBObjectStoreBackendInterface> objectStore = m_backend->objectStore(name, ec);
    if (!objectStore)
        return 0;
    return new WebIDBObjectStoreImpl(objectStore);
}

void WebIDBTransactionImpl::abort()
{
    m_backend->abort();
}

void WebIDBTransactionImpl::didCompleteTaskEvents()
{
    m_backend->didCompleteTaskEvents();
}

void WebIDBTransactionImpl::setCallbacks(WebIDBTransactionCallbacks* callbacks)
{
    RefPtr<IDBTransactionCallbacks> idbCallbacks = IDBTransactionCallbacksProxy::create(adoptPtr(callbacks));
    m_backend->setCallbacks(idbCallbacks.get());
}

IDBTransactionBackendInterface* WebIDBTransactionImpl::getIDBTransactionBackendInterface() const
{
    return m_backend.get();
}

} // namespace WebKit

#endif // ENABLE(INDEXED_DATABASE)

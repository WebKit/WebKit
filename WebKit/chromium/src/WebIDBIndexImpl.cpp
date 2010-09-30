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
#include "WebIDBIndexImpl.h"

#include "IDBCallbacksProxy.h"
#include "IDBIndex.h"
#include "IDBKeyRange.h"
#include "WebIDBCallbacks.h"
#include "WebIDBKey.h"
#include "WebIDBKeyRange.h"

#if ENABLE(INDEXED_DATABASE)

using namespace WebCore;

namespace WebKit {

WebIDBIndexImpl::WebIDBIndexImpl(PassRefPtr<IDBIndexBackendInterface> backend)
    : m_backend(backend)
{
}

WebIDBIndexImpl::~WebIDBIndexImpl()
{
}

WebString WebIDBIndexImpl::name() const
{
    return m_backend->name();
}

WebString WebIDBIndexImpl::storeName() const
{
    return m_backend->storeName();
}

WebString WebIDBIndexImpl::keyPath() const
{
    return m_backend->keyPath();
}

bool WebIDBIndexImpl::unique() const
{
    return m_backend->unique();
}

void WebIDBIndexImpl::openCursor(const WebIDBKeyRange& keyRange, unsigned short direction, WebIDBCallbacks* callbacks, const WebIDBTransaction& transaction)
{
    m_backend->openCursor(keyRange, direction, IDBCallbacksProxy::create(callbacks), transaction.getIDBTransactionBackendInterface());
}

void WebIDBIndexImpl::openObjectCursor(const WebIDBKeyRange& keyRange, unsigned short direction, WebIDBCallbacks* callbacks, const WebIDBTransaction& transaction)
{
    m_backend->openObjectCursor(keyRange, direction, IDBCallbacksProxy::create(callbacks), transaction.getIDBTransactionBackendInterface());
}

void WebIDBIndexImpl::getObject(const WebIDBKey& keyRange, WebIDBCallbacks* callbacks, const WebIDBTransaction& transaction)
{
    m_backend->getObject(keyRange, IDBCallbacksProxy::create(callbacks), transaction.getIDBTransactionBackendInterface());
}

void WebIDBIndexImpl::get(const WebIDBKey& keyRange, WebIDBCallbacks* callbacks, const WebIDBTransaction& transaction)
{
    m_backend->get(keyRange, IDBCallbacksProxy::create(callbacks), transaction.getIDBTransactionBackendInterface());
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)

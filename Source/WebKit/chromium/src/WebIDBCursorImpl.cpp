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

#include "config.h"
#include "WebIDBCursorImpl.h"

#if ENABLE(INDEXED_DATABASE)

#include "IDBAny.h"
#include "IDBCallbacksProxy.h"
#include "IDBCursorBackendInterface.h"
#include "IDBKey.h"
#include "WebIDBKey.h"

using namespace WebCore;

namespace WebKit {

WebIDBCursorImpl::WebIDBCursorImpl(PassRefPtr<IDBCursorBackendInterface> idbCursorBackend)
    : m_idbCursorBackend(idbCursorBackend)
{
}

WebIDBCursorImpl::~WebIDBCursorImpl()
{
}

unsigned short WebIDBCursorImpl::direction() const
{
    return m_idbCursorBackend->direction();
}

WebIDBKey WebIDBCursorImpl::key() const
{
    return m_idbCursorBackend->key();
}

WebIDBKey WebIDBCursorImpl::primaryKey() const
{
    return m_idbCursorBackend->primaryKey();
}

WebSerializedScriptValue WebIDBCursorImpl::value() const
{
    return m_idbCursorBackend->value();
}

void WebIDBCursorImpl::update(const WebSerializedScriptValue& value, WebIDBCallbacks* callbacks, WebExceptionCode& ec)
{
    m_idbCursorBackend->update(value, IDBCallbacksProxy::create(adoptPtr(callbacks)), ec);
}

void WebIDBCursorImpl::advance(unsigned long count, WebIDBCallbacks* callbacks, WebExceptionCode& ec)
{
    m_idbCursorBackend->advance(count, IDBCallbacksProxy::create(adoptPtr(callbacks)), ec);
}

void WebIDBCursorImpl::continueFunction(const WebIDBKey& key, WebIDBCallbacks* callbacks, WebExceptionCode& ec)
{
    m_idbCursorBackend->continueFunction(key, IDBCallbacksProxy::create(adoptPtr(callbacks)), ec);
}

void WebIDBCursorImpl::deleteFunction(WebIDBCallbacks* callbacks, WebExceptionCode& ec)
{
    m_idbCursorBackend->deleteFunction(IDBCallbacksProxy::create(adoptPtr(callbacks)), ec);
}

void WebIDBCursorImpl::prefetchContinue(int numberToFetch, WebIDBCallbacks* callbacks, WebExceptionCode& ec)
{
    m_idbCursorBackend->prefetchContinue(numberToFetch, IDBCallbacksProxy::create(adoptPtr(callbacks)), ec);
}

void WebIDBCursorImpl::prefetchReset(int usedPrefetches, int unusedPrefetches)
{
    m_idbCursorBackend->prefetchReset(usedPrefetches, unusedPrefetches);
}

} // namespace WebKit

#endif // ENABLE(INDEXED_DATABASE)

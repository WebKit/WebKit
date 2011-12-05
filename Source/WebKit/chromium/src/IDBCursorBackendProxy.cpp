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
#include "IDBCursorBackendProxy.h"

#if ENABLE(INDEXED_DATABASE)

#include "IDBAny.h"
#include "IDBCallbacks.h"
#include "IDBKey.h"
#include "SerializedScriptValue.h"
#include "WebIDBCallbacksImpl.h"
#include "WebIDBKey.h"
#include "platform/WebSerializedScriptValue.h"

using namespace WebCore;

namespace WebKit {

PassRefPtr<IDBCursorBackendInterface> IDBCursorBackendProxy::create(PassOwnPtr<WebIDBCursor> idbCursor)
{
    return adoptRef(new IDBCursorBackendProxy(idbCursor));
}

IDBCursorBackendProxy::IDBCursorBackendProxy(PassOwnPtr<WebIDBCursor> idbCursor)
    : m_idbCursor(idbCursor)
{
}

IDBCursorBackendProxy::~IDBCursorBackendProxy()
{
}

unsigned short IDBCursorBackendProxy::direction() const
{
    return m_idbCursor->direction();
}

PassRefPtr<IDBKey> IDBCursorBackendProxy::key() const
{
    return m_idbCursor->key();
}

PassRefPtr<IDBKey> IDBCursorBackendProxy::primaryKey() const
{
    return m_idbCursor->primaryKey();
}

PassRefPtr<SerializedScriptValue> IDBCursorBackendProxy::value() const
{
    return m_idbCursor->value();
}

void IDBCursorBackendProxy::update(PassRefPtr<SerializedScriptValue> value, PassRefPtr<IDBCallbacks> callbacks, ExceptionCode& ec)
{
    m_idbCursor->update(value, new WebIDBCallbacksImpl(callbacks), ec);
}

void IDBCursorBackendProxy::continueFunction(PassRefPtr<IDBKey> key, PassRefPtr<IDBCallbacks> callbacks, ExceptionCode& ec)
{
    m_idbCursor->continueFunction(key, new WebIDBCallbacksImpl(callbacks), ec);
}

void IDBCursorBackendProxy::deleteFunction(PassRefPtr<IDBCallbacks> callbacks, ExceptionCode& ec)
{
    m_idbCursor->deleteFunction(new WebIDBCallbacksImpl(callbacks), ec);
}

void IDBCursorBackendProxy::postSuccessHandlerCallback()
{
    m_idbCursor->postSuccessHandlerCallback();
}

} // namespace WebKit

#endif // ENABLE(INDEXED_DATABASE)

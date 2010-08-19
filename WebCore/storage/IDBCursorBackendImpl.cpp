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
#include "IDBCursorBackendImpl.h"

#if ENABLE(INDEXED_DATABASE)

#include "IDBAny.h"
#include "IDBCallbacks.h"
#include "IDBKeyRange.h"
#include "IDBObjectStoreBackendImpl.h"
#include "IDBRequest.h"
#include "SQLiteDatabase.h"
#include "SerializedScriptValue.h"

namespace WebCore {

IDBCursorBackendImpl::IDBCursorBackendImpl(PassRefPtr<IDBObjectStoreBackendImpl> idbObjectStore, PassRefPtr<IDBKeyRange> keyRange, IDBCursor::Direction direction, PassRefPtr<IDBKey> key, PassRefPtr<SerializedScriptValue> value)
    : m_idbObjectStore(idbObjectStore)
    , m_keyRange(keyRange)
    , m_direction(direction)
    , m_key(key)
    , m_value(IDBAny::create(value.get()))
{
}

IDBCursorBackendImpl::~IDBCursorBackendImpl()
{
}

unsigned short IDBCursorBackendImpl::direction() const
{
    return m_direction;
}

PassRefPtr<IDBKey> IDBCursorBackendImpl::key() const
{
    return m_key;
}

PassRefPtr<IDBAny> IDBCursorBackendImpl::value() const
{
    return m_value;
}

void IDBCursorBackendImpl::update(PassRefPtr<SerializedScriptValue>, PassRefPtr<IDBCallbacks>)
{
    // FIXME: Implement this method.
    ASSERT_NOT_REACHED();
}

void IDBCursorBackendImpl::continueFunction(PassRefPtr<IDBKey>, PassRefPtr<IDBCallbacks>)
{
    // FIXME: Implement this method.
    ASSERT_NOT_REACHED();
}

void IDBCursorBackendImpl::remove(PassRefPtr<IDBCallbacks>)
{
    // FIXME: Implement this method.
    ASSERT_NOT_REACHED();
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)

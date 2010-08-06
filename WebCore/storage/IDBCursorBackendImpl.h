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


#ifndef IDBCursorBackendImpl_h
#define IDBCursorBackendImpl_h

#if ENABLE(INDEXED_DATABASE)

#include "IDBCursor.h"
#include "IDBCursorBackendInterface.h"
#include <wtf/RefPtr.h>

namespace WebCore {

class IDBKeyRange;
class IDBObjectStoreBackendImpl;
class SerializedScriptValue;

class IDBCursorBackendImpl : public IDBCursorBackendInterface {
public:
    static PassRefPtr<IDBCursorBackendImpl> create(PassRefPtr<IDBObjectStoreBackendImpl> objectStore, PassRefPtr<IDBKeyRange> keyRange, IDBCursor::Direction direction, PassRefPtr<IDBKey> key, PassRefPtr<SerializedScriptValue> value)
    {
        return adoptRef(new IDBCursorBackendImpl(objectStore, keyRange, direction, key, value));
    }
    virtual ~IDBCursorBackendImpl();

    virtual unsigned short direction() const;
    virtual PassRefPtr<IDBKey> key() const;
    virtual PassRefPtr<IDBAny> value() const;
    virtual void update(PassRefPtr<SerializedScriptValue>, PassRefPtr<IDBCallbacks>);
    virtual void continueFunction(PassRefPtr<IDBKey>, PassRefPtr<IDBCallbacks>);
    virtual void remove(PassRefPtr<IDBCallbacks>);

private:
    IDBCursorBackendImpl(PassRefPtr<IDBObjectStoreBackendImpl>, PassRefPtr<IDBKeyRange>, IDBCursor::Direction, PassRefPtr<IDBKey>, PassRefPtr<SerializedScriptValue>);

    RefPtr<IDBObjectStoreBackendImpl> m_idbObjectStore;
    RefPtr<IDBKeyRange> m_keyRange;
    IDBCursor::Direction m_direction;
    RefPtr<IDBKey> m_key;
    RefPtr<IDBAny> m_value;
};

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)

#endif // IDBCursorBackendImpl_h

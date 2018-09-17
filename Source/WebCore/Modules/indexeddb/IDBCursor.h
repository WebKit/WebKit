/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(INDEXED_DATABASE)

#include "ExceptionOr.h"
#include "IDBCursorDirection.h"
#include "IDBCursorInfo.h"
#include <JavaScriptCore/Strong.h>
#include <wtf/Variant.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class IDBGetResult;
class IDBIndex;
class IDBObjectStore;
class IDBTransaction;

class IDBCursor : public ScriptWrappable, public RefCounted<IDBCursor> {
public:
    static Ref<IDBCursor> create(IDBObjectStore&, const IDBCursorInfo&);
    static Ref<IDBCursor> create(IDBIndex&, const IDBCursorInfo&);
    
    virtual ~IDBCursor();

    using Source = Variant<RefPtr<IDBObjectStore>, RefPtr<IDBIndex>>;

    const Source& source() const;
    IDBCursorDirection direction() const;
    JSC::JSValue key() const;
    JSC::JSValue primaryKey() const;
    JSC::JSValue value() const;

    ExceptionOr<Ref<IDBRequest>> update(JSC::ExecState&, JSC::JSValue);
    ExceptionOr<void> advance(unsigned);
    ExceptionOr<void> continueFunction(JSC::ExecState&, JSC::JSValue key);
    ExceptionOr<void> continuePrimaryKey(JSC::ExecState&, JSC::JSValue key, JSC::JSValue primaryKey);
    ExceptionOr<Ref<IDBRequest>> deleteFunction(JSC::ExecState&);

    ExceptionOr<void> continueFunction(const IDBKeyData&);

    const IDBCursorInfo& info() const { return m_info; }

    void setRequest(IDBRequest& request) { m_request = makeWeakPtr(&request); }
    void clearRequest() { m_request.clear(); }
    IDBRequest* request() { return m_request.get(); }

    void setGetResult(IDBRequest&, const IDBGetResult&);

    virtual bool isKeyCursorWithValue() const { return false; }

protected:
    IDBCursor(IDBObjectStore&, const IDBCursorInfo&);
    IDBCursor(IDBIndex&, const IDBCursorInfo&);

private:
    bool sourcesDeleted() const;
    IDBObjectStore& effectiveObjectStore() const;
    IDBTransaction& transaction() const;

    void uncheckedIterateCursor(const IDBKeyData&, unsigned count);
    void uncheckedIterateCursor(const IDBKeyData&, const IDBKeyData&);

    IDBCursorInfo m_info;
    Source m_source;
    WeakPtr<IDBRequest> m_request;

    bool m_gotValue { false };

    IDBKeyData m_currentKeyData;
    IDBKeyData m_currentPrimaryKeyData;

    // FIXME: The following uses of JSC::Strong are incorrect and can lead to storage leaks
    // due to reference cycles; we should use JSValueInWrappedObject instead.
    JSC::Strong<JSC::Unknown> m_currentKey;
    JSC::Strong<JSC::Unknown> m_currentPrimaryKey;
    JSC::Strong<JSC::Unknown> m_currentValue;
};


inline const IDBCursor::Source& IDBCursor::source() const
{
    return m_source;
}

inline IDBCursorDirection IDBCursor::direction() const
{
    return m_info.cursorDirection();
}

inline JSC::JSValue IDBCursor::key() const
{
    return m_currentKey.get();
}

inline JSC::JSValue IDBCursor::primaryKey() const
{
    return m_currentPrimaryKey.get();
}

inline JSC::JSValue IDBCursor::value() const
{
    return m_currentValue.get();
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)

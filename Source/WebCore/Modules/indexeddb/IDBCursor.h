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

#include "ActiveDOMObject.h"
#include "ExceptionOr.h"
#include "IDBCursorDirection.h"
#include "IDBCursorInfo.h"
#include <heap/Strong.h>
#include <wtf/Variant.h>

namespace WebCore {

class IDBGetResult;
class IDBIndex;
class IDBObjectStore;
class IDBTransaction;

class IDBCursor : public ScriptWrappable, public RefCounted<IDBCursor>, public ActiveDOMObject {
public:
    static Ref<IDBCursor> create(IDBTransaction&, IDBObjectStore&, const IDBCursorInfo&);
    static Ref<IDBCursor> create(IDBTransaction&, IDBIndex&, const IDBCursorInfo&);
    
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

    void setRequest(IDBRequest& request) { m_request = &request; }
    void clearRequest() { m_request = nullptr; }
    IDBRequest* request() { return m_request; }

    void setGetResult(IDBRequest&, const IDBGetResult&);

    virtual bool isKeyCursorWithValue() const { return false; }

    void decrementOutstandingRequestCount();

    bool hasPendingActivity() const final;

protected:
    IDBCursor(IDBTransaction&, IDBObjectStore&, const IDBCursorInfo&);
    IDBCursor(IDBTransaction&, IDBIndex&, const IDBCursorInfo&);

private:
    const char* activeDOMObjectName() const final;
    bool canSuspendForDocumentSuspension() const final;

    bool sourcesDeleted() const;
    IDBObjectStore& effectiveObjectStore() const;
    IDBTransaction& transaction() const;

    void uncheckedIterateCursor(const IDBKeyData&, unsigned count);
    void uncheckedIterateCursor(const IDBKeyData&, const IDBKeyData&);

    // Cursors are created with an outstanding iteration request.
    unsigned m_outstandingRequestCount { 1 };

    IDBCursorInfo m_info;
    Source m_source;
    IDBRequest* m_request { nullptr };

    bool m_gotValue { false };

    IDBKeyData m_currentKeyData;
    IDBKeyData m_currentPrimaryKeyData;

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

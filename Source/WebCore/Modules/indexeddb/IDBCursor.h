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
#include "ExceptionCode.h"
#include "IDBAny.h"
#include "IDBCursorInfo.h"
#include <runtime/JSCJSValue.h>

namespace WebCore {

class IDBAny;
class IDBGetResult;
class IDBIndex;
class IDBObjectStore;
class IDBTransaction;

class IDBCursor : public ScriptWrappable, public RefCounted<IDBCursor>, public ActiveDOMObject {
public:
    static Ref<IDBCursor> create(IDBTransaction&, IDBIndex&, const IDBCursorInfo&);

    static const AtomicString& directionNext();
    static const AtomicString& directionNextUnique();
    static const AtomicString& directionPrev();
    static const AtomicString& directionPrevUnique();

    static IndexedDB::CursorDirection stringToDirection(const String& modeString, ExceptionCode&);
    static const AtomicString& directionToString(IndexedDB::CursorDirection mode);
    
    virtual ~IDBCursor();

    // Implement the IDL
    const String& direction() const;
    JSC::JSValue key(JSC::ExecState&) const;
    JSC::JSValue primaryKey(JSC::ExecState&) const;
    JSC::JSValue value(JSC::ExecState&) const;
    IDBAny* source();

    RefPtr<WebCore::IDBRequest> update(JSC::ExecState&, Deprecated::ScriptValue&, ExceptionCodeWithMessage&);
    void advance(unsigned long, ExceptionCodeWithMessage&);
    void continueFunction(ScriptExecutionContext&, ExceptionCodeWithMessage&);
    void continueFunction(ScriptExecutionContext&, const Deprecated::ScriptValue& key, ExceptionCodeWithMessage&);
    RefPtr<WebCore::IDBRequest> deleteFunction(ScriptExecutionContext&, ExceptionCodeWithMessage&);

    void continueFunction(const IDBKeyData&, ExceptionCodeWithMessage&);

    const IDBCursorInfo& info() const { return m_info; }

    void setRequest(IDBRequest& request) { m_request = &request; }
    void clearRequest() { m_request = nullptr; }
    IDBRequest* request() { return m_request; }

    void setGetResult(IDBRequest&, const IDBGetResult&);

    virtual bool isKeyCursor() const { return true; }

    void decrementOutstandingRequestCount();

    // ActiveDOMObject.
    const char* activeDOMObjectName() const;
    bool canSuspendForDocumentSuspension() const;
    bool hasPendingActivity() const;

protected:
    IDBCursor(IDBTransaction&, IDBObjectStore&, const IDBCursorInfo&);
    IDBCursor(IDBTransaction&, IDBIndex&, const IDBCursorInfo&);

private:
    // Cursors are created with an outstanding iteration request.
    unsigned m_outstandingRequestCount { 1 };

    IDBCursorInfo m_info;
    Ref<IDBAny> m_source;
    IDBObjectStore* m_objectStore { nullptr };
    IDBIndex* m_index { nullptr };
    IDBRequest* m_request;

    bool sourcesDeleted() const;
    IDBObjectStore& effectiveObjectStore() const;
    IDBTransaction& transaction() const;

    void uncheckedIterateCursor(const IDBKeyData&, unsigned long count);

    bool m_gotValue { false };

    IDBKeyData m_currentKeyData;
    IDBKeyData m_currentPrimaryKeyData;

    JSC::JSValue m_currentKey;
    JSC::JSValue m_currentPrimaryKey;
    JSC::JSValue m_currentValue;
};

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)

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

#ifndef IDBCursorImpl_h
#define IDBCursorImpl_h

#if ENABLE(INDEXED_DATABASE)

#include "IDBAnyImpl.h"
#include "IDBCursorInfo.h"
#include "IDBCursorWithValue.h"

namespace WebCore {

class IDBGetResult;

namespace IDBClient {

class IDBIndex;
class IDBObjectStore;
class IDBTransaction;

class IDBCursor : public WebCore::IDBCursorWithValue {
public:
    static Ref<IDBCursor> create(IDBTransaction&, IDBIndex&, const IDBCursorInfo&);

    virtual ~IDBCursor();

    // Implement the IDL
    virtual const String& direction() const override final;
    virtual const Deprecated::ScriptValue& key() const override final;
    virtual const Deprecated::ScriptValue& primaryKey() const override final;
    virtual const Deprecated::ScriptValue& value() const override final;
    virtual IDBAny* source() override final;

    virtual RefPtr<WebCore::IDBRequest> update(JSC::ExecState&, Deprecated::ScriptValue&, ExceptionCodeWithMessage&) override final;
    virtual void advance(unsigned long, ExceptionCodeWithMessage&) override final;
    virtual void continueFunction(ScriptExecutionContext*, ExceptionCodeWithMessage&) override final;
    virtual void continueFunction(ScriptExecutionContext*, const Deprecated::ScriptValue& key, ExceptionCodeWithMessage&) override final;
    virtual RefPtr<WebCore::IDBRequest> deleteFunction(ScriptExecutionContext*, ExceptionCodeWithMessage&) override final;

    void continueFunction(const IDBKeyData&, ExceptionCodeWithMessage&);

    const IDBCursorInfo& info() const { return m_info; }

    void setRequest(IDBRequest& request) { m_request = &request; }
    void clearRequest() { m_request = nullptr; }
    IDBRequest* request() { return m_request; }

    void setGetResult(IDBRequest&, const IDBGetResult&);

    virtual bool isKeyCursor() const override { return true; }

protected:
    IDBCursor(IDBTransaction&, IDBObjectStore&, const IDBCursorInfo&);
    IDBCursor(IDBTransaction&, IDBIndex&, const IDBCursorInfo&);

private:
    IDBCursorInfo m_info;
    Ref<IDBAny> m_source;
    IDBObjectStore* m_objectStore { nullptr };
    IDBIndex* m_index { nullptr };
    IDBRequest* m_request;

    bool sourcesDeleted() const;
    IDBObjectStore& effectiveObjectStore() const;
    IDBTransaction& transaction() const;

    void uncheckedIteratorCursor(const IDBKeyData&, unsigned long count);

    bool m_gotValue { false };

    IDBKeyData m_currentPrimaryKeyData;

    // FIXME: When ditching Legacy IDB and combining this implementation with the abstract IDBCursor,
    // these Deprecated::ScriptValue members should be JSValues instead.
    Deprecated::ScriptValue m_deprecatedCurrentKey;
    Deprecated::ScriptValue m_deprecatedCurrentPrimaryKey;
    Deprecated::ScriptValue m_deprecatedCurrentValue;
};

} // namespace IDBClient
} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
#endif // IDBCursorImpl_h

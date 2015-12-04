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

#ifndef LegacyCursor_h
#define LegacyCursor_h

#if ENABLE(INDEXED_DATABASE)

#include "IDBCursorWithValue.h"
#include "IDBKey.h"
#include "IndexedDB.h"
#include "LegacyObjectStore.h"
#include "LegacyTransaction.h"
#include "ScriptWrappable.h"
#include <bindings/ScriptValue.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class DOMRequestState;
class IDBAny;
class IDBCallbacks;
class IDBCursorBackend;
class LegacyRequest;
class ScriptExecutionContext;

typedef int ExceptionCode;

class LegacyCursor : public IDBCursorWithValue {
public:
    static Ref<LegacyCursor> create(PassRefPtr<IDBCursorBackend>, IndexedDB::CursorDirection, LegacyRequest*, LegacyAny* source, LegacyTransaction*);
    virtual ~LegacyCursor();

    // Implement the IDL
    const String& direction() const override;
    const Deprecated::ScriptValue& key() const override;
    const Deprecated::ScriptValue& primaryKey() const override;
    const Deprecated::ScriptValue& value() const override;
    IDBAny* source() override;

    RefPtr<IDBRequest> update(JSC::ExecState&, Deprecated::ScriptValue&, ExceptionCodeWithMessage&) override;
    void advance(unsigned long, ExceptionCodeWithMessage&) override;
    // FIXME: Try to modify the code generator so this overload is unneeded.
    void continueFunction(ScriptExecutionContext*, ExceptionCodeWithMessage& ec) override { continueFunction(static_cast<IDBKey*>(nullptr), ec); }
    void continueFunction(ScriptExecutionContext*, const Deprecated::ScriptValue& key, ExceptionCodeWithMessage&) override;
    RefPtr<IDBRequest> deleteFunction(ScriptExecutionContext*, ExceptionCodeWithMessage&) override;

    void continueFunction(PassRefPtr<IDBKey>, ExceptionCodeWithMessage&);
    void postSuccessHandlerCallback();
    void close();
    void setValueReady(DOMRequestState*, PassRefPtr<IDBKey>, PassRefPtr<IDBKey> primaryKey, Deprecated::ScriptValue&);
    PassRefPtr<IDBKey> idbPrimaryKey() { return m_currentPrimaryKey; }

    virtual bool isKeyCursor() const override { return true; }

protected:
    LegacyCursor(PassRefPtr<IDBCursorBackend>, IndexedDB::CursorDirection, LegacyRequest*, LegacyAny* source, LegacyTransaction*);

private:
    PassRefPtr<LegacyObjectStore> effectiveObjectStore();

    RefPtr<IDBCursorBackend> m_backend;
    RefPtr<LegacyRequest> m_request;
    const IndexedDB::CursorDirection m_direction;
    RefPtr<LegacyAny> m_source;
    RefPtr<LegacyTransaction> m_transaction;
    LegacyTransaction::OpenCursorNotifier m_transactionNotifier;
    bool m_gotValue;
    // These values are held because m_backend may advance while they
    // are still valid for the current success handlers.
    Deprecated::ScriptValue m_currentKeyValue;
    Deprecated::ScriptValue m_currentPrimaryKeyValue;
    RefPtr<IDBKey> m_currentKey;
    RefPtr<IDBKey> m_currentPrimaryKey;
    Deprecated::ScriptValue m_currentValue;
};

} // namespace WebCore

#endif

#endif // LegacyCursor_h

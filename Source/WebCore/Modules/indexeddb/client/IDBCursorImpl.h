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

#include "IDBCursorWithValue.h"

namespace WebCore {
namespace IDBClient {

class IDBCursor : public WebCore::IDBCursorWithValue {
public:
    virtual ~IDBCursor();

    // Implement the IDL
    virtual const String& direction() const override final;
    virtual const Deprecated::ScriptValue& key() const override final;
    virtual const Deprecated::ScriptValue& primaryKey() const override final;
    virtual const Deprecated::ScriptValue& value() const override final;
    virtual IDBAny* source() const override final;

    virtual RefPtr<IDBRequest> update(JSC::ExecState&, Deprecated::ScriptValue&, ExceptionCode&) override final;
    virtual void advance(unsigned long, ExceptionCode&) override final;
    virtual void continueFunction(ScriptExecutionContext*, ExceptionCode&) override final;
    virtual void continueFunction(ScriptExecutionContext*, const Deprecated::ScriptValue& key, ExceptionCode&) override final;
    virtual RefPtr<IDBRequest> deleteFunction(ScriptExecutionContext*, ExceptionCode&) override final;

protected:
    IDBCursor();
};

} // namespace IDBClient
} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
#endif // IDBCursorImpl_h

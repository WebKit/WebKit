/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(SQL_DATABASE)

#include "V8SQLTransaction.h"

#include "Database.h"
#include "ExceptionCode.h"
#include "SQLValue.h"
#include "V8Binding.h"
#include "V8SQLStatementCallback.h"
#include "V8SQLStatementErrorCallback.h"
#include "V8Proxy.h"
#include <wtf/Vector.h>

using namespace WTF;

namespace WebCore {

v8::Handle<v8::Value> V8SQLTransaction::executeSqlCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.SQLTransaction.executeSql()");

    if (args.Length() == 0)
        return setDOMException(SYNTAX_ERR, args.GetIsolate());

    STRING_TO_V8PARAMETER_EXCEPTION_BLOCK(V8Parameter<>, statement, args[0]);

    Vector<SQLValue> sqlValues;

    if (args.Length() > 1 && !isUndefinedOrNull(args[1])) {
        if (!args[1]->IsObject())
            return setDOMException(TYPE_MISMATCH_ERR, args.GetIsolate());

        uint32_t sqlArgsLength = 0;
        v8::Local<v8::Object> sqlArgsObject = args[1]->ToObject();
        EXCEPTION_BLOCK(v8::Local<v8::Value>, length, sqlArgsObject->Get(v8::String::New("length")));

        if (isUndefinedOrNull(length))
            sqlArgsLength = sqlArgsObject->GetPropertyNames()->Length();
        else
            sqlArgsLength = length->Uint32Value();

        for (unsigned int i = 0; i < sqlArgsLength; ++i) {
            v8::Handle<v8::Integer> key = v8Integer(i, args.GetIsolate());
            EXCEPTION_BLOCK(v8::Local<v8::Value>, value, sqlArgsObject->Get(key));

            if (value.IsEmpty() || value->IsNull())
                sqlValues.append(SQLValue());
            else if (value->IsNumber()) {
                EXCEPTION_BLOCK(double, sqlValue, value->NumberValue());
                sqlValues.append(SQLValue(sqlValue));
            } else {
                STRING_TO_V8PARAMETER_EXCEPTION_BLOCK(V8Parameter<>, sqlValue, value);
                sqlValues.append(SQLValue(sqlValue));
            }
        }
    }

    SQLTransaction* transaction = V8SQLTransaction::toNative(args.Holder());

    ScriptExecutionContext* scriptExecutionContext = getScriptExecutionContext();
    if (!scriptExecutionContext)
        return v8::Undefined();

    RefPtr<SQLStatementCallback> callback;
    if (args.Length() > 2 && !isUndefinedOrNull(args[2])) {
        if (!args[2]->IsObject())
            return setDOMException(TYPE_MISMATCH_ERR, args.GetIsolate());
        callback = V8SQLStatementCallback::create(args[2], scriptExecutionContext);
    }

    RefPtr<SQLStatementErrorCallback> errorCallback;
    if (args.Length() > 3 && !isUndefinedOrNull(args[3])) {
        if (!args[3]->IsObject())
            return setDOMException(TYPE_MISMATCH_ERR, args.GetIsolate());
        errorCallback = V8SQLStatementErrorCallback::create(args[3], scriptExecutionContext);
    }

    ExceptionCode ec = 0;
    transaction->executeSQL(statement, sqlValues, callback, errorCallback, ec);
    setDOMException(ec, args.GetIsolate());

    return v8::Undefined();
}

} // namespace WebCore

#endif

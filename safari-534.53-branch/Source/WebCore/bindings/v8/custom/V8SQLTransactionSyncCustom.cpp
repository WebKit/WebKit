/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#if ENABLE(DATABASE)

#include "V8SQLTransactionSync.h"

#include "DatabaseSync.h"
#include "ExceptionCode.h"
#include "SQLResultSet.h"
#include "SQLValue.h"
#include "V8Binding.h"
#include "V8BindingMacros.h"
#include "V8Proxy.h"
#include "V8SQLResultSet.h"
#include <wtf/Vector.h>

using namespace WTF;

namespace WebCore {

v8::Handle<v8::Value> V8SQLTransactionSync::executeSqlCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.SQLTransactionSync.executeSql()");

    if (!args.Length())
        return throwError(SYNTAX_ERR);

    STRING_TO_V8PARAMETER_EXCEPTION_BLOCK(V8Parameter<>, statement, args[0]);

    Vector<SQLValue> sqlValues;

    if (args.Length() > 1 && !isUndefinedOrNull(args[1])) {
        if (!args[1]->IsObject())
            return throwError(TYPE_MISMATCH_ERR);

        uint32_t sqlArgsLength = 0;
        v8::Local<v8::Object> sqlArgsObject = args[1]->ToObject();
        EXCEPTION_BLOCK(v8::Local<v8::Value>, length, sqlArgsObject->Get(v8::String::New("length")));

        if (isUndefinedOrNull(length))
            sqlArgsLength = sqlArgsObject->GetPropertyNames()->Length();
        else
            sqlArgsLength = length->Uint32Value();

        for (unsigned int i = 0; i < sqlArgsLength; ++i) {
            v8::Local<v8::Integer> key = v8::Integer::New(i);
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

    SQLTransactionSync* transaction = V8SQLTransactionSync::toNative(args.Holder());

    ExceptionCode ec = 0;
    v8::Handle<v8::Value> result = toV8(transaction->executeSQL(statement, sqlValues, ec));
    V8Proxy::setDOMException(ec);

    return result;
}

} // namespace WebCore

#endif

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

#if ENABLE(DATABASE)

#include "Database.h"
#include "SQLValue.h"
#include "V8Binding.h"
#include "V8CustomBinding.h"
#include "V8CustomSQLStatementCallback.h"
#include "V8CustomSQLStatementErrorCallback.h"
#include "V8Proxy.h"
#include <wtf/Vector.h>

using namespace WTF;

namespace WebCore {

CALLBACK_FUNC_DECL(SQLTransactionExecuteSql)
{
    INC_STATS("DOM.SQLTransaction.executeSql()");

    if (args.Length() == 0) {
        V8Proxy::throwError(V8Proxy::SyntaxError, "SQL statement is required.");
        return v8::Undefined();
    }

    String statement = toWebCoreString(args[0]);

    Vector<SQLValue> sqlValues;

    if (args.Length() > 1) {
        // FIXME: Make this work for v8::Arrayish objects, as well
        if (!args[1]->IsArray()) {
            V8Proxy::throwError(V8Proxy::TypeError, "Statement arguments must be an v8::Array.");
            return v8::Undefined();
        }

        v8::Local<v8::Array> arguments = v8::Local<v8::Array>::Cast(args[1]);
        uint32_t length = arguments->Length();

        for (unsigned int i = 0; i < length; ++i) {
            v8::Local<v8::Value> value = arguments->Get(v8::Integer::New(i));
            if (value.IsEmpty() || value->IsNull())
                sqlValues.append(SQLValue());
            else if (value->IsNumber())
                sqlValues.append(SQLValue(value->NumberValue()));
            else
                sqlValues.append(SQLValue(toWebCoreString(value)));
        }
    }

    SQLTransaction* transaction = V8DOMWrapper::convertToNativeObject<SQLTransaction>(V8ClassIndex::SQLTRANSACTION, args.Holder());

    Frame* frame = V8Proxy::retrieveFrame();

    RefPtr<SQLStatementCallback> callback;
    if (args.Length() > 2) {
        if (!args[2]->IsObject()) {
            V8Proxy::throwError(V8Proxy::TypeError, "Statement callback must be of valid type.");
            return v8::Undefined();
        }

        if (frame)
            callback = V8CustomSQLStatementCallback::create(args[2], frame);
    }

    RefPtr<SQLStatementErrorCallback> errorCallback;
    if (args.Length() > 3) {
        if (!args[2]->IsObject()) {
            V8Proxy::throwError(V8Proxy::TypeError, "Statement error callback must be of valid type.");
            return v8::Undefined();
        }

        if (frame)
            errorCallback = V8CustomSQLStatementErrorCallback::create(args[3], frame);
    }

    ExceptionCode ec = 0;
    transaction->executeSQL(statement, sqlValues, callback, errorCallback, ec);
    V8Proxy::setDOMException(ec);

    return v8::Undefined();
}

} // namespace WebCore

#endif


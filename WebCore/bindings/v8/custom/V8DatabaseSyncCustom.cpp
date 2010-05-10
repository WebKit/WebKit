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
#include "V8DatabaseSync.h"

#include "DatabaseSync.h"
#include "V8Binding.h"
#include "V8BindingMacros.h"
#include "V8Proxy.h"
#include "V8SQLTransactionSyncCallback.h"

namespace WebCore {

v8::Handle<v8::Value> V8DatabaseSync::changeVersionCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.DatabaseSync.changeVersion()");

    if (args.Length() < 2)
        return throwError(SYNTAX_ERR);

    EXCEPTION_BLOCK(String, oldVersion, toWebCoreString(args[0]));
    EXCEPTION_BLOCK(String, newVersion, toWebCoreString(args[1]));

    DatabaseSync* database = V8DatabaseSync::toNative(args.Holder());

    RefPtr<V8SQLTransactionSyncCallback> callback;
    if (args.Length() > 2) {
        if (!args[2]->IsObject())
            return throwError(TYPE_MISMATCH_ERR);

        callback = V8SQLTransactionSyncCallback::create(args[2], 0);
    }

    ExceptionCode ec = 0;
    database->changeVersion(oldVersion, newVersion, callback.release(), ec);
    V8Proxy::setDOMException(ec);

    return v8::Undefined();
}

static v8::Handle<v8::Value> createTransaction(const v8::Arguments& args, bool readOnly)
{
    if (!args.Length())
        return throwError(SYNTAX_ERR);

    if (!args[0]->IsObject())
        return throwError(TYPE_MISMATCH_ERR);

    DatabaseSync* database = V8DatabaseSync::toNative(args.Holder());

    RefPtr<V8SQLTransactionSyncCallback> callback = V8SQLTransactionSyncCallback::create(args[0], 0);

    ExceptionCode ec = 0;
    database->transaction(callback.release(), readOnly, ec);
    V8Proxy::setDOMException(ec);

    return v8::Undefined();
}

v8::Handle<v8::Value> V8DatabaseSync::transactionCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.DatabaseSync.transaction()");
    return createTransaction(args, false);
}

v8::Handle<v8::Value> V8DatabaseSync::readTransactionCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.DatabaseSync.readTransaction()");
    return createTransaction(args, true);
}

} // namespace WebCore

#endif

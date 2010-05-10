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
#include "V8Database.h"

#include "Database.h"
#include "V8Binding.h"
#include "V8BindingMacros.h"
#include "V8SQLTransactionCallback.h"
#include "V8SQLTransactionErrorCallback.h"
#include "V8CustomVoidCallback.h"
#include "V8Proxy.h"

namespace WebCore {

v8::Handle<v8::Value> V8Database::changeVersionCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.Database.changeVersion()");

    if (args.Length() < 2)
        return throwError(SYNTAX_ERR);

    EXCEPTION_BLOCK(String, oldVersion, toWebCoreString(args[0]));
    EXCEPTION_BLOCK(String, newVersion, toWebCoreString(args[1]));

    Database* database = V8Database::toNative(args.Holder());

    Frame* frame = V8Proxy::retrieveFrameForCurrentContext();
    if (!frame)
        return v8::Undefined();

    RefPtr<V8SQLTransactionCallback> callback;
    if (args.Length() > 2) {
        if (!args[2]->IsObject())
            return throwError(TYPE_MISMATCH_ERR);

        callback = V8SQLTransactionCallback::create(args[2], frame);
    }

    RefPtr<V8SQLTransactionErrorCallback> errorCallback;
    if (args.Length() > 3) {
        if (!args[3]->IsObject())
            return throwError(TYPE_MISMATCH_ERR);

        errorCallback = V8SQLTransactionErrorCallback::create(args[3], frame);
    }

    RefPtr<V8CustomVoidCallback> successCallback;
    if (args.Length() > 4) {
        if (!args[4]->IsObject())
            return throwError(TYPE_MISMATCH_ERR);

        successCallback = V8CustomVoidCallback::create(args[4], frame);
    }

    database->changeVersion(oldVersion, newVersion, callback.release(), errorCallback.release(), successCallback.release());

    return v8::Undefined();
}

static v8::Handle<v8::Value> createTransaction(const v8::Arguments& args, bool readOnly)
{
    if (!args.Length())
        return throwError(SYNTAX_ERR);

    if (!args[0]->IsObject())
        return throwError(TYPE_MISMATCH_ERR);

    Database* database = V8Database::toNative(args.Holder());

    Frame* frame = V8Proxy::retrieveFrameForCurrentContext();
    if (!frame)
        return v8::Undefined();

    RefPtr<V8SQLTransactionCallback> callback = V8SQLTransactionCallback::create(args[0], frame);

    RefPtr<V8SQLTransactionErrorCallback> errorCallback;
    if (args.Length() > 1 && !isUndefinedOrNull(args[1])) {
        if (!args[1]->IsObject())
            return throwError(TYPE_MISMATCH_ERR);

        errorCallback = V8SQLTransactionErrorCallback::create(args[1], frame);
    }

    RefPtr<V8CustomVoidCallback> successCallback;
    if (args.Length() > 2 && !isUndefinedOrNull(args[2])) {
        if (!args[2]->IsObject())
            return throwError(TYPE_MISMATCH_ERR);

        successCallback = V8CustomVoidCallback::create(args[2], frame);
    }

    database->transaction(callback.release(), errorCallback.release(), successCallback.release(), readOnly);
    return v8::Undefined();
}

v8::Handle<v8::Value> V8Database::transactionCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.Database.transaction()");
    return createTransaction(args, false);
}

v8::Handle<v8::Value> V8Database::readTransactionCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.Database.readTransaction()");
    return createTransaction(args, true);
}

} // namespace WebCore

#endif

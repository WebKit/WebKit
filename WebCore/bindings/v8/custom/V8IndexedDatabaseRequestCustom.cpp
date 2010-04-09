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

#if ENABLE(INDEXED_DATABASE)
#include "V8IndexedDatabaseRequest.h"

#include "IDBDatabaseError.h"
#include "IDBDatabaseRequest.h"
#include "V8Binding.h"
#include "V8CustomIDBCallbacks.h"
#include "V8IDBDatabaseError.h"
#include "V8IDBDatabaseRequest.h"
#include "V8Proxy.h"

namespace WebCore {

v8::Handle<v8::Value> V8IndexedDatabaseRequest::openCallback(const v8::Arguments& args)
{
    IndexedDatabaseRequest* imp = V8IndexedDatabaseRequest::toNative(args.Holder());
    if (args.Length() < 2)
        return throwError(V8Proxy::TypeError);
    V8Parameter<> name = args[0];
    V8Parameter<> description = args[1];

    bool modifyDatabase = true;
    if (args.Length() > 2 && !args[2]->IsUndefined() && !args[2]->IsNull())
        modifyDatabase = args[2]->BooleanValue();

    v8::Local<v8::Value> onError;
    v8::Local<v8::Value> onSuccess;
    if (args.Length() > 3 && !args[3]->IsUndefined() && !args[3]->IsNull()) {
        if (!args[3]->IsObject())
            return throwError("onerror callback was not the proper type");
        onError = args[3];
    }
    if (args.Length() > 4 && !args[4]->IsUndefined() && !args[4]->IsNull()) {
        if (!args[4]->IsObject())
            return throwError("onsuccess callback was not the proper type");
        onSuccess = args[4];
    }
    if (!onError->IsObject() && !onSuccess->IsObject())
        return throwError("Neither the onerror nor the onsuccess callbacks were set.");

    Frame* frame = V8Proxy::retrieveFrameForCurrentContext();
    RefPtr<V8CustomIDBCallbacks<IDBDatabase, IDBDatabaseRequest> > callbacks =
        V8CustomIDBCallbacks<IDBDatabase, IDBDatabaseRequest>::create(onSuccess, onError, frame->document());

    ExceptionCode ec = 0;
    imp->open(name, description, modifyDatabase, callbacks, ec);
    if (ec)
        return throwError(ec);
    return v8::Handle<v8::Value>();
}

} // namespace WebCore

#endif

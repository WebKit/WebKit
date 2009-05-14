/*
 * Copyright (c) 2009, Google Inc. All rights reserved.
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

#include "V8CustomSQLTransactionErrorCallback.h"

#include "Frame.h"
#include "V8CustomVoidCallback.h"

namespace WebCore {

V8CustomSQLTransactionErrorCallback::V8CustomSQLTransactionErrorCallback(v8::Local<v8::Object> callback, Frame* frame)
    : m_callback(v8::Persistent<v8::Object>::New(callback))
    , m_frame(frame)
{
}

V8CustomSQLTransactionErrorCallback::~V8CustomSQLTransactionErrorCallback()
{
    m_callback.Dispose();
}

bool V8CustomSQLTransactionErrorCallback::handleEvent(SQLError* error)
{
    v8::HandleScope handleScope;

    v8::Handle<v8::Context> context = V8Proxy::GetContext(m_frame.get());
    if (context.IsEmpty())
        return true;

    v8::Context::Scope scope(context);

    v8::Handle<v8::Value> argv[] = {
        V8Proxy::ToV8Object(V8ClassIndex::SQLERROR, error)
    };

    // Protect the frame until the callback returns.
    RefPtr<Frame> protector(m_frame);

    bool callbackReturnValue = false;
    invokeCallback(m_callback, 1, argv, callbackReturnValue);

    // FIXME: Eliminate return value once SQLTransactionErrorCallback is changed
    // to match the spec.
    return true;
}

} // namespace WebCore

#endif


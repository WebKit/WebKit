/*
 * Copyright 2009, The Android Open Source Project
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "V8CustomPositionCallback.h"

#if ENABLE(GEOLOCATION)

#include "ScriptExecutionContext.h"
#include "V8CustomVoidCallback.h"  // For invokeCallback
#include "V8Geoposition.h"
#include "V8Proxy.h"

namespace WebCore {

V8CustomPositionCallback::V8CustomPositionCallback(v8::Local<v8::Object> callback, ScriptExecutionContext* context)
    : PositionCallback(context)
    , m_callback(v8::Persistent<v8::Object>::New(callback))
{
}

V8CustomPositionCallback::~V8CustomPositionCallback()
{
    m_callback.Dispose();
}

void V8CustomPositionCallback::handleEvent(Geoposition* position)
{
    v8::HandleScope handleScope;

    // ActiveDOMObject will null our pointer to the ScriptExecutionContext when it goes away.
    ScriptExecutionContext* scriptContext = scriptExecutionContext();
    if (!scriptContext)
        return;

    // The lookup of the proxy will fail if the Frame has been detached.
    V8Proxy* proxy = V8Proxy::retrieve(scriptContext);
    if (!proxy)
        return;

    v8::Handle<v8::Context> context = proxy->context();
    if (context.IsEmpty())
        return;

    v8::Context::Scope scope(context);

    v8::Handle<v8::Value> argv[] = {
        toV8(position)
    };

    // Protect the script context until the callback returns.
    RefPtr<ScriptExecutionContext> protector(scriptContext);

    bool callbackReturnValue = false;
    invokeCallback(m_callback, 1, argv, callbackReturnValue, scriptContext);
}

} // namespace WebCore

#endif // ENABLE(GEOLOCATION)

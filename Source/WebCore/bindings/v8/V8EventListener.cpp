/*
 * Copyright (C) 2006, 2007, 2008, 2009 Google Inc. All rights reserved.
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
#include "V8EventListener.h"

#if PLATFORM(CHROMIUM)
#include "TraceEvent.h"
#endif

#include "Document.h"
#include "Frame.h"
#include "V8Proxy.h"

namespace WebCore {

V8EventListener::V8EventListener(v8::Local<v8::Object> listener, bool isAttribute, const WorldContextHandle& worldContext)
    : V8AbstractEventListener(isAttribute, worldContext)
{
    setListenerObject(listener);
}

v8::Local<v8::Function> V8EventListener::getListenerFunction(ScriptExecutionContext* context)
{
    v8::Local<v8::Object> listener = getListenerObject(context);

    // Has the listener been disposed?
    if (listener.IsEmpty())
        return v8::Local<v8::Function>();

    if (listener->IsFunction())
        return v8::Local<v8::Function>::Cast(listener);

    if (listener->IsObject()) {
        v8::Local<v8::Value> property = listener->Get(v8::String::NewSymbol("handleEvent"));
        // Check that no exceptions were thrown when getting the
        // handleEvent property and that the value is a function.
        if (!property.IsEmpty() && property->IsFunction())
            return v8::Local<v8::Function>::Cast(property);
    }

    return v8::Local<v8::Function>();
}

v8::Local<v8::Value> V8EventListener::callListenerFunction(ScriptExecutionContext* context, v8::Handle<v8::Value> jsEvent, Event* event)
{

    v8::Local<v8::Function> handlerFunction = getListenerFunction(context);
    v8::Local<v8::Object> receiver = getReceiverObject(event);
    if (handlerFunction.IsEmpty() || receiver.IsEmpty())
        return v8::Local<v8::Value>();

    v8::Handle<v8::Value> parameters[1] = { jsEvent };

    // FIXME: Can |context| be 0 here?
    if (!context)
        return v8::Local<v8::Value>();

    if (!context->isDocument())
        return v8::Local<v8::Value>();

    Frame* frame = static_cast<Document*>(context)->frame();
    if (!frame)
        return v8::Local<v8::Value>();

    if (!frame->script()->canExecuteScripts(AboutToExecuteScript))
        return v8::Local<v8::Value>();

    return frame->script()->callFunction(handlerFunction, receiver, 1, parameters);
}

} // namespace WebCore

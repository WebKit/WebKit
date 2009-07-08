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
#include "V8CustomEventListener.h"

#include "V8Proxy.h"

namespace WebCore {

V8EventListener::V8EventListener(Frame* frame, v8::Local<v8::Object> listener, bool isAttribute)
    : V8AbstractEventListener(frame, isAttribute)
{
    m_listener = v8::Persistent<v8::Object>::New(listener);
#ifndef NDEBUG
    V8GCController::registerGlobalHandle(EVENT_LISTENER, this, m_listener);
#endif
}

V8EventListener::~V8EventListener()
{
    if (m_frame) {
        V8Proxy* proxy = V8Proxy::retrieve(m_frame);
        if (proxy)
            proxy->removeV8EventListener(this);
    }

    disposeListenerObject();
}

v8::Local<v8::Function> V8EventListener::getListenerFunction()
{
    // Has the listener been disposed?
    if (m_listener.IsEmpty())
        return v8::Local<v8::Function>();

    if (m_listener->IsFunction())
        return v8::Local<v8::Function>::New(v8::Persistent<v8::Function>::Cast(m_listener));

    if (m_listener->IsObject()) {
        v8::Local<v8::Value> property = m_listener->Get(v8::String::NewSymbol("handleEvent"));
        if (property->IsFunction())
            return v8::Local<v8::Function>::Cast(property);
    }

    return v8::Local<v8::Function>();
}

v8::Local<v8::Value> V8EventListener::callListenerFunction(v8::Handle<v8::Value> jsEvent, Event* event, bool isWindowEvent)
{
    v8::Local<v8::Function> handlerFunction = getListenerFunction();
    v8::Local<v8::Object> receiver = getReceiverObject(event, isWindowEvent);
    if (handlerFunction.IsEmpty() || receiver.IsEmpty())
        return v8::Local<v8::Value>();

    v8::Handle<v8::Value> parameters[1] = { jsEvent };

    V8Proxy* proxy = V8Proxy::retrieve(m_frame);
    return proxy->callFunction(handlerFunction, receiver, 1, parameters);
}

} // namespace WebCore

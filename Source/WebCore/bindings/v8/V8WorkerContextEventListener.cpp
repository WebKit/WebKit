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

#if ENABLE(WORKERS)

#include "V8WorkerContextEventListener.h"

#include "V8Binding.h"
#include "V8DOMWrapper.h"
#include "V8Event.h"
#include "WorkerContext.h"
#include "WorkerContextExecutionProxy.h"

namespace WebCore {

static WorkerContextExecutionProxy* workerProxy(ScriptExecutionContext* context)
{
    ASSERT(context->isWorkerContext());
    WorkerContext* workerContext = static_cast<WorkerContext*>(context);
    return workerContext->script()->proxy();
}

V8WorkerContextEventListener::V8WorkerContextEventListener(v8::Local<v8::Object> listener, bool isInline, const WorldContextHandle& worldContext)
    : V8EventListener(listener, isInline, worldContext)
{
}

void V8WorkerContextEventListener::handleEvent(ScriptExecutionContext* context, Event* event)
{
    if (!context)
        return;

    // The callback function on XMLHttpRequest can clear the event listener and destroys 'this' object. Keep a local reference to it.
    // See issue 889829.
    RefPtr<V8AbstractEventListener> protect(this);

    v8::HandleScope handleScope;

    WorkerContextExecutionProxy* proxy = workerProxy(context);
    if (!proxy)
        return;

    v8::Handle<v8::Context> v8Context = proxy->context();
    if (v8Context.IsEmpty())
        return;

    // Enter the V8 context in which to perform the event handling.
    v8::Context::Scope scope(v8Context);

    // Get the V8 wrapper for the event object.
    v8::Handle<v8::Value> jsEvent = toV8(event);

    invokeEventHandler(context, event, jsEvent);
}

v8::Local<v8::Value> V8WorkerContextEventListener::callListenerFunction(ScriptExecutionContext* context, v8::Handle<v8::Value> jsEvent, Event* event)
{
    v8::Local<v8::Function> handlerFunction = getListenerFunction(context);
    v8::Local<v8::Object> receiver = getReceiverObject(context, event);
    if (handlerFunction.IsEmpty() || receiver.IsEmpty())
        return v8::Local<v8::Value>();

    v8::Handle<v8::Value> parameters[1] = { jsEvent };
    v8::Local<v8::Value> result = handlerFunction->Call(receiver, 1, parameters);

    if (WorkerContextExecutionProxy* proxy = workerProxy(context))
        proxy->trackEvent(event);

    return result;
}

v8::Local<v8::Object> V8WorkerContextEventListener::getReceiverObject(ScriptExecutionContext* context, Event* event)
{
    v8::Local<v8::Object> listener = getListenerObject(context);

    if (!listener.IsEmpty() && !listener->IsFunction())
        return listener;

    EventTarget* target = event->currentTarget();
    v8::Handle<v8::Value> value = V8DOMWrapper::convertEventTargetToV8Object(target);
    if (value.IsEmpty())
        return v8::Local<v8::Object>();
    return v8::Local<v8::Object>::New(v8::Handle<v8::Object>::Cast(value));
}

} // namespace WebCore

#endif // WORKERS

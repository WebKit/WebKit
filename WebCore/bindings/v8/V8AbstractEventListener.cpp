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
#include "V8AbstractEventListener.h"

#include "Document.h"
#include "Event.h"
#include "Frame.h"
#include "Tokenizer.h"
#include "V8Binding.h"

namespace WebCore {

V8AbstractEventListener::V8AbstractEventListener(Frame* frame, bool isAttribute)
    : m_isAttribute(isAttribute)
    , m_frame(frame)
    , m_lineNumber(0)
    , m_columnNumber(0)
{
    if (!m_frame)
        return;

    // Get the position in the source if any.
    if (m_isAttribute && m_frame->document()->tokenizer()) {
        m_lineNumber = m_frame->document()->tokenizer()->lineNumber();
        m_columnNumber = m_frame->document()->tokenizer()->columnNumber();
    }
}

void V8AbstractEventListener::invokeEventHandler(v8::Handle<v8::Context> v8Context, Event* event, v8::Handle<v8::Value> jsEvent, bool isWindowEvent)
{
    // We push the event being processed into the global object, so that it can be exposed by DOMWindow's bindings.
    v8::Local<v8::String> eventSymbol = v8::String::NewSymbol("event");
    v8::Local<v8::Value> returnValue;

    {
        // Catch exceptions thrown in the event handler so they do not propagate to javascript code that caused the event to fire.
        v8::TryCatch tryCatch;
        tryCatch.SetVerbose(true);

        // Save the old 'event' property so we can restore it later.
        v8::Local<v8::Value> savedEvent = v8Context->Global()->GetHiddenValue(eventSymbol);
        tryCatch.Reset();

        // Make the event available in the global object, so DOMWindow can expose it.
        v8Context->Global()->SetHiddenValue(eventSymbol, jsEvent);
        tryCatch.Reset();

        // Call the event handler.
        returnValue = callListenerFunction(jsEvent, event, isWindowEvent);
        tryCatch.Reset();

        // Restore the old event. This must be done for all exit paths through this method.
        if (savedEvent.IsEmpty())
            v8Context->Global()->SetHiddenValue(eventSymbol, v8::Undefined());
        else
            v8Context->Global()->SetHiddenValue(eventSymbol, savedEvent);
        tryCatch.Reset();
    }

    ASSERT(!V8Proxy::handleOutOfMemory() || returnValue.IsEmpty());

    if (returnValue.IsEmpty())
        return;

    if (!returnValue->IsNull() && !returnValue->IsUndefined() && event->storesResultAsString())
        event->storeResult(toWebCoreString(returnValue));

    // Prevent default action if the return value is false;
    // FIXME: Add example, and reference to bug entry.
    if (m_isAttribute && returnValue->IsBoolean() && !returnValue->BooleanValue())
        event->preventDefault();
}

void V8AbstractEventListener::handleEvent(Event* event, bool isWindowEvent)
{
    // EventListener could be disconnected from the frame.
    if (disconnected())
        return;

    // The callback function on XMLHttpRequest can clear the event listener and destroys 'this' object. Keep a local reference to it.
    // See issue 889829.
    RefPtr<V8AbstractEventListener> protect(this);

    v8::HandleScope handleScope;

    v8::Handle<v8::Context> v8Context = V8Proxy::context(m_frame);
    if (v8Context.IsEmpty())
        return;

    // m_frame can removed by the callback function, protect it until the callback function returns.
    RefPtr<Frame> protectFrame(m_frame);

    // Enter the V8 context in which to perform the event handling.
    v8::Context::Scope scope(v8Context);

    // Get the V8 wrapper for the event object.
    v8::Handle<v8::Value> jsEvent = V8DOMWrapper::convertEventToV8Object(event);

    invokeEventHandler(v8Context, event, jsEvent, isWindowEvent);

    Document::updateStyleForAllDocuments();
}

void V8AbstractEventListener::disposeListenerObject()
{
    if (!m_listener.IsEmpty()) {
#ifndef NDEBUG
        V8GCController::unregisterGlobalHandle(this, m_listener);
#endif
        m_listener.Dispose();
        m_listener.Clear();
    }
}

v8::Local<v8::Object> V8AbstractEventListener::getReceiverObject(Event* event, bool isWindowEvent)
{
    if (!m_listener.IsEmpty() && !m_listener->IsFunction())
        return v8::Local<v8::Object>::New(m_listener);

    if (isWindowEvent)
        return v8::Context::GetCurrent()->Global();

    EventTarget* target = event->currentTarget();
    v8::Handle<v8::Value> value = V8DOMWrapper::convertEventTargetToV8Object(target);
    if (value.IsEmpty())
        return v8::Local<v8::Object>();
    return v8::Local<v8::Object>::New(v8::Handle<v8::Object>::Cast(value));
}

} // namespace WebCore

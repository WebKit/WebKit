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
#include "ScriptEventListener.h"

#include "Attribute.h"
#include "Document.h"
#include "EventListener.h"
#include "Frame.h"
#include "ScriptScope.h"
#include "DocumentParser.h"
#include "V8AbstractEventListener.h"
#include "V8Binding.h"

namespace WebCore {

static const String& eventParameterName(bool isSVGEvent)
{
    DEFINE_STATIC_LOCAL(const String, eventString, ("event"));
    DEFINE_STATIC_LOCAL(const String, evtString, ("evt"));
    return isSVGEvent ? evtString : eventString;
}

PassRefPtr<V8LazyEventListener> createAttributeEventListener(Node* node, Attribute* attr)
{
    ASSERT(node);
    ASSERT(attr);
    if (attr->isNull())
        return 0;

    // FIXME: Very strange: we initialize zero-based number with '1'.
    TextPosition position(OrdinalNumber::fromZeroBasedInt(1), OrdinalNumber::first());
    String sourceURL;

    if (Frame* frame = node->document()->frame()) {
        ScriptController* scriptController = frame->script();
        if (!scriptController->canExecuteScripts(AboutToExecuteScript))
            return 0;
        position = scriptController->eventHandlerPosition();
        sourceURL = node->document()->url().string();
    }

    return V8LazyEventListener::create(attr->localName().string(), eventParameterName(node->isSVGElement()), attr->value(), sourceURL, position, node, WorldContextHandle(UseMainWorld));
}

PassRefPtr<V8LazyEventListener> createAttributeEventListener(Frame* frame, Attribute* attr)
{
    if (!frame)
        return 0;

    ASSERT(attr);
    if (attr->isNull())
        return 0;

    ScriptController* scriptController = frame->script();
    if (!scriptController->canExecuteScripts(AboutToExecuteScript))
        return 0;

    TextPosition position = scriptController->eventHandlerPosition();
    String sourceURL = frame->document()->url().string();

    return V8LazyEventListener::create(attr->localName().string(), eventParameterName(frame->document()->isSVGDocument()), attr->value(), sourceURL, position, 0, WorldContextHandle(UseMainWorld));
}

String eventListenerHandlerBody(Document* document, EventListener* listener)
{
    if (listener->type() != EventListener::JSEventListenerType)
        return "";

    v8::HandleScope scope;
    V8AbstractEventListener* v8Listener = static_cast<V8AbstractEventListener*>(listener);
    v8::Handle<v8::Context> context = toV8Context(document, v8Listener->worldContext());
    v8::Context::Scope contextScope(context);
    v8::Handle<v8::Object> function = v8Listener->getListenerObject(document);
    if (function.IsEmpty())
        return "";

    return toWebCoreStringWithNullCheck(function);
}

bool eventListenerHandlerLocation(Document* document, EventListener* listener, String& sourceName, int& lineNumber)
{
    if (listener->type() != EventListener::JSEventListenerType)
        return false;

    v8::HandleScope scope;
    V8AbstractEventListener* v8Listener = static_cast<V8AbstractEventListener*>(listener);
    v8::Handle<v8::Context> context = toV8Context(document, v8Listener->worldContext());
    v8::Context::Scope contextScope(context);
    v8::Handle<v8::Object> object = v8Listener->getListenerObject(document);
    if (object.IsEmpty() || !object->IsFunction())
        return false;

    v8::Handle<v8::Function> function = v8::Handle<v8::Function>::Cast(object);
    v8::ScriptOrigin origin = function->GetScriptOrigin();
    if (!origin.ResourceName().IsEmpty()) {
        sourceName = toWebCoreString(origin.ResourceName());
        lineNumber = function->GetScriptLineNumber() + 1;
        return true;
    }
    return false;
}

} // namespace WebCore

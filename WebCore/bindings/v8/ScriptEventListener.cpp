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
#include "Tokenizer.h"
#include "V8AbstractEventListener.h"
#include "V8Binding.h"
#include "XSSAuditor.h"

namespace WebCore {

PassRefPtr<V8LazyEventListener> createAttributeEventListener(Node* node, Attribute* attr)
{
    ASSERT(node);
    ASSERT(attr);
    if (attr->isNull())
        return 0;

    int lineNumber = 1;
    int columnNumber = 0;
    String sourceURL;

    if (Frame* frame = node->document()->frame()) {
        ScriptController* scriptController = frame->script();
        if (!scriptController->canExecuteScripts(AboutToExecuteScript))
            return 0;

        if (!scriptController->xssAuditor()->canCreateInlineEventListener(attr->localName().string(), attr->value())) {
            // This script is not safe to execute.
            return 0;
        }

        if (frame->document()->tokenizer()) {
            // FIXME: Change to use script->eventHandlerLineNumber() when implemented.
            lineNumber = frame->document()->tokenizer()->lineNumber();
            columnNumber = frame->document()->tokenizer()->columnNumber();
        }
        sourceURL = node->document()->url().string();
    }

    return V8LazyEventListener::create(attr->localName().string(), node->isSVGElement(), attr->value(), sourceURL, lineNumber, columnNumber, WorldContextHandle(UseMainWorld));
}

PassRefPtr<V8LazyEventListener> createAttributeEventListener(Frame* frame, Attribute* attr)
{
    if (!frame)
        return 0;

    ASSERT(attr);
    if (attr->isNull())
        return 0;

    int lineNumber = 1;
    int columnNumber = 0;
    String sourceURL;

    ScriptController* scriptController = frame->script();
    if (!scriptController->canExecuteScripts(AboutToExecuteScript))
        return 0;

    if (!scriptController->xssAuditor()->canCreateInlineEventListener(attr->localName().string(), attr->value())) {
        // This script is not safe to execute.
        return 0;
    }

    if (frame->document()->tokenizer()) {
        // FIXME: Change to use script->eventHandlerLineNumber() when implemented.
        lineNumber = frame->document()->tokenizer()->lineNumber();
        columnNumber = frame->document()->tokenizer()->columnNumber();
    }
    sourceURL = frame->document()->url().string();
    return V8LazyEventListener::create(attr->localName().string(), frame->document()->isSVGDocument(), attr->value(), sourceURL, lineNumber, columnNumber, WorldContextHandle(UseMainWorld));
}

String eventListenerHandlerBody(ScriptExecutionContext* context, ScriptState* scriptState, EventListener* listener)
{
    if (listener->type() != EventListener::JSEventListenerType)
        return "";

    ScriptScope scope(scriptState);
    V8AbstractEventListener* v8Listener = static_cast<V8AbstractEventListener*>(listener);
    v8::Handle<v8::Object> function = v8Listener->getListenerObject(context);
    if (function.IsEmpty())
        return "";

    return toWebCoreStringWithNullCheck(function);
}

bool eventListenerHandlerLocation(ScriptExecutionContext* context, ScriptState* scriptState, EventListener* listener, String& sourceName, int& lineNumber)
{
    if (listener->type() != EventListener::JSEventListenerType)
        return false;

    ScriptScope scope(scriptState);
    V8AbstractEventListener* v8Listener = static_cast<V8AbstractEventListener*>(listener);
    v8::Handle<v8::Object> object = v8Listener->getListenerObject(context);
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

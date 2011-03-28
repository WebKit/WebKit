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
#include "V8LazyEventListener.h"

#include "ContentSecurityPolicy.h"
#include "Frame.h"
#include "V8Binding.h"
#include "V8HiddenPropertyName.h"
#include "V8Proxy.h"
#include "WorldContextHandle.h"

#include <wtf/StdLibExtras.h>

namespace WebCore {

V8LazyEventListener::V8LazyEventListener(const String& functionName, bool isSVGEvent, const String& code, const String sourceURL, const TextPosition0& position, const WorldContextHandle& worldContext)
    : V8AbstractEventListener(true, worldContext)
    , m_functionName(functionName)
    , m_isSVGEvent(isSVGEvent)
    , m_code(code)
    , m_sourceURL(sourceURL)
    , m_position(position)
{
}

v8::Local<v8::Value> V8LazyEventListener::callListenerFunction(ScriptExecutionContext* context, v8::Handle<v8::Value> jsEvent, Event* event)
{
    v8::Local<v8::Object> listenerObject = getListenerObject(context);
    if (listenerObject.IsEmpty())
        return v8::Local<v8::Value>();

    v8::Local<v8::Function> handlerFunction = v8::Local<v8::Function>::Cast(listenerObject);
    v8::Local<v8::Object> receiver = getReceiverObject(event);
    if (handlerFunction.IsEmpty() || receiver.IsEmpty())
        return v8::Local<v8::Value>();

    v8::Handle<v8::Value> parameters[1] = { jsEvent };

    if (V8Proxy* proxy = V8Proxy::retrieve(context))
        return proxy->callFunction(handlerFunction, receiver, 1, parameters);

    return v8::Local<v8::Value>();
}

static v8::Handle<v8::Value> V8LazyEventListenerToString(const v8::Arguments& args)
{
    return args.Holder()->GetHiddenValue(V8HiddenPropertyName::toStringString());
}

void V8LazyEventListener::prepareListenerObject(ScriptExecutionContext* context)
{
    if (hasExistingListenerObject())
        return;

    if (context->isDocument() && !static_cast<Document*>(context)->contentSecurityPolicy()->allowInlineEventHandlers())
        return;

    v8::HandleScope handleScope;

    V8Proxy* proxy = V8Proxy::retrieve(context);
    if (!proxy)
        return;

    // Use the outer scope to hold context.
    v8::Local<v8::Context> v8Context = worldContext().adjustedContext(proxy);
    // Bail out if we cannot get the context.
    if (v8Context.IsEmpty())
        return;

    v8::Context::Scope scope(v8Context);

    // FIXME: cache the wrapper function.

    // Nodes other than the document object, when executing inline event handlers push document, form, and the target node on the scope chain.
    // We do this by using 'with' statement.
    // See chrome/fast/forms/form-action.html
    //     chrome/fast/forms/selected-index-value.html
    //     base/fast/overflow/onscroll-layer-self-destruct.html
    //
    // Don't use new lines so that lines in the modified handler
    // have the same numbers as in the original code.
    String code = "(function (evt) {" \
            "with (this.ownerDocument ? this.ownerDocument : {}) {" \
            "with (this.form ? this.form : {}) {" \
            "with (this) {" \
            "return (function(evt){";
    code.append(m_code);
    // Insert '\n' otherwise //-style comments could break the handler.
    code.append(  "\n}).call(this, evt);}}}})");
    v8::Handle<v8::String> codeExternalString = v8ExternalString(code);
    v8::Handle<v8::Script> script = V8Proxy::compileScript(codeExternalString, m_sourceURL, m_position);
    if (!script.IsEmpty()) {
        v8::Local<v8::Value> value = proxy->runScript(script, false);
        if (!value.IsEmpty()) {
            ASSERT(value->IsFunction());

            v8::Local<v8::Function> wrappedFunction = v8::Local<v8::Function>::Cast(value);

            // Change the toString function on the wrapper function to avoid it
            // returning the source for the actual wrapper function. Instead it
            // returns source for a clean wrapper function with the event
            // argument wrapping the event source code. The reason for this is
            // that some web sites use toString on event functions and eval the
            // source returned (sometimes a RegExp is applied as well) for some
            // other use. That fails miserably if the actual wrapper source is
            // returned.
            DEFINE_STATIC_LOCAL(v8::Persistent<v8::FunctionTemplate>, toStringTemplate, ());
            if (toStringTemplate.IsEmpty())
                toStringTemplate = v8::Persistent<v8::FunctionTemplate>::New(v8::FunctionTemplate::New(V8LazyEventListenerToString));
            v8::Local<v8::Function> toStringFunction;
            if (!toStringTemplate.IsEmpty())
                toStringFunction = toStringTemplate->GetFunction();
            if (!toStringFunction.IsEmpty()) {
                String toStringResult = "function ";
                toStringResult.append(m_functionName);
                toStringResult.append("(");
                toStringResult.append(m_isSVGEvent ? "evt" : "event");
                toStringResult.append(") {\n  ");
                toStringResult.append(m_code);
                toStringResult.append("\n}");
                wrappedFunction->SetHiddenValue(V8HiddenPropertyName::toStringString(), v8ExternalString(toStringResult));
                wrappedFunction->Set(v8::String::New("toString"), toStringFunction);
            }

            wrappedFunction->SetName(v8::String::New(fromWebCoreString(m_functionName), m_functionName.length()));

            setListenerObject(wrappedFunction);
        }
    }
}

} // namespace WebCore

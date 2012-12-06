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
#include "Document.h"
#include "Frame.h"
#include "HTMLElement.h"
#include "HTMLFormElement.h"
#include "Node.h"
#include "ScriptSourceCode.h"
#include "V8Binding.h"
#include "V8DOMWrapper.h"
#include "V8Document.h"
#include "V8HTMLFormElement.h"
#include "V8HiddenPropertyName.h"
#include "V8Node.h"
#include "V8RecursionScope.h"
#include "WorldContextHandle.h"

#include <wtf/StdLibExtras.h>

namespace WebCore {

V8LazyEventListener::V8LazyEventListener(const AtomicString& functionName, const AtomicString& eventParameterName, const String& code, const String sourceURL, const TextPosition& position, Node* node, const WorldContextHandle& worldContext)
    : V8AbstractEventListener(true, worldContext)
    , m_functionName(functionName)
    , m_eventParameterName(eventParameterName)
    , m_code(code)
    , m_sourceURL(sourceURL)
    , m_node(node)
    , m_position(position)
{
}

template<typename T>
v8::Handle<v8::Object> toObjectWrapper(T* domObject)
{
    if (!domObject)
        return v8::Object::New();
    v8::Handle<v8::Value> value = toV8(domObject);
    if (value.IsEmpty())
        return v8::Object::New();
    return value.As<v8::Object>();
}

v8::Local<v8::Value> V8LazyEventListener::callListenerFunction(ScriptExecutionContext* context, v8::Handle<v8::Value> jsEvent, Event* event)
{
    v8::Local<v8::Object> listenerObject = getListenerObject(context);
    if (listenerObject.IsEmpty())
        return v8::Local<v8::Value>();

    v8::Local<v8::Function> handlerFunction = listenerObject.As<v8::Function>();
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

static v8::Handle<v8::Value> V8LazyEventListenerToString(const v8::Arguments& args)
{
    return args.Holder()->GetHiddenValue(V8HiddenPropertyName::toStringString());
}

void V8LazyEventListener::prepareListenerObject(ScriptExecutionContext* context)
{
    if (hasExistingListenerObject())
        return;

    if (context->isDocument() && !static_cast<Document*>(context)->contentSecurityPolicy()->allowInlineEventHandlers(m_sourceURL, m_position.m_line))
        return;

    v8::HandleScope handleScope;

    ASSERT(context->isDocument());
    Frame* frame = static_cast<Document*>(context)->frame();
    ASSERT(frame);
    if (!frame->script()->canExecuteScripts(NotAboutToExecuteScript))
        return;
    // Use the outer scope to hold context.
    v8::Local<v8::Context> v8Context = toV8Context(context, worldContext());
    // Bail out if we cannot get the context.
    if (v8Context.IsEmpty())
        return;

    v8::Context::Scope scope(v8Context);

    // Nodes other than the document object, when executing inline event
    // handlers push document, form, and the target node on the scope chain.
    // We do this by using 'with' statement.
    // See chrome/fast/forms/form-action.html
    //     chrome/fast/forms/selected-index-value.html
    //     base/fast/overflow/onscroll-layer-self-destruct.html
    //
    // Don't use new lines so that lines in the modified handler
    // have the same numbers as in the original code.
    // FIXME: V8 does not allow us to programmatically create object environments so
    //        we have to do this hack! What if m_code escapes to run arbitrary script?
    //
    // Call with 4 arguments instead of 3, pass additional null as the last parameter.
    // By calling the function with 4 arguments, we create a setter on arguments object
    // which would shadow property "3" on the prototype.
    String code = "(function() {"
        "with (this[2]) {"
        "with (this[1]) {"
        "with (this[0]) {"
            "return function(" + m_eventParameterName + ") {" +
                m_code + "\n" // Insert '\n' otherwise //-style comments could break the handler.
            "};"
        "}}}})";

    v8::Handle<v8::String> codeExternalString = deprecatedV8String(code);

    v8::Handle<v8::Script> script = ScriptSourceCode::compileScript(codeExternalString, m_sourceURL, m_position);
    if (script.IsEmpty())
        return;

    // FIXME: Remove this code when we stop doing the 'with' hack above.
    v8::Local<v8::Value> value;
    {
        V8RecursionScope::MicrotaskSuppression scope;
        value = script->Run();
    }
    if (value.IsEmpty())
        return;

    // Call the outer function to get the inner function.
    ASSERT(value->IsFunction());
    v8::Local<v8::Function> intermediateFunction = value.As<v8::Function>();

    HTMLFormElement* formElement = 0;
    if (m_node && m_node->isHTMLElement())
        formElement = static_cast<HTMLElement*>(m_node)->form();

    v8::Handle<v8::Object> nodeWrapper = toObjectWrapper<Node>(m_node);
    v8::Handle<v8::Object> formWrapper = toObjectWrapper<HTMLFormElement>(formElement);
    v8::Handle<v8::Object> documentWrapper = toObjectWrapper<Document>(m_node ? m_node->ownerDocument() : 0);

    v8::Local<v8::Object> thisObject = v8::Object::New();
    if (thisObject.IsEmpty())
        return;
    if (!thisObject->ForceSet(deprecatedV8Integer(0), nodeWrapper))
        return;
    if (!thisObject->ForceSet(deprecatedV8Integer(1), formWrapper))
        return;
    if (!thisObject->ForceSet(deprecatedV8Integer(2), documentWrapper))
        return;

    // FIXME: Remove this code when we stop doing the 'with' hack above.
    v8::Local<v8::Value> innerValue;
    {
        V8RecursionScope::MicrotaskSuppression scope;
        innerValue = intermediateFunction->Call(thisObject, 0, 0);
    }
    if (innerValue.IsEmpty() || !innerValue->IsFunction())
        return;

    v8::Local<v8::Function> wrappedFunction = innerValue.As<v8::Function>();

    // Change the toString function on the wrapper function to avoid it
    // returning the source for the actual wrapper function. Instead it
    // returns source for a clean wrapper function with the event
    // argument wrapping the event source code. The reason for this is
    // that some web sites use toString on event functions and eval the
    // source returned (sometimes a RegExp is applied as well) for some
    // other use. That fails miserably if the actual wrapper source is
    // returned.
    v8::Persistent<v8::FunctionTemplate>& toStringTemplate =
        V8PerIsolateData::current()->lazyEventListenerToStringTemplate();
    if (toStringTemplate.IsEmpty())
        toStringTemplate = v8::Persistent<v8::FunctionTemplate>::New(v8::FunctionTemplate::New(V8LazyEventListenerToString));
    v8::Local<v8::Function> toStringFunction;
    if (!toStringTemplate.IsEmpty())
        toStringFunction = toStringTemplate->GetFunction();
    if (!toStringFunction.IsEmpty()) {
        String toStringString = "function " + m_functionName + "(" + m_eventParameterName + ") {\n  " + m_code + "\n}";
        wrappedFunction->SetHiddenValue(V8HiddenPropertyName::toStringString(), deprecatedV8String(toStringString));
        wrappedFunction->Set(v8::String::NewSymbol("toString"), toStringFunction);
    }

    wrappedFunction->SetName(deprecatedV8String(m_functionName));

    // FIXME: Remove the following comment-outs.
    // See https://bugs.webkit.org/show_bug.cgi?id=85152 for more details.
    //
    // For the time being, we comment out the following code since the
    // second parsing can happen.
    // // Since we only parse once, there's no need to keep data used for parsing around anymore.
    // m_functionName = String();
    // m_code = String();
    // m_eventParameterName = String();
    // m_sourceURL = String();

    setListenerObject(wrappedFunction);
}

} // namespace WebCore

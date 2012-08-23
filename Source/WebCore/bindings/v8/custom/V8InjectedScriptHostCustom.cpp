/*
 * Copyright (C) 2007-2011 Google Inc. All rights reserved.
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
#if ENABLE(INSPECTOR)
#include "V8InjectedScriptHost.h"

#include "BindingState.h"
#include "Database.h"
#include "InjectedScript.h"
#include "InjectedScriptHost.h"
#include "InspectorDOMAgent.h"
#include "InspectorValues.h"
#include "ScriptDebugServer.h"
#include "ScriptValue.h"
#include "V8AbstractEventListener.h"
#include "V8Binding.h"
#include "V8Database.h"
#include "V8Float32Array.h"
#include "V8Float64Array.h"
#include "V8HTMLAllCollection.h"
#include "V8HTMLCollection.h"
#include "V8HiddenPropertyName.h"
#include "V8Int16Array.h"
#include "V8Int32Array.h"
#include "V8Int8Array.h"
#include "V8NodeList.h"
#include "V8Node.h"
#include "V8Storage.h"
#include "V8Uint16Array.h"
#include "V8Uint32Array.h"
#include "V8Uint8Array.h"
#include "V8Uint8ClampedArray.h"

namespace WebCore {

Node* InjectedScriptHost::scriptValueAsNode(ScriptValue value)
{
    if (!value.isObject() || value.isNull())
        return 0;
    return V8Node::toNative(v8::Handle<v8::Object>::Cast(value.v8Value()));
}

ScriptValue InjectedScriptHost::nodeAsScriptValue(ScriptState* state, Node* node)
{
    v8::HandleScope scope;
    v8::Local<v8::Context> context = state->context();
    v8::Context::Scope contextScope(context);

    return ScriptValue(toV8(node));
}

v8::Handle<v8::Value> V8InjectedScriptHost::inspectedObjectCallback(const v8::Arguments& args)
{
    INC_STATS("InjectedScriptHost.inspectedObject()");
    if (args.Length() < 1)
        return v8::Undefined();

    if (!args[0]->IsInt32())
        return throwTypeError("argument has to be an integer", args.GetIsolate());

    InjectedScriptHost* host = V8InjectedScriptHost::toNative(args.Holder());
    InjectedScriptHost::InspectableObject* object = host->inspectedObject(args[0]->ToInt32()->Value());
    return object->get(ScriptState::current()).v8Value();
}

v8::Handle<v8::Value> V8InjectedScriptHost::internalConstructorNameCallback(const v8::Arguments& args)
{
    INC_STATS("InjectedScriptHost.internalConstructorName()");
    if (args.Length() < 1)
        return v8::Undefined();

    if (!args[0]->IsObject())
        return v8::Undefined();

    return args[0]->ToObject()->GetConstructorName();
}

v8::Handle<v8::Value> V8InjectedScriptHost::isHTMLAllCollectionCallback(const v8::Arguments& args)
{
    INC_STATS("InjectedScriptHost.isHTMLAllCollectionCallback()");
    if (args.Length() < 1)
        return v8::Undefined();

    if (!args[0]->IsObject())
        return v8Boolean(false, args.GetIsolate());

    v8::HandleScope handleScope;
    return v8::Boolean::New(V8HTMLAllCollection::HasInstance(args[0]));
}

v8::Handle<v8::Value> V8InjectedScriptHost::typeCallback(const v8::Arguments& args)
{
    INC_STATS("InjectedScriptHost.typeCallback()");
    if (args.Length() < 1)
        return v8::Undefined();

    v8::Handle<v8::Value> value = args[0];
    if (value->IsString())
        return v8::String::New("string");
    if (value->IsArray())
        return v8::String::New("array");
    if (value->IsBoolean())
        return v8::String::New("boolean");
    if (value->IsNumber())
        return v8::String::New("number");
    if (value->IsDate())
        return v8::String::New("date");
    if (value->IsRegExp())
        return v8::String::New("regexp");
    if (V8Node::HasInstance(value))
        return v8::String::New("node");
    if (V8NodeList::HasInstance(value))
        return v8::String::New("array");
    if (V8HTMLCollection::HasInstance(value))
        return v8::String::New("array");
    if (V8Int8Array::HasInstance(value) || V8Int16Array::HasInstance(value) || V8Int32Array::HasInstance(value))
        return v8::String::New("array");
    if (V8Uint8Array::HasInstance(value) || V8Uint16Array::HasInstance(value) || V8Uint32Array::HasInstance(value))
        return v8::String::New("array");
    if (V8Float32Array::HasInstance(value) || V8Float64Array::HasInstance(value))
        return v8::String::New("array");
    if (V8Uint8ClampedArray::HasInstance(value))
        return v8::String::New("array");
    return v8::Undefined();
}

v8::Handle<v8::Value> V8InjectedScriptHost::functionDetailsCallback(const v8::Arguments& args)
{
    INC_STATS("InjectedScriptHost.functionDetailsCallback()");
    if (args.Length() < 1)
        return v8::Undefined();

    v8::HandleScope handleScope;

    v8::Handle<v8::Value> value = args[0];
    if (!value->IsFunction())
        return v8::Undefined();
    v8::Handle<v8::Function> function = v8::Handle<v8::Function>::Cast(value);
    int lineNumber = function->GetScriptLineNumber();
    int columnNumber = function->GetScriptColumnNumber();

    v8::Local<v8::Object> location = v8::Object::New();
    location->Set(v8::String::New("lineNumber"), v8Integer(lineNumber, args.GetIsolate()));
    location->Set(v8::String::New("columnNumber"), v8Integer(columnNumber, args.GetIsolate()));
    location->Set(v8::String::New("scriptId"), function->GetScriptId()->ToString());

    v8::Local<v8::Object> result = v8::Object::New();
    result->Set(v8::String::New("location"), location);

    v8::Handle<v8::Value> name = function->GetName();
    if (name->IsString() && v8::Handle<v8::String>::Cast(name)->Length())
        result->Set(v8::String::New("name"), name);

    v8::Handle<v8::Value> inferredName = function->GetInferredName();
    if (inferredName->IsString() && v8::Handle<v8::String>::Cast(inferredName)->Length())
        result->Set(v8::String::New("inferredName"), inferredName);

    InjectedScriptHost* host = V8InjectedScriptHost::toNative(args.Holder());
    ScriptDebugServer& debugServer = host->scriptDebugServer();
    v8::Handle<v8::Value> scopes = debugServer.functionScopes(function);
    if (!scopes.IsEmpty() && scopes->IsArray())
        result->Set(v8::String::New("rawScopes"), scopes);

    return result;
}

static v8::Handle<v8::Array> getJSListenerFunctions(Document* document, const EventListenerInfo& listenerInfo)
{
    v8::Local<v8::Array> result = v8::Array::New();
    size_t handlersCount = listenerInfo.eventListenerVector.size();
    for (size_t i = 0, outputIndex = 0; i < handlersCount; ++i) {
        RefPtr<EventListener> listener = listenerInfo.eventListenerVector[i].listener;
        if (listener->type() != EventListener::JSEventListenerType) {
            ASSERT_NOT_REACHED();
            continue;
        }
        V8AbstractEventListener* v8Listener = static_cast<V8AbstractEventListener*>(listener.get());
        v8::Local<v8::Context> context = toV8Context(document, v8Listener->worldContext());
        // Hide listeners from other contexts.
        if (context != v8::Context::GetCurrent())
            continue;
        v8::Local<v8::Object> function;
        {
            // getListenerObject() may cause JS in the event attribute to get compiled, potentially unsuccessfully.
            v8::TryCatch block;
            function = v8Listener->getListenerObject(document);
            if (block.HasCaught())
                continue;
        }
        ASSERT(!function.IsEmpty());
        v8::Local<v8::Object> listenerEntry = v8::Object::New();
        listenerEntry->Set(v8::String::New("listener"), function);
        listenerEntry->Set(v8::String::New("useCapture"), v8::Boolean::New(listenerInfo.eventListenerVector[i].useCapture));
        result->Set(v8::Number::New(outputIndex++), listenerEntry);
    }
    return result;
}

v8::Handle<v8::Value> V8InjectedScriptHost::getEventListenersCallback(const v8::Arguments& args)
{
    INC_STATS("InjectedScriptHost.queryEventListenerCallback()");
    if (args.Length() < 1)
        return v8::Undefined();

    v8::HandleScope handleScope;

    v8::Local<v8::Value> value = args[0];
    if (!V8Node::HasInstance(value))
        return v8::Undefined();
    Node* node = V8Node::toNative(value->ToObject());
    if (!node)
        return v8::Undefined();
    // This can only happen for orphan DocumentType nodes.
    Document* document = node->document();
    if (!node->document())
        return v8::Undefined();

    InjectedScriptHost* host = V8InjectedScriptHost::toNative(args.Holder());
    Vector<EventListenerInfo> listenersArray;
    host->getEventListenersImpl(node, listenersArray);

    v8::Local<v8::Object> result = v8::Object::New();
    for (size_t i = 0; i < listenersArray.size(); ++i) {
        v8::Handle<v8::Array> listeners = getJSListenerFunctions(document, listenersArray[i]);
        if (!listeners->Length())
            continue;
        AtomicString eventType = listenersArray[i].eventType;
        result->Set(v8String(eventType), listeners);
    }

    return result;
}

v8::Handle<v8::Value> V8InjectedScriptHost::inspectCallback(const v8::Arguments& args)
{
    INC_STATS("InjectedScriptHost.inspect()");
    if (args.Length() < 2)
        return v8::Undefined();

    InjectedScriptHost* host = V8InjectedScriptHost::toNative(args.Holder());
    ScriptValue object(args[0]);
    ScriptValue hints(args[1]);
    host->inspectImpl(object.toInspectorValue(ScriptState::current()), hints.toInspectorValue(ScriptState::current()));

    return v8::Undefined();
}

v8::Handle<v8::Value> V8InjectedScriptHost::databaseIdCallback(const v8::Arguments& args)
{
    INC_STATS("InjectedScriptHost.databaseId()");
    if (args.Length() < 1)
        return v8::Undefined();
#if ENABLE(SQL_DATABASE)
    InjectedScriptHost* host = V8InjectedScriptHost::toNative(args.Holder());
    Database* database = V8Database::toNative(v8::Handle<v8::Object>::Cast(args[0]));
    if (database)
        return v8StringOrUndefined(host->databaseIdImpl(database), args.GetIsolate());
#endif
    return v8::Undefined();
}

v8::Handle<v8::Value> V8InjectedScriptHost::storageIdCallback(const v8::Arguments& args)
{
    if (args.Length() < 1)
        return v8::Undefined();
    INC_STATS("InjectedScriptHost.storageId()");
    InjectedScriptHost* host = V8InjectedScriptHost::toNative(args.Holder());
    Storage* storage = V8Storage::toNative(v8::Handle<v8::Object>::Cast(args[0]));
    if (storage)
        return v8StringOrUndefined(host->storageIdImpl(storage), args.GetIsolate());
    return v8::Undefined();
}

v8::Handle<v8::Value> V8InjectedScriptHost::evaluateCallback(const v8::Arguments& args)
{
    INC_STATS("InjectedScriptHost.evaluate()");
    if (args.Length() < 1)
        return v8::ThrowException(v8::Exception::Error(v8::String::New("One argument expected.")));

    v8::Handle<v8::String> expression = args[0]->ToString();
    if (expression.IsEmpty())
        return v8::ThrowException(v8::Exception::Error(v8::String::New("The argument must be a string.")));

    v8::Handle<v8::Script> script = v8::Script::Compile(expression);
    if (script.IsEmpty()) // Return immediately in case of exception to let the caller handle it.
        return v8::Handle<v8::Value>();
    return script->Run();
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR)

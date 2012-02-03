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

#include "Database.h"
#include "InjectedScript.h"
#include "InjectedScriptHost.h"
#include "InspectorValues.h"
#include "ScriptValue.h"
#include "V8Binding.h"
#include "V8BindingState.h"
#include "V8Database.h"
#include "V8HTMLAllCollection.h"
#include "V8HTMLCollection.h"
#include "V8HiddenPropertyName.h"
#include "V8NodeList.h"
#include "V8Node.h"
#include "V8Proxy.h"
#include "V8Storage.h"

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

v8::Handle<v8::Value> V8InjectedScriptHost::inspectedNodeCallback(const v8::Arguments& args)
{
    INC_STATS("InjectedScriptHost.inspectedNode()");
    if (args.Length() < 1)
        return v8::Undefined();

    InjectedScriptHost* host = V8InjectedScriptHost::toNative(args.Holder());

    Node* node = host->inspectedNode(args[0]->ToInt32()->Value());
    if (!node)
        return v8::Undefined();

    return toV8(node);
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
        return v8::False();

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
    location->Set(v8::String::New("lineNumber"), v8::Integer::New(lineNumber));
    location->Set(v8::String::New("columnNumber"), v8::Integer::New(columnNumber));
    location->Set(v8::String::New("scriptId"), function->GetScriptId()->ToString());

    v8::Local<v8::Object> result = v8::Object::New();
    result->Set(v8::String::New("location"), location);

    v8::Handle<v8::Value> name = function->GetName();
    if (name->IsString() && v8::Handle<v8::String>::Cast(name)->Length())
        result->Set(v8::String::New("name"), name);

    v8::Handle<v8::Value> inferredName = function->GetInferredName();
    if (inferredName->IsString() && v8::Handle<v8::String>::Cast(inferredName)->Length())
        result->Set(v8::String::New("inferredName"), inferredName);
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
        return v8::Number::New(host->databaseIdImpl(database));
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
        return v8::Number::New(host->storageIdImpl(storage));
    return v8::Undefined();
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR)

/*
 * Copyright (C) 2004, 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2007, 2008, 2009 Google, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "NPV8Object.h"

#include "ChromiumBridge.h"
#include "DOMWindow.h"
#include "Frame.h"
#include "OwnArrayPtr.h"
#include "PlatformString.h"
#include "ScriptController.h"
#include "V8CustomBinding.h"
#include "V8Helpers.h"
#include "V8NPUtils.h"
#include "V8Proxy.h"
#include "bindings/npruntime.h"
#include "npruntime_priv.h"

#include <stdio.h>
#include <v8.h>

using WebCore::toV8Context;
using WebCore::toV8Proxy;
using WebCore::V8ClassIndex;
using WebCore::V8Custom;
using WebCore::V8Proxy;

// FIXME: Comments on why use malloc and free.
static NPObject* allocV8NPObject(NPP, NPClass*)
{
    return static_cast<NPObject*>(malloc(sizeof(V8NPObject)));
}

static void freeV8NPObject(NPObject* npObject)
{
    V8NPObject* v8NpObject = reinterpret_cast<V8NPObject*>(npObject);
#ifndef NDEBUG
    V8GCController::UnregisterGlobalHandle(v8NpObject, v8NpObject->v8Object);
#endif
    v8NpObject->v8Object.Dispose();
    free(v8NpObject);
}

static v8::Handle<v8::Value>* createValueListFromVariantArgs(const NPVariant* arguments, uint32_t argumentCount, NPObject* owner)
{
    v8::Handle<v8::Value>* argv = new v8::Handle<v8::Value>[argumentCount];
    for (uint32_t index = 0; index < argumentCount; index++) {
        const NPVariant* arg = &arguments[index];
        argv[index] = convertNPVariantToV8Object(arg, owner);
    }
    return argv;
}

// Create an identifier (null terminated utf8 char*) from the NPIdentifier.
static v8::Local<v8::String> npIdentifierToV8Identifier(NPIdentifier name)
{
    PrivateIdentifier* identifier = static_cast<PrivateIdentifier*>(name);
    if (identifier->isString)
        return v8::String::New(static_cast<const char*>(identifier->value.string));

    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%d", identifier->value.number);
    return v8::String::New(buffer);
}

static NPClass V8NPObjectClass = { NP_CLASS_STRUCT_VERSION,
                                   allocV8NPObject,
                                   freeV8NPObject,
                                   0, 0, 0, 0, 0, 0, 0, 0, 0 };

// NPAPI's npruntime functions.
NPClass* npScriptObjectClass = &V8NPObjectClass;

NPObject* npCreateV8ScriptObject(NPP npp, v8::Handle<v8::Object> object, WebCore::DOMWindow* root)
{
    v8::Local<v8::Value> typeIndex = object->GetInternalField(V8Custom::kDOMWrapperTypeIndex);
    // Check to see if this object is already wrapped.
    if (object->InternalFieldCount() == V8Custom::kNPObjectInternalFieldCount && typeIndex->IsNumber() && typeIndex->Uint32Value() == V8ClassIndex::NPOBJECT) {

        NPObject* returnValue = V8Proxy::ToNativeObject<NPObject>(V8ClassIndex::NPOBJECT, object);
        NPN_RetainObject(returnValue);
        return returnValue;
    }

    V8NPObject* v8npObject = reinterpret_cast<V8NPObject*>(NPN_CreateObject(npp, &V8NPObjectClass));
    v8npObject->v8Object = v8::Persistent<v8::Object>::New(object);
#ifndef NDEBUG
    V8Proxy::RegisterGlobalHandle(WebCore::NPOBJECT, v8npObject, v8npObject->v8Object);
#endif
    v8npObject->rootObject = root;
    return reinterpret_cast<NPObject*>(v8npObject);
}

bool NPN_Invoke(NPP npp, NPObject* npObject, NPIdentifier methodName, const NPVariant* arguments, uint32_t argumentCount, NPVariant* result)
{
    if (!npObject)
        return false;

    if (npObject->_class != npScriptObjectClass) {
        if (npObject->_class->invokeDefault)
            return npObject->_class->invokeDefault(npObject, arguments, argumentCount, result);

        VOID_TO_NPVARIANT(*result);
        return true;
    }

    V8NPObject* v8NpObject = reinterpret_cast<V8NPObject*>(npObject);

    PrivateIdentifier* identifier = static_cast<PrivateIdentifier*>(methodName);
    if (!identifier->isString)
        return false;

    v8::HandleScope handleScope;
    // FIXME: should use the plugin's owner frame as the security context.
    v8::Handle<v8::Context> context = toV8Context(npp, npObject);
    if (context.IsEmpty())
        return false;

    v8::Context::Scope scope(context);

    if (methodName == NPN_GetStringIdentifier("eval")) {
        if (argumentCount != 1)
            return false;
        if (arguments[0].type != NPVariantType_String)
            return false;
        return NPN_Evaluate(npp, npObject, const_cast<NPString*>(&arguments[0].value.stringValue), result);
    }

    v8::Handle<v8::Value> functionObject = v8NpObject->v8Object->Get(v8::String::New(identifier->value.string));
    if (functionObject.IsEmpty() || functionObject->IsNull()) {
        NULL_TO_NPVARIANT(*result);
        return false;
    }
    if (functionObject->IsUndefined()) {
        VOID_TO_NPVARIANT(*result);
        return false;
    }

    V8Proxy* proxy = toV8Proxy(npObject);
    ASSERT(proxy);

    // Call the function object.
    v8::Handle<v8::Function> function = v8::Handle<v8::Function>::Cast(functionObject);
    OwnArrayPtr<v8::Handle<v8::Value> > argv(createValueListFromVariantArgs(arguments, argumentCount, npObject));
    v8::Local<v8::Value> resultObject = proxy->CallFunction(function, v8NpObject->v8Object, argumentCount, argv.get());

    // If we had an error, return false.  The spec is a little unclear here, but says "Returns true if the method was
    // successfully invoked".  If we get an error return value, was that successfully invoked?
    if (resultObject.IsEmpty())
        return false;

    convertV8ObjectToNPVariant(resultObject, npObject, result);
    return true;
}

// FIXME: Fix it same as NPN_Invoke (HandleScope and such).
bool NPN_InvokeDefault(NPP npp, NPObject* npObject, const NPVariant* arguments, uint32_t argumentCount, NPVariant* result)
{
    if (!npObject)
        return false;

    if (npObject->_class != npScriptObjectClass) {
        if (npObject->_class->invokeDefault)
            return npObject->_class->invokeDefault(npObject, arguments, argumentCount, result);

        VOID_TO_NPVARIANT(*result);
        return true;
    }

    V8NPObject* v8NpObject = reinterpret_cast<V8NPObject*>(npObject);

    VOID_TO_NPVARIANT(*result);

    v8::HandleScope handleScope;
    v8::Handle<v8::Context> context = toV8Context(npp, npObject);
    if (context.IsEmpty())
        return false;

    v8::Context::Scope scope(context);

    // Lookup the function object and call it.
    v8::Handle<v8::Object> functionObject(v8NpObject->v8Object);
    if (!functionObject->IsFunction())
        return false;

    v8::Local<v8::Value> resultObject;
    v8::Handle<v8::Function> function(v8::Function::Cast(*functionObject));
    if (!function->IsNull()) {
        V8Proxy* proxy = toV8Proxy(npObject);
        ASSERT(proxy);

        OwnArrayPtr<v8::Handle<v8::Value> > argv(createValueListFromVariantArgs(arguments, argumentCount, npObject));
        resultObject = proxy->CallFunction(function, functionObject, argumentCount, argv.get());
    }
    // If we had an error, return false.  The spec is a little unclear here, but says "Returns true if the method was
    // successfully invoked".  If we get an error return value, was that successfully invoked?
    if (resultObject.IsEmpty())
        return false;

    convertV8ObjectToNPVariant(resultObject, npObject, result);
    return true;
}

bool NPN_Evaluate(NPP npp, NPObject* npObject, NPString* npScript, NPVariant* result)
{
    bool popupsAllowed = WebCore::ChromiumBridge::popupsAllowed(npp);
    return NPN_EvaluateHelper(npp, popupsAllowed, npObject, npScript, result);
}

bool NPN_EvaluateHelper(NPP npp, bool popupsAllowed, NPObject* npObject, NPString* npScript, NPVariant* result)
{
    VOID_TO_NPVARIANT(*result);
    if (!npObject)
        return false;

    if (npObject->_class != npScriptObjectClass)
        return false;

    v8::HandleScope handleScope;
    v8::Handle<v8::Context> context = toV8Context(npp, npObject);
    if (context.IsEmpty())
        return false;

    V8Proxy* proxy = toV8Proxy(npObject);
    ASSERT(proxy);

    v8::Context::Scope scope(context);

    WebCore::String filename;
    if (!popupsAllowed)
        filename = "npScript";

    WebCore::String script = WebCore::String::fromUTF8(npScript->UTF8Characters, npScript->UTF8Length);
    v8::Local<v8::Value> v8result = proxy->evaluate(WebCore::ScriptSourceCode(script, WebCore::KURL(filename)), 0);

    if (v8result.IsEmpty())
        return false;

    convertV8ObjectToNPVariant(v8result, npObject, result);
    return true;
}

bool NPN_GetProperty(NPP npp, NPObject* npObject, NPIdentifier propertyName, NPVariant* result)
{
    if (!npObject)
        return false;

    if (npObject->_class == npScriptObjectClass) {
        V8NPObject* object = reinterpret_cast<V8NPObject*>(npObject);

        v8::HandleScope handleScope;
        v8::Handle<v8::Context> context = toV8Context(npp, npObject);
        if (context.IsEmpty())
            return false;

        v8::Context::Scope scope(context);

        v8::Handle<v8::Object> obj(object->v8Object);
        v8::Local<v8::Value> v8result = obj->Get(npIdentifierToV8Identifier(propertyName));

        convertV8ObjectToNPVariant(v8result, npObject, result);
        return true;
    }

    if (npObject->_class->hasProperty && npObject->_class->getProperty) {
        if (npObject->_class->hasProperty(npObject, propertyName))
            return npObject->_class->getProperty(npObject, propertyName, result);
    }

    VOID_TO_NPVARIANT(*result);
    return false;
}

bool NPN_SetProperty(NPP npp, NPObject* npObject, NPIdentifier propertyName, const NPVariant* value)
{
    if (!npObject)
        return false;

    if (npObject->_class == npScriptObjectClass) {
        V8NPObject* object = reinterpret_cast<V8NPObject*>(npObject);

        v8::HandleScope handleScope;
        v8::Handle<v8::Context> context = toV8Context(npp, npObject);
        if (context.IsEmpty())
            return false;

        v8::Context::Scope scope(context);

        v8::Handle<v8::Object> obj(object->v8Object);
        obj->Set(npIdentifierToV8Identifier(propertyName),
                 convertNPVariantToV8Object(value, object->rootObject->frame()->script()->windowScriptNPObject()));
        return true;
    }

    if (npObject->_class->setProperty)
        return npObject->_class->setProperty(npObject, propertyName, value);

    return false;
}

bool NPN_RemoveProperty(NPP npp, NPObject* npObject, NPIdentifier propertyName)
{
    if (!npObject)
        return false;
    if (npObject->_class != npScriptObjectClass)
        return false;

    V8NPObject* object = reinterpret_cast<V8NPObject*>(npObject);

    v8::HandleScope handleScope;
    v8::Handle<v8::Context> context = toV8Context(npp, npObject);
    if (context.IsEmpty())
        return false;
    v8::Context::Scope scope(context);

    v8::Handle<v8::Object> obj(object->v8Object);
    // FIXME: Verify that setting to undefined is right.
    obj->Set(npIdentifierToV8Identifier(propertyName), v8::Undefined());
    return true;
}

bool NPN_HasProperty(NPP npp, NPObject* npObject, NPIdentifier propertyName)
{
    if (!npObject)
        return false;

    if (npObject->_class == npScriptObjectClass) {
        V8NPObject* object = reinterpret_cast<V8NPObject*>(npObject);

        v8::HandleScope handleScope;
        v8::Handle<v8::Context> context = toV8Context(npp, npObject);
        if (context.IsEmpty())
            return false;
        v8::Context::Scope scope(context);

        v8::Handle<v8::Object> obj(object->v8Object);
        return obj->Has(npIdentifierToV8Identifier(propertyName));
    }

    if (npObject->_class->hasProperty)
        return npObject->_class->hasProperty(npObject, propertyName);
    return false;
}

bool NPN_HasMethod(NPP npp, NPObject* npObject, NPIdentifier methodName)
{
    if (!npObject)
        return false;

    if (npObject->_class == npScriptObjectClass) {
        V8NPObject* object = reinterpret_cast<V8NPObject*>(npObject);

        v8::HandleScope handleScope;
        v8::Handle<v8::Context> context = toV8Context(npp, npObject);
        if (context.IsEmpty())
            return false;
        v8::Context::Scope scope(context);

        v8::Handle<v8::Object> obj(object->v8Object);
        v8::Handle<v8::Value> prop = obj->Get(npIdentifierToV8Identifier(methodName));
        return prop->IsFunction();
    }

    if (npObject->_class->hasMethod)
        return npObject->_class->hasMethod(npObject, methodName);
    return false;
}

void NPN_SetException(NPObject* npObject, const NPUTF8 *message)
{
    if (npObject->_class != npScriptObjectClass)
        return;
    v8::HandleScope handleScope;
    v8::Handle<v8::Context> context = toV8Context(0, npObject);
    if (context.IsEmpty())
        return;

    v8::Context::Scope scope(context);
    V8Proxy::ThrowError(V8Proxy::GENERAL_ERROR, message);
}

bool NPN_Enumerate(NPP npp, NPObject* npObject, NPIdentifier** identifier, uint32_t* count)
{
    if (!npObject)
        return false;

    if (npObject->_class == npScriptObjectClass) {
        V8NPObject* object = reinterpret_cast<V8NPObject*>(npObject);

        v8::HandleScope handleScope;
        v8::Handle<v8::Context> context = toV8Context(npp, npObject);
        if (context.IsEmpty())
            return false;
        v8::Context::Scope scope(context);

        v8::Handle<v8::Object> obj(object->v8Object);

        // FIXME: http://b/issue?id=1210340: Use a v8::Object::Keys() method when it exists, instead of evaluating javascript.

        // FIXME: Figure out how to cache this helper function.  Run a helper function that collects the properties
        // on the object into an array.
        const char enumeratorCode[] =
            "(function (obj) {"
            "  var props = [];"
            "  for (var prop in obj) {"
            "    props[props.length] = prop;"
            "  }"
            "  return props;"
            "});";
        v8::Handle<v8::String> source = v8::String::New(enumeratorCode);
        v8::Handle<v8::Script> script = v8::Script::Compile(source, 0);
        v8::Handle<v8::Value> enumeratorObj = script->Run();
        v8::Handle<v8::Function> enumerator = v8::Handle<v8::Function>::Cast(enumeratorObj);
        v8::Handle<v8::Value> argv[] = { obj };
        v8::Local<v8::Value> propsObj = enumerator->Call(v8::Handle<v8::Object>::Cast(enumeratorObj), ARRAYSIZE_UNSAFE(argv), argv);
        if (propsObj.IsEmpty())
            return false;

        // Convert the results into an array of NPIdentifiers.
        v8::Handle<v8::Array> props = v8::Handle<v8::Array>::Cast(propsObj);
        *count = props->Length();
        *identifier = static_cast<NPIdentifier*>(malloc(sizeof(NPIdentifier*) * *count));
        for (uint32_t i = 0; i < *count; ++i) {
            v8::Local<v8::Value> name = props->Get(v8::Integer::New(i));
            (*identifier)[i] = getStringIdentifier(v8::Local<v8::String>::Cast(name));
        }
        return true;
    }

    if (NP_CLASS_STRUCT_VERSION_HAS_ENUM(npObject->_class) && npObject->_class->enumerate)
       return npObject->_class->enumerate(npObject, identifier, count);

    return false;
}

bool NPN_Construct(NPP npp, NPObject* npObject, const NPVariant* arguments, uint32_t argumentCount, NPVariant* result)
{
    if (!npObject)
        return false;

    if (npObject->_class == npScriptObjectClass) {
        V8NPObject* object = reinterpret_cast<V8NPObject*>(npObject);

        v8::HandleScope handleScope;
        v8::Handle<v8::Context> context = toV8Context(npp, npObject);
        if (context.IsEmpty())
            return false;
        v8::Context::Scope scope(context);

        // Lookup the constructor function.
        v8::Handle<v8::Object> ctorObj(object->v8Object);
        if (!ctorObj->IsFunction())
            return false;

        // Call the constructor.
        v8::Local<v8::Value> resultObject;
        v8::Handle<v8::Function> ctor(v8::Function::Cast(*ctorObj));
        if (!ctor->IsNull()) {
            V8Proxy* proxy = toV8Proxy(npObject);
            ASSERT(proxy);

            OwnArrayPtr<v8::Handle<v8::Value> > argv(createValueListFromVariantArgs(arguments, argumentCount, npObject));
            resultObject = proxy->NewInstance(ctor, argumentCount, argv.get());
        }

        if (resultObject.IsEmpty())
            return false;

        convertV8ObjectToNPVariant(resultObject, npObject, result);
        return true;
    }

    if (NP_CLASS_STRUCT_VERSION_HAS_CTOR(npObject->_class) && npObject->_class->construct)
        return npObject->_class->construct(npObject, arguments, argumentCount, result);

    return false;
}

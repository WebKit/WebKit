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

#include "DOMWindow.h"
#include "Frame.h"
#include "NPObjectWrapper.h"
#include <wtf/OwnArrayPtr.h>
#include "ScriptSourceCode.h"
#include "UserGestureIndicator.h"
#include "V8Binding.h"
#include "V8GCController.h"
#include "V8NPUtils.h"
#include "WrapperTypeInfo.h"
#include "npruntime_impl.h"
#include "npruntime_priv.h"

#include <stdio.h>
#include <wtf/StringExtras.h>
#include <wtf/text/WTFString.h>

using namespace WebCore;

namespace WebCore {

WrapperTypeInfo* npObjectTypeInfo()
{
    static WrapperTypeInfo typeInfo = { 0, 0, 0, 0, 0, 0, WrapperTypeObjectPrototype };
    return &typeInfo;
}

static v8::Local<v8::Context> toV8Context(NPP npp, NPObject* npObject)
{
    V8NPObject* object = reinterpret_cast<V8NPObject*>(npObject);
    DOMWindow* window = object->rootObject;
    if (!window || !window->isCurrentlyDisplayedInFrame())
        return v8::Local<v8::Context>();
    return ScriptController::mainWorldContext(object->rootObject->frame());
}

// FIXME: Comments on why use malloc and free.
static NPObject* allocV8NPObject(NPP, NPClass*)
{
    return static_cast<NPObject*>(malloc(sizeof(V8NPObject)));
}

static void freeV8NPObject(NPObject* npObject)
{
    V8NPObject* v8NpObject = reinterpret_cast<V8NPObject*>(npObject);
    v8::HandleScope scope;
    ASSERT(!v8NpObject->v8Object->CreationContext().IsEmpty());
    if (V8PerContextData* perContextData = V8PerContextData::from(v8NpObject->v8Object->CreationContext())) {
        V8NPObjectMap* v8NPObjectMap = perContextData->v8NPObjectMap();
        int v8ObjectHash = v8NpObject->v8Object->GetIdentityHash();
        ASSERT(v8ObjectHash);
        V8NPObjectMap::iterator iter = v8NPObjectMap->find(v8ObjectHash);
        if (iter != v8NPObjectMap->end()) {
            V8NPObjectVector& objects = iter->value;
            for (size_t index = 0; index < objects.size(); ++index) {
                if (objects.at(index) == v8NpObject) {
                    objects.remove(index);
                    break;
                }
            }
            if (objects.isEmpty())
                v8NPObjectMap->remove(v8ObjectHash);
        }
    }
    v8NpObject->v8Object.Dispose();
    v8NpObject->v8Object.Clear();
    free(v8NpObject);
}

static PassOwnArrayPtr<v8::Handle<v8::Value> > createValueListFromVariantArgs(const NPVariant* arguments, uint32_t argumentCount, NPObject* owner)
{
    OwnArrayPtr<v8::Handle<v8::Value> > argv = adoptArrayPtr(new v8::Handle<v8::Value>[argumentCount]);
    for (uint32_t index = 0; index < argumentCount; index++) {
        const NPVariant* arg = &arguments[index];
        argv[index] = convertNPVariantToV8Object(arg, owner);
    }
    return argv.release();
}

// Create an identifier (null terminated utf8 char*) from the NPIdentifier.
static v8::Local<v8::String> npIdentifierToV8Identifier(NPIdentifier name)
{
    PrivateIdentifier* identifier = static_cast<PrivateIdentifier*>(name);
    if (identifier->isString)
        return v8::String::NewSymbol(static_cast<const char*>(identifier->value.string));

    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%d", identifier->value.number);
    return v8::String::NewSymbol(buffer);
}

NPObject* v8ObjectToNPObject(v8::Handle<v8::Object> object)
{
    return reinterpret_cast<NPObject*>(object->GetAlignedPointerFromInternalField(v8DOMWrapperObjectIndex)); 
}

static NPClass V8NPObjectClass = { NP_CLASS_STRUCT_VERSION,
                                   allocV8NPObject,
                                   freeV8NPObject,
                                   0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

// NPAPI's npruntime functions.
NPClass* npScriptObjectClass = &V8NPObjectClass;

NPObject* npCreateV8ScriptObject(NPP npp, v8::Handle<v8::Object> object, DOMWindow* root)
{
    // Check to see if this object is already wrapped.
    if (object->InternalFieldCount() == npObjectInternalFieldCount) {
        WrapperTypeInfo* typeInfo = static_cast<WrapperTypeInfo*>(object->GetAlignedPointerFromInternalField(v8DOMWrapperTypeIndex));
        if (typeInfo == npObjectTypeInfo()) {

            NPObject* returnValue = v8ObjectToNPObject(object);
            _NPN_RetainObject(returnValue);
            return returnValue;
        }
    }

    V8NPObjectVector* objectVector = 0;
    if (V8PerContextData* perContextData = V8PerContextData::from(object->CreationContext())) {
        int v8ObjectHash = object->GetIdentityHash();
        ASSERT(v8ObjectHash);
        V8NPObjectMap* v8NPObjectMap = perContextData->v8NPObjectMap();
        V8NPObjectMap::iterator iter = v8NPObjectMap->find(v8ObjectHash);
        if (iter != v8NPObjectMap->end()) {
            V8NPObjectVector& objects = iter->value;
            for (size_t index = 0; index < objects.size(); ++index) {
                V8NPObject* v8npObject = objects.at(index);
                if (v8npObject->rootObject == root) {
                    ASSERT(v8npObject->v8Object == object);
                    _NPN_RetainObject(&v8npObject->object);
                    return reinterpret_cast<NPObject*>(v8npObject);
                }
            }
        } else {
            iter = v8NPObjectMap->set(v8ObjectHash, V8NPObjectVector()).iterator;
            objectVector = &iter->value;
        }
    }
    V8NPObject* v8npObject = reinterpret_cast<V8NPObject*>(_NPN_CreateObject(npp, &V8NPObjectClass));
    v8npObject->v8Object = v8::Persistent<v8::Object>::New(object);
    v8npObject->rootObject = root;

    if (objectVector)
        objectVector->append(v8npObject);

    return reinterpret_cast<NPObject*>(v8npObject);
}

} // namespace WebCore

bool _NPN_Invoke(NPP npp, NPObject* npObject, NPIdentifier methodName, const NPVariant* arguments, uint32_t argumentCount, NPVariant* result)
{
    if (!npObject)
        return false;

    if (npObject->_class != npScriptObjectClass) {
        if (npObject->_class->invoke)
            return npObject->_class->invoke(npObject, methodName, arguments, argumentCount, result);

        VOID_TO_NPVARIANT(*result);
        return true;
    }

    V8NPObject* v8NpObject = reinterpret_cast<V8NPObject*>(npObject);

    PrivateIdentifier* identifier = static_cast<PrivateIdentifier*>(methodName);
    if (!identifier->isString)
        return false;

    if (!strcmp(identifier->value.string, "eval")) {
        if (argumentCount != 1)
            return false;
        if (arguments[0].type != NPVariantType_String)
            return false;
        return _NPN_Evaluate(npp, npObject, const_cast<NPString*>(&arguments[0].value.stringValue), result);
    }

    v8::HandleScope handleScope;
    // FIXME: should use the plugin's owner frame as the security context.
    v8::Handle<v8::Context> context = toV8Context(npp, npObject);
    if (context.IsEmpty())
        return false;

    v8::Context::Scope scope(context);
    ExceptionCatcher exceptionCatcher;

    v8::Handle<v8::Value> functionObject = v8NpObject->v8Object->Get(v8::String::NewSymbol(identifier->value.string));
    if (functionObject.IsEmpty() || functionObject->IsNull()) {
        NULL_TO_NPVARIANT(*result);
        return false;
    }
    if (functionObject->IsUndefined()) {
        VOID_TO_NPVARIANT(*result);
        return false;
    }

    Frame* frame = v8NpObject->rootObject->frame();
    ASSERT(frame);

    // Call the function object.
    v8::Handle<v8::Function> function = v8::Handle<v8::Function>::Cast(functionObject);
    OwnArrayPtr<v8::Handle<v8::Value> > argv = createValueListFromVariantArgs(arguments, argumentCount, npObject);
    v8::Local<v8::Value> resultObject = frame->script()->callFunction(function, v8NpObject->v8Object, argumentCount, argv.get());

    // If we had an error, return false.  The spec is a little unclear here, but says "Returns true if the method was
    // successfully invoked".  If we get an error return value, was that successfully invoked?
    if (resultObject.IsEmpty())
        return false;

    convertV8ObjectToNPVariant(resultObject, npObject, result);
    return true;
}

// FIXME: Fix it same as _NPN_Invoke (HandleScope and such).
bool _NPN_InvokeDefault(NPP npp, NPObject* npObject, const NPVariant* arguments, uint32_t argumentCount, NPVariant* result)
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
    ExceptionCatcher exceptionCatcher;

    // Lookup the function object and call it.
    v8::Handle<v8::Object> functionObject(v8NpObject->v8Object);
    if (!functionObject->IsFunction())
        return false;

    v8::Local<v8::Value> resultObject;
    v8::Handle<v8::Function> function(v8::Function::Cast(*functionObject));
    if (!function->IsNull()) {
        Frame* frame = v8NpObject->rootObject->frame();
        ASSERT(frame);

        OwnArrayPtr<v8::Handle<v8::Value> > argv = createValueListFromVariantArgs(arguments, argumentCount, npObject);
        resultObject = frame->script()->callFunction(function, functionObject, argumentCount, argv.get());
    }
    // If we had an error, return false.  The spec is a little unclear here, but says "Returns true if the method was
    // successfully invoked".  If we get an error return value, was that successfully invoked?
    if (resultObject.IsEmpty())
        return false;

    convertV8ObjectToNPVariant(resultObject, npObject, result);
    return true;
}

bool _NPN_Evaluate(NPP npp, NPObject* npObject, NPString* npScript, NPVariant* result)
{
    // FIXME: Give the embedder a way to control this.
    bool popupsAllowed = false;
    return _NPN_EvaluateHelper(npp, popupsAllowed, npObject, npScript, result);
}

bool _NPN_EvaluateHelper(NPP npp, bool popupsAllowed, NPObject* npObject, NPString* npScript, NPVariant* result)
{
    VOID_TO_NPVARIANT(*result);
    if (!npObject)
        return false;

    if (npObject->_class != npScriptObjectClass) {
        // Check if the object passed in is wrapped. If yes, then we need to invoke on the underlying object.
        NPObject* actualObject = NPObjectWrapper::getUnderlyingNPObject(npObject);
        if (!actualObject)
            return false;
        npObject = actualObject;
    }

    v8::HandleScope handleScope;
    v8::Handle<v8::Context> context = toV8Context(npp, npObject);
    if (context.IsEmpty())
        return false;

    v8::Context::Scope scope(context);
    ExceptionCatcher exceptionCatcher;

    // FIXME: Is this branch still needed after switching to using UserGestureIndicator?
    String filename;
    if (!popupsAllowed)
        filename = "npscript";

    V8NPObject* v8NpObject = reinterpret_cast<V8NPObject*>(npObject);
    Frame* frame = v8NpObject->rootObject->frame();
    ASSERT(frame);

    String script = String::fromUTF8(npScript->UTF8Characters, npScript->UTF8Length);

    UserGestureIndicator gestureIndicator(popupsAllowed ? DefinitelyProcessingUserGesture : PossiblyProcessingUserGesture);
    v8::Local<v8::Value> v8result = frame->script()->compileAndRunScript(ScriptSourceCode(script, KURL(ParsedURLString, filename)));

    if (v8result.IsEmpty())
        return false;

    if (_NPN_IsAlive(npObject))
        convertV8ObjectToNPVariant(v8result, npObject, result);
    return true;
}

bool _NPN_GetProperty(NPP npp, NPObject* npObject, NPIdentifier propertyName, NPVariant* result)
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
        ExceptionCatcher exceptionCatcher;

        v8::Handle<v8::Object> obj(object->v8Object);
        v8::Local<v8::Value> v8result = obj->Get(npIdentifierToV8Identifier(propertyName));
        
        if (v8result.IsEmpty())
            return false;

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

bool _NPN_SetProperty(NPP npp, NPObject* npObject, NPIdentifier propertyName, const NPVariant* value)
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
        ExceptionCatcher exceptionCatcher;

        v8::Handle<v8::Object> obj(object->v8Object);
        obj->Set(npIdentifierToV8Identifier(propertyName),
                 convertNPVariantToV8Object(value, object->rootObject->frame()->script()->windowScriptNPObject()));
        return true;
    }

    if (npObject->_class->setProperty)
        return npObject->_class->setProperty(npObject, propertyName, value);

    return false;
}

bool _NPN_RemoveProperty(NPP npp, NPObject* npObject, NPIdentifier propertyName)
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
    ExceptionCatcher exceptionCatcher;

    v8::Handle<v8::Object> obj(object->v8Object);
    // FIXME: Verify that setting to undefined is right.
    obj->Set(npIdentifierToV8Identifier(propertyName), v8::Undefined());
    return true;
}

bool _NPN_HasProperty(NPP npp, NPObject* npObject, NPIdentifier propertyName)
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
        ExceptionCatcher exceptionCatcher;

        v8::Handle<v8::Object> obj(object->v8Object);
        return obj->Has(npIdentifierToV8Identifier(propertyName));
    }

    if (npObject->_class->hasProperty)
        return npObject->_class->hasProperty(npObject, propertyName);
    return false;
}

bool _NPN_HasMethod(NPP npp, NPObject* npObject, NPIdentifier methodName)
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
        ExceptionCatcher exceptionCatcher;

        v8::Handle<v8::Object> obj(object->v8Object);
        v8::Handle<v8::Value> prop = obj->Get(npIdentifierToV8Identifier(methodName));
        return prop->IsFunction();
    }

    if (npObject->_class->hasMethod)
        return npObject->_class->hasMethod(npObject, methodName);
    return false;
}

void _NPN_SetException(NPObject* npObject, const NPUTF8 *message)
{
    if (!npObject || npObject->_class != npScriptObjectClass) {
        // We won't be able to find a proper scope for this exception, so just throw it.
        // This is consistent with JSC, which throws a global exception all the time.
        throwError(v8GeneralError, message);
        return;
    }
    v8::HandleScope handleScope;
    v8::Handle<v8::Context> context = toV8Context(0, npObject);
    if (context.IsEmpty())
        return;

    v8::Context::Scope scope(context);
    ExceptionCatcher exceptionCatcher;

    throwError(v8GeneralError, message);
}

bool _NPN_Enumerate(NPP npp, NPObject* npObject, NPIdentifier** identifier, uint32_t* count)
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
        ExceptionCatcher exceptionCatcher;

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
            v8::Local<v8::Value> name = props->Get(deprecatedV8Integer(i));
            (*identifier)[i] = getStringIdentifier(v8::Local<v8::String>::Cast(name));
        }
        return true;
    }

    if (NP_CLASS_STRUCT_VERSION_HAS_ENUM(npObject->_class) && npObject->_class->enumerate)
       return npObject->_class->enumerate(npObject, identifier, count);

    return false;
}

bool _NPN_Construct(NPP npp, NPObject* npObject, const NPVariant* arguments, uint32_t argumentCount, NPVariant* result)
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
        ExceptionCatcher exceptionCatcher;

        // Lookup the constructor function.
        v8::Handle<v8::Object> ctorObj(object->v8Object);
        if (!ctorObj->IsFunction())
            return false;

        // Call the constructor.
        v8::Local<v8::Value> resultObject;
        v8::Handle<v8::Function> ctor(v8::Function::Cast(*ctorObj));
        if (!ctor->IsNull()) {
            Frame* frame = object->rootObject->frame();
            ASSERT(frame);
            OwnArrayPtr<v8::Handle<v8::Value> > argv = createValueListFromVariantArgs(arguments, argumentCount, npObject);
            resultObject = V8ObjectConstructor::newInstanceInDocument(ctor, argumentCount, argv.get(), frame ? frame->document() : 0);
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

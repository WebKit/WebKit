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

#include "V8NPObject.h"

#include "HTMLPlugInElement.h"
#include "NPV8Object.h"
#include "V8CustomBinding.h"
#include "V8DOMMap.h"
#include "V8HTMLAppletElement.h"
#include "V8HTMLEmbedElement.h"
#include "V8HTMLObjectElement.h"
#include "V8Helpers.h"
#include "V8NPUtils.h"
#include "V8Proxy.h"
#include "npruntime_impl.h"
#include "npruntime_priv.h"
#include "wtf/OwnArrayPtr.h"

using namespace WebCore;

enum InvokeFunctionType {
    InvokeMethod = 1,
    InvokeConstruct = 2,
    InvokeDefault = 3
};

// FIXME: need comments.
// Params: holder could be HTMLEmbedElement or NPObject
static v8::Handle<v8::Value> npObjectInvokeImpl(const v8::Arguments& args, InvokeFunctionType functionId)
{
    NPObject* npObject;

    // These three types are subtypes of HTMLPlugInElement.
    if (V8HTMLAppletElement::HasInstance(args.Holder()) || V8HTMLEmbedElement::HasInstance(args.Holder())
        || V8HTMLObjectElement::HasInstance(args.Holder())) {
        // The holder object is a subtype of HTMLPlugInElement.
        HTMLPlugInElement* element = V8DOMWrapper::convertDOMWrapperToNode<HTMLPlugInElement>(args.Holder());
        ScriptInstance scriptInstance = element->getInstance();
        if (scriptInstance)
            npObject = V8DOMWrapper::convertToNativeObject<NPObject>(V8ClassIndex::NPOBJECT, scriptInstance->instance());
        else
            npObject = 0;
    } else {
        // The holder object is not a subtype of HTMLPlugInElement, it must be an NPObject which has three
        // internal fields.
        if (args.Holder()->InternalFieldCount() != V8Custom::kNPObjectInternalFieldCount)
          return throwError("NPMethod called on non-NPObject", V8Proxy::ReferenceError);

        npObject = V8DOMWrapper::convertToNativeObject<NPObject>(V8ClassIndex::NPOBJECT, args.Holder());
    }

    // Verify that our wrapper wasn't using a NPObject which has already been deleted.
    if (!npObject || !_NPN_IsAlive(npObject))
        return throwError("NPObject deleted", V8Proxy::ReferenceError);

    // Wrap up parameters.
    int numArgs = args.Length();
    OwnArrayPtr<NPVariant> npArgs(new NPVariant[numArgs]);

    for (int i = 0; i < numArgs; i++)
        convertV8ObjectToNPVariant(args[i], npObject, &npArgs[i]);

    NPVariant result;
    VOID_TO_NPVARIANT(result);

    bool retval = true;
    switch (functionId) {
    case InvokeMethod:
        if (npObject->_class->invoke) {
            v8::Handle<v8::String> functionName(v8::String::Cast(*args.Data()));
            NPIdentifier identifier = getStringIdentifier(functionName);
            retval = npObject->_class->invoke(npObject, identifier, npArgs.get(), numArgs, &result);
        }
        break;
    case InvokeConstruct:
        if (npObject->_class->construct)
            retval = npObject->_class->construct(npObject, npArgs.get(), numArgs, &result);
        break;
    case InvokeDefault:
        if (npObject->_class->invokeDefault)
            retval = npObject->_class->invokeDefault(npObject, npArgs.get(), numArgs, &result);
        break;
    default:
        break;
    }

    if (!retval)
        throwError("Error calling method on NPObject!", V8Proxy::GeneralError);

    for (int i=0; i < numArgs; i++)
        _NPN_ReleaseVariantValue(&npArgs[i]);

    // Unwrap return values.
    v8::Handle<v8::Value> returnValue = convertNPVariantToV8Object(&result, npObject);
    _NPN_ReleaseVariantValue(&result);

    return returnValue;
}


v8::Handle<v8::Value> npObjectMethodHandler(const v8::Arguments& args)
{
    return npObjectInvokeImpl(args, InvokeMethod);
}


v8::Handle<v8::Value> npObjectInvokeDefaultHandler(const v8::Arguments& args)
{
    if (args.IsConstructCall())
        return npObjectInvokeImpl(args, InvokeConstruct);
    else
        return npObjectInvokeImpl(args, InvokeDefault);
}


static void weakTemplateCallback(v8::Persistent<v8::Value>, void* parameter);

// NPIdentifier is PrivateIdentifier*.
static WeakReferenceMap<PrivateIdentifier, v8::FunctionTemplate> staticTemplateMap(&weakTemplateCallback);

static void weakTemplateCallback(v8::Persistent<v8::Value> object, void* parameter)
{
    PrivateIdentifier* identifier = static_cast<PrivateIdentifier*>(parameter);
    ASSERT(identifier);
    ASSERT(staticTemplateMap.contains(identifier));

    staticTemplateMap.forget(identifier);
}


static v8::Handle<v8::Value> npObjectGetProperty(v8::Local<v8::Object> self, NPIdentifier identifier, v8::Local<v8::Value> key)
{
    NPObject* npObject = V8DOMWrapper::convertToNativeObject<NPObject>(V8ClassIndex::NPOBJECT, self);

    // Verify that our wrapper wasn't using a NPObject which
    // has already been deleted.
    if (!npObject || !_NPN_IsAlive(npObject))
        return throwError("NPObject deleted", V8Proxy::ReferenceError);


    if (npObject->_class->hasProperty && npObject->_class->hasProperty(npObject, identifier)
        && npObject->_class->getProperty) {

        NPVariant result;
        VOID_TO_NPVARIANT(result);
        if (!npObject->_class->getProperty(npObject, identifier, &result))
            return v8::Handle<v8::Value>();

        v8::Handle<v8::Value> returnValue = convertNPVariantToV8Object(&result, npObject);
        _NPN_ReleaseVariantValue(&result);
        return returnValue;

    } else if (key->IsString() && npObject->_class->hasMethod && npObject->_class->hasMethod(npObject, identifier)) {
        PrivateIdentifier* id = static_cast<PrivateIdentifier*>(identifier);
        v8::Persistent<v8::FunctionTemplate> functionTemplate = staticTemplateMap.get(id);
        // Cache templates using identifier as the key.
        if (functionTemplate.IsEmpty()) {
            // Create a new template.
            v8::Local<v8::FunctionTemplate> temp = v8::FunctionTemplate::New();
            temp->SetCallHandler(npObjectMethodHandler, key);
            functionTemplate = v8::Persistent<v8::FunctionTemplate>::New(temp);
            staticTemplateMap.set(id, functionTemplate);
        }

        // FunctionTemplate caches function for each context.
        v8::Local<v8::Function> v8Function = functionTemplate->GetFunction();
        v8Function->SetName(v8::Handle<v8::String>::Cast(key));
        return v8Function;
    }

    return v8::Handle<v8::Value>();
}

v8::Handle<v8::Value> npObjectNamedPropertyGetter(v8::Local<v8::String> name, const v8::AccessorInfo& info)
{
    NPIdentifier identifier = getStringIdentifier(name);
    return npObjectGetProperty(info.Holder(), identifier, name);
}

v8::Handle<v8::Value> npObjectIndexedPropertyGetter(uint32_t index, const v8::AccessorInfo& info)
{
    NPIdentifier identifier = _NPN_GetIntIdentifier(index);
    return npObjectGetProperty(info.Holder(), identifier, v8::Number::New(index));
}

v8::Handle<v8::Value> npObjectGetNamedProperty(v8::Local<v8::Object> self, v8::Local<v8::String> name)
{
    NPIdentifier identifier = getStringIdentifier(name);
    return npObjectGetProperty(self, identifier, name);
}

v8::Handle<v8::Value> npObjectGetIndexedProperty(v8::Local<v8::Object> self, uint32_t index)
{
    NPIdentifier identifier = _NPN_GetIntIdentifier(index);
    return npObjectGetProperty(self, identifier, v8::Number::New(index));
}

static v8::Handle<v8::Value> npObjectSetProperty(v8::Local<v8::Object> self, NPIdentifier identifier, v8::Local<v8::Value> value)
{
    NPObject* npObject = V8DOMWrapper::convertToNativeObject<NPObject>(V8ClassIndex::NPOBJECT, self);

    // Verify that our wrapper wasn't using a NPObject which has already been deleted.
    if (!npObject || !_NPN_IsAlive(npObject)) {
        throwError("NPObject deleted", V8Proxy::ReferenceError);
        return value;  // Intercepted, but an exception was thrown.
    }

    if (npObject->_class->hasProperty && npObject->_class->hasProperty(npObject, identifier)
        && npObject->_class->setProperty) {

        NPVariant npValue;
        VOID_TO_NPVARIANT(npValue);
        convertV8ObjectToNPVariant(value, npObject, &npValue);
        bool success = npObject->_class->setProperty(npObject, identifier, &npValue);
        _NPN_ReleaseVariantValue(&npValue);
        if (success)
            return value; // Intercept the call.
    }
    return notHandledByInterceptor();
}


v8::Handle<v8::Value> npObjectNamedPropertySetter(v8::Local<v8::String> name, v8::Local<v8::Value> value, const v8::AccessorInfo& info)
{
    NPIdentifier identifier = getStringIdentifier(name);
    return npObjectSetProperty(info.Holder(), identifier, value);
}


v8::Handle<v8::Value> npObjectIndexedPropertySetter(uint32_t index, v8::Local<v8::Value> value, const v8::AccessorInfo& info)
{
    NPIdentifier identifier = _NPN_GetIntIdentifier(index);
    return npObjectSetProperty(info.Holder(), identifier, value);
}

v8::Handle<v8::Value> npObjectSetNamedProperty(v8::Local<v8::Object> self, v8::Local<v8::String> name, v8::Local<v8::Value> value)
{
    NPIdentifier identifier = getStringIdentifier(name);
    return npObjectSetProperty(self, identifier, value);
}

v8::Handle<v8::Value> npObjectSetIndexedProperty(v8::Local<v8::Object> self, uint32_t index, v8::Local<v8::Value> value)
{
    NPIdentifier identifier = _NPN_GetIntIdentifier(index);
    return npObjectSetProperty(self, identifier, value);
}


static void weakNPObjectCallback(v8::Persistent<v8::Value>, void* parameter);

static DOMWrapperMap<NPObject> staticNPObjectMap(&weakNPObjectCallback);

static void weakNPObjectCallback(v8::Persistent<v8::Value> object, void* parameter)
{
    NPObject* npObject = static_cast<NPObject*>(parameter);
    ASSERT(staticNPObjectMap.contains(npObject));
    ASSERT(npObject);

    // Must remove from our map before calling _NPN_ReleaseObject(). _NPN_ReleaseObject can call ForgetV8ObjectForNPObject, which
    // uses the table as well.
    staticNPObjectMap.forget(npObject);

    if (_NPN_IsAlive(npObject))
        _NPN_ReleaseObject(npObject);
}


v8::Local<v8::Object> createV8ObjectForNPObject(NPObject* object, NPObject* root)
{
    static v8::Persistent<v8::FunctionTemplate> npObjectDesc;

    ASSERT(v8::Context::InContext());

    // If this is a v8 object, just return it.
    if (object->_class == npScriptObjectClass) {
        V8NPObject* v8NPObject = reinterpret_cast<V8NPObject*>(object);
        return v8::Local<v8::Object>::New(v8NPObject->v8Object);
    }

    // If we've already wrapped this object, just return it.
    if (staticNPObjectMap.contains(object))
        return v8::Local<v8::Object>::New(staticNPObjectMap.get(object));

    // FIXME: we should create a Wrapper type as a subclass of JSObject. It has two internal fields, field 0 is the wrapped
    // pointer, and field 1 is the type. There should be an api function that returns unused type id. The same Wrapper type
    // can be used by DOM bindings.
    if (npObjectDesc.IsEmpty()) {
        npObjectDesc = v8::Persistent<v8::FunctionTemplate>::New(v8::FunctionTemplate::New());
        npObjectDesc->InstanceTemplate()->SetInternalFieldCount(V8Custom::kNPObjectInternalFieldCount);
        npObjectDesc->InstanceTemplate()->SetNamedPropertyHandler(npObjectNamedPropertyGetter, npObjectNamedPropertySetter);
        npObjectDesc->InstanceTemplate()->SetIndexedPropertyHandler(npObjectIndexedPropertyGetter, npObjectIndexedPropertySetter);
        npObjectDesc->InstanceTemplate()->SetCallAsFunctionHandler(npObjectInvokeDefaultHandler);
    }

    v8::Handle<v8::Function> v8Function = npObjectDesc->GetFunction();
    v8::Local<v8::Object> value = SafeAllocation::newInstance(v8Function);

    // If we were unable to allocate the instance, we avoid wrapping and registering the NP object.
    if (value.IsEmpty())
        return value;

    wrapNPObject(value, object);

    // KJS retains the object as part of its wrapper (see Bindings::CInstance).
    _NPN_RetainObject(object);

    _NPN_RegisterObject(object, root);

    // Maintain a weak pointer for v8 so we can cleanup the object.
    v8::Persistent<v8::Object> weakRef = v8::Persistent<v8::Object>::New(value);
    staticNPObjectMap.set(object, weakRef);

    return value;
}

void forgetV8ObjectForNPObject(NPObject* object)
{
    if (staticNPObjectMap.contains(object)) {
        v8::HandleScope scope;
        v8::Persistent<v8::Object> handle(staticNPObjectMap.get(object));
        V8DOMWrapper::setDOMWrapper(handle, WebCore::V8ClassIndex::NPOBJECT, 0);
        staticNPObjectMap.forget(object);
        _NPN_ReleaseObject(object);
    }
}

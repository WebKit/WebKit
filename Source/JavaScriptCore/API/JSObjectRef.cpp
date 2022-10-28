/*
 * Copyright (C) 2006-2020 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Kelvin W Sherlock (ksherlock@gmail.com)
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "JSObjectRef.h"
#include "JSObjectRefPrivate.h"

#include "APICast.h"
#include "APIUtils.h"
#include "DateConstructor.h"
#include "FunctionConstructor.h"
#include "Identifier.h"
#include "InitializeThreading.h"
#include "JSAPIWrapperObject.h"
#include "JSArray.h"
#include "JSCInlines.h"
#include "JSCallbackConstructor.h"
#include "JSCallbackFunction.h"
#include "JSCallbackObject.h"
#include "JSClassRef.h"
#include "JSPromise.h"
#include "JSString.h"
#include "ObjectConstructor.h"
#include "ObjectPrototype.h"
#include "PropertyNameArray.h"
#include "ProxyObject.h"
#include "RegExpConstructor.h"

#if ENABLE(REMOTE_INSPECTOR)
#include "JSGlobalObjectInspectorController.h"
#endif

using namespace JSC;

JSClassRef JSClassCreate(const JSClassDefinition* definition)
{
    JSC::initialize();
    auto jsClass = (definition->attributes & kJSClassAttributeNoAutomaticPrototype)
        ? OpaqueJSClass::createNoAutomaticPrototype(definition)
        : OpaqueJSClass::create(definition);
    
    return &jsClass.leakRef();
}

JSClassRef JSClassRetain(JSClassRef jsClass)
{
    jsClass->ref();
    return jsClass;
}

void JSClassRelease(JSClassRef jsClass)
{
    jsClass->deref();
}

JSObjectRef JSObjectMake(JSContextRef ctx, JSClassRef jsClass, void* data)
{
    if (!ctx) {
        ASSERT_NOT_REACHED();
        return nullptr;
    }
    JSGlobalObject* globalObject = toJS(ctx);
    VM& vm = globalObject->vm();


    if (!jsClass)
        return toRef(constructEmptyObject(globalObject));

    JSCallbackObject<JSNonFinalObject>* object = JSCallbackObject<JSNonFinalObject>::create(globalObject, globalObject->callbackObjectStructure(), jsClass, data);
    if (JSObject* prototype = jsClass->prototype(globalObject))
        object->setPrototypeDirect(vm, prototype);

    return toRef(object);
}

JSObjectRef JSObjectMakeFunctionWithCallback(JSContextRef ctx, JSStringRef name, JSObjectCallAsFunctionCallback callAsFunction)
{
    if (!ctx) {
        ASSERT_NOT_REACHED();
        return nullptr;
    }
    JSGlobalObject* globalObject = toJS(ctx);
    VM& vm = globalObject->vm();

    return toRef(JSCallbackFunction::create(vm, globalObject, callAsFunction, name ? name->string() : "anonymous"_s));
}

JSObjectRef JSObjectMakeConstructor(JSContextRef ctx, JSClassRef jsClass, JSObjectCallAsConstructorCallback callAsConstructor)
{
    if (!ctx) {
        ASSERT_NOT_REACHED();
        return nullptr;
    }
    JSGlobalObject* globalObject = toJS(ctx);
    VM& vm = globalObject->vm();


    JSValue jsPrototype = jsClass ? jsClass->prototype(globalObject) : nullptr;
    if (!jsPrototype)
        jsPrototype = globalObject->objectPrototype();

    JSCallbackConstructor* constructor = JSCallbackConstructor::create(globalObject, globalObject->callbackConstructorStructure(), jsClass, callAsConstructor);
    constructor->putDirect(vm, vm.propertyNames->prototype, jsPrototype, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly);
    return toRef(constructor);
}

JSObjectRef JSObjectMakeFunction(JSContextRef ctx, JSStringRef name, unsigned parameterCount, const JSStringRef parameterNames[], JSStringRef body, JSStringRef sourceURLString, int startingLineNumber, JSValueRef* exception)
{
    if (!ctx) {
        ASSERT_NOT_REACHED();
        return nullptr;
    }
    JSGlobalObject* globalObject = toJS(ctx);
    VM& vm = globalObject->vm();

    auto scope = DECLARE_CATCH_SCOPE(vm);

    startingLineNumber = std::max(1, startingLineNumber);
    Identifier nameID = name ? name->identifier(&vm) : Identifier::fromString(vm, "anonymous"_s);
    
    MarkedArgumentBuffer args;
    for (unsigned i = 0; i < parameterCount; i++)
        args.append(parameterNames[i]->isExternal() ? jsOwnedString(vm, parameterNames[i]->string()) : jsString(vm, parameterNames[i]->string()));
    args.append(body->isExternal() ? jsOwnedString(vm, body->string()) : jsString(vm, body->string()));
    if (UNLIKELY(args.hasOverflowed())) {
        auto throwScope = DECLARE_THROW_SCOPE(vm);
        throwOutOfMemoryError(globalObject, throwScope);
        handleExceptionIfNeeded(scope, ctx, exception);
        return nullptr;
    }

    auto sourceURL = sourceURLString ? URL({ }, sourceURLString->string()) : URL();
    JSObject* result = constructFunction(globalObject, args, nameID, SourceOrigin { sourceURL }, sourceURL.string(), TextPosition(OrdinalNumber::fromOneBasedInt(startingLineNumber), OrdinalNumber()));
    if (handleExceptionIfNeeded(scope, ctx, exception) == ExceptionStatus::DidThrow)
        result = nullptr;
    return toRef(result);
}

JSObjectRef JSObjectMakeArray(JSContextRef ctx, size_t argumentCount, const JSValueRef arguments[],  JSValueRef* exception)
{
    if (!ctx) {
        ASSERT_NOT_REACHED();
        return nullptr;
    }
    JSGlobalObject* globalObject = toJS(ctx);
    VM& vm = globalObject->vm();

    auto scope = DECLARE_CATCH_SCOPE(vm);

    JSObject* result;
    if (argumentCount) {
        MarkedArgumentBuffer argList;
        for (size_t i = 0; i < argumentCount; ++i)
            argList.append(toJS(globalObject, arguments[i]));
        if (UNLIKELY(argList.hasOverflowed())) {
            auto throwScope = DECLARE_THROW_SCOPE(vm);
            throwOutOfMemoryError(globalObject, throwScope);
            handleExceptionIfNeeded(scope, ctx, exception);
            return nullptr;
        }

        result = constructArray(globalObject, static_cast<ArrayAllocationProfile*>(nullptr), argList);
    } else
        result = constructEmptyArray(globalObject, nullptr);

    if (handleExceptionIfNeeded(scope, ctx, exception) == ExceptionStatus::DidThrow)
        result = nullptr;

    return toRef(result);
}

JSObjectRef JSObjectMakeDate(JSContextRef ctx, size_t argumentCount, const JSValueRef arguments[],  JSValueRef* exception)
{
    if (!ctx) {
        ASSERT_NOT_REACHED();
        return nullptr;
    }
    JSGlobalObject* globalObject = toJS(ctx);
    VM& vm = globalObject->vm();

    auto scope = DECLARE_CATCH_SCOPE(vm);

    MarkedArgumentBuffer argList;
    for (size_t i = 0; i < argumentCount; ++i)
        argList.append(toJS(globalObject, arguments[i]));
    if (UNLIKELY(argList.hasOverflowed())) {
        auto throwScope = DECLARE_THROW_SCOPE(vm);
        throwOutOfMemoryError(globalObject, throwScope);
        handleExceptionIfNeeded(scope, ctx, exception);
        return nullptr;
    }

    JSObject* result = constructDate(globalObject, JSValue(), argList);
    if (handleExceptionIfNeeded(scope, ctx, exception) == ExceptionStatus::DidThrow)
        result = nullptr;

    return toRef(result);
}

JSObjectRef JSObjectMakeError(JSContextRef ctx, size_t argumentCount, const JSValueRef arguments[],  JSValueRef* exception)
{
    if (!ctx) {
        ASSERT_NOT_REACHED();
        return nullptr;
    }
    JSGlobalObject* globalObject = toJS(ctx);
    VM& vm = globalObject->vm();

    auto scope = DECLARE_CATCH_SCOPE(vm);

    JSValue message = argumentCount ? toJS(globalObject, arguments[0]) : jsUndefined();
    JSValue options = argumentCount > 1 ? toJS(globalObject, arguments[1]) : jsUndefined();
    Structure* errorStructure = globalObject->errorStructure();
    JSObject* result = ErrorInstance::create(globalObject, errorStructure, message, options);

    if (handleExceptionIfNeeded(scope, ctx, exception) == ExceptionStatus::DidThrow)
        result = nullptr;

    return toRef(result);
}

JSObjectRef JSObjectMakeRegExp(JSContextRef ctx, size_t argumentCount, const JSValueRef arguments[],  JSValueRef* exception)
{
    if (!ctx) {
        ASSERT_NOT_REACHED();
        return nullptr;
    }
    JSGlobalObject* globalObject = toJS(ctx);
    VM& vm = globalObject->vm();

    auto scope = DECLARE_CATCH_SCOPE(vm);

    MarkedArgumentBuffer argList;
    for (size_t i = 0; i < argumentCount; ++i)
        argList.append(toJS(globalObject, arguments[i]));
    if (UNLIKELY(argList.hasOverflowed())) {
        auto throwScope = DECLARE_THROW_SCOPE(vm);
        throwOutOfMemoryError(globalObject, throwScope);
        handleExceptionIfNeeded(scope, ctx, exception);
        return nullptr;
    }

    JSObject* result = constructRegExp(globalObject, argList);
    if (handleExceptionIfNeeded(scope, ctx, exception) == ExceptionStatus::DidThrow)
        result = nullptr;
    
    return toRef(result);
}

JSObjectRef JSObjectMakeDeferredPromise(JSContextRef ctx, JSObjectRef* resolve, JSObjectRef* reject, JSValueRef* exception)
{
    if (!ctx) {
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    JSGlobalObject* globalObject = toJS(ctx);
    VM& vm = globalObject->vm();

    auto scope = DECLARE_CATCH_SCOPE(vm);

    JSPromise::DeferredData data = JSPromise::createDeferredData(globalObject, globalObject->promiseConstructor());
    if (handleExceptionIfNeeded(scope, ctx, exception) == ExceptionStatus::DidThrow)
        return nullptr;

    if (resolve)
        *resolve = toRef(data.resolve);
    if (reject)
        *reject = toRef(data.reject);
    return toRef(data.promise);
}

JSValueRef JSObjectGetPrototype(JSContextRef ctx, JSObjectRef object)
{
    if (!ctx) {
        ASSERT_NOT_REACHED();
        return nullptr;
    }
    JSGlobalObject* globalObject = toJS(ctx);


    JSObject* jsObject = toJS(object); 
    return toRef(globalObject, jsObject->getPrototypeDirect());
}

void JSObjectSetPrototype(JSContextRef ctx, JSObjectRef object, JSValueRef value)
{
    if (!ctx) {
        ASSERT_NOT_REACHED();
        return;
    }
    JSGlobalObject* globalObject = toJS(ctx);
    VM& vm = globalObject->vm();

    auto scope = DECLARE_CATCH_SCOPE(vm);

    JSObject* jsObject = toJS(object);
    JSValue jsValue = toJS(globalObject, value);
    jsObject->setPrototype(vm, globalObject, jsValue.isObject() ? jsValue : jsNull());
    handleExceptionIfNeeded(scope, ctx, nullptr);
}

bool JSObjectHasProperty(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName)
{
    if (!ctx) {
        ASSERT_NOT_REACHED();
        return false;
    }
    JSGlobalObject* globalObject = toJS(ctx);
    VM& vm = globalObject->vm();


    JSObject* jsObject = toJS(object);
    
    return jsObject->hasProperty(globalObject, propertyName->identifier(&vm));
}

JSValueRef JSObjectGetProperty(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef* exception)
{
    if (!ctx || !object) {
        ASSERT_NOT_REACHED();
        return nullptr;
    }
    JSGlobalObject* globalObject = toJS(ctx);
    VM& vm = globalObject->vm();

    auto scope = DECLARE_CATCH_SCOPE(vm);

    JSObject* jsObject = toJS(object);

    JSValue jsValue = jsObject->get(globalObject, propertyName->identifier(&vm));
    if (handleExceptionIfNeeded(scope, ctx, exception) == ExceptionStatus::DidThrow)
        return nullptr;
    return toRef(globalObject, jsValue);
}

void JSObjectSetProperty(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef value, JSPropertyAttributes attributes, JSValueRef* exception)
{
    if (!ctx) {
        ASSERT_NOT_REACHED();
        return;
    }
    JSGlobalObject* globalObject = toJS(ctx);
    VM& vm = globalObject->vm();

    auto scope = DECLARE_CATCH_SCOPE(vm);

    JSObject* jsObject = toJS(object);
    Identifier name(propertyName->identifier(&vm));
    JSValue jsValue = toJS(globalObject, value);

    bool doesNotHaveProperty = attributes && !jsObject->hasProperty(globalObject, name);
    if (LIKELY(!scope.exception())) {
        if (doesNotHaveProperty) {
            PropertyDescriptor desc(jsValue, attributes);
            jsObject->methodTable()->defineOwnProperty(jsObject, globalObject, name, desc, false);
        } else {
            PutPropertySlot slot(jsObject);
            jsObject->methodTable()->put(jsObject, globalObject, name, jsValue, slot);
        }
    }
    handleExceptionIfNeeded(scope, ctx, exception);
}

bool JSObjectHasPropertyForKey(JSContextRef ctx, JSObjectRef object, JSValueRef key, JSValueRef* exception)
{
    if (!ctx) {
        ASSERT_NOT_REACHED();
        return false;
    }
    JSGlobalObject* globalObject = toJS(ctx);
    VM& vm = globalObject->vm();

    auto scope = DECLARE_CATCH_SCOPE(vm);

    JSObject* jsObject = toJS(object);
    Identifier ident = toJS(globalObject, key).toPropertyKey(globalObject);
    if (handleExceptionIfNeeded(scope, ctx, exception) == ExceptionStatus::DidThrow)
        return false;

    bool result = jsObject->hasProperty(globalObject, ident);
    if (handleExceptionIfNeeded(scope, ctx, exception) == ExceptionStatus::DidThrow)
        return false;
    return result;
}

JSValueRef JSObjectGetPropertyForKey(JSContextRef ctx, JSObjectRef object, JSValueRef key, JSValueRef* exception)
{
    if (!ctx) {
        ASSERT_NOT_REACHED();
        return nullptr;
    }
    JSGlobalObject* globalObject = toJS(ctx);
    VM& vm = globalObject->vm();

    auto scope = DECLARE_CATCH_SCOPE(vm);

    JSObject* jsObject = toJS(object);
    Identifier ident = toJS(globalObject, key).toPropertyKey(globalObject);
    if (handleExceptionIfNeeded(scope, ctx, exception) == ExceptionStatus::DidThrow)
        return nullptr;

    JSValue jsValue = jsObject->get(globalObject, ident);
    if (handleExceptionIfNeeded(scope, ctx, exception) == ExceptionStatus::DidThrow)
        return nullptr;
    return toRef(globalObject, jsValue);
}

void JSObjectSetPropertyForKey(JSContextRef ctx, JSObjectRef object, JSValueRef key, JSValueRef value, JSPropertyAttributes attributes, JSValueRef* exception)
{
    if (!ctx) {
        ASSERT_NOT_REACHED();
        return;
    }
    JSGlobalObject* globalObject = toJS(ctx);
    VM& vm = globalObject->vm();

    auto scope = DECLARE_CATCH_SCOPE(vm);

    JSObject* jsObject = toJS(object);
    JSValue jsValue = toJS(globalObject, value);

    Identifier ident = toJS(globalObject, key).toPropertyKey(globalObject);
    if (handleExceptionIfNeeded(scope, ctx, exception) == ExceptionStatus::DidThrow)
        return;

    bool doesNotHaveProperty = attributes && !jsObject->hasProperty(globalObject, ident);
    if (LIKELY(!scope.exception())) {
        if (doesNotHaveProperty) {
            PropertyDescriptor desc(jsValue, attributes);
            jsObject->methodTable()->defineOwnProperty(jsObject, globalObject, ident, desc, false);
        } else {
            PutPropertySlot slot(jsObject);
            jsObject->methodTable()->put(jsObject, globalObject, ident, jsValue, slot);
        }
    }
    handleExceptionIfNeeded(scope, ctx, exception);
}

bool JSObjectDeletePropertyForKey(JSContextRef ctx, JSObjectRef object, JSValueRef key, JSValueRef* exception)
{
    if (!ctx) {
        ASSERT_NOT_REACHED();
        return false;
    }
    JSGlobalObject* globalObject = toJS(ctx);
    VM& vm = globalObject->vm();

    auto scope = DECLARE_CATCH_SCOPE(vm);

    JSObject* jsObject = toJS(object);
    Identifier ident = toJS(globalObject, key).toPropertyKey(globalObject);
    if (handleExceptionIfNeeded(scope, ctx, exception) == ExceptionStatus::DidThrow)
        return false;

    bool result = JSCell::deleteProperty(jsObject, globalObject, ident);
    if (handleExceptionIfNeeded(scope, ctx, exception) == ExceptionStatus::DidThrow)
        return false;
    return result;
}

JSValueRef JSObjectGetPropertyAtIndex(JSContextRef ctx, JSObjectRef object, unsigned propertyIndex, JSValueRef* exception)
{
    if (!ctx) {
        ASSERT_NOT_REACHED();
        return nullptr;
    }
    JSGlobalObject* globalObject = toJS(ctx);
    VM& vm = globalObject->vm();

    auto scope = DECLARE_CATCH_SCOPE(vm);

    JSObject* jsObject = toJS(object);

    JSValue jsValue = jsObject->get(globalObject, propertyIndex);
    if (handleExceptionIfNeeded(scope, ctx, exception) == ExceptionStatus::DidThrow)
        return nullptr;
    return toRef(globalObject, jsValue);
}


void JSObjectSetPropertyAtIndex(JSContextRef ctx, JSObjectRef object, unsigned propertyIndex, JSValueRef value, JSValueRef* exception)
{
    if (!ctx) {
        ASSERT_NOT_REACHED();
        return;
    }
    JSGlobalObject* globalObject = toJS(ctx);
    VM& vm = globalObject->vm();

    auto scope = DECLARE_CATCH_SCOPE(vm);

    JSObject* jsObject = toJS(object);
    JSValue jsValue = toJS(globalObject, value);
    
    jsObject->methodTable()->putByIndex(jsObject, globalObject, propertyIndex, jsValue, false);
    handleExceptionIfNeeded(scope, ctx, exception);
}

bool JSObjectDeleteProperty(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef* exception)
{
    if (!ctx) {
        ASSERT_NOT_REACHED();
        return false;
    }
    JSGlobalObject* globalObject = toJS(ctx);
    VM& vm = globalObject->vm();

    auto scope = DECLARE_CATCH_SCOPE(vm);

    JSObject* jsObject = toJS(object);

    bool result = JSCell::deleteProperty(jsObject, globalObject, propertyName->identifier(&vm));
    if (handleExceptionIfNeeded(scope, ctx, exception) == ExceptionStatus::DidThrow)
        return false;
    return result;
}

// API objects have private properties, which may get accessed during destruction. This
// helper lets us get the ClassInfo of an API object from a function that may get called
// during destruction.
static const ClassInfo* classInfoPrivate(JSObject* jsObject)
{
    VM& vm = jsObject->vm();
    if (vm.currentlyDestructingCallbackObject != jsObject)
        return jsObject->classInfo();

    return vm.currentlyDestructingCallbackObjectClassInfo;
}

void* JSObjectGetPrivate(JSObjectRef object)
{
    JSObject* jsObject = uncheckedToJS(object);

    const ClassInfo* classInfo = classInfoPrivate(jsObject);
    
    // Get wrapped object if proxied
    if (classInfo->isSubClassOf(JSProxy::info())) {
        jsObject = static_cast<JSProxy*>(jsObject)->target();
        classInfo = jsObject->classInfo();
    }

    if (classInfo->isSubClassOf(JSCallbackObject<JSGlobalObject>::info()))
        return static_cast<JSCallbackObject<JSGlobalObject>*>(jsObject)->getPrivate();
    if (classInfo->isSubClassOf(JSCallbackObject<JSNonFinalObject>::info()))
        return static_cast<JSCallbackObject<JSNonFinalObject>*>(jsObject)->getPrivate();
#if JSC_OBJC_API_ENABLED
    if (classInfo->isSubClassOf(JSCallbackObject<JSAPIWrapperObject>::info()))
        return static_cast<JSCallbackObject<JSAPIWrapperObject>*>(jsObject)->getPrivate();
#endif
    
    return nullptr;
}

bool JSObjectSetPrivate(JSObjectRef object, void* data)
{
    JSObject* jsObject = uncheckedToJS(object);

    const ClassInfo* classInfo = classInfoPrivate(jsObject);
    
    // Get wrapped object if proxied
    if (classInfo->isSubClassOf(JSProxy::info())) {
        jsObject = static_cast<JSProxy*>(jsObject)->target();
        classInfo = jsObject->classInfo();
    }

    if (classInfo->isSubClassOf(JSCallbackObject<JSGlobalObject>::info())) {
        static_cast<JSCallbackObject<JSGlobalObject>*>(jsObject)->setPrivate(data);
        return true;
    }
    if (classInfo->isSubClassOf(JSCallbackObject<JSNonFinalObject>::info())) {
        static_cast<JSCallbackObject<JSNonFinalObject>*>(jsObject)->setPrivate(data);
        return true;
    }
#if JSC_OBJC_API_ENABLED
    if (classInfo->isSubClassOf(JSCallbackObject<JSAPIWrapperObject>::info())) {
        static_cast<JSCallbackObject<JSAPIWrapperObject>*>(jsObject)->setPrivate(data);
        return true;
    }
#endif
        
    return false;
}

JSValueRef JSObjectGetPrivateProperty(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName)
{
    JSGlobalObject* globalObject = toJS(ctx);
    VM& vm = globalObject->vm();

    JSObject* jsObject = toJS(object);
    JSValue result;
    Identifier name(propertyName->identifier(&vm));


    // Get wrapped object if proxied
    if (jsObject->inherits<JSProxy>())
        jsObject = jsCast<JSProxy*>(jsObject)->target();

    if (jsObject->inherits<JSCallbackObject<JSGlobalObject>>())
        result = jsCast<JSCallbackObject<JSGlobalObject>*>(jsObject)->getPrivateProperty(name);
    else if (jsObject->inherits<JSCallbackObject<JSNonFinalObject>>())
        result = jsCast<JSCallbackObject<JSNonFinalObject>*>(jsObject)->getPrivateProperty(name);
#if JSC_OBJC_API_ENABLED
    else if (jsObject->inherits<JSCallbackObject<JSAPIWrapperObject>>())
        result = jsCast<JSCallbackObject<JSAPIWrapperObject>*>(jsObject)->getPrivateProperty(name);
#endif
    return toRef(globalObject, result);
}

bool JSObjectSetPrivateProperty(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef value)
{
    JSGlobalObject* globalObject = toJS(ctx);
    VM& vm = globalObject->vm();

    JSObject* jsObject = toJS(object);
    JSValue jsValue = value ? toJS(globalObject, value) : JSValue();
    Identifier name(propertyName->identifier(&vm));

    // Get wrapped object if proxied
    if (jsObject->inherits<JSProxy>())
        jsObject = jsCast<JSProxy*>(jsObject)->target();

    if (jsObject->inherits<JSCallbackObject<JSGlobalObject>>()) {
        jsCast<JSCallbackObject<JSGlobalObject>*>(jsObject)->setPrivateProperty(vm, name, jsValue);
        return true;
    }
    if (jsObject->inherits<JSCallbackObject<JSNonFinalObject>>()) {
        jsCast<JSCallbackObject<JSNonFinalObject>*>(jsObject)->setPrivateProperty(vm, name, jsValue);
        return true;
    }
#if JSC_OBJC_API_ENABLED
    if (jsObject->inherits<JSCallbackObject<JSAPIWrapperObject>>()) {
        jsCast<JSCallbackObject<JSAPIWrapperObject>*>(jsObject)->setPrivateProperty(vm, name, jsValue);
        return true;
    }
#endif
    return false;
}

bool JSObjectDeletePrivateProperty(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName)
{
    JSGlobalObject* globalObject = toJS(ctx);
    VM& vm = globalObject->vm();

    JSObject* jsObject = toJS(object);
    Identifier name(propertyName->identifier(&vm));

    // Get wrapped object if proxied
    if (jsObject->inherits<JSProxy>())
        jsObject = jsCast<JSProxy*>(jsObject)->target();

    if (jsObject->inherits<JSCallbackObject<JSGlobalObject>>()) {
        jsCast<JSCallbackObject<JSGlobalObject>*>(jsObject)->deletePrivateProperty(name);
        return true;
    }
    if (jsObject->inherits<JSCallbackObject<JSNonFinalObject>>()) {
        jsCast<JSCallbackObject<JSNonFinalObject>*>(jsObject)->deletePrivateProperty(name);
        return true;
    }
#if JSC_OBJC_API_ENABLED
    if (jsObject->inherits<JSCallbackObject<JSAPIWrapperObject>>()) {
        jsCast<JSCallbackObject<JSAPIWrapperObject>*>(jsObject)->deletePrivateProperty(name);
        return true;
    }
#endif
    return false;
}

bool JSObjectIsFunction(JSContextRef ctx, JSObjectRef object)
{
    UNUSED_PARAM(ctx);
    if (!object)
        return false;
    JSCell* cell = toJS(object);
    return cell->isCallable();
}

JSValueRef JSObjectCallAsFunction(JSContextRef ctx, JSObjectRef object, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    JSGlobalObject* globalObject = toJS(ctx);
    VM& vm = globalObject->vm();

    auto scope = DECLARE_CATCH_SCOPE(vm);

    if (!object)
        return nullptr;

    JSObject* jsObject = toJS(object);
    JSObject* jsThisObject = toJS(thisObject);

    if (!jsThisObject)
        jsThisObject = globalObject->globalThis();

    MarkedArgumentBuffer argList;
    for (size_t i = 0; i < argumentCount; i++)
        argList.append(toJS(globalObject, arguments[i]));
    if (UNLIKELY(argList.hasOverflowed())) {
        auto throwScope = DECLARE_THROW_SCOPE(vm);
        throwOutOfMemoryError(globalObject, throwScope);
        handleExceptionIfNeeded(scope, ctx, exception);
        return nullptr;
    }

    auto callData = JSC::getCallData(jsObject);
    if (callData.type == CallData::Type::None)
        return nullptr;

    JSValueRef result = toRef(globalObject, profiledCall(globalObject, ProfilingReason::API, jsObject, callData, jsThisObject, argList));
    if (handleExceptionIfNeeded(scope, ctx, exception) == ExceptionStatus::DidThrow)
        result = nullptr;
    return result;
}

bool JSObjectIsConstructor(JSContextRef ctx, JSObjectRef object)
{
    UNUSED_PARAM(ctx);
    if (!object)
        return false;
    return toJS(object)->isConstructor();
}

JSObjectRef JSObjectCallAsConstructor(JSContextRef ctx, JSObjectRef object, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    JSGlobalObject* globalObject = toJS(ctx);
    VM& vm = globalObject->vm();

    auto scope = DECLARE_CATCH_SCOPE(vm);

    if (!object)
        return nullptr;

    JSObject* jsObject = toJS(object);

    auto constructData = JSC::getConstructData(jsObject);
    if (constructData.type == CallData::Type::None)
        return nullptr;

    MarkedArgumentBuffer argList;
    for (size_t i = 0; i < argumentCount; i++)
        argList.append(toJS(globalObject, arguments[i]));
    if (UNLIKELY(argList.hasOverflowed())) {
        auto throwScope = DECLARE_THROW_SCOPE(vm);
        throwOutOfMemoryError(globalObject, throwScope);
        handleExceptionIfNeeded(scope, ctx, exception);
        return nullptr;
    }

    JSObjectRef result = toRef(profiledConstruct(globalObject, ProfilingReason::API, jsObject, constructData, argList));
    if (handleExceptionIfNeeded(scope, ctx, exception) == ExceptionStatus::DidThrow)
        result = nullptr;
    return result;
}

struct OpaqueJSPropertyNameArray {
    WTF_MAKE_FAST_ALLOCATED;
public:
    // FIXME: Why not inherit from RefCounted?
    OpaqueJSPropertyNameArray(VM* vm)
        : refCount(0)
        , vm(vm)
    {
    }
    
    unsigned refCount;
    VM* vm;
    Vector<Ref<OpaqueJSString>> array;
};

JSPropertyNameArrayRef JSObjectCopyPropertyNames(JSContextRef ctx, JSObjectRef object)
{
    if (!ctx) {
        ASSERT_NOT_REACHED();
        return nullptr;
    }
    JSGlobalObject* globalObject = toJS(ctx);


    VM& vm = globalObject->vm();

    JSObject* jsObject = toJS(object);
    JSPropertyNameArrayRef propertyNames = new OpaqueJSPropertyNameArray(&vm);
    PropertyNameArray array(vm, PropertyNameMode::Strings, PrivateSymbolMode::Exclude);
    jsObject->getPropertyNames(globalObject, array, DontEnumPropertiesMode::Exclude);

    size_t size = array.size();
    propertyNames->array.reserveInitialCapacity(size);
    for (size_t i = 0; i < size; ++i)
        propertyNames->array.uncheckedAppend(OpaqueJSString::tryCreate(array[i].string()).releaseNonNull());

    return JSPropertyNameArrayRetain(propertyNames);
}

JSPropertyNameArrayRef JSPropertyNameArrayRetain(JSPropertyNameArrayRef array)
{
    ++array->refCount;
    return array;
}

void JSPropertyNameArrayRelease(JSPropertyNameArrayRef array)
{
    if (--array->refCount == 0) {
        JSLockHolder locker(array->vm);
        delete array;
    }
}

size_t JSPropertyNameArrayGetCount(JSPropertyNameArrayRef array)
{
    return array->array.size();
}

JSStringRef JSPropertyNameArrayGetNameAtIndex(JSPropertyNameArrayRef array, size_t index)
{
    return array->array[static_cast<unsigned>(index)].ptr();
}

void JSPropertyNameAccumulatorAddName(JSPropertyNameAccumulatorRef array, JSStringRef propertyName)
{
    PropertyNameArray* propertyNames = toJS(array);
    VM& vm = propertyNames->vm();

    propertyNames->add(propertyName->identifier(&vm));
}

JSObjectRef JSObjectGetProxyTarget(JSObjectRef objectRef)
{
    JSObject* object = toJS(objectRef);
    if (!object)
        return nullptr;

    JSObject* result = nullptr;
    if (JSProxy* proxy = jsDynamicCast<JSProxy*>(object))
        result = proxy->target();
    else if (ProxyObject* proxy = jsDynamicCast<ProxyObject*>(object))
        result = proxy->target();
    return toRef(result);
}

JSGlobalContextRef JSObjectGetGlobalContext(JSObjectRef objectRef)
{
    JSObject* object = toJS(objectRef);
    if (!object)
        return nullptr;
    return reinterpret_cast<JSGlobalContextRef>(object->globalObject());
}


/*
 * Copyright (C) 2010-2019 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "JSNPObject.h"

#if ENABLE(NETSCAPE_PLUGIN_API)

#include "JSNPMethod.h"
#include "NPJSObject.h"
#include "NPRuntimeObjectMap.h"
#include "NPRuntimeUtilities.h"
#include <JavaScriptCore/AuxiliaryBarrierInlines.h>
#include <JavaScriptCore/Error.h>
#include <JavaScriptCore/IdentifierInlines.h>
#include <JavaScriptCore/IsoSubspacePerVM.h>
#include <JavaScriptCore/JSDestructibleObjectHeapCellType.h>
#include <JavaScriptCore/JSGlobalObject.h>
#include <JavaScriptCore/JSLock.h>
#include <JavaScriptCore/ObjectPrototype.h>
#include <WebCore/CommonVM.h>
#include <WebCore/DOMWindow.h>
#include <WebCore/IdentifierRep.h>
#include <WebCore/JSDOMWindowBase.h>
#include <wtf/Assertions.h>
#include <wtf/text/WTFString.h>

namespace WebKit {
using namespace JSC;
using namespace WebCore;

static JSC_DECLARE_CUSTOM_GETTER(propertyGetter);
static JSC_DECLARE_CUSTOM_GETTER(methodGetter);

static NPIdentifier npIdentifierFromIdentifier(PropertyName propertyName)
{
    String name(propertyName.publicName());
    // If the propertyName is Symbol.
    if (name.isNull())
        return nullptr;
    return static_cast<NPIdentifier>(IdentifierRep::get(name.utf8().data()));
}

static JSC::Exception* throwInvalidAccessError(JSGlobalObject* lexicalGlobalObject, ThrowScope& scope)
{
    return throwException(lexicalGlobalObject, scope, createReferenceError(lexicalGlobalObject, "Trying to access object from destroyed plug-in."));
}

const ClassInfo JSNPObject::s_info = { "NPObject", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSNPObject) };

JSNPObject::JSNPObject(JSGlobalObject* globalObject, Structure* structure, NPRuntimeObjectMap* objectMap, NPObject* npObject)
    : JSDestructibleObject(globalObject->vm(), structure)
    , m_objectMap(objectMap)
    , m_npObject(npObject)
{
    ASSERT(globalObject == structure->globalObject());
}

void JSNPObject::finishCreation(JSGlobalObject* globalObject)
{
    VM& vm = globalObject->vm();
    Base::finishCreation(vm);
    ASSERT(inherits(vm, info()));

    // We should never have an NPJSObject inside a JSNPObject.
    ASSERT(!NPJSObject::isNPJSObject(m_npObject));

    retainNPObject(m_npObject);
}

JSNPObject::~JSNPObject()
{
    if (m_npObject)
        invalidate();
}

void JSNPObject::destroy(JSCell* cell)
{
    static_cast<JSNPObject*>(cell)->JSNPObject::~JSNPObject();
}

void JSNPObject::invalidate()
{
    ASSERT(m_npObject);

    releaseNPObject(m_npObject);
    m_npObject = 0;
}

NPObject* JSNPObject::leakNPObject()
{
    ASSERT(m_npObject);

    NPObject* object = m_npObject;
    m_npObject = 0;
    return object;
}

JSValue JSNPObject::callMethod(JSGlobalObject* lexicalGlobalObject, CallFrame* callFrame, NPIdentifier methodName)
{
    VM& vm = lexicalGlobalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    ASSERT_THIS_GC_OBJECT_INHERITS(info());
    if (!m_npObject)
        return throwInvalidAccessError(lexicalGlobalObject, scope);

    // If the propertyName is symbol.
    if (!methodName)
        return jsUndefined();

    size_t argumentCount = callFrame->argumentCount();
    Vector<NPVariant, 8> arguments(argumentCount);

    // Convert all arguments to NPVariants.
    for (size_t i = 0; i < argumentCount; ++i)
        m_objectMap->convertJSValueToNPVariant(lexicalGlobalObject, callFrame->uncheckedArgument(i), arguments[i]);

    // Calling NPClass::invoke will call into plug-in code, and there's no telling what the plug-in can do.
    // (including destroying the plug-in). Because of this, we make sure to keep the plug-in alive until 
    // the call has finished.
    NPRuntimeObjectMap::PluginProtector protector(m_objectMap);

    bool returnValue;
    NPVariant result;
    VOID_TO_NPVARIANT(result);
    
    {
        JSLock::DropAllLocks dropAllLocks(commonVM());
        returnValue = m_npObject->_class->invoke(m_npObject, methodName, arguments.data(), argumentCount, &result);
        NPRuntimeObjectMap::moveGlobalExceptionToExecState(lexicalGlobalObject);
    }

    // Release all arguments.
    for (size_t i = 0; i < argumentCount; ++i)
        releaseNPVariantValue(&arguments[i]);

    if (!returnValue)
        throwException(lexicalGlobalObject, scope, createError(lexicalGlobalObject, "Error calling method on NPObject."));

    JSValue propertyValue = m_objectMap->convertNPVariantToJSValue(globalObject(), result);
    releaseNPVariantValue(&result);
    return propertyValue;
}

JSC::JSValue JSNPObject::callObject(JSC::JSGlobalObject* lexicalGlobalObject, JSC::CallFrame* callFrame)
{
    VM& vm = lexicalGlobalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    ASSERT_THIS_GC_OBJECT_INHERITS(info());
    if (!m_npObject)
        return throwInvalidAccessError(lexicalGlobalObject, scope);

    size_t argumentCount = callFrame->argumentCount();
    Vector<NPVariant, 8> arguments(argumentCount);
    
    // Convert all arguments to NPVariants.
    for (size_t i = 0; i < argumentCount; ++i)
        m_objectMap->convertJSValueToNPVariant(lexicalGlobalObject, callFrame->uncheckedArgument(i), arguments[i]);

    // Calling NPClass::invokeDefault will call into plug-in code, and there's no telling what the plug-in can do.
    // (including destroying the plug-in). Because of this, we make sure to keep the plug-in alive until 
    // the call has finished.
    NPRuntimeObjectMap::PluginProtector protector(m_objectMap);
    
    bool returnValue;
    NPVariant result;
    VOID_TO_NPVARIANT(result);

    {
        JSLock::DropAllLocks dropAllLocks(commonVM());
        returnValue = m_npObject->_class->invokeDefault(m_npObject, arguments.data(), argumentCount, &result);
        NPRuntimeObjectMap::moveGlobalExceptionToExecState(lexicalGlobalObject);
    }

    // Release all arguments;
    for (size_t i = 0; i < argumentCount; ++i)
        releaseNPVariantValue(&arguments[i]);

    if (!returnValue)
        throwException(lexicalGlobalObject, scope, createError(lexicalGlobalObject, "Error calling method on NPObject."));

    JSValue propertyValue = m_objectMap->convertNPVariantToJSValue(globalObject(), result);
    releaseNPVariantValue(&result);
    return propertyValue;
}

JSValue JSNPObject::callConstructor(JSGlobalObject* lexicalGlobalObject, CallFrame* callFrame)
{
    VM& vm = lexicalGlobalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    ASSERT_THIS_GC_OBJECT_INHERITS(info());
    if (!m_npObject)
        return throwInvalidAccessError(lexicalGlobalObject, scope);

    size_t argumentCount = callFrame->argumentCount();
    Vector<NPVariant, 8> arguments(argumentCount);

    // Convert all arguments to NPVariants.
    for (size_t i = 0; i < argumentCount; ++i)
        m_objectMap->convertJSValueToNPVariant(lexicalGlobalObject, callFrame->uncheckedArgument(i), arguments[i]);

    // Calling NPClass::construct will call into plug-in code, and there's no telling what the plug-in can do.
    // (including destroying the plug-in). Because of this, we make sure to keep the plug-in alive until 
    // the call has finished.
    NPRuntimeObjectMap::PluginProtector protector(m_objectMap);
    
    bool returnValue;
    NPVariant result;
    VOID_TO_NPVARIANT(result);
    
    {
        JSLock::DropAllLocks dropAllLocks(commonVM());
        returnValue = m_npObject->_class->construct(m_npObject, arguments.data(), argumentCount, &result);
        NPRuntimeObjectMap::moveGlobalExceptionToExecState(lexicalGlobalObject);
    }

    if (!returnValue)
        throwException(lexicalGlobalObject, scope, createError(lexicalGlobalObject, "Error calling method on NPObject."));
    
    JSValue value = m_objectMap->convertNPVariantToJSValue(globalObject(), result);
    releaseNPVariantValue(&result);
    return value;
}

static JSC_DECLARE_HOST_FUNCTION(callNPJSObject);
static JSC_DECLARE_HOST_FUNCTION(constructWithConstructor);

JSC_DEFINE_HOST_FUNCTION(callNPJSObject, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    JSObject* object = callFrame->jsCallee();
    ASSERT_UNUSED(globalObject, object->inherits<JSNPObject>(globalObject->vm()));

    return JSValue::encode(jsCast<JSNPObject*>(object)->callObject(globalObject, callFrame));
}

CallData JSNPObject::getCallData(JSCell* cell)
{
    CallData callData;
    JSNPObject* thisObject = JSC::jsCast<JSNPObject*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    if (thisObject->m_npObject && thisObject->m_npObject->_class->invokeDefault) {
        callData.type = CallData::Type::Native;
        callData.native.function = callNPJSObject;
    }
    return callData;
}

JSC_DEFINE_HOST_FUNCTION(constructWithConstructor, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    JSObject* constructor = callFrame->jsCallee();
    ASSERT_UNUSED(globalObject, constructor->inherits<JSNPObject>(globalObject->vm()));

    return JSValue::encode(jsCast<JSNPObject*>(constructor)->callConstructor(globalObject, callFrame));
}

CallData JSNPObject::getConstructData(JSCell* cell)
{
    CallData constructData;

    JSNPObject* thisObject = JSC::jsCast<JSNPObject*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    if (thisObject->m_npObject && thisObject->m_npObject->_class->construct) {
        constructData.type = CallData::Type::Native;
        constructData.native.function = constructWithConstructor;
    }

    return constructData;
}

bool JSNPObject::getOwnPropertySlot(JSObject* object, JSGlobalObject* lexicalGlobalObject, PropertyName propertyName, PropertySlot& slot)
{
    VM& vm = lexicalGlobalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSNPObject* thisObject = JSC::jsCast<JSNPObject*>(object);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    if (!thisObject->m_npObject) {
        throwInvalidAccessError(lexicalGlobalObject, scope);
        return false;
    }
    
    NPIdentifier npIdentifier = npIdentifierFromIdentifier(propertyName);
    // If the propertyName is symbol.
    if (!npIdentifier)
        return false;

    // Calling NPClass::invoke will call into plug-in code, and there's no telling what the plug-in can do.
    // (including destroying the plug-in). Because of this, we make sure to keep the plug-in alive until 
    // the call has finished.
    NPRuntimeObjectMap::PluginProtector protector(thisObject->m_objectMap);

    // First, check if the NPObject has a property with this name.
    if (thisObject->m_npObject->_class->hasProperty && thisObject->m_npObject->_class->hasProperty(thisObject->m_npObject, npIdentifier)) {
        slot.setCustom(thisObject, static_cast<unsigned>(JSC::PropertyAttribute::DontDelete), propertyGetter);
        return true;
    }

    // Second, check if the NPObject has a method with this name.
    if (thisObject->m_npObject->_class->hasMethod && thisObject->m_npObject->_class->hasMethod(thisObject->m_npObject, npIdentifier)) {
        slot.setCustom(thisObject, JSC::PropertyAttribute::DontDelete | JSC::PropertyAttribute::ReadOnly, methodGetter);
        return true;
    }
    
    return false;
}

bool JSNPObject::put(JSCell* cell, JSGlobalObject* lexicalGlobalObject, PropertyName propertyName, JSValue value, PutPropertySlot&)
{
    VM& vm = lexicalGlobalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSNPObject* thisObject = JSC::jsCast<JSNPObject*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    if (!thisObject->m_npObject) {
        throwInvalidAccessError(lexicalGlobalObject, scope);
        return false;
    }

    NPIdentifier npIdentifier = npIdentifierFromIdentifier(propertyName);
    // If the propertyName is symbol.
    if (!npIdentifier)
        return false;
    
    if (!thisObject->m_npObject->_class->hasProperty || !thisObject->m_npObject->_class->hasProperty(thisObject->m_npObject, npIdentifier)) {
        // FIXME: Should we throw an exception here?
        return false;
    }

    if (!thisObject->m_npObject->_class->setProperty)
        return false;

    NPVariant variant;
    thisObject->m_objectMap->convertJSValueToNPVariant(lexicalGlobalObject, value, variant);

    // Calling NPClass::setProperty will call into plug-in code, and there's no telling what the plug-in can do.
    // (including destroying the plug-in). Because of this, we make sure to keep the plug-in alive until 
    // the call has finished.
    NPRuntimeObjectMap::PluginProtector protector(thisObject->m_objectMap);

    bool result = false;
    {
        JSLock::DropAllLocks dropAllLocks(commonVM());
        result = thisObject->m_npObject->_class->setProperty(thisObject->m_npObject, npIdentifier, &variant);

        NPRuntimeObjectMap::moveGlobalExceptionToExecState(lexicalGlobalObject);

        // FIXME: Should we throw an exception if setProperty returns false?
    }

    releaseNPVariantValue(&variant);
    return result;
}

bool JSNPObject::deleteProperty(JSCell* cell, JSGlobalObject* lexicalGlobalObject, PropertyName propertyName, DeletePropertySlot& slot)
{
    return jsCast<JSNPObject*>(cell)->deleteProperty(lexicalGlobalObject, npIdentifierFromIdentifier(propertyName));
}

bool JSNPObject::deletePropertyByIndex(JSCell* cell, JSGlobalObject* lexicalGlobalObject, unsigned propertyName)
{
    return jsCast<JSNPObject*>(cell)->deleteProperty(lexicalGlobalObject, static_cast<NPIdentifier>(IdentifierRep::get(propertyName)));
}

bool JSNPObject::deleteProperty(JSGlobalObject* lexicalGlobalObject, NPIdentifier propertyName)
{
    VM& vm = lexicalGlobalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    ASSERT_THIS_GC_OBJECT_INHERITS(info());

    // If the propertyName is symbol.
    if (!propertyName)
        return false;

    if (!m_npObject) {
        throwInvalidAccessError(lexicalGlobalObject, scope);
        return false;
    }

    if (!m_npObject->_class->removeProperty) {
        // FIXME: Should we throw an exception here?
        return false;
    }

    // Calling NPClass::setProperty will call into plug-in code, and there's no telling what the plug-in can do.
    // (including destroying the plug-in). Because of this, we make sure to keep the plug-in alive until 
    // the call has finished.
    NPRuntimeObjectMap::PluginProtector protector(m_objectMap);

    {
        JSLock::DropAllLocks dropAllLocks(commonVM());

        // FIXME: Should we throw an exception if removeProperty returns false?
        if (!m_npObject->_class->removeProperty(m_npObject, propertyName))
            return false;

        NPRuntimeObjectMap::moveGlobalExceptionToExecState(lexicalGlobalObject);
    }

    return true;
}

void JSNPObject::getOwnPropertyNames(JSObject* object, JSGlobalObject* lexicalGlobalObject, PropertyNameArray& propertyNameArray, EnumerationMode)
{
    VM& vm = lexicalGlobalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSNPObject* thisObject = jsCast<JSNPObject*>(object);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    if (!thisObject->m_npObject) {
        throwInvalidAccessError(lexicalGlobalObject, scope);
        return;
    }

    if (!NP_CLASS_STRUCT_VERSION_HAS_ENUM(thisObject->m_npObject->_class) || !thisObject->m_npObject->_class->enumerate)
        return;

    NPIdentifier* identifiers = 0;
    uint32_t identifierCount = 0;
    
    // Calling NPClass::enumerate will call into plug-in code, and there's no telling what the plug-in can do.
    // (including destroying the plug-in). Because of this, we make sure to keep the plug-in alive until 
    // the call has finished.
    NPRuntimeObjectMap::PluginProtector protector(thisObject->m_objectMap);
    
    {
        JSLock::DropAllLocks dropAllLocks(commonVM());

        // FIXME: Should we throw an exception if enumerate returns false?
        if (!thisObject->m_npObject->_class->enumerate(thisObject->m_npObject, &identifiers, &identifierCount))
            return;

        NPRuntimeObjectMap::moveGlobalExceptionToExecState(lexicalGlobalObject);
    }

    for (uint32_t i = 0; i < identifierCount; ++i) {
        IdentifierRep* identifierRep = static_cast<IdentifierRep*>(identifiers[i]);
        
        Identifier identifier;
        if (identifierRep->isString()) {
            const char* string = identifierRep->string();
            int length = strlen(string);
            
            identifier = Identifier::fromString(vm, String::fromUTF8WithLatin1Fallback(string, length));
        } else
            identifier = Identifier::from(vm, identifierRep->number());

        propertyNameArray.add(identifier);
    }

    npnMemFree(identifiers);
}

JSC_DEFINE_CUSTOM_GETTER(propertyGetter, (JSGlobalObject* lexicalGlobalObject, EncodedJSValue thisValue, PropertyName propertyName))
{
    VM& vm = lexicalGlobalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSNPObject* thisObj = jsCast<JSNPObject*>(JSValue::decode(thisValue));
    ASSERT_GC_OBJECT_INHERITS(thisObj, JSNPObject::info());
    
    if (!thisObj->npObject())
        return JSValue::encode(throwInvalidAccessError(lexicalGlobalObject, scope));

    if (!thisObj->npObject()->_class->getProperty)
        return JSValue::encode(jsUndefined());

    NPVariant result;
    VOID_TO_NPVARIANT(result);

    // Calling NPClass::getProperty will call into plug-in code, and there's no telling what the plug-in can do.
    // (including destroying the plug-in). Because of this, we make sure to keep the plug-in alive until 
    // the call has finished.
    NPRuntimeObjectMap::PluginProtector protector(thisObj->objectMap());
    
    bool returnValue;
    {
        JSLock::DropAllLocks dropAllLocks(commonVM());
        NPIdentifier npIdentifier = npIdentifierFromIdentifier(propertyName);
        // If the propertyName is symbol.
        if (!npIdentifier)
            return JSValue::encode(jsUndefined());

        returnValue = thisObj->npObject()->_class->getProperty(thisObj->npObject(), npIdentifier, &result);
        
        NPRuntimeObjectMap::moveGlobalExceptionToExecState(lexicalGlobalObject);
    }

    if (!returnValue)
        return JSValue::encode(jsUndefined());

    JSValue propertyValue = thisObj->objectMap()->convertNPVariantToJSValue(thisObj->globalObject(), result);
    releaseNPVariantValue(&result);
    return JSValue::encode(propertyValue);
}

JSC_DEFINE_CUSTOM_GETTER(methodGetter, (JSGlobalObject* lexicalGlobalObject, EncodedJSValue thisValue, PropertyName propertyName))
{
    VM& vm = lexicalGlobalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSNPObject* thisObj = jsCast<JSNPObject*>(JSValue::decode(thisValue));
    ASSERT_GC_OBJECT_INHERITS(thisObj, JSNPObject::info());
    
    if (!thisObj->npObject())
        return JSValue::encode(throwInvalidAccessError(lexicalGlobalObject, scope));

    NPIdentifier npIdentifier = npIdentifierFromIdentifier(propertyName);
    // If the propertyName is symbol.
    if (!npIdentifier)
        return JSValue::encode(throwInvalidAccessError(lexicalGlobalObject, scope));

    return JSValue::encode(JSNPMethod::create(thisObj->globalObject(), propertyName.publicName(), npIdentifier));
}

IsoSubspace* JSNPObject::subspaceForImpl(VM& vm)
{
    static NeverDestroyed<IsoSubspacePerVM> perVM([] (VM& vm) { return ISO_SUBSPACE_PARAMETERS(vm.destructibleObjectHeapCellType.get(), JSNPObject); });
    return &perVM.get().forVM(vm);
}

} // namespace WebKit

#endif // ENABLE(NETSCAPE_PLUGIN_API)

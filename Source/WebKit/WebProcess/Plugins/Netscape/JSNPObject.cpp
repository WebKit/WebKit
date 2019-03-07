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

static NPIdentifier npIdentifierFromIdentifier(PropertyName propertyName)
{
    String name(propertyName.publicName());
    // If the propertyName is Symbol.
    if (name.isNull())
        return nullptr;
    return static_cast<NPIdentifier>(IdentifierRep::get(name.utf8().data()));
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

JSValue JSNPObject::callMethod(ExecState* exec, NPIdentifier methodName)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    ASSERT_THIS_GC_OBJECT_INHERITS(info());
    if (!m_npObject)
        return throwInvalidAccessError(exec, scope);

    // If the propertyName is symbol.
    if (!methodName)
        return jsUndefined();

    size_t argumentCount = exec->argumentCount();
    Vector<NPVariant, 8> arguments(argumentCount);

    // Convert all arguments to NPVariants.
    for (size_t i = 0; i < argumentCount; ++i)
        m_objectMap->convertJSValueToNPVariant(exec, exec->uncheckedArgument(i), arguments[i]);

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
        NPRuntimeObjectMap::moveGlobalExceptionToExecState(exec);
    }

    // Release all arguments.
    for (size_t i = 0; i < argumentCount; ++i)
        releaseNPVariantValue(&arguments[i]);

    if (!returnValue)
        throwException(exec, scope, createError(exec, "Error calling method on NPObject."));

    JSValue propertyValue = m_objectMap->convertNPVariantToJSValue(exec, globalObject(), result);
    releaseNPVariantValue(&result);
    return propertyValue;
}

JSC::JSValue JSNPObject::callObject(JSC::ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    ASSERT_THIS_GC_OBJECT_INHERITS(info());
    if (!m_npObject)
        return throwInvalidAccessError(exec, scope);

    size_t argumentCount = exec->argumentCount();
    Vector<NPVariant, 8> arguments(argumentCount);
    
    // Convert all arguments to NPVariants.
    for (size_t i = 0; i < argumentCount; ++i)
        m_objectMap->convertJSValueToNPVariant(exec, exec->uncheckedArgument(i), arguments[i]);

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
        NPRuntimeObjectMap::moveGlobalExceptionToExecState(exec);
    }

    // Release all arguments;
    for (size_t i = 0; i < argumentCount; ++i)
        releaseNPVariantValue(&arguments[i]);

    if (!returnValue)
        throwException(exec, scope, createError(exec, "Error calling method on NPObject."));

    JSValue propertyValue = m_objectMap->convertNPVariantToJSValue(exec, globalObject(), result);
    releaseNPVariantValue(&result);
    return propertyValue;
}

JSValue JSNPObject::callConstructor(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    ASSERT_THIS_GC_OBJECT_INHERITS(info());
    if (!m_npObject)
        return throwInvalidAccessError(exec, scope);

    size_t argumentCount = exec->argumentCount();
    Vector<NPVariant, 8> arguments(argumentCount);

    // Convert all arguments to NPVariants.
    for (size_t i = 0; i < argumentCount; ++i)
        m_objectMap->convertJSValueToNPVariant(exec, exec->uncheckedArgument(i), arguments[i]);

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
        NPRuntimeObjectMap::moveGlobalExceptionToExecState(exec);
    }

    if (!returnValue)
        throwException(exec, scope, createError(exec, "Error calling method on NPObject."));
    
    JSValue value = m_objectMap->convertNPVariantToJSValue(exec, globalObject(), result);
    releaseNPVariantValue(&result);
    return value;
}

static EncodedJSValue JSC_HOST_CALL callNPJSObject(ExecState* exec)
{
    JSObject* object = exec->jsCallee();
    ASSERT(object->inherits<JSNPObject>(exec->vm()));

    return JSValue::encode(jsCast<JSNPObject*>(object)->callObject(exec));
}

JSC::CallType JSNPObject::getCallData(JSC::JSCell* cell, JSC::CallData& callData)
{
    JSNPObject* thisObject = JSC::jsCast<JSNPObject*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    if (!thisObject->m_npObject || !thisObject->m_npObject->_class->invokeDefault)
        return CallType::None;

    callData.native.function = callNPJSObject;
    return CallType::Host;
}

static EncodedJSValue JSC_HOST_CALL constructWithConstructor(ExecState* exec)
{
    JSObject* constructor = exec->jsCallee();
    ASSERT(constructor->inherits<JSNPObject>(exec->vm()));

    return JSValue::encode(jsCast<JSNPObject*>(constructor)->callConstructor(exec));
}

ConstructType JSNPObject::getConstructData(JSCell* cell, ConstructData& constructData)
{
    JSNPObject* thisObject = JSC::jsCast<JSNPObject*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    if (!thisObject->m_npObject || !thisObject->m_npObject->_class->construct)
        return ConstructType::None;

    constructData.native.function = constructWithConstructor;
    return ConstructType::Host;
}

bool JSNPObject::getOwnPropertySlot(JSObject* object, ExecState* exec, PropertyName propertyName, PropertySlot& slot)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSNPObject* thisObject = JSC::jsCast<JSNPObject*>(object);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    if (!thisObject->m_npObject) {
        throwInvalidAccessError(exec, scope);
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
        slot.setCustom(thisObject, static_cast<unsigned>(JSC::PropertyAttribute::DontDelete), thisObject->propertyGetter);
        return true;
    }

    // Second, check if the NPObject has a method with this name.
    if (thisObject->m_npObject->_class->hasMethod && thisObject->m_npObject->_class->hasMethod(thisObject->m_npObject, npIdentifier)) {
        slot.setCustom(thisObject, JSC::PropertyAttribute::DontDelete | JSC::PropertyAttribute::ReadOnly, thisObject->methodGetter);
        return true;
    }
    
    return false;
}

bool JSNPObject::put(JSCell* cell, ExecState* exec, PropertyName propertyName, JSValue value, PutPropertySlot&)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSNPObject* thisObject = JSC::jsCast<JSNPObject*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    if (!thisObject->m_npObject) {
        throwInvalidAccessError(exec, scope);
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
    thisObject->m_objectMap->convertJSValueToNPVariant(exec, value, variant);

    // Calling NPClass::setProperty will call into plug-in code, and there's no telling what the plug-in can do.
    // (including destroying the plug-in). Because of this, we make sure to keep the plug-in alive until 
    // the call has finished.
    NPRuntimeObjectMap::PluginProtector protector(thisObject->m_objectMap);

    bool result = false;
    {
        JSLock::DropAllLocks dropAllLocks(commonVM());
        result = thisObject->m_npObject->_class->setProperty(thisObject->m_npObject, npIdentifier, &variant);

        NPRuntimeObjectMap::moveGlobalExceptionToExecState(exec);

        // FIXME: Should we throw an exception if setProperty returns false?
    }

    releaseNPVariantValue(&variant);
    return result;
}

bool JSNPObject::deleteProperty(JSCell* cell, ExecState* exec, PropertyName propertyName)
{
    return jsCast<JSNPObject*>(cell)->deleteProperty(exec, npIdentifierFromIdentifier(propertyName));
}

bool JSNPObject::deletePropertyByIndex(JSCell* cell, ExecState* exec, unsigned propertyName)
{
    return jsCast<JSNPObject*>(cell)->deleteProperty(exec, static_cast<NPIdentifier>(IdentifierRep::get(propertyName)));
}

bool JSNPObject::deleteProperty(ExecState* exec, NPIdentifier propertyName)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    ASSERT_THIS_GC_OBJECT_INHERITS(info());

    // If the propertyName is symbol.
    if (!propertyName)
        return false;

    if (!m_npObject) {
        throwInvalidAccessError(exec, scope);
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

        NPRuntimeObjectMap::moveGlobalExceptionToExecState(exec);
    }

    return true;
}

void JSNPObject::getOwnPropertyNames(JSObject* object, ExecState* exec, PropertyNameArray& propertyNameArray, EnumerationMode)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSNPObject* thisObject = jsCast<JSNPObject*>(object);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    if (!thisObject->m_npObject) {
        throwInvalidAccessError(exec, scope);
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

        NPRuntimeObjectMap::moveGlobalExceptionToExecState(exec);
    }

    for (uint32_t i = 0; i < identifierCount; ++i) {
        IdentifierRep* identifierRep = static_cast<IdentifierRep*>(identifiers[i]);
        
        Identifier identifier;
        if (identifierRep->isString()) {
            const char* string = identifierRep->string();
            int length = strlen(string);
            
            identifier = Identifier::fromString(exec, String::fromUTF8WithLatin1Fallback(string, length));
        } else
            identifier = Identifier::from(exec, identifierRep->number());

        propertyNameArray.add(identifier);
    }

    npnMemFree(identifiers);
}

EncodedJSValue JSNPObject::propertyGetter(ExecState* exec, EncodedJSValue thisValue, PropertyName propertyName)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSNPObject* thisObj = jsCast<JSNPObject*>(JSValue::decode(thisValue));
    ASSERT_GC_OBJECT_INHERITS(thisObj, info());
    
    if (!thisObj->m_npObject)
        return JSValue::encode(throwInvalidAccessError(exec, scope));

    if (!thisObj->m_npObject->_class->getProperty)
        return JSValue::encode(jsUndefined());

    NPVariant result;
    VOID_TO_NPVARIANT(result);

    // Calling NPClass::getProperty will call into plug-in code, and there's no telling what the plug-in can do.
    // (including destroying the plug-in). Because of this, we make sure to keep the plug-in alive until 
    // the call has finished.
    NPRuntimeObjectMap::PluginProtector protector(thisObj->m_objectMap);
    
    bool returnValue;
    {
        JSLock::DropAllLocks dropAllLocks(commonVM());
        NPIdentifier npIdentifier = npIdentifierFromIdentifier(propertyName);
        // If the propertyName is symbol.
        if (!npIdentifier)
            return JSValue::encode(jsUndefined());

        returnValue = thisObj->m_npObject->_class->getProperty(thisObj->m_npObject, npIdentifier, &result);
        
        NPRuntimeObjectMap::moveGlobalExceptionToExecState(exec);
    }

    if (!returnValue)
        return JSValue::encode(jsUndefined());

    JSValue propertyValue = thisObj->m_objectMap->convertNPVariantToJSValue(exec, thisObj->globalObject(), result);
    releaseNPVariantValue(&result);
    return JSValue::encode(propertyValue);
}

EncodedJSValue JSNPObject::methodGetter(ExecState* exec, EncodedJSValue thisValue, PropertyName propertyName)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSNPObject* thisObj = jsCast<JSNPObject*>(JSValue::decode(thisValue));
    ASSERT_GC_OBJECT_INHERITS(thisObj, info());
    
    if (!thisObj->m_npObject)
        return JSValue::encode(throwInvalidAccessError(exec, scope));

    NPIdentifier npIdentifier = npIdentifierFromIdentifier(propertyName);
    // If the propertyName is symbol.
    if (!npIdentifier)
        return JSValue::encode(throwInvalidAccessError(exec, scope));

    return JSValue::encode(JSNPMethod::create(exec, thisObj->globalObject(), propertyName.publicName(), npIdentifier));
}

JSC::Exception* JSNPObject::throwInvalidAccessError(ExecState* exec, ThrowScope& scope)
{
    return throwException(exec, scope, createReferenceError(exec, "Trying to access object from destroyed plug-in."));
}

IsoSubspace* JSNPObject::subspaceForImpl(VM& vm)
{
    static NeverDestroyed<IsoSubspacePerVM> perVM([] (VM& vm) { return ISO_SUBSPACE_PARAMETERS(vm.destructibleObjectHeapCellType.get(), JSNPObject); });
    return &perVM.get().forVM(vm);
}

} // namespace WebKit

#endif // ENABLE(NETSCAPE_PLUGIN_API)

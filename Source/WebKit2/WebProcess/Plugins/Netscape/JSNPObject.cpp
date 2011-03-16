/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#include "JSNPMethod.h"
#include "NPJSObject.h"
#include "NPRuntimeObjectMap.h"
#include "NPRuntimeUtilities.h"
#include <JavaScriptCore/Error.h>
#include <JavaScriptCore/JSGlobalObject.h>
#include <JavaScriptCore/JSLock.h>
#include <JavaScriptCore/ObjectPrototype.h>
#include <WebCore/IdentifierRep.h>
#include <wtf/text/WTFString.h>

using namespace JSC;
using namespace WebCore;

namespace WebKit {

static NPIdentifier npIdentifierFromIdentifier(const Identifier& identifier)
{
    return static_cast<NPIdentifier>(IdentifierRep::get(identifier.ustring().utf8().data()));
}

const ClassInfo JSNPObject::s_info = { "NPObject", &JSObjectWithGlobalObject::s_info, 0, 0 };

JSNPObject::JSNPObject(JSGlobalObject* globalObject, NPRuntimeObjectMap* objectMap, NPObject* npObject)
    : JSObjectWithGlobalObject(globalObject, createStructure(globalObject->globalData(), globalObject->objectPrototype()))
    , m_objectMap(objectMap)
    , m_npObject(npObject)
{
    ASSERT(inherits(&s_info));

    // We should never have an NPJSObject inside a JSNPObject.
    ASSERT(!NPJSObject::isNPJSObject(m_npObject));

    retainNPObject(m_npObject);
}

JSNPObject::~JSNPObject()
{
    if (!m_npObject)
        return;

    m_objectMap->jsNPObjectDestroyed(this);
    releaseNPObject(m_npObject);
}

void JSNPObject::invalidate()
{
    ASSERT(m_npObject);

    releaseNPObject(m_npObject);
    m_npObject = 0;
}

JSValue JSNPObject::callMethod(ExecState* exec, NPIdentifier methodName)
{
    if (!m_npObject)
        return throwInvalidAccessError(exec);

    size_t argumentCount = exec->argumentCount();
    Vector<NPVariant, 8> arguments(argumentCount);

    // Convert all arguments to NPVariants.
    for (size_t i = 0; i < argumentCount; ++i)
        m_objectMap->convertJSValueToNPVariant(exec, exec->argument(i), arguments[i]);

    // Calling NPClass::invoke will call into plug-in code, and there's no telling what the plug-in can do.
    // (including destroying the plug-in). Because of this, we make sure to keep the plug-in alive until 
    // the call has finished.
    NPRuntimeObjectMap::PluginProtector protector(m_objectMap);

    bool returnValue;
    NPVariant result;
    VOID_TO_NPVARIANT(result);
    
    {
        JSLock::DropAllLocks dropAllLocks(SilenceAssertionsOnly);
        returnValue = m_npObject->_class->invoke(m_npObject, methodName, arguments.data(), argumentCount, &result);
        NPRuntimeObjectMap::moveGlobalExceptionToExecState(exec);
    }

    // Release all arguments;
    for (size_t i = 0; i < argumentCount; ++i)
        releaseNPVariantValue(&arguments[i]);

    if (!returnValue)
        throwError(exec, createError(exec, "Error calling method on NPObject."));

    JSValue propertyValue = m_objectMap->convertNPVariantToJSValue(exec, globalObject(), result);
    releaseNPVariantValue(&result);
    return propertyValue;
}

JSC::JSValue JSNPObject::callObject(JSC::ExecState* exec)
{
    if (!m_npObject)
        return throwInvalidAccessError(exec);

    size_t argumentCount = exec->argumentCount();
    Vector<NPVariant, 8> arguments(argumentCount);
    
    // Convert all arguments to NPVariants.
    for (size_t i = 0; i < argumentCount; ++i)
        m_objectMap->convertJSValueToNPVariant(exec, exec->argument(i), arguments[i]);

    // Calling NPClass::invokeDefault will call into plug-in code, and there's no telling what the plug-in can do.
    // (including destroying the plug-in). Because of this, we make sure to keep the plug-in alive until 
    // the call has finished.
    NPRuntimeObjectMap::PluginProtector protector(m_objectMap);
    
    bool returnValue;
    NPVariant result;
    VOID_TO_NPVARIANT(result);

    {
        JSLock::DropAllLocks dropAllLocks(SilenceAssertionsOnly);
        returnValue = m_npObject->_class->invokeDefault(m_npObject, arguments.data(), argumentCount, &result);
        NPRuntimeObjectMap::moveGlobalExceptionToExecState(exec);
    }

    // Release all arguments;
    for (size_t i = 0; i < argumentCount; ++i)
        releaseNPVariantValue(&arguments[i]);

    if (!returnValue)
        throwError(exec, createError(exec, "Error calling method on NPObject."));

    JSValue propertyValue = m_objectMap->convertNPVariantToJSValue(exec, globalObject(), result);
    releaseNPVariantValue(&result);
    return propertyValue;
}

JSValue JSNPObject::callConstructor(ExecState* exec)
{
    if (!m_npObject)
        return throwInvalidAccessError(exec);

    size_t argumentCount = exec->argumentCount();
    Vector<NPVariant, 8> arguments(argumentCount);

    // Convert all arguments to NPVariants.
    for (size_t i = 0; i < argumentCount; ++i)
        m_objectMap->convertJSValueToNPVariant(exec, exec->argument(i), arguments[i]);

    // Calling NPClass::construct will call into plug-in code, and there's no telling what the plug-in can do.
    // (including destroying the plug-in). Because of this, we make sure to keep the plug-in alive until 
    // the call has finished.
    NPRuntimeObjectMap::PluginProtector protector(m_objectMap);
    
    bool returnValue;
    NPVariant result;
    VOID_TO_NPVARIANT(result);
    
    {
        JSLock::DropAllLocks dropAllLocks(SilenceAssertionsOnly);
        returnValue = m_npObject->_class->construct(m_npObject, arguments.data(), argumentCount, &result);
        NPRuntimeObjectMap::moveGlobalExceptionToExecState(exec);
    }

    if (!returnValue)
        throwError(exec, createError(exec, "Error calling method on NPObject."));
    
    JSValue value = m_objectMap->convertNPVariantToJSValue(exec, globalObject(), result);
    releaseNPVariantValue(&result);
    return value;
}

static EncodedJSValue JSC_HOST_CALL callNPJSObject(ExecState* exec)
{
    JSObject* object = exec->callee();
    ASSERT(object->inherits(&JSNPObject::s_info));

    return JSValue::encode(static_cast<JSNPObject*>(object)->callObject(exec));
}

JSC::CallType JSNPObject::getCallData(JSC::CallData& callData)
{
    if (!m_npObject || !m_npObject->_class->invokeDefault)
        return CallTypeNone;

    callData.native.function = callNPJSObject;
    return CallTypeHost;
}

static EncodedJSValue JSC_HOST_CALL constructWithConstructor(ExecState* exec)
{
    JSObject* constructor = exec->callee();
    ASSERT(constructor->inherits(&JSNPObject::s_info));

    return JSValue::encode(static_cast<JSNPObject*>(constructor)->callConstructor(exec));
}

ConstructType JSNPObject::getConstructData(ConstructData& constructData)
{
    if (!m_npObject || !m_npObject->_class->construct)
        return ConstructTypeNone;

    constructData.native.function = constructWithConstructor;
    return ConstructTypeHost;
}

bool JSNPObject::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    if (!m_npObject) {
        throwInvalidAccessError(exec);
        return false;
    }
    
    NPIdentifier npIdentifier = npIdentifierFromIdentifier(propertyName);

    // First, check if the NPObject has a property with this name.
    if (m_npObject->_class->hasProperty && m_npObject->_class->hasProperty(m_npObject, npIdentifier)) {
        slot.setCustom(this, propertyGetter);
        return true;
    }

    // Second, check if the NPObject has a method with this name.
    if (m_npObject->_class->hasMethod && m_npObject->_class->hasMethod(m_npObject, npIdentifier)) {
        slot.setCustom(this, methodGetter);
        return true;
    }
    
    return false;
}

bool JSNPObject::getOwnPropertyDescriptor(ExecState* exec, const Identifier& propertyName, PropertyDescriptor& descriptor)
{
    if (!m_npObject) {
        throwInvalidAccessError(exec);
        return false;
    }

    NPIdentifier npIdentifier = npIdentifierFromIdentifier(propertyName);

    // First, check if the NPObject has a property with this name.
    if (m_npObject->_class->hasProperty && m_npObject->_class->hasProperty(m_npObject, npIdentifier)) {
        PropertySlot slot;
        slot.setCustom(this, propertyGetter);
        descriptor.setDescriptor(slot.getValue(exec, propertyName), DontDelete);
        return true;
    }

    // Second, check if the NPObject has a method with this name.
    if (m_npObject->_class->hasMethod && m_npObject->_class->hasMethod(m_npObject, npIdentifier)) {
        PropertySlot slot;
        slot.setCustom(this, methodGetter);
        descriptor.setDescriptor(slot.getValue(exec, propertyName), DontDelete | ReadOnly);
        return true;
    }

    return false;
}

void JSNPObject::put(ExecState* exec, const Identifier& propertyName, JSValue value, PutPropertySlot&)
{
    if (!m_npObject) {
        throwInvalidAccessError(exec);
        return;
    }

    NPIdentifier npIdentifier = npIdentifierFromIdentifier(propertyName);
    
    if (!m_npObject->_class->hasProperty || !m_npObject->_class->hasProperty(m_npObject, npIdentifier)) {
        // FIXME: Should we throw an exception here?
        return;
    }

    if (!m_npObject->_class->setProperty)
        return;

    NPVariant variant;
    m_objectMap->convertJSValueToNPVariant(exec, value, variant);

    // Calling NPClass::setProperty will call into plug-in code, and there's no telling what the plug-in can do.
    // (including destroying the plug-in). Because of this, we make sure to keep the plug-in alive until 
    // the call has finished.
    NPRuntimeObjectMap::PluginProtector protector(m_objectMap);

    {
        JSLock::DropAllLocks dropAllLocks(SilenceAssertionsOnly);
        m_npObject->_class->setProperty(m_npObject, npIdentifier, &variant);

        NPRuntimeObjectMap::moveGlobalExceptionToExecState(exec);

        // FIXME: Should we throw an exception if setProperty returns false?
    }

    releaseNPVariantValue(&variant);
}

void JSNPObject::getOwnPropertyNames(ExecState* exec, PropertyNameArray& propertyNameArray, EnumerationMode mode)
{
    if (!m_npObject) {
        throwInvalidAccessError(exec);
        return;
    }

    if (!NP_CLASS_STRUCT_VERSION_HAS_ENUM(m_npObject->_class) || !m_npObject->_class->enumerate)
        return;

    NPIdentifier* identifiers = 0;
    uint32_t identifierCount = 0;
    
    // Calling NPClass::enumerate will call into plug-in code, and there's no telling what the plug-in can do.
    // (including destroying the plug-in). Because of this, we make sure to keep the plug-in alive until 
    // the call has finished.
    NPRuntimeObjectMap::PluginProtector protector(m_objectMap);
    
    {
        JSLock::DropAllLocks dropAllLocks(SilenceAssertionsOnly);

        // FIXME: Should we throw an exception if enumerate returns false?
        if (!m_npObject->_class->enumerate(m_npObject, &identifiers, &identifierCount))
            return;

        NPRuntimeObjectMap::moveGlobalExceptionToExecState(exec);
    }

    for (uint32_t i = 0; i < identifierCount; ++i) {
        IdentifierRep* identifierRep = static_cast<IdentifierRep*>(identifiers[i]);
        
        Identifier identifier;
        if (identifierRep->isString()) {
            const char* string = identifierRep->string();
            int length = strlen(string);
            
            identifier = Identifier(exec, String::fromUTF8WithLatin1Fallback(string, length).impl());
        } else
            identifier = Identifier::from(exec, identifierRep->number());

        propertyNameArray.add(identifier);
    }

    npnMemFree(identifiers);
}

JSValue JSNPObject::propertyGetter(ExecState* exec, JSValue slotBase, const Identifier& propertyName)
{
    JSNPObject* thisObj = static_cast<JSNPObject*>(asObject(slotBase));

    if (!thisObj->m_npObject)
        return throwInvalidAccessError(exec);

    if (!thisObj->m_npObject->_class->getProperty)
        return jsUndefined();

    NPVariant result;
    VOID_TO_NPVARIANT(result);

    // Calling NPClass::getProperty will call into plug-in code, and there's no telling what the plug-in can do.
    // (including destroying the plug-in). Because of this, we make sure to keep the plug-in alive until 
    // the call has finished.
    NPRuntimeObjectMap::PluginProtector protector(thisObj->m_objectMap);
    
    bool returnValue;
    {
        JSLock::DropAllLocks dropAllLocks(SilenceAssertionsOnly);
        NPIdentifier npIdentifier = npIdentifierFromIdentifier(propertyName);
        returnValue = thisObj->m_npObject->_class->getProperty(thisObj->m_npObject, npIdentifier, &result);
        
        NPRuntimeObjectMap::moveGlobalExceptionToExecState(exec);
    }

    if (!returnValue)
        return jsUndefined();

    JSValue propertyValue = thisObj->m_objectMap->convertNPVariantToJSValue(exec, thisObj->globalObject(), result);
    releaseNPVariantValue(&result);
    return propertyValue;
}

JSValue JSNPObject::methodGetter(ExecState* exec, JSValue slotBase, const Identifier& methodName)
{
    JSNPObject* thisObj = static_cast<JSNPObject*>(asObject(slotBase));
    
    if (!thisObj->m_npObject)
        return throwInvalidAccessError(exec);

    NPIdentifier npIdentifier = npIdentifierFromIdentifier(methodName);
    return new (exec) JSNPMethod(exec, thisObj->globalObject(), methodName, npIdentifier);
}

JSObject* JSNPObject::throwInvalidAccessError(ExecState* exec)
{
    return throwError(exec, createReferenceError(exec, "Trying to access object from destroyed plug-in."));
}

} // namespace WebKit

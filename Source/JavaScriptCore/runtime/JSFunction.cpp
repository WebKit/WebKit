/*
 *  Copyright (C) 1999-2002 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 *  Copyright (C) 2007 Cameron Zwarich (cwzwarich@uwaterloo.ca)
 *  Copyright (C) 2007 Maks Orlovich
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "JSFunction.h"

#include "CodeBlock.h"
#include "CommonIdentifiers.h"
#include "CallFrame.h"
#include "ExceptionHelpers.h"
#include "FunctionPrototype.h"
#include "GetterSetter.h"
#include "JSArray.h"
#include "JSGlobalObject.h"
#include "JSNotAnObject.h"
#include "Interpreter.h"
#include "ObjectPrototype.h"
#include "Parser.h"
#include "PropertyNameArray.h"
#include "ScopeChainMark.h"

using namespace WTF;
using namespace Unicode;

namespace JSC {
EncodedJSValue JSC_HOST_CALL callHostFunctionAsConstructor(ExecState* exec)
{
    return throwVMError(exec, createNotAConstructorError(exec, exec->callee()));
}

ASSERT_CLASS_FITS_IN_CELL(JSFunction);
ASSERT_HAS_TRIVIAL_DESTRUCTOR(JSFunction);

const ClassInfo JSFunction::s_info = { "Function", &Base::s_info, 0, 0, CREATE_METHOD_TABLE(JSFunction) };

bool JSFunction::isHostFunctionNonInline() const
{
    return isHostFunction();
}

JSFunction* JSFunction::create(ExecState* exec, JSGlobalObject* globalObject, int length, const UString& name, NativeFunction nativeFunction, Intrinsic intrinsic, NativeFunction nativeConstructor)
{
    NativeExecutable* executable;
#if !ENABLE(JIT)
    UNUSED_PARAM(intrinsic);
#else
    if (intrinsic != NoIntrinsic && exec->globalData().canUseJIT()) {
        ASSERT(nativeConstructor == callHostFunctionAsConstructor);
        executable = exec->globalData().getHostFunction(nativeFunction, intrinsic);
    } else
#endif
        executable = exec->globalData().getHostFunction(nativeFunction, nativeConstructor);

    JSFunction* function = new (NotNull, allocateCell<JSFunction>(*exec->heap())) JSFunction(exec, globalObject, globalObject->functionStructure());
    // Can't do this during initialization because getHostFunction might do a GC allocation.
    function->finishCreation(exec, executable, length, name);
    return function;
}

JSFunction::JSFunction(ExecState* exec, JSGlobalObject* globalObject, Structure* structure)
    : Base(exec->globalData(), structure)
    , m_executable()
    , m_scopeChain(exec->globalData(), this, globalObject->globalScopeChain())
{
}

JSFunction::JSFunction(ExecState* exec, FunctionExecutable* executable, ScopeChainNode* scopeChainNode)
    : Base(exec->globalData(), scopeChainNode->globalObject->functionStructure())
    , m_executable(exec->globalData(), this, executable)
    , m_scopeChain(exec->globalData(), this, scopeChainNode)
{
}

void JSFunction::finishCreation(ExecState* exec, NativeExecutable* executable, int length, const UString& name)
{
    Base::finishCreation(exec->globalData());
    ASSERT(inherits(&s_info));
    m_executable.set(exec->globalData(), this, executable);
    putDirect(exec->globalData(), exec->globalData().propertyNames->name, jsString(exec, name.isNull() ? "" : name), DontDelete | ReadOnly | DontEnum);
    putDirect(exec->globalData(), exec->propertyNames().length, jsNumber(length), DontDelete | ReadOnly | DontEnum);
}

void JSFunction::finishCreation(ExecState* exec, FunctionExecutable* executable, ScopeChainNode* scopeChainNode)
{
    Base::finishCreation(exec->globalData());
    ASSERT(inherits(&s_info));

    // Switching the structure here is only safe if we currently have the function structure!
    ASSERT(structure() == scopeChainNode->globalObject->functionStructure());
    setStructure(exec->globalData(), scopeChainNode->globalObject->namedFunctionStructure());
    putDirectOffset(exec->globalData(), scopeChainNode->globalObject->functionNameOffset(), executable->nameValue());
}

Structure* JSFunction::cacheInheritorID(ExecState* exec)
{
    JSValue prototype = get(exec, exec->globalData().propertyNames->prototype);
    if (prototype.isObject())
        m_cachedInheritorID.set(exec->globalData(), this, asObject(prototype)->inheritorID(exec->globalData()));
    else
        m_cachedInheritorID.set(exec->globalData(), this, globalObject()->emptyObjectStructure());
    return m_cachedInheritorID.get();
}

const UString& JSFunction::name(ExecState* exec)
{
    return asString(getDirect(exec->globalData(), exec->globalData().propertyNames->name))->tryGetValue();
}

const UString JSFunction::displayName(ExecState* exec)
{
    JSValue displayName = getDirect(exec->globalData(), exec->globalData().propertyNames->displayName);
    
    if (displayName && isJSString(displayName))
        return asString(displayName)->tryGetValue();
    
    return UString();
}

const UString JSFunction::calculatedDisplayName(ExecState* exec)
{
    const UString explicitName = displayName(exec);
    
    if (!explicitName.isEmpty())
        return explicitName;
    
    const UString actualName = name(exec);
    if (!actualName.isEmpty() || isHostFunction())
        return actualName;
    
    return jsExecutable()->inferredName().ustring();
}

const SourceCode* JSFunction::sourceCode() const
{
    if (isHostFunction())
        return 0;
    return &jsExecutable()->source();
}

void JSFunction::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    JSFunction* thisObject = jsCast<JSFunction*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, &s_info);
    COMPILE_ASSERT(StructureFlags & OverridesVisitChildren, OverridesVisitChildrenWithoutSettingFlag);
    ASSERT(thisObject->structure()->typeInfo().overridesVisitChildren());
    Base::visitChildren(thisObject, visitor);

    visitor.append(&thisObject->m_scopeChain);
    if (thisObject->m_executable)
        visitor.append(&thisObject->m_executable);
}

CallType JSFunction::getCallData(JSCell* cell, CallData& callData)
{
    JSFunction* thisObject = jsCast<JSFunction*>(cell);
    if (thisObject->isHostFunction()) {
        callData.native.function = thisObject->nativeFunction();
        return CallTypeHost;
    }
    callData.js.functionExecutable = thisObject->jsExecutable();
    callData.js.scopeChain = thisObject->scope();
    return CallTypeJS;
}

JSValue JSFunction::argumentsGetter(ExecState* exec, JSValue slotBase, PropertyName)
{
    JSFunction* thisObj = jsCast<JSFunction*>(slotBase);
    ASSERT(!thisObj->isHostFunction());
    return exec->interpreter()->retrieveArgumentsFromVMCode(exec, thisObj);
}

JSValue JSFunction::callerGetter(ExecState* exec, JSValue slotBase, PropertyName)
{
    JSFunction* thisObj = jsCast<JSFunction*>(slotBase);
    ASSERT(!thisObj->isHostFunction());
    JSValue caller = exec->interpreter()->retrieveCallerFromVMCode(exec, thisObj);

    // See ES5.1 15.3.5.4 - Function.caller may not be used to retrieve a strict caller.
    if (!caller.isObject() || !asObject(caller)->inherits(&JSFunction::s_info))
        return caller;
    JSFunction* function = jsCast<JSFunction*>(caller);
    if (function->isHostFunction() || !function->jsExecutable()->isStrictMode())
        return caller;
    return throwTypeError(exec, "Function.caller used to retrieve strict caller");
}

JSValue JSFunction::lengthGetter(ExecState*, JSValue slotBase, PropertyName)
{
    JSFunction* thisObj = jsCast<JSFunction*>(slotBase);
    ASSERT(!thisObj->isHostFunction());
    return jsNumber(thisObj->jsExecutable()->parameterCount());
}

bool JSFunction::getOwnPropertySlot(JSCell* cell, ExecState* exec, PropertyName propertyName, PropertySlot& slot)
{
    JSFunction* thisObject = jsCast<JSFunction*>(cell);
    if (thisObject->isHostFunction())
        return Base::getOwnPropertySlot(thisObject, exec, propertyName, slot);

    if (propertyName == exec->propertyNames().prototype) {
        WriteBarrierBase<Unknown>* location = thisObject->getDirectLocation(exec->globalData(), propertyName);

        if (!location) {
            JSObject* prototype = constructEmptyObject(exec, thisObject->globalObject()->emptyObjectStructure());
            prototype->putDirect(exec->globalData(), exec->propertyNames().constructor, thisObject, DontEnum);
            thisObject->putDirect(exec->globalData(), exec->propertyNames().prototype, prototype, DontDelete | DontEnum);
            location = thisObject->getDirectLocation(exec->globalData(), exec->propertyNames().prototype);
        }

        slot.setValue(thisObject, location->get(), thisObject->offsetForLocation(location));
    }

    if (propertyName == exec->propertyNames().arguments) {
        if (thisObject->jsExecutable()->isStrictMode()) {
            bool result = Base::getOwnPropertySlot(thisObject, exec, propertyName, slot);
            if (!result) {
                thisObject->putDirectAccessor(exec->globalData(), propertyName, thisObject->globalObject()->throwTypeErrorGetterSetter(exec), DontDelete | DontEnum | Accessor);
                result = Base::getOwnPropertySlot(thisObject, exec, propertyName, slot);
                ASSERT(result);
            }
            return result;
        }
        slot.setCacheableCustom(thisObject, argumentsGetter);
        return true;
    }

    if (propertyName == exec->propertyNames().length) {
        slot.setCacheableCustom(thisObject, lengthGetter);
        return true;
    }

    if (propertyName == exec->propertyNames().caller) {
        if (thisObject->jsExecutable()->isStrictMode()) {
            bool result = Base::getOwnPropertySlot(thisObject, exec, propertyName, slot);
            if (!result) {
                thisObject->putDirectAccessor(exec->globalData(), propertyName, thisObject->globalObject()->throwTypeErrorGetterSetter(exec), DontDelete | DontEnum | Accessor);
                result = Base::getOwnPropertySlot(thisObject, exec, propertyName, slot);
                ASSERT(result);
            }
            return result;
        }
        slot.setCacheableCustom(thisObject, callerGetter);
        return true;
    }

    return Base::getOwnPropertySlot(thisObject, exec, propertyName, slot);
}

bool JSFunction::getOwnPropertyDescriptor(JSObject* object, ExecState* exec, PropertyName propertyName, PropertyDescriptor& descriptor)
{
    JSFunction* thisObject = jsCast<JSFunction*>(object);
    if (thisObject->isHostFunction())
        return Base::getOwnPropertyDescriptor(thisObject, exec, propertyName, descriptor);
    
    if (propertyName == exec->propertyNames().prototype) {
        PropertySlot slot;
        thisObject->methodTable()->getOwnPropertySlot(thisObject, exec, propertyName, slot);
        return Base::getOwnPropertyDescriptor(thisObject, exec, propertyName, descriptor);
    }
    
    if (propertyName == exec->propertyNames().arguments) {
        if (thisObject->jsExecutable()->isStrictMode()) {
            bool result = Base::getOwnPropertyDescriptor(thisObject, exec, propertyName, descriptor);
            if (!result) {
                thisObject->putDirectAccessor(exec->globalData(), propertyName, thisObject->globalObject()->throwTypeErrorGetterSetter(exec), DontDelete | DontEnum | Accessor);
                result = Base::getOwnPropertyDescriptor(thisObject, exec, propertyName, descriptor);
                ASSERT(result);
            }
            return result;
        }
        descriptor.setDescriptor(exec->interpreter()->retrieveArgumentsFromVMCode(exec, thisObject), ReadOnly | DontEnum | DontDelete);
        return true;
    }
    
    if (propertyName == exec->propertyNames().length) {
        descriptor.setDescriptor(jsNumber(thisObject->jsExecutable()->parameterCount()), ReadOnly | DontEnum | DontDelete);
        return true;
    }
    
    if (propertyName == exec->propertyNames().caller) {
        if (thisObject->jsExecutable()->isStrictMode()) {
            bool result = Base::getOwnPropertyDescriptor(thisObject, exec, propertyName, descriptor);
            if (!result) {
                thisObject->putDirectAccessor(exec->globalData(), propertyName, thisObject->globalObject()->throwTypeErrorGetterSetter(exec), DontDelete | DontEnum | Accessor);
                result = Base::getOwnPropertyDescriptor(thisObject, exec, propertyName, descriptor);
                ASSERT(result);
            }
            return result;
        }
        descriptor.setDescriptor(exec->interpreter()->retrieveCallerFromVMCode(exec, thisObject), ReadOnly | DontEnum | DontDelete);
        return true;
    }
    
    return Base::getOwnPropertyDescriptor(thisObject, exec, propertyName, descriptor);
}

void JSFunction::getOwnPropertyNames(JSObject* object, ExecState* exec, PropertyNameArray& propertyNames, EnumerationMode mode)
{
    JSFunction* thisObject = jsCast<JSFunction*>(object);
    if (!thisObject->isHostFunction() && (mode == IncludeDontEnumProperties)) {
        // Make sure prototype has been reified.
        PropertySlot slot;
        thisObject->methodTable()->getOwnPropertySlot(thisObject, exec, exec->propertyNames().prototype, slot);

        propertyNames.add(exec->propertyNames().arguments);
        propertyNames.add(exec->propertyNames().caller);
        propertyNames.add(exec->propertyNames().length);
    }
    Base::getOwnPropertyNames(thisObject, exec, propertyNames, mode);
}

void JSFunction::put(JSCell* cell, ExecState* exec, PropertyName propertyName, JSValue value, PutPropertySlot& slot)
{
    JSFunction* thisObject = jsCast<JSFunction*>(cell);
    if (thisObject->isHostFunction()) {
        Base::put(thisObject, exec, propertyName, value, slot);
        return;
    }
    if (propertyName == exec->propertyNames().prototype) {
        // Make sure prototype has been reified, such that it can only be overwritten
        // following the rules set out in ECMA-262 8.12.9.
        PropertySlot slot;
        thisObject->methodTable()->getOwnPropertySlot(thisObject, exec, propertyName, slot);
        thisObject->m_cachedInheritorID.clear();
    }
    if (thisObject->jsExecutable()->isStrictMode() && (propertyName == exec->propertyNames().arguments || propertyName == exec->propertyNames().caller)) {
        // This will trigger the property to be reified, if this is not already the case!
        bool okay = thisObject->hasProperty(exec, propertyName);
        ASSERT_UNUSED(okay, okay);
        Base::put(thisObject, exec, propertyName, value, slot);
        return;
    }
    if (propertyName == exec->propertyNames().arguments || propertyName == exec->propertyNames().length || propertyName == exec->propertyNames().caller) {
        if (slot.isStrictMode())
            throwTypeError(exec, StrictModeReadonlyPropertyWriteError);
        return;
    }
    Base::put(thisObject, exec, propertyName, value, slot);
}

bool JSFunction::deleteProperty(JSCell* cell, ExecState* exec, PropertyName propertyName)
{
    JSFunction* thisObject = jsCast<JSFunction*>(cell);
    // For non-host functions, don't let these properties by deleted - except by DefineOwnProperty.
    if (!thisObject->isHostFunction() && !exec->globalData().isInDefineOwnProperty()
        && (propertyName == exec->propertyNames().arguments
            || propertyName == exec->propertyNames().length
            || propertyName == exec->propertyNames().prototype
            || propertyName == exec->propertyNames().caller))
        return false;
    return Base::deleteProperty(thisObject, exec, propertyName);
}

bool JSFunction::defineOwnProperty(JSObject* object, ExecState* exec, PropertyName propertyName, PropertyDescriptor& descriptor, bool throwException)
{
    JSFunction* thisObject = jsCast<JSFunction*>(object);
    if (thisObject->isHostFunction())
        return Base::defineOwnProperty(object, exec, propertyName, descriptor, throwException);

    if (propertyName == exec->propertyNames().prototype) {
        // Make sure prototype has been reified, such that it can only be overwritten
        // following the rules set out in ECMA-262 8.12.9.
        PropertySlot slot;
        thisObject->methodTable()->getOwnPropertySlot(thisObject, exec, propertyName, slot);
        thisObject->m_cachedInheritorID.clear();
        return Base::defineOwnProperty(object, exec, propertyName, descriptor, throwException);
    }

    bool valueCheck;
    if (propertyName == exec->propertyNames().arguments) {
        if (thisObject->jsExecutable()->isStrictMode()) {
            if (!Base::getOwnPropertyDescriptor(thisObject, exec, propertyName, descriptor))
                thisObject->putDirectAccessor(exec->globalData(), propertyName, thisObject->globalObject()->throwTypeErrorGetterSetter(exec), DontDelete | DontEnum | Accessor);
            return Base::defineOwnProperty(object, exec, propertyName, descriptor, throwException);
        }
        valueCheck = !descriptor.value() || sameValue(exec, descriptor.value(), exec->interpreter()->retrieveArgumentsFromVMCode(exec, thisObject));
    } else if (propertyName == exec->propertyNames().caller) {
        if (thisObject->jsExecutable()->isStrictMode()) {
            if (!Base::getOwnPropertyDescriptor(thisObject, exec, propertyName, descriptor))
                thisObject->putDirectAccessor(exec->globalData(), propertyName, thisObject->globalObject()->throwTypeErrorGetterSetter(exec), DontDelete | DontEnum | Accessor);
            return Base::defineOwnProperty(object, exec, propertyName, descriptor, throwException);
        }
        valueCheck = !descriptor.value() || sameValue(exec, descriptor.value(), exec->interpreter()->retrieveCallerFromVMCode(exec, thisObject));
    } else if (propertyName == exec->propertyNames().length)
        valueCheck = !descriptor.value() || sameValue(exec, descriptor.value(), jsNumber(thisObject->jsExecutable()->parameterCount()));
    else
        return Base::defineOwnProperty(object, exec, propertyName, descriptor, throwException);
     
    if (descriptor.configurablePresent() && descriptor.configurable()) {
        if (throwException)
            throwError(exec, createTypeError(exec, "Attempting to configurable attribute of unconfigurable property."));
        return false;
    }
    if (descriptor.enumerablePresent() && descriptor.enumerable()) {
        if (throwException)
            throwError(exec, createTypeError(exec, "Attempting to change enumerable attribute of unconfigurable property."));
        return false;
    }
    if (descriptor.isAccessorDescriptor()) {
        if (throwException)
            throwError(exec, createTypeError(exec, "Attempting to change access mechanism for an unconfigurable property."));
        return false;
    }
    if (descriptor.writablePresent() && descriptor.writable()) {
        if (throwException)
            throwError(exec, createTypeError(exec, "Attempting to change writable attribute of unconfigurable property."));
        return false;
    }
    if (!valueCheck) {
        if (throwException)
            throwError(exec, createTypeError(exec, "Attempting to change value of a readonly property."));
        return false;
    }
    return true;
}

// ECMA 13.2.2 [[Construct]]
ConstructType JSFunction::getConstructData(JSCell* cell, ConstructData& constructData)
{
    JSFunction* thisObject = jsCast<JSFunction*>(cell);
    if (thisObject->isHostFunction()) {
        constructData.native.function = thisObject->nativeConstructor();
        return ConstructTypeHost;
    }
    constructData.js.functionExecutable = thisObject->jsExecutable();
    constructData.js.scopeChain = thisObject->scope();
    return ConstructTypeJS;
}
    

UString getCalculatedDisplayName(CallFrame* callFrame, JSObject* object)
{
    if (JSFunction* function = jsDynamicCast<JSFunction*>(object))
        return function->calculatedDisplayName(callFrame);
    if (InternalFunction* function = jsDynamicCast<InternalFunction*>(object))
        return function->calculatedDisplayName(callFrame);
    return "";
}

} // namespace JSC

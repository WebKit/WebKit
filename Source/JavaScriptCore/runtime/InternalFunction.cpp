/*
 *  Copyright (C) 1999-2002 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2004-2022 Apple Inc. All rights reserved.
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
#include "InternalFunction.h"

#include "JSBoundFunction.h"
#include "JSCInlines.h"
#include "ProxyObject.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(InternalFunction);

const ClassInfo InternalFunction::s_info = { "Function"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(InternalFunction) };

InternalFunction::InternalFunction(VM& vm, Structure* structure, NativeFunction functionForCall, NativeFunction functionForConstruct)
    : Base(vm, structure)
    , m_functionForCall(toTagged(functionForCall))
    , m_functionForConstruct(functionForConstruct ? toTagged(functionForConstruct) : callHostFunctionAsConstructor)
    , m_globalObject(vm, this, structure->globalObject())
{
    ASSERT_WITH_MESSAGE(m_functionForCall, "[[Call]] must be implemented");
    ASSERT(m_functionForConstruct);

    ASSERT(jsDynamicCast<InternalFunction*>(this));
    // JSCell::{getCallData,getConstructData} relies on the following conditions.
    ASSERT(methodTable()->getCallData == InternalFunction::info()->methodTable.getCallData);
    ASSERT(methodTable()->getConstructData == InternalFunction::info()->methodTable.getConstructData);
    ASSERT(type() == InternalFunctionType || type() == NullSetterFunctionType);
}

void InternalFunction::finishCreation(VM& vm, unsigned length, const String& name, PropertyAdditionMode nameAdditionMode)
{
    Base::finishCreation(vm);

    JSString* nameString = jsString(vm, name);
    m_originalName.set(vm, this, nameString);
    // The enumeration order is length followed by name. So, we make sure to add the properties in that order.
    if (nameAdditionMode == PropertyAdditionMode::WithStructureTransition) {
        putDirect(vm, vm.propertyNames->length, jsNumber(length), PropertyAttribute::ReadOnly | PropertyAttribute::DontEnum);
        putDirect(vm, vm.propertyNames->name, nameString, PropertyAttribute::ReadOnly | PropertyAttribute::DontEnum);
    } else {
        putDirectWithoutTransition(vm, vm.propertyNames->length, jsNumber(length), PropertyAttribute::ReadOnly | PropertyAttribute::DontEnum);
        putDirectWithoutTransition(vm, vm.propertyNames->name, nameString, PropertyAttribute::ReadOnly | PropertyAttribute::DontEnum);
    }
}

void InternalFunction::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
}

template<typename Visitor>
void InternalFunction::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    InternalFunction* thisObject = jsCast<InternalFunction*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
    
    visitor.append(thisObject->m_originalName);
}

DEFINE_VISIT_CHILDREN_WITH_MODIFIER(JS_EXPORT_PRIVATE, InternalFunction);

String InternalFunction::name()
{
    const String& name = m_originalName->tryGetValue();
    ASSERT(name); // m_originalName was built from a String, and hence, there is no rope to resolve.
    return name;
}

String InternalFunction::displayName(VM& vm)
{
    JSValue displayName = getDirect(vm, vm.propertyNames->displayName);
    
    if (displayName && isJSString(displayName))
        return asString(displayName)->tryGetValue();
    
    return String();
}

CallData InternalFunction::getCallData(JSCell* cell)
{
    // Keep this function OK for invocation from concurrent compilers.
    auto* function = jsCast<InternalFunction*>(cell);
    ASSERT(function->m_functionForCall);

    CallData callData;
    callData.type = CallData::Type::Native;
    callData.native.function = function->m_functionForCall;
    return callData;
}

CallData InternalFunction::getConstructData(JSCell* cell)
{
    // Keep this function OK for invocation from concurrent compilers.
    CallData constructData;
    auto* function = jsCast<InternalFunction*>(cell);
    if (function->m_functionForConstruct != callHostFunctionAsConstructor) {
        constructData.type = CallData::Type::Native;
        constructData.native.function = function->m_functionForConstruct;
    }
    return constructData;
}

String InternalFunction::calculatedDisplayName(VM& vm)
{
    String explicitName = displayName(vm);
    
    if (!explicitName.isEmpty())
        return explicitName;
    
    return name();
}

Structure* InternalFunction::createSubclassStructure(JSGlobalObject* globalObject, JSObject* newTarget, Structure* baseClass)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSGlobalObject* baseGlobalObject = baseClass->globalObject();

    ASSERT(baseClass->hasMonoProto());

    // newTarget may be an InternalFunction if we were called from Reflect.construct.
    JSFunction* targetFunction = jsDynamicCast<JSFunction*>(newTarget);

    if (LIKELY(targetFunction)) {
        FunctionRareData* rareData = targetFunction->ensureRareData(vm);
        Structure* structure = rareData->internalFunctionAllocationStructure();
        if (LIKELY(structure && structure->classInfoForCells() == baseClass->classInfoForCells() && structure->globalObject() == baseGlobalObject))
            return structure;

        // Note, Reflect.construct might cause the profile to churn but we don't care.
        JSValue prototypeValue = targetFunction->get(globalObject, vm.propertyNames->prototype);
        RETURN_IF_EXCEPTION(scope, nullptr);
        if (JSObject* prototype = jsDynamicCast<JSObject*>(prototypeValue))
            return rareData->createInternalFunctionAllocationStructureFromBase(vm, baseGlobalObject, prototype, baseClass);
    } else {
        JSValue prototypeValue = newTarget->get(globalObject, vm.propertyNames->prototype);
        RETURN_IF_EXCEPTION(scope, nullptr);
        if (JSObject* prototype = jsDynamicCast<JSObject*>(prototypeValue)) {
            // This only happens if someone Reflect.constructs our builtin constructor with another builtin constructor as the new.target.
            // Thus, we don't care about the cost of looking up the structure from our hash table every time.
            return baseGlobalObject->structureCache().emptyStructureForPrototypeFromBaseStructure(baseGlobalObject, prototype, baseClass);
        }
    }
    
    return baseClass;
}

InternalFunction* InternalFunction::createFunctionThatMasqueradesAsUndefined(VM& vm, JSGlobalObject* globalObject, unsigned length, const String& name, NativeFunction nativeFunction)
{
    Structure* structure = Structure::create(vm, globalObject, globalObject->objectPrototype(), TypeInfo(InternalFunctionType, InternalFunction::StructureFlags | MasqueradesAsUndefined), InternalFunction::info());
    globalObject->masqueradesAsUndefinedWatchpointSet().fireAll(globalObject->vm(), "Allocated masquerading object");
    InternalFunction* function = new (NotNull, allocateCell<InternalFunction>(vm)) InternalFunction(vm, structure, nativeFunction);
    function->finishCreation(vm, length, name, PropertyAdditionMode::WithoutStructureTransition);
    return function;
}

// https://tc39.es/ecma262/#sec-getfunctionrealm
JSGlobalObject* getFunctionRealm(JSGlobalObject* globalObject, JSObject* object)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    ASSERT(object->isCallable());

    while (true) {
        if (object->inherits<JSBoundFunction>()) {
            object = jsCast<JSBoundFunction*>(object)->targetFunction();
            continue;
        }
        if (object->inherits<JSRemoteFunction>()) {
            object = jsCast<JSRemoteFunction*>(object)->targetFunction();
            continue;
        }

        if (object->type() == ProxyObjectType) {
            auto* proxy = jsCast<ProxyObject*>(object);
            if (proxy->isRevoked()) {
                throwTypeError(globalObject, scope, "Cannot get function realm from revoked Proxy"_s);
                return nullptr;
            }
            object = proxy->target();
            continue;
        }

        return object->globalObject();
    }
}


} // namespace JSC

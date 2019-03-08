/*
 *  Copyright (C) 1999-2002 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2004-2019 Apple Inc. All rights reserved.
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

#include "FunctionPrototype.h"
#include "JSGlobalObject.h"
#include "JSString.h"
#include "JSCInlines.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(InternalFunction);

const ClassInfo InternalFunction::s_info = { "Function", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(InternalFunction) };

InternalFunction::InternalFunction(VM& vm, Structure* structure, NativeFunction functionForCall, NativeFunction functionForConstruct)
    : JSDestructibleObject(vm, structure)
    , m_functionForCall(functionForCall)
    , m_functionForConstruct(functionForConstruct ? functionForConstruct : callHostFunctionAsConstructor)
{
    // exec->vm() wants callees to not be large allocations.
    RELEASE_ASSERT(!isLargeAllocation());
    ASSERT_WITH_MESSAGE(m_functionForCall, "[[Call]] must be implemented");
    ASSERT(m_functionForConstruct);
}

void InternalFunction::finishCreation(VM& vm, const String& name, NameVisibility nameVisibility, NameAdditionMode nameAdditionMode)
{
    Base::finishCreation(vm);
    ASSERT(jsDynamicCast<InternalFunction*>(vm, this));
    ASSERT(methodTable(vm)->getCallData == InternalFunction::info()->methodTable.getCallData);
    ASSERT(methodTable(vm)->getConstructData == InternalFunction::info()->methodTable.getConstructData);
    ASSERT(type() == InternalFunctionType);
    JSString* nameString = jsString(&vm, name);
    m_originalName.set(vm, this, nameString);
    if (nameVisibility == NameVisibility::Visible) {
        if (nameAdditionMode == NameAdditionMode::WithStructureTransition)
            putDirect(vm, vm.propertyNames->name, nameString, PropertyAttribute::ReadOnly | PropertyAttribute::DontEnum);
        else
            putDirectWithoutTransition(vm, vm.propertyNames->name, nameString, PropertyAttribute::ReadOnly | PropertyAttribute::DontEnum);
    }
}

void InternalFunction::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    InternalFunction* thisObject = jsCast<InternalFunction*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
    
    visitor.append(thisObject->m_originalName);
}

const String& InternalFunction::name()
{
    const String& name = m_originalName->tryGetValue();
    ASSERT(name); // m_originalName was built from a String, and hence, there is no rope to resolve.
    return name;
}

const String InternalFunction::displayName(VM& vm)
{
    JSValue displayName = getDirect(vm, vm.propertyNames->displayName);
    
    if (displayName && isJSString(displayName))
        return asString(displayName)->tryGetValue();
    
    return String();
}

CallType InternalFunction::getCallData(JSCell* cell, CallData& callData)
{
    auto* function = jsCast<InternalFunction*>(cell);
    ASSERT(function->m_functionForCall);
    callData.native.function = function->m_functionForCall;
    return CallType::Host;
}

ConstructType InternalFunction::getConstructData(JSCell* cell, ConstructData& constructData)
{
    auto* function = jsCast<InternalFunction*>(cell);
    if (function->m_functionForConstruct == callHostFunctionAsConstructor)
        return ConstructType::None;
    constructData.native.function = function->m_functionForConstruct;
    return ConstructType::Host;
}

const String InternalFunction::calculatedDisplayName(VM& vm)
{
    const String explicitName = displayName(vm);
    
    if (!explicitName.isEmpty())
        return explicitName;
    
    return name();
}

Structure* InternalFunction::createSubclassStructureSlow(ExecState* exec, JSValue newTarget, Structure* baseClass)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    ASSERT(!newTarget || newTarget.isConstructor(vm));
    ASSERT(newTarget && newTarget != exec->jsCallee());

    ASSERT(baseClass->hasMonoProto());

    // newTarget may be an InternalFunction if we were called from Reflect.construct.
    JSFunction* targetFunction = jsDynamicCast<JSFunction*>(vm, newTarget);
    JSGlobalObject* lexicalGlobalObject = exec->lexicalGlobalObject();

    if (LIKELY(targetFunction)) {
        Structure* structure = targetFunction->rareData(vm)->internalFunctionAllocationStructure();
        if (LIKELY(structure && structure->classInfo() == baseClass->classInfo()))
            return structure;

        // Note, Reflect.construct might cause the profile to churn but we don't care.
        JSValue prototypeValue = newTarget.get(exec, vm.propertyNames->prototype);
        RETURN_IF_EXCEPTION(scope, nullptr);
        if (JSObject* prototype = jsDynamicCast<JSObject*>(vm, prototypeValue))
            return targetFunction->rareData(vm)->createInternalFunctionAllocationStructureFromBase(vm, lexicalGlobalObject, prototype, baseClass);
    } else {
        JSValue prototypeValue = newTarget.get(exec, vm.propertyNames->prototype);
        RETURN_IF_EXCEPTION(scope, nullptr);
        if (JSObject* prototype = jsDynamicCast<JSObject*>(vm, prototypeValue)) {
            // This only happens if someone Reflect.constructs our builtin constructor with another builtin constructor as the new.target.
            // Thus, we don't care about the cost of looking up the structure from our hash table every time.
            return vm.structureCache.emptyStructureForPrototypeFromBaseStructure(lexicalGlobalObject, prototype, baseClass);
        }
    }
    
    return baseClass;
}


} // namespace JSC

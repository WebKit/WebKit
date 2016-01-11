/*
 *  Copyright (C) 1999-2002 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2004, 2007, 2008, 2016 Apple Inc. All rights reserved.
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

const ClassInfo InternalFunction::s_info = { "Function", &Base::s_info, 0, CREATE_METHOD_TABLE(InternalFunction) };

InternalFunction::InternalFunction(VM& vm, Structure* structure)
    : JSDestructibleObject(vm, structure)
{
}

void InternalFunction::finishCreation(VM& vm, const String& name)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));
    ASSERT(methodTable()->getCallData != InternalFunction::info()->methodTable.getCallData);
    putDirect(vm, vm.propertyNames->name, jsString(&vm, name), DontDelete | ReadOnly | DontEnum);
}

const String& InternalFunction::name(ExecState* exec)
{
    return asString(getDirect(exec->vm(), exec->vm().propertyNames->name))->tryGetValue();
}

const String InternalFunction::displayName(ExecState* exec)
{
    JSValue displayName = getDirect(exec->vm(), exec->vm().propertyNames->displayName);
    
    if (displayName && isJSString(displayName))
        return asString(displayName)->tryGetValue();
    
    return String();
}

CallType InternalFunction::getCallData(JSCell*, CallData&)
{
    RELEASE_ASSERT_NOT_REACHED();
    return CallTypeNone;
}

const String InternalFunction::calculatedDisplayName(ExecState* exec)
{
    const String explicitName = displayName(exec);
    
    if (!explicitName.isEmpty())
        return explicitName;
    
    return name(exec);
}

Structure* InternalFunction::createSubclassStructure(ExecState* exec, JSValue newTarget, Structure* baseClass)
{

    VM& vm = exec->vm();
    // We allow newTarget == JSValue() because the API needs to be able to create classes without having a real JS frame.
    // Since we don't allow subclassing in the API we just treat newTarget == JSValue() as newTarget == exec->callee()
    ASSERT(!newTarget || newTarget.isFunction());

    if (newTarget && newTarget != exec->callee()) {
        // newTarget may be an InternalFunction if we were called from Reflect.construct.
        JSFunction* targetFunction = jsDynamicCast<JSFunction*>(newTarget);

        if (LIKELY(targetFunction)) {
            Structure* structure = targetFunction->rareData(vm)->internalFunctionAllocationStructure();
            if (LIKELY(structure && structure->classInfo() == baseClass->classInfo()))
                return structure;

            // Note, Reflect.construct might cause the profile to churn but we don't care.
            JSObject* prototype = jsDynamicCast<JSObject*>(newTarget.get(exec, exec->propertyNames().prototype));
            if (prototype)
                return targetFunction->rareData(vm)->createInternalFunctionAllocationStructureFromBase(vm, prototype, baseClass);
        } else {
            JSObject* prototype = jsDynamicCast<JSObject*>(newTarget.get(exec, exec->propertyNames().prototype));
            if (prototype) {
                // This only happens if someone Reflect.constructs our builtin constructor with another builtin constructor as the new.target.
                // Thus, we don't care about the cost of looking up the structure from our hash table every time.
                return vm.prototypeMap.emptyStructureForPrototypeFromBaseStructure(prototype, baseClass);
            }
        }
    }
    
    return baseClass;
}


} // namespace JSC

/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003, 2008, 2016 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "config.h"
#include "BooleanConstructor.h"

#include "BooleanPrototype.h"
#include "JSGlobalObject.h"
#include "JSCInlines.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(BooleanConstructor);

const ClassInfo BooleanConstructor::s_info = { "Function", &Base::s_info, 0, CREATE_METHOD_TABLE(BooleanConstructor) };

BooleanConstructor::BooleanConstructor(VM& vm, Structure* structure)
    : InternalFunction(vm, structure)
{
}

void BooleanConstructor::finishCreation(VM& vm, BooleanPrototype* booleanPrototype)
{
    Base::finishCreation(vm, booleanPrototype->classInfo()->className);
    putDirectWithoutTransition(vm, vm.propertyNames->prototype, booleanPrototype, DontEnum | DontDelete | ReadOnly);

    // no. of arguments for constructor
    putDirectWithoutTransition(vm, vm.propertyNames->length, jsNumber(1), ReadOnly | DontDelete | DontEnum);
}

// ECMA 15.6.2
static EncodedJSValue JSC_HOST_CALL constructWithBooleanConstructor(ExecState* exec)
{
    JSValue boolean = jsBoolean(exec->argument(0).toBoolean(exec));

    JSValue prototype = JSValue();
    JSValue newTarget = exec->newTarget();
    if (newTarget != exec->callee())
        prototype = newTarget.get(exec, exec->propertyNames().prototype);

    BooleanObject* obj = BooleanObject::create(exec->vm(), Structure::createSubclassStructure(exec->vm(), asInternalFunction(exec->callee())->globalObject()->booleanObjectStructure(), prototype));
    obj->setInternalValue(exec->vm(), boolean);
    return JSValue::encode(obj);
}

ConstructType BooleanConstructor::getConstructData(JSCell*, ConstructData& constructData)
{
    constructData.native.function = constructWithBooleanConstructor;
    return ConstructTypeHost;
}

// ECMA 15.6.1
static EncodedJSValue JSC_HOST_CALL callBooleanConstructor(ExecState* exec)
{
    return JSValue::encode(jsBoolean(exec->argument(0).toBoolean(exec)));
}

CallType BooleanConstructor::getCallData(JSCell*, CallData& callData)
{
    callData.native.function = callBooleanConstructor;
    return CallTypeHost;
}

JSObject* constructBooleanFromImmediateBoolean(ExecState* exec, JSGlobalObject* globalObject, JSValue immediateBooleanValue)
{
    BooleanObject* obj = BooleanObject::create(exec->vm(), globalObject->booleanObjectStructure());
    obj->setInternalValue(exec->vm(), immediateBooleanValue);
    return obj;
}

} // namespace JSC

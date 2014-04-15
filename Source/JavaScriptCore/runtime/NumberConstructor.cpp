/*
 *  Copyright (C) 1999-2000,2003 Harri Porten (porten@kde.org)
 *  Copyright (C) 2007, 2008, 2011 Apple Inc. All rights reserved.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 *  USA
 *
 */

#include "config.h"
#include "NumberConstructor.h"

#include "Lookup.h"
#include "NumberObject.h"
#include "NumberPrototype.h"
#include "JSCInlines.h"

namespace JSC {

static EncodedJSValue numberConstructorNaNValue(ExecState*, JSObject*, EncodedJSValue, PropertyName);
static EncodedJSValue numberConstructorNegInfinity(ExecState*, JSObject*, EncodedJSValue, PropertyName);
static EncodedJSValue numberConstructorPosInfinity(ExecState*, JSObject*, EncodedJSValue, PropertyName);
static EncodedJSValue numberConstructorMaxValue(ExecState*, JSObject*, EncodedJSValue, PropertyName);
static EncodedJSValue numberConstructorMinValue(ExecState*, JSObject*, EncodedJSValue, PropertyName);

} // namespace JSC

#include "NumberConstructor.lut.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(NumberConstructor);

const ClassInfo NumberConstructor::s_info = { "Function", &InternalFunction::s_info, 0, ExecState::numberConstructorTable, CREATE_METHOD_TABLE(NumberConstructor) };

/* Source for NumberConstructor.lut.h
@begin numberConstructorTable
   NaN                   numberConstructorNaNValue       DontEnum|DontDelete|ReadOnly
   NEGATIVE_INFINITY     numberConstructorNegInfinity    DontEnum|DontDelete|ReadOnly
   POSITIVE_INFINITY     numberConstructorPosInfinity    DontEnum|DontDelete|ReadOnly
   MAX_VALUE             numberConstructorMaxValue       DontEnum|DontDelete|ReadOnly
   MIN_VALUE             numberConstructorMinValue       DontEnum|DontDelete|ReadOnly
@end
*/

NumberConstructor::NumberConstructor(VM& vm, Structure* structure)
    : InternalFunction(vm, structure)
{
}

void NumberConstructor::finishCreation(VM& vm, NumberPrototype* numberPrototype)
{
    Base::finishCreation(vm, NumberPrototype::info()->className);
    ASSERT(inherits(info()));

    // Number.Prototype
    putDirectWithoutTransition(vm, vm.propertyNames->prototype, numberPrototype, DontEnum | DontDelete | ReadOnly);

    // no. of arguments for constructor
    putDirectWithoutTransition(vm, vm.propertyNames->length, jsNumber(1), ReadOnly | DontEnum | DontDelete);
}

bool NumberConstructor::getOwnPropertySlot(JSObject* object, ExecState* exec, PropertyName propertyName, PropertySlot& slot)
{
    return getStaticValueSlot<NumberConstructor, InternalFunction>(exec, ExecState::numberConstructorTable(exec->vm()), jsCast<NumberConstructor*>(object), propertyName, slot);
}

static EncodedJSValue numberConstructorNaNValue(ExecState*, JSObject*, EncodedJSValue, PropertyName)
{
    return JSValue::encode(jsNaN());
}

static EncodedJSValue numberConstructorNegInfinity(ExecState*, JSObject*, EncodedJSValue, PropertyName)
{
    return JSValue::encode(jsNumber(-std::numeric_limits<double>::infinity()));
}

static EncodedJSValue numberConstructorPosInfinity(ExecState*, JSObject*, EncodedJSValue, PropertyName)
{
    return JSValue::encode(jsNumber(std::numeric_limits<double>::infinity()));
}

static EncodedJSValue numberConstructorMaxValue(ExecState*, JSObject*, EncodedJSValue, PropertyName)
{
    return JSValue::encode(jsNumber(1.7976931348623157E+308));
}

static EncodedJSValue numberConstructorMinValue(ExecState*, JSObject*, EncodedJSValue, PropertyName)
{
    return JSValue::encode(jsNumber(5E-324));
}

// ECMA 15.7.1
static EncodedJSValue JSC_HOST_CALL constructWithNumberConstructor(ExecState* exec)
{
    NumberObject* object = NumberObject::create(exec->vm(), asInternalFunction(exec->callee())->globalObject()->numberObjectStructure());
    double n = exec->argumentCount() ? exec->uncheckedArgument(0).toNumber(exec) : 0;
    object->setInternalValue(exec->vm(), jsNumber(n));
    return JSValue::encode(object);
}

ConstructType NumberConstructor::getConstructData(JSCell*, ConstructData& constructData)
{
    constructData.native.function = constructWithNumberConstructor;
    return ConstructTypeHost;
}

// ECMA 15.7.2
static EncodedJSValue JSC_HOST_CALL callNumberConstructor(ExecState* exec)
{
    return JSValue::encode(jsNumber(!exec->argumentCount() ? 0 : exec->uncheckedArgument(0).toNumber(exec)));
}

CallType NumberConstructor::getCallData(JSCell*, CallData& callData)
{
    callData.native.function = callNumberConstructor;
    return CallTypeHost;
}

} // namespace JSC

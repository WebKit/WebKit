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
#include "JSGlobalObjectFunctions.h"

namespace JSC {

static EncodedJSValue numberConstructorEpsilonValue(ExecState*, JSObject*, EncodedJSValue, PropertyName);
static EncodedJSValue numberConstructorNaNValue(ExecState*, JSObject*, EncodedJSValue, PropertyName);
static EncodedJSValue numberConstructorNegInfinity(ExecState*, JSObject*, EncodedJSValue, PropertyName);
static EncodedJSValue numberConstructorPosInfinity(ExecState*, JSObject*, EncodedJSValue, PropertyName);
static EncodedJSValue numberConstructorMaxValue(ExecState*, JSObject*, EncodedJSValue, PropertyName);
static EncodedJSValue numberConstructorMinValue(ExecState*, JSObject*, EncodedJSValue, PropertyName);
static EncodedJSValue numberConstructorMaxSafeInteger(ExecState*, JSObject*, EncodedJSValue, PropertyName);
static EncodedJSValue numberConstructorMinSafeInteger(ExecState*, JSObject*, EncodedJSValue, PropertyName);
static EncodedJSValue JSC_HOST_CALL numberConstructorFuncIsFinite(ExecState*);
static EncodedJSValue JSC_HOST_CALL numberConstructorFuncIsInteger(ExecState*);
static EncodedJSValue JSC_HOST_CALL numberConstructorFuncIsNaN(ExecState*);
static EncodedJSValue JSC_HOST_CALL numberConstructorFuncIsSafeInteger(ExecState*);

} // namespace JSC

#include "NumberConstructor.lut.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(NumberConstructor);

const ClassInfo NumberConstructor::s_info = { "Function", &InternalFunction::s_info, &numberConstructorTable, CREATE_METHOD_TABLE(NumberConstructor) };

/* Source for NumberConstructor.lut.h
@begin numberConstructorTable
   EPSILON               numberConstructorEpsilonValue      DontEnum|DontDelete|ReadOnly
   NaN                   numberConstructorNaNValue          DontEnum|DontDelete|ReadOnly
   NEGATIVE_INFINITY     numberConstructorNegInfinity       DontEnum|DontDelete|ReadOnly
   POSITIVE_INFINITY     numberConstructorPosInfinity       DontEnum|DontDelete|ReadOnly
   MAX_VALUE             numberConstructorMaxValue          DontEnum|DontDelete|ReadOnly
   MIN_VALUE             numberConstructorMinValue          DontEnum|DontDelete|ReadOnly
   MAX_SAFE_INTEGER      numberConstructorMaxSafeInteger    DontEnum|DontDelete|ReadOnly
   MIN_SAFE_INTEGER      numberConstructorMinSafeInteger    DontEnum|DontDelete|ReadOnly
   isFinite              numberConstructorFuncIsFinite      DontEnum|Function 1
   isInteger             numberConstructorFuncIsInteger     DontEnum|Function 1
   isNaN                 numberConstructorFuncIsNaN         DontEnum|Function 1
   isSafeInteger         numberConstructorFuncIsSafeInteger DontEnum|Function 1
   parseFloat            globalFuncParseFloat                DontEnum|Function 1
   parseInt              globalFuncParseInt                  DontEnum|Function 2
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
    if (isFunction(exec->vm(), propertyName.uid())) {
        return getStaticFunctionSlot<InternalFunction>(exec, numberConstructorTable,
            jsCast<NumberConstructor*>(object), propertyName, slot);
    }
    return getStaticValueSlot<NumberConstructor, InternalFunction>(exec, numberConstructorTable,
        jsCast<NumberConstructor*>(object), propertyName, slot);
}

bool ALWAYS_INLINE NumberConstructor::isFunction(VM& vm, AtomicStringImpl* propertyName)
{
    return propertyName == vm.propertyNames->isFinite
        || propertyName == vm.propertyNames->isInteger
        || propertyName == vm.propertyNames->isNaN
        || propertyName == vm.propertyNames->isSafeInteger
        || propertyName == vm.propertyNames->parseFloat
        || propertyName == vm.propertyNames->parseInt;
}

// ECMA-262 20.1.2.1
static EncodedJSValue numberConstructorEpsilonValue(ExecState*, JSObject*, EncodedJSValue, PropertyName)
{
    return JSValue::encode(jsDoubleNumber((std::numeric_limits<double>::epsilon())));
}

// ECMA-262 20.1.2.8
static EncodedJSValue numberConstructorNaNValue(ExecState*, JSObject*, EncodedJSValue, PropertyName)
{
    return JSValue::encode(jsNaN());
}

// ECMA-262 20.1.2.9
static EncodedJSValue numberConstructorNegInfinity(ExecState*, JSObject*, EncodedJSValue, PropertyName)
{
    return JSValue::encode(jsDoubleNumber(-std::numeric_limits<double>::infinity()));
}

// ECMA-262 20.1.2.14
static EncodedJSValue numberConstructorPosInfinity(ExecState*, JSObject*, EncodedJSValue, PropertyName)
{
    return JSValue::encode(jsDoubleNumber(std::numeric_limits<double>::infinity()));
}

// ECMA-262 20.1.2.7
static EncodedJSValue numberConstructorMaxValue(ExecState*, JSObject*, EncodedJSValue, PropertyName)
{
    return JSValue::encode(jsDoubleNumber(1.7976931348623157E+308));
}

// ECMA-262 20.1.2.11
static EncodedJSValue numberConstructorMinValue(ExecState*, JSObject*, EncodedJSValue, PropertyName)
{
    return JSValue::encode(jsDoubleNumber(5E-324));
}

// ECMA-262 20.1.2.6
static EncodedJSValue numberConstructorMaxSafeInteger(ExecState*, JSObject*, EncodedJSValue, PropertyName)
{
    return JSValue::encode(jsDoubleNumber(9007199254740991.0));
}

// ECMA-262 20.1.2.7
static EncodedJSValue numberConstructorMinSafeInteger(ExecState*, JSObject*, EncodedJSValue, PropertyName)
{
    return JSValue::encode(jsDoubleNumber(-9007199254740991.0));
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

// ECMA-262 20.1.2.2
static EncodedJSValue JSC_HOST_CALL numberConstructorFuncIsFinite(ExecState* exec)
{
    JSValue argument = exec->argument(0);
    return JSValue::encode(jsBoolean(argument.isNumber() && (argument.isInt32() || std::isfinite(argument.asDouble()))));
}

// ECMA-262 20.1.2.3
static EncodedJSValue JSC_HOST_CALL numberConstructorFuncIsInteger(ExecState* exec)
{
    JSValue argument = exec->argument(0);
    bool isInteger;
    if (argument.isInt32())
        isInteger = true;
    else if (!argument.isDouble())
        isInteger = false;
    else {
        double number = argument.asDouble();
        isInteger = std::isfinite(number) && trunc(number) == number;
    }
    return JSValue::encode(jsBoolean(isInteger));
}

// ECMA-262 20.1.2.4
static EncodedJSValue JSC_HOST_CALL numberConstructorFuncIsNaN(ExecState* exec)
{
    JSValue argument = exec->argument(0);
    return JSValue::encode(jsBoolean(argument.isDouble() && std::isnan(argument.asDouble())));
}

// ECMA-262 20.1.2.5
static EncodedJSValue JSC_HOST_CALL numberConstructorFuncIsSafeInteger(ExecState* exec)
{
    JSValue argument = exec->argument(0);
    bool isInteger;
    if (argument.isInt32())
        isInteger = true;
    else if (!argument.isDouble())
        isInteger = false;
    else {
        double number = argument.asDouble();
        isInteger = trunc(number) == number && std::abs(number) <= 9007199254740991.0;
    }
    return JSValue::encode(jsBoolean(isInteger));
}

} // namespace JSC

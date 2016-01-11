/*
 *  Copyright (C) 1999-2000,2003 Harri Porten (porten@kde.org)
 *  Copyright (C) 2007, 2008, 2011, 2015-2016 Apple Inc. All rights reserved.
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
#include "StructureInlines.h"

namespace JSC {

static EncodedJSValue JSC_HOST_CALL numberConstructorFuncIsFinite(ExecState*);
static EncodedJSValue JSC_HOST_CALL numberConstructorFuncIsInteger(ExecState*);
static EncodedJSValue JSC_HOST_CALL numberConstructorFuncIsNaN(ExecState*);
static EncodedJSValue JSC_HOST_CALL numberConstructorFuncIsSafeInteger(ExecState*);

} // namespace JSC

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(NumberConstructor);

const ClassInfo NumberConstructor::s_info = { "Function", &InternalFunction::s_info, 0, CREATE_METHOD_TABLE(NumberConstructor) };

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

    putDirectWithoutTransition(vm, Identifier::fromString(&vm, "EPSILON"), jsDoubleNumber(std::numeric_limits<double>::epsilon()), DontDelete | DontEnum | ReadOnly);
    putDirectWithoutTransition(vm, Identifier::fromString(&vm, "MAX_VALUE"), jsDoubleNumber(1.7976931348623157E+308), DontDelete | DontEnum | ReadOnly);
    putDirectWithoutTransition(vm, Identifier::fromString(&vm, "MIN_VALUE"), jsDoubleNumber(5E-324), DontDelete | DontEnum | ReadOnly);
    putDirectWithoutTransition(vm, Identifier::fromString(&vm, "MAX_SAFE_INTEGER"), jsDoubleNumber(9007199254740991.0), DontDelete | DontEnum | ReadOnly);
    putDirectWithoutTransition(vm, Identifier::fromString(&vm, "MIN_SAFE_INTEGER"), jsDoubleNumber(-9007199254740991.0), DontDelete | DontEnum | ReadOnly);
    putDirectWithoutTransition(vm, Identifier::fromString(&vm, "NEGATIVE_INFINITY"), jsDoubleNumber(-std::numeric_limits<double>::infinity()), DontDelete | DontEnum | ReadOnly);
    putDirectWithoutTransition(vm, Identifier::fromString(&vm, "POSITIVE_INFINITY"), jsDoubleNumber(std::numeric_limits<double>::infinity()), DontDelete | DontEnum | ReadOnly);
    putDirectWithoutTransition(vm, Identifier::fromString(&vm, "NaN"), jsNaN(), DontDelete | DontEnum | ReadOnly);

    putDirectNativeFunctionWithoutTransition(vm, numberPrototype->globalObject(), Identifier::fromString(&vm, "isFinite"), 1, numberConstructorFuncIsFinite, NoIntrinsic, DontEnum);
    putDirectNativeFunctionWithoutTransition(vm, numberPrototype->globalObject(), Identifier::fromString(&vm, "isInteger"), 1, numberConstructorFuncIsInteger, NoIntrinsic, DontEnum);
    putDirectNativeFunctionWithoutTransition(vm, numberPrototype->globalObject(), Identifier::fromString(&vm, "isNaN"), 1, numberConstructorFuncIsNaN, NoIntrinsic, DontEnum);
    putDirectNativeFunctionWithoutTransition(vm, numberPrototype->globalObject(), Identifier::fromString(&vm, "isSafeInteger"), 1, numberConstructorFuncIsSafeInteger, NoIntrinsic, DontEnum);
    putDirectNativeFunctionWithoutTransition(vm, numberPrototype->globalObject(), Identifier::fromString(&vm, "parseFloat"), 1, globalFuncParseFloat, NoIntrinsic, DontEnum);
    putDirectWithoutTransition(vm, Identifier::fromString(&vm, "parseInt"), numberPrototype->globalObject()->parseIntFunction(), DontEnum);
}

// ECMA 15.7.1
static EncodedJSValue JSC_HOST_CALL constructWithNumberConstructor(ExecState* exec)
{
    double n = exec->argumentCount() ? exec->uncheckedArgument(0).toNumber(exec) : 0;
    NumberObject* object = NumberObject::create(exec->vm(), InternalFunction::createSubclassStructure(exec, exec->newTarget(), asInternalFunction(exec->callee())->globalObject()->numberObjectStructure()));

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

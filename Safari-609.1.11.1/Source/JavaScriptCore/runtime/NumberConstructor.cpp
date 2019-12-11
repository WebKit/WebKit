/*
 *  Copyright (C) 1999-2000,2003 Harri Porten (porten@kde.org)
 *  Copyright (C) 2007-2019 Apple Inc. All rights reserved.
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

static EncodedJSValue JSC_HOST_CALL numberConstructorFuncIsInteger(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL numberConstructorFuncIsSafeInteger(JSGlobalObject*, CallFrame*);

} // namespace JSC

#include "NumberConstructor.lut.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(NumberConstructor);

const ClassInfo NumberConstructor::s_info = { "Function", &InternalFunction::s_info, &numberConstructorTable, nullptr, CREATE_METHOD_TABLE(NumberConstructor) };

/* Source for NumberConstructor.lut.h
@begin numberConstructorTable
  isFinite       JSBuiltin                           DontEnum|Function 1
  isNaN          JSBuiltin                           DontEnum|Function 1
  isSafeInteger  numberConstructorFuncIsSafeInteger  DontEnum|Function 1
@end
*/

static EncodedJSValue JSC_HOST_CALL callNumberConstructor(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL constructNumberConstructor(JSGlobalObject*, CallFrame*);

NumberConstructor::NumberConstructor(VM& vm, Structure* structure)
    : InternalFunction(vm, structure, callNumberConstructor, constructNumberConstructor)
{
}

void NumberConstructor::finishCreation(VM& vm, NumberPrototype* numberPrototype)
{
    Base::finishCreation(vm, vm.propertyNames->Number.string(), NameAdditionMode::WithoutStructureTransition);
    ASSERT(inherits(vm, info()));

    JSGlobalObject* globalObject = numberPrototype->globalObject(vm);

    putDirectWithoutTransition(vm, vm.propertyNames->prototype, numberPrototype, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly);
    putDirectWithoutTransition(vm, vm.propertyNames->length, jsNumber(1), PropertyAttribute::ReadOnly | PropertyAttribute::DontEnum);

    putDirectWithoutTransition(vm, Identifier::fromString(vm, "EPSILON"), jsDoubleNumber(std::numeric_limits<double>::epsilon()), PropertyAttribute::DontDelete | PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly);
    putDirectWithoutTransition(vm, Identifier::fromString(vm, "MAX_VALUE"), jsDoubleNumber(1.7976931348623157E+308), PropertyAttribute::DontDelete | PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly);
    putDirectWithoutTransition(vm, Identifier::fromString(vm, "MIN_VALUE"), jsDoubleNumber(5E-324), PropertyAttribute::DontDelete | PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly);
    putDirectWithoutTransition(vm, Identifier::fromString(vm, "MAX_SAFE_INTEGER"), jsDoubleNumber(maxSafeInteger()), PropertyAttribute::DontDelete | PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly);
    putDirectWithoutTransition(vm, Identifier::fromString(vm, "MIN_SAFE_INTEGER"), jsDoubleNumber(minSafeInteger()), PropertyAttribute::DontDelete | PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly);
    putDirectWithoutTransition(vm, Identifier::fromString(vm, "NEGATIVE_INFINITY"), jsDoubleNumber(-std::numeric_limits<double>::infinity()), PropertyAttribute::DontDelete | PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly);
    putDirectWithoutTransition(vm, Identifier::fromString(vm, "POSITIVE_INFINITY"), jsDoubleNumber(std::numeric_limits<double>::infinity()), PropertyAttribute::DontDelete | PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly);
    putDirectWithoutTransition(vm, vm.propertyNames->NaN, jsNaN(), PropertyAttribute::DontDelete | PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly);

    putDirectWithoutTransition(vm, vm.propertyNames->parseInt, numberPrototype->globalObject(vm)->parseIntFunction(), static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectWithoutTransition(vm, vm.propertyNames->parseFloat, numberPrototype->globalObject(vm)->parseFloatFunction(), static_cast<unsigned>(PropertyAttribute::DontEnum));

    JSC_NATIVE_INTRINSIC_FUNCTION_WITHOUT_TRANSITION(Identifier::fromString(vm, "isInteger"), numberConstructorFuncIsInteger, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, NumberIsIntegerIntrinsic);
}

// ECMA 15.7.1
static EncodedJSValue JSC_HOST_CALL constructNumberConstructor(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    double n = callFrame->argumentCount() ? callFrame->uncheckedArgument(0).toNumber(globalObject) : 0;
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    Structure* structure = InternalFunction::createSubclassStructure(globalObject, callFrame->jsCallee(), callFrame->newTarget(), globalObject->numberObjectStructure());
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    NumberObject* object = NumberObject::create(vm, structure);
    object->setInternalValue(vm, jsNumber(n));
    return JSValue::encode(object);
}

// ECMA 15.7.2
static EncodedJSValue JSC_HOST_CALL callNumberConstructor(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    return JSValue::encode(jsNumber(!callFrame->argumentCount() ? 0 : callFrame->uncheckedArgument(0).toNumber(globalObject)));
}

// ECMA-262 20.1.2.3
static EncodedJSValue JSC_HOST_CALL numberConstructorFuncIsInteger(JSGlobalObject*, CallFrame* callFrame)
{
    return JSValue::encode(jsBoolean(NumberConstructor::isIntegerImpl(callFrame->argument(0))));
}

// ECMA-262 20.1.2.5
static EncodedJSValue JSC_HOST_CALL numberConstructorFuncIsSafeInteger(JSGlobalObject*, CallFrame* callFrame)
{
    JSValue argument = callFrame->argument(0);
    bool isInteger;
    if (argument.isInt32())
        isInteger = true;
    else if (!argument.isDouble())
        isInteger = false;
    else {
        double number = argument.asDouble();
        isInteger = trunc(number) == number && std::abs(number) <= maxSafeInteger();
    }
    return JSValue::encode(jsBoolean(isInteger));
}

} // namespace JSC

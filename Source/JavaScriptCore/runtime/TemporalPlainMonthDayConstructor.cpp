/*
 * Copyright (C) 2025 Igalia, S.L. All rights reserved.
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
#include "TemporalPlainMonthDayConstructor.h"

#include "IntlObjectInlines.h"
#include "JSCInlines.h"
#include "TemporalPlainMonthDay.h"
#include "TemporalPlainMonthDayPrototype.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(TemporalPlainMonthDayConstructor);

static JSC_DECLARE_HOST_FUNCTION(temporalPlainMonthDayConstructorFuncFrom);

}

#include "TemporalPlainMonthDayConstructor.lut.h"

namespace JSC {

const ClassInfo TemporalPlainMonthDayConstructor::s_info = { "Function"_s, &Base::s_info, &temporalPlainMonthDayConstructorTable, nullptr, CREATE_METHOD_TABLE(TemporalPlainMonthDayConstructor) };

/* Source for TemporalPlainMonthDayConstructor.lut.h
@begin temporalPlainMonthDayConstructorTable
  from             temporalPlainMonthDayConstructorFuncFrom             DontEnum|Function 1
@end
*/

TemporalPlainMonthDayConstructor* TemporalPlainMonthDayConstructor::create(VM& vm, Structure* structure, TemporalPlainMonthDayPrototype* plainDatePrototype)
{
    auto* constructor = new (NotNull, allocateCell<TemporalPlainMonthDayConstructor>(vm)) TemporalPlainMonthDayConstructor(vm, structure);
    constructor->finishCreation(vm, plainDatePrototype);
    return constructor;
}

Structure* TemporalPlainMonthDayConstructor::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(InternalFunctionType, StructureFlags), info());
}

static JSC_DECLARE_HOST_FUNCTION(callTemporalPlainMonthDay);
static JSC_DECLARE_HOST_FUNCTION(constructTemporalPlainMonthDay);

TemporalPlainMonthDayConstructor::TemporalPlainMonthDayConstructor(VM& vm, Structure* structure)
    : Base(vm, structure, callTemporalPlainMonthDay, constructTemporalPlainMonthDay)
{
}

void TemporalPlainMonthDayConstructor::finishCreation(VM& vm, TemporalPlainMonthDayPrototype* plainMonthDayPrototype)
{
    Base::finishCreation(vm, 2, "PlainMonthDay"_s, PropertyAdditionMode::WithoutStructureTransition);
    putDirectWithoutTransition(vm, vm.propertyNames->prototype, plainMonthDayPrototype, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly);
    plainMonthDayPrototype->putDirectWithoutTransition(vm, vm.propertyNames->constructor, this, static_cast<unsigned>(PropertyAttribute::DontEnum));
}

JSC_DEFINE_HOST_FUNCTION(constructTemporalPlainMonthDay, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* newTarget = asObject(callFrame->newTarget());
    Structure* structure = JSC_GET_DERIVED_STRUCTURE(vm, plainMonthDayStructure, newTarget, callFrame->jsCallee());
    RETURN_IF_EXCEPTION(scope, { });

    double isoMonth = 1;
    double isoDay = 1;
    auto argumentCount = callFrame->argumentCount();

    if (argumentCount > 0) {
        auto value = callFrame->uncheckedArgument(0).toIntegerWithTruncation(globalObject);
        if (!std::isfinite(value))
            return throwVMRangeError(globalObject, scope, "Temporal.PlainMonthDay month property must be finite"_s);
        isoMonth = value;
        RETURN_IF_EXCEPTION(scope, { });
    }

    if (argumentCount > 1) {
        auto value = callFrame->uncheckedArgument(1).toIntegerWithTruncation(globalObject);
        if (!std::isfinite(value))
            return throwVMRangeError(globalObject, scope, "Temporal.PlainMonthDay day property must be finite"_s);
        isoDay = value;
        RETURN_IF_EXCEPTION(scope, { });
    }

    if (argumentCount < 2)
        return throwVMRangeError(globalObject, scope, "Temporal.PlainMonthDay requires at least two arguments"_s);

    // Argument 2 is calendar -- ignored for now. FIXME

    double referenceYear = 1972; // First ISO leap year after the epoch
    if (argumentCount > 3) {
        auto value = callFrame->uncheckedArgument(3).toIntegerWithTruncation(globalObject);
        if (!std::isfinite(value))
            return throwVMRangeError(globalObject, scope, "Temporal.PlainMonthDay reference year must be finite"_s);
        referenceYear = value;
        RETURN_IF_EXCEPTION(scope, { });
    }

    RELEASE_AND_RETURN(scope, JSValue::encode(TemporalPlainMonthDay::tryCreateIfValid(globalObject, structure, ISO8601::PlainDate(referenceYear, isoMonth, isoDay))));
}

JSC_DEFINE_HOST_FUNCTION(callTemporalPlainMonthDay, (JSGlobalObject* globalObject, CallFrame*))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    return JSValue::encode(throwConstructorCannotBeCalledAsFunctionTypeError(globalObject, scope, "PlainMonthDay"_s));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plainmonthday.from
JSC_DEFINE_HOST_FUNCTION(temporalPlainMonthDayConstructorFuncFrom, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue itemValue = callFrame->argument(0);

    if (itemValue.inherits<TemporalPlainMonthDay>()) {
        // Overflow needs to be validated, although it's not used here
        toTemporalOverflow(globalObject, callFrame->argument(1));
        RETURN_IF_EXCEPTION(scope, { });

        RELEASE_AND_RETURN(scope, JSValue::encode(TemporalPlainMonthDay::create(vm, globalObject->plainMonthDayStructure(), jsCast<TemporalPlainMonthDay*>(itemValue)->plainMonthDay())));
    }
    RELEASE_AND_RETURN(scope, JSValue::encode(TemporalPlainMonthDay::from(globalObject, itemValue, callFrame->argument(1))));
}

} // namespace JSC

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
#include "TemporalCalendar.h"
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

// https://tc39.es/proposal-temporal/#sec-temporal.plainmonthday
JSC_DEFINE_HOST_FUNCTION(constructTemporalPlainMonthDay, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Step 1: NewTarget check done by JSC engine (callFrame->newTarget() is never undefined here).
    JSObject* newTarget = asObject(callFrame->newTarget());
    Structure* structure = JSC_GET_DERIVED_STRUCTURE(vm, plainMonthDayStructure, newTarget, callFrame->jsCallee());
    RETURN_IF_EXCEPTION(scope, { });

    // Step 3: m = ? ToIntegerWithTruncation(isoMonth).
    double isoMonth = callFrame->argument(0).toIntegerWithTruncation(globalObject);
    RETURN_IF_EXCEPTION(scope, { });
    if (!std::isfinite(isoMonth)) [[unlikely]]
        return throwVMRangeError(globalObject, scope, "Temporal.PlainMonthDay month property must be finite"_s);

    // Step 4: d = ? ToIntegerWithTruncation(isoDay).
    double isoDay = callFrame->argument(1).toIntegerWithTruncation(globalObject);
    RETURN_IF_EXCEPTION(scope, { });
    if (!std::isfinite(isoDay)) [[unlikely]]
        return throwVMRangeError(globalObject, scope, "Temporal.PlainMonthDay day property must be finite"_s);

    // Steps 5-7: if calendar is undefined use "iso8601"; if not a String throw TypeError; CanonicalizeCalendar.
    CalendarID calId = iso8601CalendarID();
    JSValue calArg = callFrame->argument(2);
    if (!calArg.isUndefined()) {
        if (!calArg.isString()) [[unlikely]]
            return throwVMTypeError(globalObject, scope, "calendar must be a string"_s);
        auto rawCalendarId = asString(calArg)->value(globalObject);
        RETURN_IF_EXCEPTION(scope, { });
        auto canonicalized = isBuiltinCalendar(rawCalendarId);
        if (!canonicalized) [[unlikely]]
            return throwVMRangeError(globalObject, scope, "invalid calendar ID"_s);
        calId = *canonicalized;
    }

    // Step 2/8: y = referenceISOYear default 1972 | ? ToIntegerWithTruncation(referenceISOYear).
    double referenceYear = 1972;
    JSValue refYearArg = callFrame->argument(3);
    if (!refYearArg.isUndefined()) {
        referenceYear = refYearArg.toIntegerWithTruncation(globalObject);
        RETURN_IF_EXCEPTION(scope, { });
        if (!std::isfinite(referenceYear)) [[unlikely]]
            return throwVMRangeError(globalObject, scope, "Temporal.PlainMonthDay reference year must be finite"_s);
    }

    // Step 9: If IsValidISODate(y, m, d) is false, throw RangeError.
    // Step 10: If ISODateWithinLimits(CreateISODateRecord(y, m, d)) is false, throw RangeError.
    if (!ISO8601::isYearWithinLimits(referenceYear)
        || !ISO8601::isValidISODate(referenceYear, isoMonth, isoDay)
        || !ISO8601::isDateTimeWithinLimits(referenceYear, isoMonth, isoDay, 12, 0, 0, 0, 0, 0)) [[unlikely]]
        return throwVMRangeError(globalObject, scope, "PlainMonthDay: date out of range of ECMAScript representation"_s);

    // Step 11: Return ? CreateTemporalMonthDay(isoDate, calendar).
    auto* result = TemporalPlainMonthDay::create(vm, structure, ISO8601::PlainMonthDay(ISO8601::PlainDate(referenceYear, isoMonth, isoDay)));
    if (result && calId != iso8601CalendarID())
        result->setCalendarID(calId);
    return JSValue::encode(result);
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
    // Step 1: Return ? ToTemporalMonthDay(item, options).
    return JSValue::encode(TemporalPlainMonthDay::from(globalObject, callFrame->argument(0), callFrame->argument(1)));
}

} // namespace JSC

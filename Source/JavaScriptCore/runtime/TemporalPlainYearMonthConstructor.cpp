/*
 * Copyright (C) 2022 Apple Inc.
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
#include "TemporalPlainYearMonthConstructor.h"

#include "IntlObjectInlines.h"
#include "JSCInlines.h"
#include "TemporalPlainYearMonth.h"
#include "TemporalPlainYearMonthPrototype.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(TemporalPlainYearMonthConstructor);

static JSC_DECLARE_HOST_FUNCTION(temporalPlainYearMonthConstructorFuncFrom);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainYearMonthConstructorFuncCompare);

}

#include "TemporalPlainYearMonthConstructor.lut.h"

namespace JSC {

const ClassInfo TemporalPlainYearMonthConstructor::s_info = { "Function"_s, &Base::s_info, &temporalPlainYearMonthConstructorTable, nullptr, CREATE_METHOD_TABLE(TemporalPlainYearMonthConstructor) };

/* Source for TemporalPlainYearMonthConstructor.lut.h
@begin temporalPlainYearMonthConstructorTable
  from             temporalPlainYearMonthConstructorFuncFrom             DontEnum|Function 1
  compare          temporalPlainYearMonthConstructorFuncCompare          DontEnum|Function 2
@end
*/

TemporalPlainYearMonthConstructor* TemporalPlainYearMonthConstructor::create(VM& vm, Structure* structure, TemporalPlainYearMonthPrototype* plainDatePrototype)
{
    auto* constructor = new (NotNull, allocateCell<TemporalPlainYearMonthConstructor>(vm)) TemporalPlainYearMonthConstructor(vm, structure);
    constructor->finishCreation(vm, plainDatePrototype);
    return constructor;
}

Structure* TemporalPlainYearMonthConstructor::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(InternalFunctionType, StructureFlags), info());
}

static JSC_DECLARE_HOST_FUNCTION(callTemporalPlainYearMonth);
static JSC_DECLARE_HOST_FUNCTION(constructTemporalPlainYearMonth);

TemporalPlainYearMonthConstructor::TemporalPlainYearMonthConstructor(VM& vm, Structure* structure)
    : Base(vm, structure, callTemporalPlainYearMonth, constructTemporalPlainYearMonth)
{
}

void TemporalPlainYearMonthConstructor::finishCreation(VM& vm, TemporalPlainYearMonthPrototype* plainYearMonthPrototype)
{
    Base::finishCreation(vm, 2, "PlainYearMonth"_s, PropertyAdditionMode::WithoutStructureTransition);
    putDirectWithoutTransition(vm, vm.propertyNames->prototype, plainYearMonthPrototype, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly);
}

// https://tc39.es/proposal-temporal/#sec-temporal.plainyearmonth
JSC_DEFINE_HOST_FUNCTION(constructTemporalPlainYearMonth, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Step 1: If NewTarget is undefined, throw TypeError. (Enforced by callTemporalPlainYearMonth dispatch.)
    JSObject* newTarget = asObject(callFrame->newTarget());
    Structure* structure = JSC_GET_DERIVED_STRUCTURE(vm, plainYearMonthStructure, newTarget, callFrame->jsCallee());
    RETURN_IF_EXCEPTION(scope, { });

    // Step 3: Let y be ? ToIntegerWithTruncation(isoYear).
    //         Spec ToIntegerWithTruncation throws RangeError on non-finite (NaN/±Inf); JSC's helper
    //         doesn't, so enforce finiteness explicitly. Missing isoYear → undefined → throws.
    double isoYear = callFrame->argument(0).toIntegerWithTruncation(globalObject);
    RETURN_IF_EXCEPTION(scope, { });
    if (!std::isfinite(isoYear)) [[unlikely]]
        return throwVMRangeError(globalObject, scope, "Temporal.PlainYearMonth year property must be finite"_s);

    // Step 4: Let m be ? ToIntegerWithTruncation(isoMonth).
    double isoMonth = callFrame->argument(1).toIntegerWithTruncation(globalObject);
    RETURN_IF_EXCEPTION(scope, { });
    if (!std::isfinite(isoMonth)) [[unlikely]]
        return throwVMRangeError(globalObject, scope, "Temporal.PlainYearMonth month property must be finite"_s);

    // Step 5: If calendar is undefined, set calendar to "iso8601".
    // Step 6: If calendar is not a String, throw TypeError.
    // Step 7: Set calendar to ? CanonicalizeCalendar(calendar).
    CalendarID calId = iso8601CalendarID();
    JSValue calendarArg = callFrame->argument(2);
    if (!calendarArg.isUndefined()) {
        if (!calendarArg.isString()) [[unlikely]]
            return throwVMTypeError(globalObject, scope, "calendarId must be a string"_s);
        auto rawCalendarId = asString(calendarArg)->value(globalObject);
        RETURN_IF_EXCEPTION(scope, { });
        auto canonicalized = isBuiltinCalendar(rawCalendarId);
        if (!canonicalized) [[unlikely]]
            return throwVMRangeError(globalObject, scope, "invalid calendar ID"_s);
        calId = *canonicalized;
    }

    // Step 2: If referenceISODay is undefined, set referenceISODay to 1.
    // Step 8: Let ref be ? ToIntegerWithTruncation(referenceISODay).
    double referenceDay = 1;
    JSValue refArg = callFrame->argument(3);
    if (!refArg.isUndefined()) {
        referenceDay = refArg.toIntegerWithTruncation(globalObject);
        RETURN_IF_EXCEPTION(scope, { });
        if (!std::isfinite(referenceDay)) [[unlikely]]
            return throwVMRangeError(globalObject, scope, "Temporal.PlainYearMonth reference day must be finite"_s);
    }

    // Step 9: If IsValidISODate(y, m, ref) is false, throw RangeError.
    if (!ISO8601::isValidISODate(isoYear, isoMonth, referenceDay)) [[unlikely]]
        return throwVMRangeError(globalObject, scope, "Temporal.PlainYearMonth: not a valid ISO date"_s);

    if (!ISO8601::isYearWithinLimits(isoYear)) [[unlikely]]
        return throwVMRangeError(globalObject, scope, "year is out of range"_s);
    if (!isInBounds<int32_t>(isoMonth)) [[unlikely]]
        return throwVMRangeError(globalObject, scope, "month is out of range"_s);
    if (!isInBounds<int32_t>(referenceDay)) [[unlikely]]
        return throwVMRangeError(globalObject, scope, "reference day is out of range"_s);

    // Step 10: Let isoDate be CreateISODateRecord(y, m, ref).
    // Step 11: Return ? CreateTemporalYearMonth(isoDate, calendar, NewTarget).
    auto* result = TemporalPlainYearMonth::tryCreateIfValid(globalObject, structure, ISO8601::PlainDate(static_cast<int32_t>(isoYear), static_cast<unsigned>(isoMonth), static_cast<unsigned>(referenceDay)));
    RETURN_IF_EXCEPTION(scope, { });
    if (result && calId != iso8601CalendarID())
        result->setCalendarID(calId);
    return JSValue::encode(result);
}

JSC_DEFINE_HOST_FUNCTION(callTemporalPlainYearMonth, (JSGlobalObject* globalObject, CallFrame*))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    return JSValue::encode(throwConstructorCannotBeCalledAsFunctionTypeError(globalObject, scope, "PlainYearMonth"_s));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plainyearmonth.from
JSC_DEFINE_HOST_FUNCTION(temporalPlainYearMonthConstructorFuncFrom, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    // Step 1: Return ? ToTemporalYearMonth(item, options).
    return JSValue::encode(TemporalPlainYearMonth::from(globalObject, callFrame->argument(0), callFrame->argument(1)));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plainyearmonth.compare
JSC_DEFINE_HOST_FUNCTION(temporalPlainYearMonthConstructorFuncCompare, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Step 1: Set one to ? ToTemporalYearMonth(one).
    auto* one = TemporalPlainYearMonth::from(globalObject, callFrame->argument(0), jsUndefined());
    RETURN_IF_EXCEPTION(scope, { });

    // Step 2: Set two to ? ToTemporalYearMonth(two).
    auto* two = TemporalPlainYearMonth::from(globalObject, callFrame->argument(1), jsUndefined());
    RETURN_IF_EXCEPTION(scope, { });

    // Step 3: Return 𝔽(CompareISODate(one.[[ISODate]], two.[[ISODate]])).
    return JSValue::encode(jsNumber(TemporalCore::isoDateCompare(one->plainYearMonth().isoPlainDate(), two->plainYearMonth().isoPlainDate())));
}

} // namespace JSC

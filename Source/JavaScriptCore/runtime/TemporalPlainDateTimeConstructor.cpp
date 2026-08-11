/*
 * Copyright (C) 2022 Sony Interactive Entertainment Inc.
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
#include "TemporalPlainDateTimeConstructor.h"

#include "IntlObjectInlines.h"
#include "JSCInlines.h"
#include "PlainDateTimeCore.h"
#include "TemporalCalendar.h"
#include "TemporalPlainDate.h"
#include "TemporalPlainDateTime.h"
#include "TemporalPlainDateTimePrototype.h"
#include "TemporalPlainTime.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(TemporalPlainDateTimeConstructor);

static JSC_DECLARE_HOST_FUNCTION(temporalPlainDateTimeConstructorFuncFrom);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainDateTimeConstructorFuncCompare);

}

#include "TemporalPlainDateTimeConstructor.lut.h"

namespace JSC {

const ClassInfo TemporalPlainDateTimeConstructor::s_info = { "Function"_s, &Base::s_info, &temporalPlainDateTimeConstructorTable, nullptr, CREATE_METHOD_TABLE(TemporalPlainDateTimeConstructor) };

/* Source for TemporalPlainDateTimeConstructor.lut.h
@begin temporalPlainDateTimeConstructorTable
  from             temporalPlainDateTimeConstructorFuncFrom             DontEnum|Function 1
  compare          temporalPlainDateTimeConstructorFuncCompare          DontEnum|Function 2
@end
*/

TemporalPlainDateTimeConstructor* TemporalPlainDateTimeConstructor::create(VM& vm, Structure* structure, TemporalPlainDateTimePrototype* plainDateTimePrototype)
{
    auto* constructor = new (NotNull, allocateCell<TemporalPlainDateTimeConstructor>(vm)) TemporalPlainDateTimeConstructor(vm, structure);
    constructor->finishCreation(vm, plainDateTimePrototype);
    return constructor;
}

Structure* TemporalPlainDateTimeConstructor::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(InternalFunctionType, StructureFlags), info());
}

static JSC_DECLARE_HOST_FUNCTION(callTemporalPlainDateTime);
static JSC_DECLARE_HOST_FUNCTION(constructTemporalPlainDateTime);

TemporalPlainDateTimeConstructor::TemporalPlainDateTimeConstructor(VM& vm, Structure* structure)
    : Base(vm, structure, callTemporalPlainDateTime, constructTemporalPlainDateTime)
{
}

void TemporalPlainDateTimeConstructor::finishCreation(VM& vm, TemporalPlainDateTimePrototype* plainDateTimePrototype)
{
    Base::finishCreation(vm, 3, "PlainDateTime"_s, PropertyAdditionMode::WithoutStructureTransition);
    putDirectWithoutTransition(vm, vm.propertyNames->prototype, plainDateTimePrototype, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly);
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindatetime
JSC_DEFINE_HOST_FUNCTION(constructTemporalPlainDateTime, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Step 1: If NewTarget is undefined, throw a TypeError. (Enforced by JSC engine.)

    // Steps 2-4: Set isoYear/isoMonth/isoDay to ? ToIntegerWithTruncation(arg).
    // Steps 5-10: For each of hour/minute/second/millisecond/microsecond/nanosecond:
    //             if undefined, set to 0; else ? ToIntegerWithTruncation(arg).
    ISO8601::Duration duration { };
    constexpr unsigned dateUnits = numberOfTemporalPlainDateUnits;
    constexpr unsigned totalUnits = dateUnits + numberOfTemporalPlainTimeUnits;
    for (unsigned i = 0; i < totalUnits; i++) {
        unsigned durationIndex = i >= static_cast<unsigned>(TemporalUnit::Week) ? i + 1 : i;
        JSValue arg = callFrame->argument(i);
        if (i >= dateUnits && arg.isUndefined())
            continue;
        double v = arg.toIntegerWithTruncation(globalObject);
        RETURN_IF_EXCEPTION(scope, { });
        if (!std::isfinite(v)) [[unlikely]]
            return throwVMRangeError(globalObject, scope, "Temporal.PlainDateTime properties must be finite"_s);
        duration.setField(durationIndex, v);
    }

    // Steps 11-13: if calendar undefined → "iso8601"; if not String → TypeError; CanonicalizeCalendar.
    CalendarID calId = iso8601CalendarID();
    if (callFrame->argumentCount() > numberOfTemporalPlainDateUnits + numberOfTemporalPlainTimeUnits) {
        JSValue calendarArg = callFrame->uncheckedArgument(numberOfTemporalPlainDateUnits + numberOfTemporalPlainTimeUnits);
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
    }

    // Steps 14-15: IsValidISODate + CreateISODateRecord.
    auto plainDate = TemporalPlainDate::validateAndCreateISODateRecord(globalObject, duration);
    RETURN_IF_EXCEPTION(scope, { });

    // Steps 16-17: IsValidTime + CreateTimeRecord.
    auto plainTime = TemporalPlainTime::validateAndCreateTimeRecord(globalObject, duration);
    RETURN_IF_EXCEPTION(scope, { });

    // Steps 18-19: CombineISODateAndTimeRecord + ? CreateTemporalDateTime(isoDateTime, calendar, NewTarget).
    auto* result = createTemporalDateTime(globalObject, WTF::move(plainDate), WTF::move(plainTime), calId,
        { asObject(callFrame->newTarget()), callFrame->jsCallee() });
    RETURN_IF_EXCEPTION(scope, { });
    return JSValue::encode(result);
}

JSC_DEFINE_HOST_FUNCTION(callTemporalPlainDateTime, (JSGlobalObject* globalObject, CallFrame*))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    return JSValue::encode(throwConstructorCannotBeCalledAsFunctionTypeError(globalObject, scope, "PlainDateTime"_s));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindatetime.from
JSC_DEFINE_HOST_FUNCTION(temporalPlainDateTimeConstructorFuncFrom, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    // Step 1: Return ? ToTemporalDateTime(item, options).
    return JSValue::encode(TemporalPlainDateTime::from(globalObject, callFrame->argument(0), callFrame->argument(1)));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindatetime.compare
JSC_DEFINE_HOST_FUNCTION(temporalPlainDateTimeConstructorFuncCompare, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Step 1: one = ToTemporalDateTime(one).
    auto* one = TemporalPlainDateTime::from(globalObject, callFrame->argument(0), jsUndefined());
    RETURN_IF_EXCEPTION(scope, { });

    // Step 2: two = ToTemporalDateTime(two).
    auto* two = TemporalPlainDateTime::from(globalObject, callFrame->argument(1), jsUndefined());
    RETURN_IF_EXCEPTION(scope, { });

    // Step 3: CompareISODateTime(one.[[ISODateTime]], two.[[ISODateTime]]).
    return JSValue::encode(jsNumber(TemporalCore::compareISODateTime(one->plainDate(), one->plainTime(), two->plainDate(), two->plainTime())));
}

} // namespace JSC

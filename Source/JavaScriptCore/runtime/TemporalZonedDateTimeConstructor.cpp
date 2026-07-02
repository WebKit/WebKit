/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "TemporalZonedDateTimeConstructor.h"

#include "ISO8601.h"
#include "JSCInlines.h"
#include "TemporalCalendar.h"
#include "TemporalInstant.h"
#include "TemporalObject.h"
#include "TemporalZonedDateTime.h"
#include "TemporalZonedDateTimePrototype.h"
#include <wtf/text/MakeString.h>

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(TemporalZonedDateTimeConstructor);

static JSC_DECLARE_HOST_FUNCTION(temporalZonedDateTimeConstructorFuncFrom);
static JSC_DECLARE_HOST_FUNCTION(temporalZonedDateTimeConstructorFuncCompare);

}

#include "TemporalZonedDateTimeConstructor.lut.h"

namespace JSC {

const ClassInfo TemporalZonedDateTimeConstructor::s_info = { "Function"_s, &Base::s_info, &temporalZonedDateTimeConstructorTable, nullptr, CREATE_METHOD_TABLE(TemporalZonedDateTimeConstructor) };

/* Source for TemporalZonedDateTimeConstructor.lut.h
@begin temporalZonedDateTimeConstructorTable
  from     temporalZonedDateTimeConstructorFuncFrom     DontEnum|Function 1
  compare  temporalZonedDateTimeConstructorFuncCompare  DontEnum|Function 2
@end
*/

TemporalZonedDateTimeConstructor* TemporalZonedDateTimeConstructor::create(VM& vm, Structure* structure, TemporalZonedDateTimePrototype* zonedDateTimePrototype)
{
    auto* constructor = new (NotNull, allocateCell<TemporalZonedDateTimeConstructor>(vm)) TemporalZonedDateTimeConstructor(vm, structure);
    constructor->finishCreation(vm, zonedDateTimePrototype);
    return constructor;
}

Structure* TemporalZonedDateTimeConstructor::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(InternalFunctionType, StructureFlags), info());
}

static JSC_DECLARE_HOST_FUNCTION(callTemporalZonedDateTime);
static JSC_DECLARE_HOST_FUNCTION(constructTemporalZonedDateTime);

TemporalZonedDateTimeConstructor::TemporalZonedDateTimeConstructor(VM& vm, Structure* structure)
    : Base(vm, structure, callTemporalZonedDateTime, constructTemporalZonedDateTime)
{
}

void TemporalZonedDateTimeConstructor::finishCreation(VM& vm, TemporalZonedDateTimePrototype* zonedDateTimePrototype)
{
    Base::finishCreation(vm, 2, "ZonedDateTime"_s, PropertyAdditionMode::WithoutStructureTransition);
    putDirectWithoutTransition(vm, vm.propertyNames->prototype, zonedDateTimePrototype, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly);
    zonedDateTimePrototype->putDirectWithoutTransition(vm, vm.propertyNames->constructor, this, static_cast<unsigned>(PropertyAttribute::DontEnum));
}

// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime
JSC_DEFINE_HOST_FUNCTION(constructTemporalZonedDateTime, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Step 1: If NewTarget is *undefined*, throw *TypeError* — handled by callTemporalZonedDateTime.
    JSObject* newTarget = asObject(callFrame->newTarget());
    Structure* structure = JSC_GET_DERIVED_STRUCTURE(vm, zonedDateTimeStructure, newTarget, callFrame->jsCallee());
    RETURN_IF_EXCEPTION(scope, { });

    // Step 2: Set _epochNanoseconds_ to ? ToBigInt(_epochNanoseconds_).
    JSValue bigIntValue = callFrame->argument(0).toBigInt(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    // Step 3: If IsValidEpochNanoseconds(_epochNanoseconds_) is *false*, throw *RangeError*.
    auto exactTimeOpt = bigIntValueToExactTime(globalObject, bigIntValue, "Temporal.ZonedDateTime"_s);
    RETURN_IF_EXCEPTION(scope, { });
    ISO8601::ExactTime exactTime = *exactTimeOpt;

    // Step 4: If _timeZone_ is not a String, throw *TypeError*.
    JSValue tzValue = callFrame->argument(1);
    if (!tzValue.isString()) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime timeZoneIdentifier must be a string"_s);
    auto tzString = asString(tzValue)->value(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    // Step 5: Let _timeZoneParse_ be ? ParseTimeZoneIdentifier(_timeZone_).
    // ParseTimeZoneIdentifier accepts |UTCOffset[~SubMinutePrecision]| or |TimeZoneIANAName|;
    // datetime strings and "Z" are rejected (those use ParseTemporalTimeZoneString, not this AO).
    auto parsedTZ = ISO8601::parseTimeZoneIdentifierStrict(tzString);
    if (!parsedTZ) [[unlikely]]
        return throwVMRangeError(globalObject, scope, makeString("'"_s, ellipsizeAt(100, tzString), "' is not a valid time zone identifier"_s));

    // Steps 6-7: [[TimeZone]] = the parsed identifier in canonical case-normalized form
    // (named zones) or formatted offset (UTC offsets). parseTimeZoneIdentifierStrict already
    // produced both, with intlResolveTimeZoneID validating the named case (step 6b).
    // Step 8: If _calendar_ is *undefined*, set _calendar_ to *"iso8601"*.
    // Step 9: If _calendar_ is not a String, throw *TypeError*.
    // Step 10: Set _calendar_ to ? CanonicalizeCalendar(_calendar_).
    CalendarID calendarID = iso8601CalendarID();
    JSValue calendarArg = callFrame->argument(2);
    if (!calendarArg.isUndefined()) {
        if (!calendarArg.isString()) [[unlikely]]
            return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime calendar must be a string"_s);
        auto calStr = asString(calendarArg)->value(globalObject);
        RETURN_IF_EXCEPTION(scope, { });
        auto calIdx = isBuiltinCalendar(calStr); // CanonicalizeCalendar: case-insensitive lookup + legacy aliases
        if (!calIdx) [[unlikely]]
            return throwVMRangeError(globalObject, scope, makeString("'"_s, ellipsizeAt(100, calStr), "' is not a valid calendar identifier"_s));
        calendarID = *calIdx;
    }

    // Step 11: Return ? CreateTemporalZonedDateTime(_epochNanoseconds_, _timeZone_, _calendar_, NewTarget).
    // tryCreate re-validates isValid() as a safety net (primarily for subclasses).
    RELEASE_AND_RETURN(scope, JSValue::encode(TemporalZonedDateTime::tryCreate(globalObject, structure, exactTime, *parsedTZ, calendarID)));
}

// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime
// Step 1: If NewTarget is undefined, throw TypeError.
JSC_DEFINE_HOST_FUNCTION(callTemporalZonedDateTime, (JSGlobalObject* globalObject, CallFrame*))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    return JSValue::encode(throwConstructorCannotBeCalledAsFunctionTypeError(globalObject, scope, "ZonedDateTime"_s));
}

// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.from
JSC_DEFINE_HOST_FUNCTION(temporalZonedDateTimeConstructorFuncFrom, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Step 1: Return ? ToTemporalZonedDateTime(item, options).
    auto* zdt = TemporalZonedDateTime::from(globalObject, callFrame->argument(0), callFrame->argument(1));
    RETURN_IF_EXCEPTION(scope, { });
    ASSERT(zdt);
    return JSValue::encode(zdt);
}

// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.compare
JSC_DEFINE_HOST_FUNCTION(temporalZonedDateTimeConstructorFuncCompare, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Step 1: Set one to ? ToTemporalZonedDateTime(one).
    auto* one = TemporalZonedDateTime::from(globalObject, callFrame->argument(0));
    RETURN_IF_EXCEPTION(scope, { });
    ASSERT(one);
    // Step 2: Set two to ? ToTemporalZonedDateTime(two).
    auto* two = TemporalZonedDateTime::from(globalObject, callFrame->argument(1));
    RETURN_IF_EXCEPTION(scope, { });
    ASSERT(two);

    // Step 3: Return 𝔽(CompareEpochNanoseconds(one.[[EpochNanoseconds]], two.[[EpochNanoseconds]])).
    if (one->exactTime() < two->exactTime())
        return JSValue::encode(jsNumber(-1));
    if (one->exactTime() > two->exactTime())
        return JSValue::encode(jsNumber(1));
    return JSValue::encode(jsNumber(0));
}

} // namespace JSC

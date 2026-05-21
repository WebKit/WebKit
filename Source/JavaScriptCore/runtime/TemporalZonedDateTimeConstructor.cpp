/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "JSBigInt.h"
#include "JSCInlines.h"
#include "TemporalCalendar.h"
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

    JSObject* newTarget = asObject(callFrame->newTarget());
    Structure* structure = JSC_GET_DERIVED_STRUCTURE(vm, zonedDateTimeStructure, newTarget, callFrame->jsCallee());
    RETURN_IF_EXCEPTION(scope, { });

    if (callFrame->argumentCount() < 2)
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime requires at least two arguments: epochNanoseconds and timeZoneIdentifier"_s);

    // 1. epochNanoseconds — must be a BigInt; convert to ISO8601::ExactTime.
    JSValue epochNsValue = callFrame->uncheckedArgument(0).toBigInt(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    ISO8601::ExactTime exactTime;
#if USE(BIGINT32)
    if (epochNsValue.isBigInt32())
        exactTime = ISO8601::ExactTime { Int128 { epochNsValue.bigInt32AsInt32() } };
    else
#endif
    {
        JSBigInt* bigint = asHeapBigInt(epochNsValue);
        bool bigIntTooLong = false;
        Int128 total;
        if constexpr (sizeof(JSBigInt::Digit) == 4) {
            Int128 d0 { bigint->length() > 0 ? bigint->digit(0) : 0 };
            Int128 d1 { bigint->length() > 1 ? bigint->digit(1) : 0 };
            Int128 d2 { bigint->length() > 2 ? bigint->digit(2) : 0 };
            total = d2 << 64 | d1 << 32 | d0;
            bigIntTooLong = bigint->length() > 3;
        } else {
            ASSERT(sizeof(JSBigInt::Digit) == 8);
            if (bigint->length() > 1 && (bigint->digit(1) & 0x8000'0000'0000'0000)) {
                total = 0;
                bigIntTooLong = true;
            } else {
                Int128 low { bigint->length() > 0 ? bigint->digit(0) : 0 };
                Int128 high { bigint->length() > 1 ? bigint->digit(1) : 0 };
                total = high << 64 | low;
                bigIntTooLong = bigint->length() > 2;
            }
        }
        exactTime = ISO8601::ExactTime { total * (bigint->sign() ? -1 : 1) };

        if (bigIntTooLong || !exactTime.isValid()) {
            String argAsString = bigint->toString(globalObject, 10);
            if (scope.exception()) {
                TRY_CLEAR_EXCEPTION(scope, { });
                argAsString = "The given number of"_s;
            }
            throwRangeError(globalObject, scope, makeString(ellipsizeAt(100, argAsString), " epoch nanoseconds is outside of the supported range for Temporal.ZonedDateTime"_s));
            return { };
        }
    }

    // 2. timeZoneIdentifier — must be a string; parse as IANA ID or UTC offset.
    JSValue tzValue = callFrame->uncheckedArgument(1);
    if (!tzValue.isString())
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime timeZoneIdentifier must be a string"_s);
    auto tzString = asString(tzValue)->value(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    auto parsedTZ = ISO8601::parseTimeZoneIdentifierStrict(tzString);
    if (!parsedTZ)
        return throwVMRangeError(globalObject, scope, makeString("'"_s, ellipsizeAt(100, tzString), "' is not a valid time zone identifier"_s));

    // 3. calendarIdentifier — optional; default "iso8601".
    // Per spec CanonicalizeCalendar: only plain calendar IDs are valid.
    // ISO date strings like "2020-01-01[u-ca=iso8601]" must be rejected.
    String calendarId = "iso8601"_s;
    if (callFrame->argumentCount() > 2) {
        JSValue calendarArg = callFrame->uncheckedArgument(2);
        if (!calendarArg.isUndefined()) {
            if (!calendarArg.isString())
                return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime calendar must be a string"_s);
            auto calStr = asString(calendarArg)->value(globalObject);
            RETURN_IF_EXCEPTION(scope, { });
            auto calIdx = isBuiltinCalendar(calStr);
            if (!calIdx)
                return throwVMRangeError(globalObject, scope, makeString("'"_s, ellipsizeAt(100, calStr), "' is not a valid calendar identifier"_s));
            calendarId = intlAvailableCalendars()[*calIdx];
        }
    }

    String timeZoneId;
    if (auto namedTz = intlAvailableNamedTimeZone(tzString))
        timeZoneId = namedTz->identifier;
    else
        timeZoneId = tzString; // Preserve original offset form (e.g., "+00:00"), per temporal_rs identifier()
    RELEASE_AND_RETURN(scope, JSValue::encode(TemporalZonedDateTime::tryCreate(globalObject, structure, exactTime, *parsedTZ, WTF::move(timeZoneId), WTF::move(calendarId))));
}

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

    auto* one = TemporalZonedDateTime::from(globalObject, callFrame->argument(0));
    RETURN_IF_EXCEPTION(scope, { });
    ASSERT(one);
    auto* two = TemporalZonedDateTime::from(globalObject, callFrame->argument(1));
    RETURN_IF_EXCEPTION(scope, { });
    ASSERT(two);

    if (one->exactTime() < two->exactTime())
        return JSValue::encode(jsNumber(-1));
    if (one->exactTime() > two->exactTime())
        return JSValue::encode(jsNumber(1));
    return JSValue::encode(jsNumber(0));
}

} // namespace JSC

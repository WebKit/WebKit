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
#include "TemporalPlainDateTime.h"
#include "TemporalPlainDateTimePrototype.h"

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
    plainDateTimePrototype->putDirectWithoutTransition(vm, vm.propertyNames->constructor, this, static_cast<unsigned>(PropertyAttribute::DontEnum));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindatetime
JSC_DEFINE_HOST_FUNCTION(constructTemporalPlainDateTime, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Step 1: NewTarget check done by JSC engine.
    JSObject* newTarget = asObject(callFrame->newTarget());
    Structure* structure = JSC_GET_DERIVED_STRUCTURE(vm, plainDateTimeStructure, newTarget, callFrame->jsCallee());
    RETURN_IF_EXCEPTION(scope, { });

    // Steps 2-10: ToIntegerWithTruncation for each positional arg.
    // For date args (year/month/day, i < 3): NaN and ±Infinity throw RangeError.
    // For time args (hour..nanosecond, i ≥ 3): undefined (NaN) defaults to 0; ±Infinity throws.
    ISO8601::Duration duration { };
    auto count = std::min<size_t>(callFrame->argumentCount(), numberOfTemporalPlainDateUnits + numberOfTemporalPlainTimeUnits);
    for (unsigned i = 0; i < count; i++) {
        unsigned durationIndex = i >= static_cast<unsigned>(TemporalUnit::Week) ? i + 1 : i;
        double v = callFrame->uncheckedArgument(i).toNumber(globalObject);
        RETURN_IF_EXCEPTION(scope, { });
        if (std::isnan(v)) {
            if (i < static_cast<unsigned>(numberOfTemporalPlainDateUnits)) [[unlikely]]
                return throwVMRangeError(globalObject, scope, "Temporal.PlainDateTime year/month/day must not be NaN"_s);
            v = 0;
        }
        if (!std::isfinite(v)) [[unlikely]]
            return throwVMRangeError(globalObject, scope, "Temporal.PlainDateTime properties must be finite"_s);
        v = std::trunc(v);
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

    // Steps 14-19: IsValidISODate + IsValidTime + CreateTemporalDateTime.
    auto* result = TemporalPlainDateTime::tryCreateIfValid(globalObject, structure, WTF::move(duration));
    RETURN_IF_EXCEPTION(scope, { });
    if (result && calId != iso8601CalendarID())
        result->setCalendarID(calId);
    return JSValue::encode(result);
}

JSC_DEFINE_HOST_FUNCTION(callTemporalPlainDateTime, (JSGlobalObject* globalObject, CallFrame*))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    return JSValue::encode(throwConstructorCannotBeCalledAsFunctionTypeError(globalObject, scope, "PlainDateTime"_s));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindatetime.from
// Step 1: Return ToTemporalDateTime(item, options).
JSC_DEFINE_HOST_FUNCTION(temporalPlainDateTimeConstructorFuncFrom, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue itemValue = callFrame->argument(0);
    JSValue optionsValue = callFrame->argument(1);

    if (itemValue.inherits<TemporalPlainDateTime>()) {
        // ToTemporalDateTime step 2.a: GetOptionsObject + GetTemporalOverflowOption, return new instance.
        toTemporalOverflow(globalObject, optionsValue);
        RETURN_IF_EXCEPTION(scope, { });
        auto* src = uncheckedDowncast<TemporalPlainDateTime>(itemValue);
        auto* cloned = TemporalPlainDateTime::create(vm, globalObject->plainDateTimeStructure(), src->plainDate(), src->plainTime());
        if (src->calendarID() != iso8601CalendarID())
            cloned->setCalendarID(src->calendarID());
        RELEASE_AND_RETURN(scope, JSValue::encode(cloned));
    }

    // ToTemporalDateTime remaining steps: property bag or string path,
    // fields read before options for bags, string parsed first for strings.
    RELEASE_AND_RETURN(scope, JSValue::encode(TemporalPlainDateTime::from(globalObject, itemValue, optionsValue)));
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

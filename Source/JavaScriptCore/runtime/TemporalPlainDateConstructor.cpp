/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "TemporalPlainDateConstructor.h"

#include "IntlObjectInlines.h"
#include "JSCInlines.h"
#include "TemporalCalendar.h"
#include "TemporalPlainDate.h"
#include "TemporalPlainDatePrototype.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(TemporalPlainDateConstructor);

static JSC_DECLARE_HOST_FUNCTION(temporalPlainDateConstructorFuncFrom);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainDateConstructorFuncCompare);

}

#include "TemporalPlainDateConstructor.lut.h"

namespace JSC {

const ClassInfo TemporalPlainDateConstructor::s_info = { "Function"_s, &Base::s_info, &temporalPlainDateConstructorTable, nullptr, CREATE_METHOD_TABLE(TemporalPlainDateConstructor) };

/* Source for TemporalPlainDateConstructor.lut.h
@begin temporalPlainDateConstructorTable
  from             temporalPlainDateConstructorFuncFrom             DontEnum|Function 1
  compare          temporalPlainDateConstructorFuncCompare          DontEnum|Function 2
@end
*/

TemporalPlainDateConstructor* TemporalPlainDateConstructor::create(VM& vm, Structure* structure, TemporalPlainDatePrototype* plainDatePrototype)
{
    auto* constructor = new (NotNull, allocateCell<TemporalPlainDateConstructor>(vm)) TemporalPlainDateConstructor(vm, structure);
    constructor->finishCreation(vm, plainDatePrototype);
    return constructor;
}

Structure* TemporalPlainDateConstructor::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(InternalFunctionType, StructureFlags), info());
}

static JSC_DECLARE_HOST_FUNCTION(callTemporalPlainDate);
static JSC_DECLARE_HOST_FUNCTION(constructTemporalPlainDate);

TemporalPlainDateConstructor::TemporalPlainDateConstructor(VM& vm, Structure* structure)
    : Base(vm, structure, callTemporalPlainDate, constructTemporalPlainDate)
{
}

void TemporalPlainDateConstructor::finishCreation(VM& vm, TemporalPlainDatePrototype* plainDatePrototype)
{
    Base::finishCreation(vm, 3, "PlainDate"_s, PropertyAdditionMode::WithoutStructureTransition);
    putDirectWithoutTransition(vm, vm.propertyNames->prototype, plainDatePrototype, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly);
    plainDatePrototype->putDirectWithoutTransition(vm, vm.propertyNames->constructor, this, static_cast<unsigned>(PropertyAttribute::DontEnum));
}

JSC_DEFINE_HOST_FUNCTION(constructTemporalPlainDate, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* newTarget = asObject(callFrame->newTarget());
    Structure* structure = JSC_GET_DERIVED_STRUCTURE(vm, plainDateStructure, newTarget, callFrame->jsCallee());
    RETURN_IF_EXCEPTION(scope, { });

    ISO8601::Duration duration { };
    auto argumentCount = callFrame->argumentCount();

    if (argumentCount > 0) {
        auto value = callFrame->uncheckedArgument(0).toIntegerWithTruncation(globalObject);
        if (!std::isfinite(value))
            return throwVMRangeError(globalObject, scope, "Temporal.PlainDate year property must be finite"_s);
        duration.setField(TemporalUnit::Year, value);
        RETURN_IF_EXCEPTION(scope, { });
    }

    if (argumentCount > 1) {
        auto value = callFrame->uncheckedArgument(1).toIntegerWithTruncation(globalObject);
        if (!std::isfinite(value))
            return throwVMRangeError(globalObject, scope, "Temporal.PlainDate month property must be finite"_s);
        duration.setField(TemporalUnit::Month, value);
        RETURN_IF_EXCEPTION(scope, { });
    }

    if (argumentCount > 2) {
        auto value = callFrame->uncheckedArgument(2).toIntegerWithTruncation(globalObject);
        if (!std::isfinite(value))
            return throwVMRangeError(globalObject, scope, "Temporal.PlainDate day property must be finite"_s);
        duration.setField(TemporalUnit::Day, value);
        RETURN_IF_EXCEPTION(scope, { });
    }

    // Stage 4: optional calendarId argument — must be a String or undefined.
    // CanonicalizeCalendar: lowercase the identifier.
    String calendarId = "iso8601"_s;
    if (argumentCount > 3) {
        JSValue calendarArg = callFrame->uncheckedArgument(3);
        if (!calendarArg.isUndefined()) {
            if (!calendarArg.isString())
                return throwVMTypeError(globalObject, scope, "calendarId must be a string"_s);
            auto rawCalendarId = asString(calendarArg)->value(globalObject);
            RETURN_IF_EXCEPTION(scope, { });
            auto canonicalized = isBuiltinCalendar(rawCalendarId);
            if (!canonicalized)
                return throwVMRangeError(globalObject, scope, "invalid calendar ID"_s);
            calendarId = intlAvailableCalendars().at(*canonicalized);
        }
    }

    auto plainDate = TemporalPlainDate::toPlainDate(globalObject, duration);
    RETURN_IF_EXCEPTION(scope, { });
    RELEASE_AND_RETURN(scope, JSValue::encode(TemporalPlainDate::tryCreateIfValid(globalObject, structure, WTF::move(plainDate), WTF::move(calendarId))));
}

JSC_DEFINE_HOST_FUNCTION(callTemporalPlainDate, (JSGlobalObject* globalObject, CallFrame*))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    return JSValue::encode(throwConstructorCannotBeCalledAsFunctionTypeError(globalObject, scope, "PlainDate"_s));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindate.from
JSC_DEFINE_HOST_FUNCTION(temporalPlainDateConstructorFuncFrom, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue itemValue = callFrame->argument(0);
    JSValue optionsValue = callFrame->argument(1);

    if (itemValue.inherits<TemporalPlainDate>()) {
        // Clone path: validate options first per spec (GetOptionsObject before clone).
        auto overflow = toTemporalOverflow(globalObject, optionsValue);
        RETURN_IF_EXCEPTION(scope, { });
        UNUSED_PARAM(overflow);
        auto* src = uncheckedDowncast<TemporalPlainDate>(itemValue);
        RELEASE_AND_RETURN(scope, JSValue::encode(TemporalPlainDate::create(vm, globalObject->plainDateStructure(), src->plainDate(), String(src->calendarId()))));
    }

    // For property bags and strings: pass raw options so from() can defer type checking.
    RELEASE_AND_RETURN(scope, JSValue::encode(TemporalPlainDate::from(globalObject, itemValue, optionsValue)));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindate.compare
JSC_DEFINE_HOST_FUNCTION(temporalPlainDateConstructorFuncCompare, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* one = TemporalPlainDate::from(globalObject, callFrame->argument(0), TemporalOverflow::Constrain);
    RETURN_IF_EXCEPTION(scope, { });

    auto* two = TemporalPlainDate::from(globalObject, callFrame->argument(1), TemporalOverflow::Constrain);
    RETURN_IF_EXCEPTION(scope, { });

    return JSValue::encode(jsNumber(TemporalCore::isoDateCompare(one->plainDate(), two->plainDate())));
}

} // namespace JSC

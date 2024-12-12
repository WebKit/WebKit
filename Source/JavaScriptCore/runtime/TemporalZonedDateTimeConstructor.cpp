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
#include "TemporalZonedDateTimeConstructor.h"

#include "IntlObjectInlines.h"
#include "JSCInlines.h"
#include "TemporalInstant.h"
#include "TemporalZonedDateTime.h"
#include "TemporalZonedDateTimePrototype.h"

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
  from             temporalZonedDateTimeConstructorFuncFrom             DontEnum|Function 1
  compare          temporalZonedDateTimeConstructorFuncCompare          DontEnum|Function 2
@end
*/

TemporalZonedDateTimeConstructor* TemporalZonedDateTimeConstructor::create(VM& vm, Structure* structure, TemporalZonedDateTimePrototype* plainDateTimePrototype)
{
    auto* constructor = new (NotNull, allocateCell<TemporalZonedDateTimeConstructor>(vm)) TemporalZonedDateTimeConstructor(vm, structure);
    constructor->finishCreation(vm, plainDateTimePrototype);
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

JSC_DEFINE_HOST_FUNCTION(constructTemporalZonedDateTime, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* newTarget = asObject(callFrame->newTarget());
    Structure* structure = JSC_GET_DERIVED_STRUCTURE(vm, zonedDateTimeStructure, newTarget, callFrame->jsCallee());
    RETURN_IF_EXCEPTION(scope, { });

    if (callFrame->argumentCount() < 2)
        return throwVMTypeError(globalObject, scope, "Temporal.ZonedDateTime requires epochNanoseconds and timeZone arguments"_s);

    ISO8601::ExactTime exactTime = TemporalInstant::exactTimeFromJSValue(
        globalObject, callFrame->uncheckedArgument(0));
    RETURN_IF_EXCEPTION(scope, { });

    auto timeZoneVal = callFrame->argument(1);
    if (!timeZoneVal.isString())
        return throwVMTypeError(globalObject, scope, "Second argument to Temporal.ZonedDateTime constructor must be a string"_s);

    String timeZoneString = timeZoneVal.toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    ISO8601::TimeZone timeZone;
    // TODO: non-UTC named time zones
    if (timeZoneString.convertToASCIIUppercase() == "UTC"_s)
        timeZone = utcTimeZoneID();
    else {
        std::optional<ISO8601::TimeZoneRecord> timeZoneParse = ISO8601::parseTimeZone(timeZoneString);
        if (!timeZoneParse)
            return throwVMRangeError(globalObject, scope, "Couldn't parse time zone name"_s);
        if (!timeZoneParse->m_offset_string) {
            if (timeZoneParse->m_annotation) {
                auto name = timeZoneParse->m_annotation.value();
                auto identifierRecord = TemporalZonedDateTime::getAvailableNamedTimeZoneIdentifier(globalObject, name);
                RETURN_IF_EXCEPTION(scope, { });
                if (!identifierRecord)
                    return throwVMRangeError(globalObject, scope, "Unknown time zone name"_s);
                timeZone = identifierRecord.value();
            } else {
                return throwVMRangeError(globalObject, scope, "Invalid result parsing time zone "_s);
            }
        } else
            timeZone = std::get<1>(timeZoneParse->m_offset_string.value());
    }
    RELEASE_AND_RETURN(scope, JSValue::encode(TemporalZonedDateTime::tryCreateIfValid(globalObject, structure, WTFMove(exactTime), WTFMove(timeZone))));
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

    JSObject* options = intlGetOptionsObject(globalObject, callFrame->argument(1));
    RETURN_IF_EXCEPTION(scope, { });

    JSValue itemValue = callFrame->argument(0);

    if (itemValue.inherits<TemporalZonedDateTime>()) {
        // Validate overflow
        toTemporalOverflow(globalObject, options);
        RETURN_IF_EXCEPTION(scope, { });

        RELEASE_AND_RETURN(scope, JSValue::encode(TemporalZonedDateTime::create(vm, globalObject->zonedDateTimeStructure(), jsCast<TemporalZonedDateTime*>(itemValue)->exactTime(), jsCast<TemporalZonedDateTime*>(itemValue)->timeZone())));
    }

    RELEASE_AND_RETURN(scope, JSValue::encode(TemporalZonedDateTime::from(globalObject, itemValue, options)));
}

// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.compare
JSC_DEFINE_HOST_FUNCTION(temporalZonedDateTimeConstructorFuncCompare, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* one = TemporalZonedDateTime::from(globalObject, callFrame->argument(0), std::nullopt);
    RETURN_IF_EXCEPTION(scope, { });

    auto* two = TemporalZonedDateTime::from(globalObject, callFrame->argument(1), std::nullopt);
    RETURN_IF_EXCEPTION(scope, { });

    return JSValue::encode(jsNumber(TemporalZonedDateTime::compare(one, two)));
}

} // namespace JSC

/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "TemporalPlainTimeConstructor.h"

#include "IntlObjectInlines.h"
#include "JSCInlines.h"
#include "TemporalPlainTime.h"
#include "TemporalPlainTimePrototype.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(TemporalPlainTimeConstructor);

static JSC_DECLARE_HOST_FUNCTION(temporalPlainTimeConstructorFuncFrom);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainTimeConstructorFuncCompare);

}

#include "TemporalPlainTimeConstructor.lut.h"

namespace JSC {

const ClassInfo TemporalPlainTimeConstructor::s_info = { "Function"_s, &Base::s_info, &temporalPlainTimeConstructorTable, nullptr, CREATE_METHOD_TABLE(TemporalPlainTimeConstructor) };

/* Source for TemporalPlainTimeConstructor.lut.h
@begin temporalPlainTimeConstructorTable
  from             temporalPlainTimeConstructorFuncFrom             DontEnum|Function 1
  compare          temporalPlainTimeConstructorFuncCompare          DontEnum|Function 2
@end
*/

TemporalPlainTimeConstructor* TemporalPlainTimeConstructor::create(VM& vm, Structure* structure, TemporalPlainTimePrototype* plainTimePrototype)
{
    auto* constructor = new (NotNull, allocateCell<TemporalPlainTimeConstructor>(vm)) TemporalPlainTimeConstructor(vm, structure);
    constructor->finishCreation(vm, plainTimePrototype);
    return constructor;
}

Structure* TemporalPlainTimeConstructor::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(InternalFunctionType, StructureFlags), info());
}

static JSC_DECLARE_HOST_FUNCTION(callTemporalPlainTime);
static JSC_DECLARE_HOST_FUNCTION(constructTemporalPlainTime);

TemporalPlainTimeConstructor::TemporalPlainTimeConstructor(VM& vm, Structure* structure)
    : Base(vm, structure, callTemporalPlainTime, constructTemporalPlainTime)
{
}

void TemporalPlainTimeConstructor::finishCreation(VM& vm, TemporalPlainTimePrototype* plainTimePrototype)
{
    Base::finishCreation(vm, 0, "PlainTime"_s, PropertyAdditionMode::WithoutStructureTransition);
    putDirectWithoutTransition(vm, vm.propertyNames->prototype, plainTimePrototype, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly);
    plainTimePrototype->putDirectWithoutTransition(vm, vm.propertyNames->constructor, this, static_cast<unsigned>(PropertyAttribute::DontEnum));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaintime
// Temporal.PlainTime ( [ hour [ , minute [ , second [ , millisecond [ , microsecond [ , nanosecond ] ] ] ] ] ] )
JSC_DEFINE_HOST_FUNCTION(constructTemporalPlainTime, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Step 1: NewTarget undefined -> TypeError (handled by JSC dispatch).
    JSObject* newTarget = asObject(callFrame->newTarget());
    Structure* structure = JSC_GET_DERIVED_STRUCTURE(vm, plainTimeStructure, newTarget, callFrame->jsCallee());
    RETURN_IF_EXCEPTION(scope, { });

    // Steps 2-7: If each arg is undefined set to 0; else ?ToIntegerWithTruncation(arg).
    ISO8601::Duration duration { };
    auto count = std::min<size_t>(callFrame->argumentCount(), numberOfTemporalPlainTimeUnits);
    for (unsigned i = 0; i < count; i++) {
        unsigned durationIndex = i + static_cast<unsigned>(TemporalUnit::Hour);
        JSValue arg = callFrame->uncheckedArgument(i);
        if (arg.isUndefined()) {
            duration.setField(durationIndex, 0);
            continue;
        }
        double v = arg.toNumber(globalObject);
        RETURN_IF_EXCEPTION(scope, { });
        if (std::isnan(v)) [[unlikely]]
            return throwVMRangeError(globalObject, scope, "Temporal.PlainTime argument must not be NaN"_s);
        if (!std::isfinite(v)) [[unlikely]]
            return throwVMRangeError(globalObject, scope, "Temporal.PlainTime properties must be finite"_s);
        duration.setField(durationIndex, std::trunc(v));
    }
    // Steps 8-10: IsValidTime -> CreateTimeRecord -> CreateTemporalTime.
    RELEASE_AND_RETURN(scope, JSValue::encode(TemporalPlainTime::tryCreateIfValid(globalObject, structure, WTF::move(duration))));
}

JSC_DEFINE_HOST_FUNCTION(callTemporalPlainTime, (JSGlobalObject* globalObject, CallFrame*))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    return JSValue::encode(throwConstructorCannotBeCalledAsFunctionTypeError(globalObject, scope, "PlainTime"_s));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaintime.from
JSC_DEFINE_HOST_FUNCTION(temporalPlainTimeConstructorFuncFrom, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Step 1: Return ? ToTemporalTime(item, options).
    JSValue itemValue = callFrame->argument(0);
    JSValue optionsValue = callFrame->argument(1);
    RELEASE_AND_RETURN(scope, JSValue::encode(TemporalPlainTime::from(globalObject, itemValue, optionsValue)));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaintime.compare
JSC_DEFINE_HOST_FUNCTION(temporalPlainTimeConstructorFuncCompare, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Step 1: Set one to ? ToTemporalTime(one).
    auto* one = TemporalPlainTime::from(globalObject, callFrame->argument(0), jsUndefined());
    RETURN_IF_EXCEPTION(scope, { });

    // Step 2: Set two to ? ToTemporalTime(two).
    auto* two = TemporalPlainTime::from(globalObject, callFrame->argument(1), jsUndefined());
    RETURN_IF_EXCEPTION(scope, { });

    // Step 3: Return CompareTimeRecord(one.[[Time]], two.[[Time]]).
    return JSValue::encode(jsNumber(TemporalPlainTime::compare(one->plainTime(), two->plainTime())));
}

} // namespace JSC

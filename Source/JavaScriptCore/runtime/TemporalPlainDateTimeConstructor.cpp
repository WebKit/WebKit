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

JSC_DEFINE_HOST_FUNCTION(constructTemporalPlainDateTime, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* newTarget = asObject(callFrame->newTarget());
    Structure* structure = JSC_GET_DERIVED_STRUCTURE(vm, plainDateTimeStructure, newTarget, callFrame->jsCallee());
    RETURN_IF_EXCEPTION(scope, { });

    ISO8601::Duration duration { };
    auto count = std::min<size_t>(callFrame->argumentCount(), numberOfTemporalPlainDateUnits + numberOfTemporalPlainTimeUnits);
    for (unsigned i = 0; i < count; i++) {
        unsigned durationIndex = i >= static_cast<unsigned>(TemporalUnit::Week) ? i + 1 : i;
        duration[durationIndex] = callFrame->uncheckedArgument(i).toIntegerOrInfinity(globalObject);
        RETURN_IF_EXCEPTION(scope, { });
        if (!std::isfinite(duration[durationIndex]))
            return throwVMRangeError(globalObject, scope, "Temporal.PlainDateTime properties must be finite"_s);
    }

    RELEASE_AND_RETURN(scope, JSValue::encode(TemporalPlainDateTime::tryCreateIfValid(globalObject, structure, WTFMove(duration))));
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
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* options = intlGetOptionsObject(globalObject, callFrame->argument(1));
    RETURN_IF_EXCEPTION(scope, { });

    JSValue itemValue = callFrame->argument(0);

    if (itemValue.inherits<TemporalPlainDateTime>()) {
        // Validate overflow
        toTemporalOverflow(globalObject, options);
        RETURN_IF_EXCEPTION(scope, { });

        RELEASE_AND_RETURN(scope, JSValue::encode(TemporalPlainDateTime::create(vm, globalObject->plainDateTimeStructure(), jsCast<TemporalPlainDateTime*>(itemValue)->plainDate(), jsCast<TemporalPlainDateTime*>(itemValue)->plainTime())));
    }

    RELEASE_AND_RETURN(scope, JSValue::encode(TemporalPlainDateTime::from(globalObject, itemValue, options)));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindatetime.compare
JSC_DEFINE_HOST_FUNCTION(temporalPlainDateTimeConstructorFuncCompare, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* one = TemporalPlainDateTime::from(globalObject, callFrame->argument(0), std::nullopt);
    RETURN_IF_EXCEPTION(scope, { });

    auto* two = TemporalPlainDateTime::from(globalObject, callFrame->argument(1), std::nullopt);
    RETURN_IF_EXCEPTION(scope, { });

    return JSValue::encode(jsNumber(TemporalPlainDateTime::compare(one, two)));
}

} // namespace JSC

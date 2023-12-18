/*
 * Copyright (C) 2021 Sony Interactive Entertainment Inc.
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
#include "TemporalDurationConstructor.h"

#include "JSCInlines.h"
#include "TemporalDuration.h"
#include "TemporalDurationPrototype.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(TemporalDurationConstructor);

static JSC_DECLARE_HOST_FUNCTION(temporalDurationConstructorFuncFrom);
static JSC_DECLARE_HOST_FUNCTION(temporalDurationConstructorFuncCompare);

}

#include "TemporalDurationConstructor.lut.h"

namespace JSC {

const ClassInfo TemporalDurationConstructor::s_info = { "Function"_s, &Base::s_info, &temporalDurationConstructorTable, nullptr, CREATE_METHOD_TABLE(TemporalDurationConstructor) };

/* Source for TemporalDurationConstructor.lut.h
@begin temporalDurationConstructorTable
  from             temporalDurationConstructorFuncFrom             DontEnum|Function 1
  compare          temporalDurationConstructorFuncCompare          DontEnum|Function 2
@end
*/

TemporalDurationConstructor* TemporalDurationConstructor::create(VM& vm, Structure* structure, TemporalDurationPrototype* durationPrototype)
{
    auto* constructor = new (NotNull, allocateCell<TemporalDurationConstructor>(vm)) TemporalDurationConstructor(vm, structure);
    constructor->finishCreation(vm, durationPrototype);
    return constructor;
}

Structure* TemporalDurationConstructor::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(InternalFunctionType, StructureFlags), info());
}

static JSC_DECLARE_HOST_FUNCTION(callTemporalDuration);
static JSC_DECLARE_HOST_FUNCTION(constructTemporalDuration);

TemporalDurationConstructor::TemporalDurationConstructor(VM& vm, Structure* structure)
    : Base(vm, structure, callTemporalDuration, constructTemporalDuration)
{
}

void TemporalDurationConstructor::finishCreation(VM& vm, TemporalDurationPrototype* durationPrototype)
{
    Base::finishCreation(vm, 0, "Duration"_s, PropertyAdditionMode::WithoutStructureTransition);
    putDirectWithoutTransition(vm, vm.propertyNames->prototype, durationPrototype, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly);
    durationPrototype->putDirectWithoutTransition(vm, vm.propertyNames->constructor, this, static_cast<unsigned>(PropertyAttribute::DontEnum));
}

JSC_DEFINE_HOST_FUNCTION(constructTemporalDuration, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* newTarget = asObject(callFrame->newTarget());
    Structure* structure = JSC_GET_DERIVED_STRUCTURE(vm, durationStructure, newTarget, callFrame->jsCallee());
    RETURN_IF_EXCEPTION(scope, { });

    ISO8601::Duration result;
    auto count = std::min<size_t>(callFrame->argumentCount(), numberOfTemporalUnits);
    for (size_t i = 0; i < count; i++) {
        JSValue value = callFrame->uncheckedArgument(i);
        if (value.isUndefined())
            continue;

        result[i] = value.toNumber(globalObject) + 0.0;
        RETURN_IF_EXCEPTION(scope, { });

        if (!isInteger(result[i]))
            return throwVMRangeError(globalObject, scope, "Temporal.Duration properties must be integers"_s);
    }

    RELEASE_AND_RETURN(scope, JSValue::encode(TemporalDuration::tryCreateIfValid(globalObject, WTFMove(result), structure)));
}

JSC_DEFINE_HOST_FUNCTION(callTemporalDuration, (JSGlobalObject* globalObject, CallFrame*))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    return JSValue::encode(throwConstructorCannotBeCalledAsFunctionTypeError(globalObject, scope, "Duration"));
}

JSC_DEFINE_HOST_FUNCTION(temporalDurationConstructorFuncFrom, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return JSValue::encode(TemporalDuration::from(globalObject, callFrame->argument(0)));
}

JSC_DEFINE_HOST_FUNCTION(temporalDurationConstructorFuncCompare, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return JSValue::encode(TemporalDuration::compare(globalObject, callFrame->argument(0), callFrame->argument(1)));
}

} // namespace JSC

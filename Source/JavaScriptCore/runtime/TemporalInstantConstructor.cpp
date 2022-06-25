/*
 * Copyright (C) 2021 Igalia S.L.
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
#include "TemporalInstantConstructor.h"

#include "JSCInlines.h"
#include "TemporalInstant.h"
#include "TemporalInstantPrototype.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(TemporalInstantConstructor);

static JSC_DECLARE_HOST_FUNCTION(temporalInstantConstructorFuncFrom);
static JSC_DECLARE_HOST_FUNCTION(temporalInstantConstructorFuncFromEpochSeconds);
static JSC_DECLARE_HOST_FUNCTION(temporalInstantConstructorFuncFromEpochMilliseconds);
static JSC_DECLARE_HOST_FUNCTION(temporalInstantConstructorFuncFromEpochMicroseconds);
static JSC_DECLARE_HOST_FUNCTION(temporalInstantConstructorFuncFromEpochNanoseconds);
static JSC_DECLARE_HOST_FUNCTION(temporalInstantConstructorFuncCompare);

}

#include "TemporalInstantConstructor.lut.h"

namespace JSC {

const ClassInfo TemporalInstantConstructor::s_info = { "Function"_s, &Base::s_info, &temporalInstantConstructorTable, nullptr, CREATE_METHOD_TABLE(TemporalInstantConstructor) };

/* Source for TemporalInstantConstructor.lut.h
@begin temporalInstantConstructorTable
  from                   temporalInstantConstructorFuncFrom                   DontEnum|Function 1
  fromEpochSeconds       temporalInstantConstructorFuncFromEpochSeconds       DontEnum|Function 1
  fromEpochMilliseconds  temporalInstantConstructorFuncFromEpochMilliseconds  DontEnum|Function 1
  fromEpochMicroseconds  temporalInstantConstructorFuncFromEpochMicroseconds  DontEnum|Function 1
  fromEpochNanoseconds   temporalInstantConstructorFuncFromEpochNanoseconds   DontEnum|Function 1
  compare                temporalInstantConstructorFuncCompare                DontEnum|Function 2
@end
*/

TemporalInstantConstructor* TemporalInstantConstructor::create(VM& vm, Structure* structure, TemporalInstantPrototype* instantPrototype)
{
    auto* constructor = new (NotNull, allocateCell<TemporalInstantConstructor>(vm)) TemporalInstantConstructor(vm, structure);
    constructor->finishCreation(vm, instantPrototype);
    return constructor;
}

Structure* TemporalInstantConstructor::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(InternalFunctionType, StructureFlags), info());
}

static JSC_DECLARE_HOST_FUNCTION(callTemporalInstant);
static JSC_DECLARE_HOST_FUNCTION(constructTemporalInstant);

TemporalInstantConstructor::TemporalInstantConstructor(VM& vm, Structure* structure)
    : Base(vm, structure, callTemporalInstant, constructTemporalInstant)
{
}

void TemporalInstantConstructor::finishCreation(VM& vm, TemporalInstantPrototype* instantPrototype)
{
    Base::finishCreation(vm, 1, "Instant"_s, PropertyAdditionMode::WithoutStructureTransition);
    putDirectWithoutTransition(vm, vm.propertyNames->prototype, instantPrototype, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly);
    instantPrototype->putDirectWithoutTransition(vm, vm.propertyNames->constructor, this, static_cast<unsigned>(PropertyAttribute::DontEnum));
}

JSC_DEFINE_HOST_FUNCTION(constructTemporalInstant, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* newTarget = asObject(callFrame->newTarget());
    Structure* structure = JSC_GET_DERIVED_STRUCTURE(vm, instantStructure, newTarget, callFrame->jsCallee());
    RETURN_IF_EXCEPTION(scope, { });

    if (callFrame->argumentCount() < 1)
        return throwVMTypeError(globalObject, scope, "Missing required epochNanoseconds argument to Temporal.Instant"_s);

    RELEASE_AND_RETURN(scope, JSValue::encode(TemporalInstant::tryCreateIfValid(globalObject, callFrame->uncheckedArgument(0), structure)));
}

JSC_DEFINE_HOST_FUNCTION(callTemporalInstant, (JSGlobalObject* globalObject, CallFrame*))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    return JSValue::encode(throwConstructorCannotBeCalledAsFunctionTypeError(globalObject, scope, "Instant"));
}

JSC_DEFINE_HOST_FUNCTION(temporalInstantConstructorFuncFrom, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return JSValue::encode(TemporalInstant::from(globalObject, callFrame->argument(0)));
}

JSC_DEFINE_HOST_FUNCTION(temporalInstantConstructorFuncFromEpochSeconds, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return JSValue::encode(TemporalInstant::fromEpochSeconds(globalObject, callFrame->argument(0)));
}

JSC_DEFINE_HOST_FUNCTION(temporalInstantConstructorFuncFromEpochMilliseconds, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return JSValue::encode(TemporalInstant::fromEpochMilliseconds(globalObject, callFrame->argument(0)));
}

JSC_DEFINE_HOST_FUNCTION(temporalInstantConstructorFuncFromEpochMicroseconds, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return JSValue::encode(TemporalInstant::fromEpochMicroseconds(globalObject, callFrame->argument(0)));
}

JSC_DEFINE_HOST_FUNCTION(temporalInstantConstructorFuncFromEpochNanoseconds, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return JSValue::encode(TemporalInstant::fromEpochNanoseconds(globalObject, callFrame->argument(0)));
}

JSC_DEFINE_HOST_FUNCTION(temporalInstantConstructorFuncCompare, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return JSValue::encode(TemporalInstant::compare(globalObject, callFrame->argument(0), callFrame->argument(1)));
}

} // namespace JSC

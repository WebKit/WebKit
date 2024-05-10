/*
 * Copyright (C) 2021 Apple Inc. All Rights Reserved.
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
#include "TemporalTimeZoneConstructor.h"

#include "ISO8601.h"
#include "JSCInlines.h"
#include "TemporalTimeZone.h"
#include "TemporalTimeZonePrototype.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(TemporalTimeZoneConstructor);
static JSC_DECLARE_HOST_FUNCTION(temporalTimeZoneConstructorFuncFrom);

}

#include "TemporalTimeZoneConstructor.lut.h"

namespace JSC {

const ClassInfo TemporalTimeZoneConstructor::s_info = { "Function"_s, &InternalFunction::s_info, &temporalTimeZoneConstructorTable, nullptr, CREATE_METHOD_TABLE(TemporalTimeZoneConstructor) };

/* Source for TemporalTimeZoneConstructor.lut.h
@begin temporalTimeZoneConstructorTable
    from             temporalTimeZoneConstructorFuncFrom             DontEnum|Function 1
@end
*/

TemporalTimeZoneConstructor* TemporalTimeZoneConstructor::create(VM& vm, Structure* structure, TemporalTimeZonePrototype* temporalTimeZonePrototype)
{
    TemporalTimeZoneConstructor* constructor = new (NotNull, allocateCell<TemporalTimeZoneConstructor>(vm)) TemporalTimeZoneConstructor(vm, structure);
    constructor->finishCreation(vm, temporalTimeZonePrototype);
    return constructor;
}

Structure* TemporalTimeZoneConstructor::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(InternalFunctionType, StructureFlags), info());
}

static JSC_DECLARE_HOST_FUNCTION(callTemporalTimeZone);
static JSC_DECLARE_HOST_FUNCTION(constructTemporalTimeZone);

TemporalTimeZoneConstructor::TemporalTimeZoneConstructor(VM& vm, Structure* structure)
    : InternalFunction(vm, structure, callTemporalTimeZone, constructTemporalTimeZone)
{
}

void TemporalTimeZoneConstructor::finishCreation(VM& vm, TemporalTimeZonePrototype* temporalTimeZonePrototype)
{
    Base::finishCreation(vm, 0, "TimeZone"_s, PropertyAdditionMode::WithoutStructureTransition);
    putDirectWithoutTransition(vm, vm.propertyNames->prototype, temporalTimeZonePrototype, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly);
    temporalTimeZonePrototype->putDirectWithoutTransition(vm, vm.propertyNames->constructor, this, static_cast<unsigned>(PropertyAttribute::DontEnum));
}

JSC_DEFINE_HOST_FUNCTION(constructTemporalTimeZone, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* newTarget = asObject(callFrame->newTarget());
    Structure* structure = JSC_GET_DERIVED_STRUCTURE(vm, timeZoneStructure, newTarget, callFrame->jsCallee());
    RETURN_IF_EXCEPTION(scope, { });

    auto timeZoneString = callFrame->argument(0).toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    std::optional<int64_t> utcOffset = ISO8601::parseTimeZoneNumericUTCOffset(timeZoneString);
    if (utcOffset)
        return JSValue::encode(TemporalTimeZone::createFromUTCOffset(vm, structure, utcOffset.value()));

    std::optional<TimeZoneID> identifier = ISO8601::parseTimeZoneName(timeZoneString);
    if (!identifier) {
        throwRangeError(globalObject, scope, "argument needs to be UTC offset string or TimeZone identifier"_s);
        return { };
    }
    return JSValue::encode(TemporalTimeZone::createFromID(vm, structure, identifier.value()));
}

JSC_DEFINE_HOST_FUNCTION(callTemporalTimeZone, (JSGlobalObject* globalObject, CallFrame*))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    return JSValue::encode(throwConstructorCannotBeCalledAsFunctionTypeError(globalObject, scope, "TimeZone"_s));
}

JSC_DEFINE_HOST_FUNCTION(temporalTimeZoneConstructorFuncFrom, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return JSValue::encode(TemporalTimeZone::from(globalObject, callFrame->argument(0)));
}

} // namespace JSC

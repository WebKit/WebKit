/*
 * Copyright (C) 2022 Apple Inc.
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
#include "TemporalPlainDatePrototype.h"

#include "JSCInlines.h"
#include "ObjectConstructor.h"
#include "TemporalDuration.h"
#include "TemporalPlainDate.h"

namespace JSC {

static JSC_DECLARE_HOST_FUNCTION(temporalPlainDatePrototypeFuncToString);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainDatePrototypeGetterYear);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainDatePrototypeGetterMonth);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainDatePrototypeGetterDay);

}

#include "TemporalPlainDatePrototype.lut.h"

namespace JSC {

const ClassInfo TemporalPlainDatePrototype::s_info = { "Temporal.PlainDate", &Base::s_info, &plainDatePrototypeTable, nullptr, CREATE_METHOD_TABLE(TemporalPlainDatePrototype) };

/* Source for TemporalPlainDatePrototype.lut.h
@begin plainDatePrototypeTable
  toString         temporalPlainDatePrototypeFuncToString           DontEnum|Function 0
  year             temporalPlainDatePrototypeGetterYear             DontEnum|ReadOnly|CustomAccessor
  month            temporalPlainDatePrototypeGetterMonth            DontEnum|ReadOnly|CustomAccessor
  day              temporalPlainDatePrototypeGetterDay              DontEnum|ReadOnly|CustomAccessor
@end
*/

TemporalPlainDatePrototype* TemporalPlainDatePrototype::create(VM& vm, JSGlobalObject* globalObject, Structure* structure)
{
    auto* prototype = new (NotNull, allocateCell<TemporalPlainDatePrototype>(vm)) TemporalPlainDatePrototype(vm, structure);
    prototype->finishCreation(vm, globalObject);
    return prototype;
}

Structure* TemporalPlainDatePrototype::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

TemporalPlainDatePrototype::TemporalPlainDatePrototype(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

void TemporalPlainDatePrototype::finishCreation(VM& vm, JSGlobalObject*)
{
    Base::finishCreation(vm);
    ASSERT(inherits(vm, info()));
    JSC_TO_STRING_TAG_WITHOUT_TRANSITION();
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindate.prototype.tostring
JSC_DEFINE_HOST_FUNCTION(temporalPlainDatePrototypeFuncToString, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDate = jsDynamicCast<TemporalPlainDate*>(vm, callFrame->thisValue());
    if (!plainDate)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.toString called on value that's not a PlainDate"_s);

    RELEASE_AND_RETURN(scope, JSValue::encode(jsString(vm, plainDate->toString(globalObject, callFrame->argument(0)))));
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainDatePrototypeGetterYear, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDate = jsDynamicCast<TemporalPlainDate*>(vm, JSValue::decode(thisValue));
    if (!plainDate)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.year called on value that's not a plainDate"_s);

    return JSValue::encode(jsNumber(plainDate->year()));
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainDatePrototypeGetterMonth, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDate = jsDynamicCast<TemporalPlainDate*>(vm, JSValue::decode(thisValue));
    if (!plainDate)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.month called on value that's not a plainDate"_s);

    return JSValue::encode(jsNumber(plainDate->month()));
}

JSC_DEFINE_CUSTOM_GETTER(temporalPlainDatePrototypeGetterDay, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* plainDate = jsDynamicCast<TemporalPlainDate*>(vm, JSValue::decode(thisValue));
    if (!plainDate)
        return throwVMTypeError(globalObject, scope, "Temporal.PlainDate.prototype.day called on value that's not a plainDate"_s);

    return JSValue::encode(jsNumber(plainDate->day()));
}


} // namespace JSC

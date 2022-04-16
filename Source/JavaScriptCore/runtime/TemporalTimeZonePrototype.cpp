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
#include "TemporalTimeZonePrototype.h"

#include "BuiltinNames.h"
#include "ISO8601.h"
#include "JSCInlines.h"
#include "TemporalTimeZone.h"

namespace JSC {

static JSC_DECLARE_HOST_FUNCTION(temporalTimeZonePrototypeFuncToString);
static JSC_DECLARE_HOST_FUNCTION(temporalTimeZonePrototypeFuncToJSON);
static JSC_DECLARE_CUSTOM_GETTER(temporalTimeZonePrototypeGetterId);

}

#include "TemporalTimeZonePrototype.lut.h"

namespace JSC {

const ClassInfo TemporalTimeZonePrototype::s_info = { "Temporal.TimeZone"_s, &Base::s_info, &temporalTimeZonePrototypeTable, nullptr, CREATE_METHOD_TABLE(TemporalTimeZonePrototype) };

/* Source for TemporalTimeZonePrototype.lut.h
@begin temporalTimeZonePrototypeTable
    toString        temporalTimeZonePrototypeFuncToString     DontEnum|Function 0
    toJSON          temporalTimeZonePrototypeFuncToJSON       DontEnum|Function 0
    id              temporalTimeZonePrototypeGetterId         ReadOnly|DontEnum|CustomAccessor
@end
*/

TemporalTimeZonePrototype* TemporalTimeZonePrototype::create(VM& vm, JSGlobalObject* globalObject, Structure* structure)
{
    TemporalTimeZonePrototype* object = new (NotNull, allocateCell<TemporalTimeZonePrototype>(vm)) TemporalTimeZonePrototype(vm, structure);
    object->finishCreation(vm, globalObject);
    return object;
}

Structure* TemporalTimeZonePrototype::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

TemporalTimeZonePrototype::TemporalTimeZonePrototype(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

void TemporalTimeZonePrototype::finishCreation(VM& vm, JSGlobalObject*)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));
    JSC_TO_STRING_TAG_WITHOUT_TRANSITION();
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.timezone.prototype.id
JSC_DEFINE_CUSTOM_GETTER(temporalTimeZonePrototypeGetterId, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    return JSValue::encode(JSValue::decode(thisValue).toString(globalObject));
}

// https://tc39.es/proposal-temporal/#sec-temporal.timezone.prototype.tostring
JSC_DEFINE_HOST_FUNCTION(temporalTimeZonePrototypeFuncToString, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* timeZone = jsDynamicCast<TemporalTimeZone*>(callFrame->thisValue());
    if (!timeZone)
        return throwVMTypeError(globalObject, scope, "Temporal.TimeZone.prototype.toString called on value that's not a TimeZone"_s);

    auto variant = timeZone->timeZone();
    auto string = WTF::switchOn(variant,
        [](TimeZoneID identifier) -> String {
            return intlAvailableTimeZones()[identifier];
        },
        [](int64_t offset) -> String {
            return ISO8601::formatTimeZoneOffsetString(offset);
        });
    return JSValue::encode(jsString(vm, WTFMove(string)));
}

// https://tc39.es/proposal-temporal/#sec-temporal.timezone.prototype.tojson
JSC_DEFINE_HOST_FUNCTION(temporalTimeZonePrototypeFuncToJSON, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return JSValue::encode(callFrame->thisValue().toString(globalObject));
}

} // namespace JSC

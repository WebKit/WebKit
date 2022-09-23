/*
 * Copyright (C) 2020-2021 Apple Inc. All rights reserved.
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
#include "IntlDurationFormatPrototype.h"

#include "IntlDurationFormat.h"
#include "JSCInlines.h"
#include "TemporalDuration.h"

namespace JSC {

static JSC_DECLARE_HOST_FUNCTION(intlDurationFormatPrototypeFuncFormat);
static JSC_DECLARE_HOST_FUNCTION(intlDurationFormatPrototypeFuncFormatToParts);
static JSC_DECLARE_HOST_FUNCTION(intlDurationFormatPrototypeFuncResolvedOptions);

}

#include "IntlDurationFormatPrototype.lut.h"

namespace JSC {

const ClassInfo IntlDurationFormatPrototype::s_info = { "Intl.DurationFormat"_s, &Base::s_info, &durationFormatPrototypeTable, nullptr, CREATE_METHOD_TABLE(IntlDurationFormatPrototype) };

/* Source for IntlDurationFormatPrototype.lut.h
@begin durationFormatPrototypeTable
  format           intlDurationFormatPrototypeFuncFormat             DontEnum|Function 1
  formatToParts    intlDurationFormatPrototypeFuncFormatToParts      DontEnum|Function 1
  resolvedOptions  intlDurationFormatPrototypeFuncResolvedOptions    DontEnum|Function 0
@end
*/

IntlDurationFormatPrototype* IntlDurationFormatPrototype::create(VM& vm, Structure* structure)
{
    auto* object = new (NotNull, allocateCell<IntlDurationFormatPrototype>(vm)) IntlDurationFormatPrototype(vm, structure);
    object->finishCreation(vm);
    return object;
}

Structure* IntlDurationFormatPrototype::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

IntlDurationFormatPrototype::IntlDurationFormatPrototype(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

void IntlDurationFormatPrototype::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));
    JSC_TO_STRING_TAG_WITHOUT_TRANSITION();
}

// https://tc39.es/proposal-intl-duration-format/#sec-Intl.DurationFormat.prototype.format
JSC_DEFINE_HOST_FUNCTION(intlDurationFormatPrototypeFuncFormat, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* durationFormat = jsDynamicCast<IntlDurationFormat*>(callFrame->thisValue());
    if (!durationFormat)
        return throwVMTypeError(globalObject, scope, "Intl.DurationFormat.prototype.format called on value that's not a DurationFormat"_s);

    auto* object = jsDynamicCast<JSObject*>(callFrame->argument(0));
    if (UNLIKELY(!object))
        return throwVMTypeError(globalObject, scope, "Intl.DurationFormat.prototype.format argument needs to be an object"_s);

    auto duration = TemporalDuration::toISO8601Duration(globalObject, object);
    RETURN_IF_EXCEPTION(scope, { });

    RELEASE_AND_RETURN(scope, JSValue::encode(durationFormat->format(globalObject, WTFMove(duration))));
}

// https://tc39.es/proposal-intl-duration-format/#sec-Intl.DurationFormat.prototype.formatToParts
JSC_DEFINE_HOST_FUNCTION(intlDurationFormatPrototypeFuncFormatToParts, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* durationFormat = jsDynamicCast<IntlDurationFormat*>(callFrame->thisValue());
    if (!durationFormat)
        return throwVMTypeError(globalObject, scope, "Intl.DurationFormat.prototype.formatToParts called on value that's not a DurationFormat"_s);

    auto* object = jsDynamicCast<JSObject*>(callFrame->argument(0));
    if (UNLIKELY(!object))
        return throwVMTypeError(globalObject, scope, "Intl.DurationFormat.prototype.formatToParts argument needs to be an object"_s);

    auto duration = TemporalDuration::toISO8601Duration(globalObject, object);
    RETURN_IF_EXCEPTION(scope, { });

    RELEASE_AND_RETURN(scope, JSValue::encode(durationFormat->formatToParts(globalObject, WTFMove(duration))));
}

// https://tc39.es/proposal-intl-duration-format/#sec-Intl.DurationFormat.prototype.resolvedOptions
JSC_DEFINE_HOST_FUNCTION(intlDurationFormatPrototypeFuncResolvedOptions, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* durationFormat = jsDynamicCast<IntlDurationFormat*>(callFrame->thisValue());
    if (!durationFormat)
        return throwVMTypeError(globalObject, scope, "Intl.DurationFormat.prototype.resolvedOptions called on value that's not a DurationFormat"_s);

    RELEASE_AND_RETURN(scope, JSValue::encode(durationFormat->resolvedOptions(globalObject)));
}

} // namespace JSC

/*
 * Copyright (C) 2020 Sony Interactive Entertainment Inc.
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
#include "IntlRelativeTimeFormatPrototype.h"

#include "IntlRelativeTimeFormat.h"
#include "JSCInlines.h"

namespace JSC {

static JSC_DECLARE_HOST_FUNCTION(intlRelativeTimeFormatPrototypeFuncFormat);
static JSC_DECLARE_HOST_FUNCTION(intlRelativeTimeFormatPrototypeFuncFormatToParts);
static JSC_DECLARE_HOST_FUNCTION(intlRelativeTimeFormatPrototypeFuncResolvedOptions);

}

#include "IntlRelativeTimeFormatPrototype.lut.h"

namespace JSC {

const ClassInfo IntlRelativeTimeFormatPrototype::s_info = { "Intl.RelativeTimeFormat"_s, &Base::s_info, &relativeTimeFormatPrototypeTable, nullptr, CREATE_METHOD_TABLE(IntlRelativeTimeFormatPrototype) };

/* Source for IntlRelativeTimeFormatPrototype.lut.h
@begin relativeTimeFormatPrototypeTable
  format           intlRelativeTimeFormatPrototypeFuncFormat           DontEnum|Function 2
  formatToParts    intlRelativeTimeFormatPrototypeFuncFormatToParts    DontEnum|Function 2
  resolvedOptions  intlRelativeTimeFormatPrototypeFuncResolvedOptions  DontEnum|Function 0
@end
*/

IntlRelativeTimeFormatPrototype* IntlRelativeTimeFormatPrototype::create(VM& vm, Structure* structure)
{
    auto* object = new (NotNull, allocateCell<IntlRelativeTimeFormatPrototype>(vm)) IntlRelativeTimeFormatPrototype(vm, structure);
    object->finishCreation(vm);
    return object;
}

Structure* IntlRelativeTimeFormatPrototype::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

IntlRelativeTimeFormatPrototype::IntlRelativeTimeFormatPrototype(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

void IntlRelativeTimeFormatPrototype::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));
    JSC_TO_STRING_TAG_WITHOUT_TRANSITION();
}

// https://tc39.es/ecma402/#sec-Intl.RelativeTimeFormat.prototype.format
JSC_DEFINE_HOST_FUNCTION(intlRelativeTimeFormatPrototypeFuncFormat, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* relativeTimeFormat = jsDynamicCast<IntlRelativeTimeFormat*>(callFrame->thisValue());
    if (!relativeTimeFormat)
        return JSValue::encode(throwTypeError(globalObject, scope, "Intl.RelativeTimeFormat.prototype.format called on value that's not a RelativeTimeFormat"_s));

    double value = callFrame->argument(0).toNumber(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    String unit = callFrame->argument(1).toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    RELEASE_AND_RETURN(scope, JSValue::encode(relativeTimeFormat->format(globalObject, value, unit)));
}

// https://tc39.es/ecma402/#sec-Intl.RelativeTimeFormat.prototype.formatToParts
JSC_DEFINE_HOST_FUNCTION(intlRelativeTimeFormatPrototypeFuncFormatToParts, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* relativeTimeFormat = jsDynamicCast<IntlRelativeTimeFormat*>(callFrame->thisValue());
    if (!relativeTimeFormat)
        return JSValue::encode(throwTypeError(globalObject, scope, "Intl.RelativeTimeFormat.prototype.formatToParts called on value that's not a RelativeTimeFormat"_s));

    double value = callFrame->argument(0).toNumber(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    String unit = callFrame->argument(1).toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    RELEASE_AND_RETURN(scope, JSValue::encode(relativeTimeFormat->formatToParts(globalObject, value, unit)));
}

// https://tc39.es/ecma402/#sec-intl.relativetimeformat.prototype.resolvedoptions
JSC_DEFINE_HOST_FUNCTION(intlRelativeTimeFormatPrototypeFuncResolvedOptions, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* relativeTimeFormat = jsDynamicCast<IntlRelativeTimeFormat*>(callFrame->thisValue());
    if (!relativeTimeFormat)
        return JSValue::encode(throwTypeError(globalObject, scope, "Intl.RelativeTimeFormat.prototype.resolvedOptions called on value that's not a RelativeTimeFormat"_s));

    RELEASE_AND_RETURN(scope, JSValue::encode(relativeTimeFormat->resolvedOptions(globalObject)));
}

} // namespace JSC

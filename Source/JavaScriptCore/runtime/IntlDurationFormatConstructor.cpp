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
#include "IntlDurationFormatConstructor.h"

#include "IntlDurationFormat.h"
#include "IntlDurationFormatPrototype.h"
#include "JSCInlines.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(IntlDurationFormatConstructor);

static JSC_DECLARE_HOST_FUNCTION(intlDurationFormatConstructorSupportedLocalesOf);

}

#include "IntlDurationFormatConstructor.lut.h"

namespace JSC {

const ClassInfo IntlDurationFormatConstructor::s_info = { "Function"_s, &Base::s_info, &durationFormatConstructorTable, nullptr, CREATE_METHOD_TABLE(IntlDurationFormatConstructor) };

/* Source for IntlDurationFormatConstructor.lut.h
@begin durationFormatConstructorTable
  supportedLocalesOf             intlDurationFormatConstructorSupportedLocalesOf             DontEnum|Function 1
@end
*/

IntlDurationFormatConstructor* IntlDurationFormatConstructor::create(VM& vm, Structure* structure, IntlDurationFormatPrototype* durationFormatPrototype)
{
    auto* constructor = new (NotNull, allocateCell<IntlDurationFormatConstructor>(vm)) IntlDurationFormatConstructor(vm, structure);
    constructor->finishCreation(vm, durationFormatPrototype);
    return constructor;
}

Structure* IntlDurationFormatConstructor::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(InternalFunctionType, StructureFlags), info());
}

static JSC_DECLARE_HOST_FUNCTION(callIntlDurationFormat);
static JSC_DECLARE_HOST_FUNCTION(constructIntlDurationFormat);

IntlDurationFormatConstructor::IntlDurationFormatConstructor(VM& vm, Structure* structure)
    : Base(vm, structure, callIntlDurationFormat, constructIntlDurationFormat)
{
}

void IntlDurationFormatConstructor::finishCreation(VM& vm, IntlDurationFormatPrototype* durationFormatPrototype)
{
    Base::finishCreation(vm, 0, "DurationFormat"_s, PropertyAdditionMode::WithoutStructureTransition);
    putDirectWithoutTransition(vm, vm.propertyNames->prototype, durationFormatPrototype, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly);
    durationFormatPrototype->putDirectWithoutTransition(vm, vm.propertyNames->constructor, this, static_cast<unsigned>(PropertyAttribute::DontEnum));
}

// https://tc39.es/proposal-intl-duration-format/#sec-Intl.DurationFormat
JSC_DEFINE_HOST_FUNCTION(constructIntlDurationFormat, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* newTarget = asObject(callFrame->newTarget());
    Structure* structure = JSC_GET_DERIVED_STRUCTURE(vm, durationFormatStructure, newTarget, callFrame->jsCallee());
    RETURN_IF_EXCEPTION(scope, { });

    IntlDurationFormat* durationFormat = IntlDurationFormat::create(vm, structure);
    ASSERT(durationFormat);

    scope.release();
    durationFormat->initializeDurationFormat(globalObject, callFrame->argument(0), callFrame->argument(1));
    return JSValue::encode(durationFormat);
}

// https://tc39.es/proposal-intl-duration-format/#sec-Intl.DurationFormat
JSC_DEFINE_HOST_FUNCTION(callIntlDurationFormat, (JSGlobalObject* globalObject, CallFrame*))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    return JSValue::encode(throwConstructorCannotBeCalledAsFunctionTypeError(globalObject, scope, "DurationFormat"));
}

JSC_DEFINE_HOST_FUNCTION(intlDurationFormatConstructorSupportedLocalesOf, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    // Intl.DurationFormat.supportedLocalesOf(locales [, options])
    // https://tc39.es/proposal-intl-duration-format/#sec-Intl.DurationFormat.supportedLocalesOf

    // 1. Let availableLocales be %DurationFormat%.[[availableLocales]].
    const auto& availableLocales = intlDurationFormatAvailableLocales();

    // 2. Let requestedLocales be CanonicalizeLocaleList(locales).
    Vector<String> requestedLocales = canonicalizeLocaleList(globalObject, callFrame->argument(0));
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    // 3. Return SupportedLocales(availableLocales, requestedLocales, options).
    RELEASE_AND_RETURN(scope, JSValue::encode(supportedLocales(globalObject, availableLocales, requestedLocales, callFrame->argument(1))));
}

} // namespace JSC

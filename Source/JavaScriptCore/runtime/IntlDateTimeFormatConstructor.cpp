/*
 * Copyright (C) 2015 Andy VanWagoner (andy@vanwagoner.family)
 * Copyright (C) 2016-2021 Apple Inc. All Rights Reserved.
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
#include "IntlDateTimeFormatConstructor.h"

#include "IntlDateTimeFormat.h"
#include "IntlDateTimeFormatPrototype.h"
#include "IntlObjectInlines.h"
#include "JSCInlines.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(IntlDateTimeFormatConstructor);

static JSC_DECLARE_HOST_FUNCTION(intlDateTimeFormatConstructorFuncSupportedLocalesOf);

}

#include "IntlDateTimeFormatConstructor.lut.h"

namespace JSC {

const ClassInfo IntlDateTimeFormatConstructor::s_info = { "Function"_s, &InternalFunction::s_info, &dateTimeFormatConstructorTable, nullptr, CREATE_METHOD_TABLE(IntlDateTimeFormatConstructor) };

/* Source for IntlDateTimeFormatConstructor.lut.h
@begin dateTimeFormatConstructorTable
  supportedLocalesOf             intlDateTimeFormatConstructorFuncSupportedLocalesOf             DontEnum|Function 1
@end
*/

IntlDateTimeFormatConstructor* IntlDateTimeFormatConstructor::create(VM& vm, Structure* structure, IntlDateTimeFormatPrototype* dateTimeFormatPrototype)
{
    IntlDateTimeFormatConstructor* constructor = new (NotNull, allocateCell<IntlDateTimeFormatConstructor>(vm)) IntlDateTimeFormatConstructor(vm, structure);
    constructor->finishCreation(vm, dateTimeFormatPrototype);
    return constructor;
}

Structure* IntlDateTimeFormatConstructor::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(InternalFunctionType, StructureFlags), info());
}

static JSC_DECLARE_HOST_FUNCTION(callIntlDateTimeFormat);
static JSC_DECLARE_HOST_FUNCTION(constructIntlDateTimeFormat);

IntlDateTimeFormatConstructor::IntlDateTimeFormatConstructor(VM& vm, Structure* structure)
    : InternalFunction(vm, structure, callIntlDateTimeFormat, constructIntlDateTimeFormat)
{
}

void IntlDateTimeFormatConstructor::finishCreation(VM& vm, IntlDateTimeFormatPrototype* dateTimeFormatPrototype)
{
    Base::finishCreation(vm, 0, "DateTimeFormat"_s, PropertyAdditionMode::WithoutStructureTransition);
    putDirectWithoutTransition(vm, vm.propertyNames->prototype, dateTimeFormatPrototype, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly);
}

JSC_DEFINE_HOST_FUNCTION(constructIntlDateTimeFormat, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    // 12.1.2 Intl.DateTimeFormat ([locales [, options]]) (ECMA-402 2.0)
    // 1. If NewTarget is undefined, let newTarget be the active function object, else let newTarget be NewTarget.
    // 2. Let dateTimeFormat be OrdinaryCreateFromConstructor(newTarget, %DateTimeFormatPrototype%).
    // 3. ReturnIfAbrupt(dateTimeFormat).
    JSObject* newTarget = asObject(callFrame->newTarget());
    Structure* structure = JSC_GET_DERIVED_STRUCTURE(vm, dateTimeFormatStructure, newTarget, callFrame->jsCallee());
    RETURN_IF_EXCEPTION(scope, { });

    IntlDateTimeFormat* dateTimeFormat = IntlDateTimeFormat::create(vm, structure);
    ASSERT(dateTimeFormat);

    // 4. Return InitializeDateTimeFormat(dateTimeFormat, locales, options).
    scope.release();
    dateTimeFormat->initializeDateTimeFormat(globalObject, callFrame->argument(0), callFrame->argument(1));
    return JSValue::encode(dateTimeFormat);
}

JSC_DEFINE_HOST_FUNCTION(callIntlDateTimeFormat, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    // 12.1.2 Intl.DateTimeFormat ([locales [, options]]) (ECMA-402 2.0)
    // 1. If NewTarget is undefined, let newTarget be the active function object, else let newTarget be NewTarget.
    // NewTarget is always undefined when called as a function.

    // FIXME: Workaround to provide compatibility with ECMA-402 1.0 call/apply patterns.
    // https://bugs.webkit.org/show_bug.cgi?id=153679
    return JSValue::encode(constructIntlInstanceWithWorkaroundForLegacyIntlConstructor(globalObject, callFrame->thisValue(), callFrame->jsCallee(), [&] (VM& vm) {
        // 2. Let dateTimeFormat be OrdinaryCreateFromConstructor(newTarget, %DateTimeFormatPrototype%).
        // 3. ReturnIfAbrupt(dateTimeFormat).
        IntlDateTimeFormat* dateTimeFormat = IntlDateTimeFormat::create(vm, globalObject->dateTimeFormatStructure());
        ASSERT(dateTimeFormat);

        // 4. Return InitializeDateTimeFormat(dateTimeFormat, locales, options).
        dateTimeFormat->initializeDateTimeFormat(globalObject, callFrame->argument(0), callFrame->argument(1));
        return dateTimeFormat;
    }));
}

JSC_DEFINE_HOST_FUNCTION(intlDateTimeFormatConstructorFuncSupportedLocalesOf, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    // 12.2.2 Intl.DateTimeFormat.supportedLocalesOf(locales [, options]) (ECMA-402 2.0)

    // 1. Let availableLocales be %DateTimeFormat%.[[availableLocales]].
    const auto& availableLocales = intlDateTimeFormatAvailableLocales();

    // 2. Let requestedLocales be CanonicalizeLocaleList(locales).
    Vector<String> requestedLocales = canonicalizeLocaleList(globalObject, callFrame->argument(0));
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    // 3. Return SupportedLocales(availableLocales, requestedLocales, options).
    RELEASE_AND_RETURN(scope, JSValue::encode(supportedLocales(globalObject, availableLocales, requestedLocales, callFrame->argument(1))));
}

} // namespace JSC

/*
 * Copyright (C) 2015 Andy VanWagoner (andy@vanwagoner.family)
 * Copyright (C) 2016-2020 Apple Inc. All Rights Reserved.
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
#include "IntlNumberFormatConstructor.h"

#include "IntlNumberFormat.h"
#include "IntlNumberFormatPrototype.h"
#include "IntlObjectInlines.h"
#include "JSCInlines.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(IntlNumberFormatConstructor);

static EncodedJSValue JSC_HOST_CALL IntlNumberFormatConstructorFuncSupportedLocalesOf(JSGlobalObject*, CallFrame*);

}

#include "IntlNumberFormatConstructor.lut.h"

namespace JSC {

const ClassInfo IntlNumberFormatConstructor::s_info = { "Function", &Base::s_info, &numberFormatConstructorTable, nullptr, CREATE_METHOD_TABLE(IntlNumberFormatConstructor) };

/* Source for IntlNumberFormatConstructor.lut.h
@begin numberFormatConstructorTable
  supportedLocalesOf             IntlNumberFormatConstructorFuncSupportedLocalesOf             DontEnum|Function 1
@end
*/

IntlNumberFormatConstructor* IntlNumberFormatConstructor::create(VM& vm, Structure* structure, IntlNumberFormatPrototype* numberFormatPrototype)
{
    IntlNumberFormatConstructor* constructor = new (NotNull, allocateCell<IntlNumberFormatConstructor>(vm.heap)) IntlNumberFormatConstructor(vm, structure);
    constructor->finishCreation(vm, numberFormatPrototype);
    return constructor;
}

Structure* IntlNumberFormatConstructor::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(InternalFunctionType, StructureFlags), info());
}

static EncodedJSValue JSC_HOST_CALL callIntlNumberFormat(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL constructIntlNumberFormat(JSGlobalObject*, CallFrame*);

IntlNumberFormatConstructor::IntlNumberFormatConstructor(VM& vm, Structure* structure)
    : InternalFunction(vm, structure, callIntlNumberFormat, constructIntlNumberFormat)
{
}

void IntlNumberFormatConstructor::finishCreation(VM& vm, IntlNumberFormatPrototype* numberFormatPrototype)
{
    Base::finishCreation(vm, "NumberFormat"_s, NameAdditionMode::WithoutStructureTransition);
    putDirectWithoutTransition(vm, vm.propertyNames->prototype, numberFormatPrototype, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly);
    putDirectWithoutTransition(vm, vm.propertyNames->length, jsNumber(0), PropertyAttribute::ReadOnly | PropertyAttribute::DontEnum);
    numberFormatPrototype->putDirectWithoutTransition(vm, vm.propertyNames->constructor, this, static_cast<unsigned>(PropertyAttribute::DontEnum));
}

static EncodedJSValue JSC_HOST_CALL constructIntlNumberFormat(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    // 11.1.2 Intl.NumberFormat ([locales [, options]]) (ECMA-402 2.0)
    // 1. If NewTarget is undefined, let newTarget be the active function object, else let newTarget be NewTarget.
    // 2. Let numberFormat be OrdinaryCreateFromConstructor(newTarget, %NumberFormatPrototype%).
    // 3. ReturnIfAbrupt(numberFormat).
    JSObject* newTarget = asObject(callFrame->newTarget());
    Structure* structure = newTarget == callFrame->jsCallee()
        ? globalObject->numberFormatStructure()
        : InternalFunction::createSubclassStructure(globalObject, newTarget, getFunctionRealm(vm, newTarget)->numberFormatStructure());
    RETURN_IF_EXCEPTION(scope, { });

    IntlNumberFormat* numberFormat = IntlNumberFormat::create(vm, structure);
    ASSERT(numberFormat);

    // 4. Return InitializeNumberFormat(numberFormat, locales, options).
    scope.release();
    numberFormat->initializeNumberFormat(globalObject, callFrame->argument(0), callFrame->argument(1));
    return JSValue::encode(numberFormat);
}

static EncodedJSValue JSC_HOST_CALL callIntlNumberFormat(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    // 11.1.2 Intl.NumberFormat ([locales [, options]]) (ECMA-402 2.0)
    // 1. If NewTarget is undefined, let newTarget be the active function object, else let newTarget be NewTarget.
    // NewTarget is always undefined when called as a function.

    // FIXME: Workaround to provide compatibility with ECMA-402 1.0 call/apply patterns.
    // https://bugs.webkit.org/show_bug.cgi?id=153679
    return JSValue::encode(constructIntlInstanceWithWorkaroundForLegacyIntlConstructor<IntlNumberFormat>(globalObject, callFrame->thisValue(), callFrame->jsCallee(), [&] (VM& vm) {
        // 2. Let numberFormat be OrdinaryCreateFromConstructor(newTarget, %NumberFormatPrototype%).
        // 3. ReturnIfAbrupt(numberFormat).
        IntlNumberFormat* numberFormat = IntlNumberFormat::create(vm, globalObject->numberFormatStructure());
        ASSERT(numberFormat);

        // 4. Return InitializeNumberFormat(numberFormat, locales, options).
        numberFormat->initializeNumberFormat(globalObject, callFrame->argument(0), callFrame->argument(1));
        return numberFormat;
    }));
}

EncodedJSValue JSC_HOST_CALL IntlNumberFormatConstructorFuncSupportedLocalesOf(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    // 11.2.2 Intl.NumberFormat.supportedLocalesOf(locales [, options]) (ECMA-402 2.0)

    // 1. Let availableLocales be %NumberFormat%.[[availableLocales]].
    const HashSet<String>& availableLocales = intlNumberFormatAvailableLocales();

    // 2. Let requestedLocales be CanonicalizeLocaleList(locales).
    Vector<String> requestedLocales = canonicalizeLocaleList(globalObject, callFrame->argument(0));
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    // 3. Return SupportedLocales(availableLocales, requestedLocales, options).
    RELEASE_AND_RETURN(scope, JSValue::encode(supportedLocales(globalObject, availableLocales, requestedLocales, callFrame->argument(1))));
}

} // namespace JSC

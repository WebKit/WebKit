/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "IntlListFormatConstructor.h"

#include "IntlListFormat.h"
#include "IntlListFormatPrototype.h"
#include "JSCInlines.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(IntlListFormatConstructor);

static JSC_DECLARE_HOST_FUNCTION(intlListFormatConstructorSupportedLocalesOf);

}

#include "IntlListFormatConstructor.lut.h"

namespace JSC {

const ClassInfo IntlListFormatConstructor::s_info = { "Function", &Base::s_info, &listFormatConstructorTable, nullptr, CREATE_METHOD_TABLE(IntlListFormatConstructor) };

/* Source for IntlListFormatConstructor.lut.h
@begin listFormatConstructorTable
  supportedLocalesOf             intlListFormatConstructorSupportedLocalesOf             DontEnum|Function 1
@end
*/

IntlListFormatConstructor* IntlListFormatConstructor::create(VM& vm, Structure* structure, IntlListFormatPrototype* listFormatPrototype)
{
    auto* constructor = new (NotNull, allocateCell<IntlListFormatConstructor>(vm.heap)) IntlListFormatConstructor(vm, structure);
    constructor->finishCreation(vm, listFormatPrototype);
    return constructor;
}

Structure* IntlListFormatConstructor::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(InternalFunctionType, StructureFlags), info());
}

static JSC_DECLARE_HOST_FUNCTION(callIntlListFormat);
static JSC_DECLARE_HOST_FUNCTION(constructIntlListFormat);

IntlListFormatConstructor::IntlListFormatConstructor(VM& vm, Structure* structure)
    : Base(vm, structure, callIntlListFormat, constructIntlListFormat)
{
}

void IntlListFormatConstructor::finishCreation(VM& vm, IntlListFormatPrototype* listFormatPrototype)
{
    Base::finishCreation(vm, 0, "ListFormat"_s, PropertyAdditionMode::WithoutStructureTransition);
    putDirectWithoutTransition(vm, vm.propertyNames->prototype, listFormatPrototype, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly);
    listFormatPrototype->putDirectWithoutTransition(vm, vm.propertyNames->constructor, this, static_cast<unsigned>(PropertyAttribute::DontEnum));
}

// https://tc39.es/proposal-intl-list-format/#sec-Intl.ListFormat
JSC_DEFINE_HOST_FUNCTION(constructIntlListFormat, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* newTarget = asObject(callFrame->newTarget());
    Structure* structure = JSC_GET_DERIVED_STRUCTURE(vm, listFormatStructure, newTarget, callFrame->jsCallee());
    RETURN_IF_EXCEPTION(scope, { });

    IntlListFormat* listFormat = IntlListFormat::create(vm, structure);
    ASSERT(listFormat);

    scope.release();
    listFormat->initializeListFormat(globalObject, callFrame->argument(0), callFrame->argument(1));
    return JSValue::encode(listFormat);
}

// https://tc39.es/proposal-intl-list-format/#sec-Intl.ListFormat
JSC_DEFINE_HOST_FUNCTION(callIntlListFormat, (JSGlobalObject* globalObject, CallFrame*))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    return JSValue::encode(throwConstructorCannotBeCalledAsFunctionTypeError(globalObject, scope, "ListFormat"));
}

JSC_DEFINE_HOST_FUNCTION(intlListFormatConstructorSupportedLocalesOf, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    // Intl.ListFormat.supportedLocalesOf(locales [, options])
    // https://tc39.es/proposal-intl-list-format/#sec-Intl.ListFormat.supportedLocalesOf

    // 1. Let availableLocales be %ListFormat%.[[availableLocales]].
    const auto& availableLocales = intlListFormatAvailableLocales();

    // 2. Let requestedLocales be CanonicalizeLocaleList(locales).
    Vector<String> requestedLocales = canonicalizeLocaleList(globalObject, callFrame->argument(0));
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    // 3. Return SupportedLocales(availableLocales, requestedLocales, options).
    RELEASE_AND_RETURN(scope, JSValue::encode(supportedLocales(globalObject, availableLocales, requestedLocales, callFrame->argument(1))));
}


} // namespace JSC

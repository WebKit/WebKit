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
#include "IntlDisplayNamesConstructor.h"

#include "IntlDisplayNames.h"
#include "IntlDisplayNamesPrototype.h"
#include "JSCInlines.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(IntlDisplayNamesConstructor);

static EncodedJSValue JSC_HOST_CALL IntlDisplayNamesConstructorSupportedLocalesOf(JSGlobalObject*, CallFrame*);

}

#include "IntlDisplayNamesConstructor.lut.h"

namespace JSC {

const ClassInfo IntlDisplayNamesConstructor::s_info = { "Function", &Base::s_info, &displayNamesConstructorTable, nullptr, CREATE_METHOD_TABLE(IntlDisplayNamesConstructor) };

/* Source for IntlDisplayNamesConstructor.lut.h
@begin displayNamesConstructorTable
  supportedLocalesOf             IntlDisplayNamesConstructorSupportedLocalesOf             DontEnum|Function 1
@end
*/

IntlDisplayNamesConstructor* IntlDisplayNamesConstructor::create(VM& vm, Structure* structure, IntlDisplayNamesPrototype* displayNamesPrototype)
{
    auto* constructor = new (NotNull, allocateCell<IntlDisplayNamesConstructor>(vm.heap)) IntlDisplayNamesConstructor(vm, structure);
    constructor->finishCreation(vm, displayNamesPrototype);
    return constructor;
}

Structure* IntlDisplayNamesConstructor::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(InternalFunctionType, StructureFlags), info());
}

static EncodedJSValue JSC_HOST_CALL callIntlDisplayNames(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL constructIntlDisplayNames(JSGlobalObject*, CallFrame*);

IntlDisplayNamesConstructor::IntlDisplayNamesConstructor(VM& vm, Structure* structure)
    : Base(vm, structure, callIntlDisplayNames, constructIntlDisplayNames)
{
}

void IntlDisplayNamesConstructor::finishCreation(VM& vm, IntlDisplayNamesPrototype* displayNamesPrototype)
{
    Base::finishCreation(vm, 2, "DisplayNames"_s, PropertyAdditionMode::WithoutStructureTransition);
    putDirectWithoutTransition(vm, vm.propertyNames->prototype, displayNamesPrototype, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly);
    displayNamesPrototype->putDirectWithoutTransition(vm, vm.propertyNames->constructor, this, static_cast<unsigned>(PropertyAttribute::DontEnum));
}

// https://tc39.es/ecma402/#sec-Intl.DisplayNames
static EncodedJSValue JSC_HOST_CALL constructIntlDisplayNames(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* newTarget = asObject(callFrame->newTarget());
    Structure* structure = newTarget == callFrame->jsCallee()
        ? globalObject->displayNamesStructure()
        : InternalFunction::createSubclassStructure(globalObject, newTarget, getFunctionRealm(vm, newTarget)->displayNamesStructure());
    RETURN_IF_EXCEPTION(scope, { });

    IntlDisplayNames* displayNames = IntlDisplayNames::create(vm, structure);
    ASSERT(displayNames);

    scope.release();
    displayNames->initializeDisplayNames(globalObject, callFrame->argument(0), callFrame->argument(1));
    return JSValue::encode(displayNames);
}

// https://tc39.es/ecma402/#sec-Intl.DisplayNames
static EncodedJSValue JSC_HOST_CALL callIntlDisplayNames(JSGlobalObject* globalObject, CallFrame*)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    return JSValue::encode(throwConstructorCannotBeCalledAsFunctionTypeError(globalObject, scope, "DisplayNames"));
}

EncodedJSValue JSC_HOST_CALL IntlDisplayNamesConstructorSupportedLocalesOf(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    // Intl.DisplayNames.supportedLocalesOf(locales [, options]) (ECMA-402 2.0)
    // https://tc39.es/proposal-intl-displaynames/#sec-Intl.DisplayNames.supportedLocalesOf

    // 1. Let availableLocales be %DisplayNames%.[[availableLocales]].
    const HashSet<String>& availableLocales = intlDisplayNamesAvailableLocales();

    // 2. Let requestedLocales be CanonicalizeLocaleList(locales).
    Vector<String> requestedLocales = canonicalizeLocaleList(globalObject, callFrame->argument(0));
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    // 3. Return SupportedLocales(availableLocales, requestedLocales, options).
    RELEASE_AND_RETURN(scope, JSValue::encode(supportedLocales(globalObject, availableLocales, requestedLocales, callFrame->argument(1))));
}


} // namespace JSC

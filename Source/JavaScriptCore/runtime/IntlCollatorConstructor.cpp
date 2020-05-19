/*
 * Copyright (C) 2015 Andy VanWagoner (andy@vanwagoner.family)
 * Copyright (C) 2015 Sukolsak Sakshuwong (sukolsak@gmail.com)
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
#include "IntlCollatorConstructor.h"

#include "IntlCollator.h"
#include "IntlCollatorPrototype.h"
#include "IntlObject.h"
#include "JSCInlines.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(IntlCollatorConstructor);

static EncodedJSValue JSC_HOST_CALL IntlCollatorConstructorFuncSupportedLocalesOf(JSGlobalObject*, CallFrame*);

}

#include "IntlCollatorConstructor.lut.h"

namespace JSC {

static EncodedJSValue JSC_HOST_CALL callIntlCollator(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL constructIntlCollator(JSGlobalObject*, CallFrame*);

const ClassInfo IntlCollatorConstructor::s_info = { "Function", &InternalFunction::s_info, &collatorConstructorTable, nullptr, CREATE_METHOD_TABLE(IntlCollatorConstructor) };

/* Source for IntlCollatorConstructor.lut.h
@begin collatorConstructorTable
  supportedLocalesOf             IntlCollatorConstructorFuncSupportedLocalesOf             DontEnum|Function 1
@end
*/

IntlCollatorConstructor* IntlCollatorConstructor::create(VM& vm, Structure* structure, IntlCollatorPrototype* collatorPrototype)
{
    IntlCollatorConstructor* constructor = new (NotNull, allocateCell<IntlCollatorConstructor>(vm.heap)) IntlCollatorConstructor(vm, structure);
    constructor->finishCreation(vm, collatorPrototype);
    return constructor;
}

Structure* IntlCollatorConstructor::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(InternalFunctionType, StructureFlags), info());
}

IntlCollatorConstructor::IntlCollatorConstructor(VM& vm, Structure* structure)
    : InternalFunction(vm, structure, callIntlCollator, constructIntlCollator)
{
}

void IntlCollatorConstructor::finishCreation(VM& vm, IntlCollatorPrototype* collatorPrototype)
{
    Base::finishCreation(vm, "Collator"_s, NameAdditionMode::WithoutStructureTransition);
    putDirectWithoutTransition(vm, vm.propertyNames->prototype, collatorPrototype, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly);
    putDirectWithoutTransition(vm, vm.propertyNames->length, jsNumber(0), PropertyAttribute::ReadOnly | PropertyAttribute::DontEnum);
    collatorPrototype->putDirectWithoutTransition(vm, vm.propertyNames->constructor, this, static_cast<unsigned>(PropertyAttribute::DontEnum));
}

static EncodedJSValue JSC_HOST_CALL constructIntlCollator(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    // 10.1.2 Intl.Collator ([locales [, options]]) (ECMA-402 2.0)
    // 1. If NewTarget is undefined, let newTarget be the active function object, else let newTarget be NewTarget.
    // 2. Let collator be OrdinaryCreateFromConstructor(newTarget, %CollatorPrototype%).
    // 3. ReturnIfAbrupt(collator).
    JSObject* newTarget = asObject(callFrame->newTarget());
    Structure* structure = newTarget == callFrame->jsCallee()
        ? globalObject->collatorStructure()
        : InternalFunction::createSubclassStructure(globalObject, newTarget, getFunctionRealm(vm, newTarget)->collatorStructure());
    RETURN_IF_EXCEPTION(scope, { });

    IntlCollator* collator = IntlCollator::create(vm, structure);
    ASSERT(collator);

    // 4. Return InitializeCollator(collator, locales, options).
    scope.release();
    collator->initializeCollator(globalObject, callFrame->argument(0), callFrame->argument(1));
    return JSValue::encode(collator);
}

static EncodedJSValue JSC_HOST_CALL callIntlCollator(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    // 10.1.2 Intl.Collator ([locales [, options]]) (ECMA-402 2.0)
    // 1. If NewTarget is undefined, let newTarget be the active function object, else let newTarget be NewTarget.
    // NewTarget is always undefined when called as a function.

    VM& vm = globalObject->vm();
    // Collator does not require the workaround for ECMA-402 1.0 compatibility.
    // https://bugs.webkit.org/show_bug.cgi?id=153679

    // 2. Let collator be OrdinaryCreateFromConstructor(newTarget, %CollatorPrototype%).
    // 3. ReturnIfAbrupt(collator).
    IntlCollator* collator = IntlCollator::create(vm, globalObject->collatorStructure());
    ASSERT(collator);

    // 4. Return InitializeCollator(collator, locales, options).
    collator->initializeCollator(globalObject, callFrame->argument(0), callFrame->argument(1));
    return JSValue::encode(collator);
}

EncodedJSValue JSC_HOST_CALL IntlCollatorConstructorFuncSupportedLocalesOf(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    // 10.2.2 Intl.Collator.supportedLocalesOf(locales [, options]) (ECMA-402 2.0)

    // 1. Let requestedLocales be CanonicalizeLocaleList(locales).
    Vector<String> requestedLocales = canonicalizeLocaleList(globalObject, callFrame->argument(0));

    // 2. ReturnIfAbrupt(requestedLocales).
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    // 3. Return SupportedLocales(%Collator%.[[availableLocales]], requestedLocales, options).
    RELEASE_AND_RETURN(scope, JSValue::encode(supportedLocales(globalObject, intlCollatorAvailableLocales(), requestedLocales, callFrame->argument(1))));
}

} // namespace JSC

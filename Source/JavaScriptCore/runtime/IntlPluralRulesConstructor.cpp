/*
 * Copyright (C) 2018 Andy VanWagoner (andy@vanwagoner.family)
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
#include "IntlPluralRulesConstructor.h"

#if ENABLE(INTL)

#include "Error.h"
#include "IntlObject.h"
#include "IntlPluralRules.h"
#include "IntlPluralRulesPrototype.h"
#include "JSCInlines.h"
#include "Lookup.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(IntlPluralRulesConstructor);

static EncodedJSValue JSC_HOST_CALL IntlPluralRulesConstructorFuncSupportedLocalesOf(ExecState*);

}

#include "IntlPluralRulesConstructor.lut.h"

namespace JSC {

const ClassInfo IntlPluralRulesConstructor::s_info = { "Function", &InternalFunction::s_info, &pluralRulesConstructorTable, nullptr, CREATE_METHOD_TABLE(IntlPluralRulesConstructor) };

/* Source for IntlPluralRulesConstructor.lut.h
@begin pluralRulesConstructorTable
  supportedLocalesOf             IntlPluralRulesConstructorFuncSupportedLocalesOf             DontEnum|Function 1
@end
*/

IntlPluralRulesConstructor* IntlPluralRulesConstructor::create(VM& vm, Structure* structure, IntlPluralRulesPrototype* pluralRulesPrototype, Structure* pluralRulesStructure)
{
    IntlPluralRulesConstructor* constructor = new (NotNull, allocateCell<IntlPluralRulesConstructor>(vm.heap)) IntlPluralRulesConstructor(vm, structure);
    constructor->finishCreation(vm, pluralRulesPrototype, pluralRulesStructure);
    return constructor;
}

Structure* IntlPluralRulesConstructor::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(InternalFunctionType, StructureFlags), info());
}

static EncodedJSValue JSC_HOST_CALL callIntlPluralRules(ExecState*);
static EncodedJSValue JSC_HOST_CALL constructIntlPluralRules(ExecState*);

IntlPluralRulesConstructor::IntlPluralRulesConstructor(VM& vm, Structure* structure)
    : InternalFunction(vm, structure, callIntlPluralRules, constructIntlPluralRules)
{
}

void IntlPluralRulesConstructor::finishCreation(VM& vm, IntlPluralRulesPrototype* pluralRulesPrototype, Structure* pluralRulesStructure)
{
    Base::finishCreation(vm, "PluralRules"_s);
    putDirectWithoutTransition(vm, vm.propertyNames->prototype, pluralRulesPrototype, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly);
    putDirectWithoutTransition(vm, vm.propertyNames->length, jsNumber(0), PropertyAttribute::ReadOnly | PropertyAttribute::DontEnum);
    pluralRulesPrototype->putDirectWithoutTransition(vm, vm.propertyNames->constructor, this, static_cast<unsigned>(PropertyAttribute::DontEnum));
    m_pluralRulesStructure.set(vm, this, pluralRulesStructure);
}

static EncodedJSValue JSC_HOST_CALL constructIntlPluralRules(ExecState* state)
{
    VM& vm = state->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // 13.2.1 Intl.PluralRules ([ locales [ , options ] ])
    // https://tc39.github.io/ecma402/#sec-intl.pluralrules
    Structure* structure = InternalFunction::createSubclassStructure(state, state->newTarget(), jsCast<IntlPluralRulesConstructor*>(state->jsCallee())->pluralRulesStructure());
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    IntlPluralRules* pluralRules = IntlPluralRules::create(vm, structure);
    ASSERT(pluralRules);

    scope.release();
    pluralRules->initializePluralRules(*state, state->argument(0), state->argument(1));
    return JSValue::encode(pluralRules);
}

static EncodedJSValue JSC_HOST_CALL callIntlPluralRules(ExecState* state)
{
    VM& vm = state->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // 13.2.1 Intl.PluralRules ([ locales [ , options ] ])
    // https://tc39.github.io/ecma402/#sec-intl.pluralrules
    return JSValue::encode(throwConstructorCannotBeCalledAsFunctionTypeError(state, scope, "PluralRules"));
}

EncodedJSValue JSC_HOST_CALL IntlPluralRulesConstructorFuncSupportedLocalesOf(ExecState* state)
{
    VM& vm = state->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // 13.3.2 Intl.PluralRules.supportedLocalesOf (locales [, options ])
    // https://tc39.github.io/ecma402/#sec-intl.pluralrules.supportedlocalesof
    JSGlobalObject* globalObject = state->jsCallee()->globalObject(vm);
    const HashSet<String> availableLocales = globalObject->intlNumberFormatAvailableLocales();

    Vector<String> requestedLocales = canonicalizeLocaleList(*state, state->argument(0));
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    RELEASE_AND_RETURN(scope, JSValue::encode(supportedLocales(*state, availableLocales, requestedLocales, state->argument(1))));
}

void IntlPluralRulesConstructor::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    IntlPluralRulesConstructor* thisObject = jsCast<IntlPluralRulesConstructor*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());

    Base::visitChildren(thisObject, visitor);

    visitor.append(thisObject->m_pluralRulesStructure);
}

} // namespace JSC

#endif // ENABLE(INTL)

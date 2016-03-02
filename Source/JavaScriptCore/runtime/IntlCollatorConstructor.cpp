/*
 * Copyright (C) 2015 Andy VanWagoner (thetalecrafter@gmail.com)
 * Copyright (C) 2015 Sukolsak Sakshuwong (sukolsak@gmail.com)
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

#if ENABLE(INTL)

#include "Error.h"
#include "IntlCollator.h"
#include "IntlCollatorPrototype.h"
#include "IntlObject.h"
#include "JSCJSValueInlines.h"
#include "JSCellInlines.h"
#include "Lookup.h"
#include "SlotVisitorInlines.h"
#include "StructureInlines.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(IntlCollatorConstructor);

static EncodedJSValue JSC_HOST_CALL IntlCollatorConstructorFuncSupportedLocalesOf(ExecState*);

}

#include "IntlCollatorConstructor.lut.h"

namespace JSC {

const ClassInfo IntlCollatorConstructor::s_info = { "Function", &InternalFunction::s_info, &collatorConstructorTable, CREATE_METHOD_TABLE(IntlCollatorConstructor) };

/* Source for IntlCollatorConstructor.lut.h
@begin collatorConstructorTable
  supportedLocalesOf             IntlCollatorConstructorFuncSupportedLocalesOf             DontEnum|Function 1
@end
*/

IntlCollatorConstructor* IntlCollatorConstructor::create(VM& vm, Structure* structure, IntlCollatorPrototype* collatorPrototype, Structure* collatorStructure)
{
    IntlCollatorConstructor* constructor = new (NotNull, allocateCell<IntlCollatorConstructor>(vm.heap)) IntlCollatorConstructor(vm, structure);
    constructor->finishCreation(vm, collatorPrototype, collatorStructure);
    return constructor;
}

Structure* IntlCollatorConstructor::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

IntlCollatorConstructor::IntlCollatorConstructor(VM& vm, Structure* structure)
    : InternalFunction(vm, structure)
{
}

void IntlCollatorConstructor::finishCreation(VM& vm, IntlCollatorPrototype* collatorPrototype, Structure* collatorStructure)
{
    Base::finishCreation(vm, ASCIILiteral("Collator"));
    putDirectWithoutTransition(vm, vm.propertyNames->prototype, collatorPrototype, DontEnum | DontDelete | ReadOnly);
    putDirectWithoutTransition(vm, vm.propertyNames->length, jsNumber(0), ReadOnly | DontEnum | DontDelete);
    m_collatorStructure.set(vm, this, collatorStructure);
}

static EncodedJSValue JSC_HOST_CALL constructIntlCollator(ExecState* state)
{
    // 10.1.2 Intl.Collator ([locales [, options]]) (ECMA-402 2.0)
    // 1. If NewTarget is undefined, let newTarget be the active function object, else let newTarget be NewTarget.
    JSValue newTarget = state->newTarget();
    if (newTarget.isUndefined())
        newTarget = state->callee();

    // 2. Let collator be OrdinaryCreateFromConstructor(newTarget, %CollatorPrototype%).
    VM& vm = state->vm();
    IntlCollator* collator = IntlCollator::create(vm, jsCast<IntlCollatorConstructor*>(state->callee()));
    if (collator && !jsDynamicCast<IntlCollatorConstructor*>(newTarget)) {
        JSValue proto = asObject(newTarget)->getDirect(vm, vm.propertyNames->prototype);
        asObject(collator)->setPrototypeOfInline(vm, state, proto);
        if (vm.exception())
            return JSValue::encode(JSValue());
    }

    // 3. ReturnIfAbrupt(collator).
    ASSERT(collator);

    // 4. Return InitializeCollator(collator, locales, options).
    JSValue locales = state->argument(0);
    JSValue options = state->argument(1);
    collator->initializeCollator(*state, locales, options);
    return JSValue::encode(collator);
}

static EncodedJSValue JSC_HOST_CALL callIntlCollator(ExecState* state)
{
    // 10.1.2 Intl.Collator ([locales [, options]]) (ECMA-402 2.0)
    // 1. If NewTarget is undefined, let newTarget be the active function object, else let newTarget be NewTarget.
    // NewTarget is always undefined when called as a function.

    // 2. Let collator be OrdinaryCreateFromConstructor(newTarget, %CollatorPrototype%).
    VM& vm = state->vm();
    IntlCollator* collator = IntlCollator::create(vm, jsCast<IntlCollatorConstructor*>(state->callee()));

    // 3. ReturnIfAbrupt(collator).
    ASSERT(collator);

    // 4. Return InitializeCollator(collator, locales, options).
    JSValue locales = state->argument(0);
    JSValue options = state->argument(1);
    collator->initializeCollator(*state, locales, options);
    return JSValue::encode(collator);
}

ConstructType IntlCollatorConstructor::getConstructData(JSCell*, ConstructData& constructData)
{
    constructData.native.function = constructIntlCollator;
    return ConstructTypeHost;
}

CallType IntlCollatorConstructor::getCallData(JSCell*, CallData& callData)
{
    callData.native.function = callIntlCollator;
    return CallTypeHost;
}

bool IntlCollatorConstructor::getOwnPropertySlot(JSObject* object, ExecState* state, PropertyName propertyName, PropertySlot& slot)
{
    return getStaticFunctionSlot<InternalFunction>(state, collatorConstructorTable, jsCast<IntlCollatorConstructor*>(object), propertyName, slot);
}

EncodedJSValue JSC_HOST_CALL IntlCollatorConstructorFuncSupportedLocalesOf(ExecState* state)
{
    // 10.2.2 Intl.Collator.supportedLocalesOf(locales [, options]) (ECMA-402 2.0)

    // 1. Let requestedLocales be CanonicalizeLocaleList(locales).
    Vector<String> requestedLocales = canonicalizeLocaleList(*state, state->argument(0));

    // 2. ReturnIfAbrupt(requestedLocales).
    if (state->hadException())
        return JSValue::encode(jsUndefined());

    // 3. Return SupportedLocales(%Collator%.[[availableLocales]], requestedLocales, options).
    JSGlobalObject* globalObject = state->callee()->globalObject();
    return JSValue::encode(supportedLocales(*state, globalObject->intlCollatorAvailableLocales(), requestedLocales, state->argument(1)));
}

void IntlCollatorConstructor::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    IntlCollatorConstructor* thisObject = jsCast<IntlCollatorConstructor*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());

    Base::visitChildren(thisObject, visitor);

    visitor.append(&thisObject->m_collatorStructure);
}

} // namespace JSC

#endif // ENABLE(INTL)

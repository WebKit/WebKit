/*
 * Copyright (C) 2015 Andy VanWagoner (thetalecrafter@gmail.com)
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

#if ENABLE(INTL)

#include "Error.h"
#include "IntlDateTimeFormat.h"
#include "IntlDateTimeFormatPrototype.h"
#include "IntlObject.h"
#include "JSCJSValueInlines.h"
#include "JSCellInlines.h"
#include "Lookup.h"
#include "SlotVisitorInlines.h"
#include "StructureInlines.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(IntlDateTimeFormatConstructor);

static EncodedJSValue JSC_HOST_CALL IntlDateTimeFormatConstructorFuncSupportedLocalesOf(ExecState*);

}

#include "IntlDateTimeFormatConstructor.lut.h"

namespace JSC {

const ClassInfo IntlDateTimeFormatConstructor::s_info = { "Function", &InternalFunction::s_info, &dateTimeFormatConstructorTable, CREATE_METHOD_TABLE(IntlDateTimeFormatConstructor) };

/* Source for IntlDateTimeFormatConstructor.lut.h
@begin dateTimeFormatConstructorTable
  supportedLocalesOf             IntlDateTimeFormatConstructorFuncSupportedLocalesOf             DontEnum|Function 1
@end
*/

IntlDateTimeFormatConstructor* IntlDateTimeFormatConstructor::create(VM& vm, Structure* structure, IntlDateTimeFormatPrototype* dateTimeFormatPrototype, Structure* dateTimeFormatStructure)
{
    IntlDateTimeFormatConstructor* constructor = new (NotNull, allocateCell<IntlDateTimeFormatConstructor>(vm.heap)) IntlDateTimeFormatConstructor(vm, structure);
    constructor->finishCreation(vm, dateTimeFormatPrototype, dateTimeFormatStructure);
    return constructor;
}

Structure* IntlDateTimeFormatConstructor::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

IntlDateTimeFormatConstructor::IntlDateTimeFormatConstructor(VM& vm, Structure* structure)
    : InternalFunction(vm, structure)
{
}

void IntlDateTimeFormatConstructor::finishCreation(VM& vm, IntlDateTimeFormatPrototype* dateTimeFormatPrototype, Structure* dateTimeFormatStructure)
{
    Base::finishCreation(vm, ASCIILiteral("DateTimeFormat"));
    putDirectWithoutTransition(vm, vm.propertyNames->prototype, dateTimeFormatPrototype, DontEnum | DontDelete | ReadOnly);
    putDirectWithoutTransition(vm, vm.propertyNames->length, jsNumber(0), ReadOnly | DontEnum | DontDelete);
    m_dateTimeFormatStructure.set(vm, this, dateTimeFormatStructure);
}

static EncodedJSValue JSC_HOST_CALL constructIntlDateTimeFormat(ExecState* state)
{
    // 12.1.2 Intl.DateTimeFormat ([locales [, options]]) (ECMA-402 2.0)
    // 1. If NewTarget is undefined, let newTarget be the active function object, else let newTarget be NewTarget.
    JSValue newTarget = state->newTarget();
    if (newTarget.isUndefined())
        newTarget = state->callee();

    // 2. Let dateTimeFormat be OrdinaryCreateFromConstructor(newTarget, %DateTimeFormatPrototype%).
    VM& vm = state->vm();
    IntlDateTimeFormat* dateTimeFormat = IntlDateTimeFormat::create(vm, jsCast<IntlDateTimeFormatConstructor*>(state->callee()));
    if (dateTimeFormat && !jsDynamicCast<IntlDateTimeFormatConstructor*>(newTarget)) {
        JSValue proto = asObject(newTarget)->getDirect(vm, vm.propertyNames->prototype);
        asObject(dateTimeFormat)->setPrototype(vm, state, proto);
        if (vm.exception())
            return JSValue::encode(JSValue());
    }

    // 3. ReturnIfAbrupt(dateTimeFormat).
    ASSERT(dateTimeFormat);

    // 4. Return InitializeDateTimeFormat(dateTimeFormat, locales, options).
    JSValue locales = state->argument(0);
    JSValue options = state->argument(1);
    dateTimeFormat->initializeDateTimeFormat(*state, locales, options);
    return JSValue::encode(dateTimeFormat);
}

static EncodedJSValue JSC_HOST_CALL callIntlDateTimeFormat(ExecState* state)
{
    // 12.1.2 Intl.DateTimeFormat ([locales [, options]]) (ECMA-402 2.0)
    // 1. If NewTarget is undefined, let newTarget be the active function object, else let newTarget be NewTarget.
    // NewTarget is always undefined when called as a function.

    // 2. Let dateTimeFormat be OrdinaryCreateFromConstructor(newTarget, %DateTimeFormatPrototype%).
    VM& vm = state->vm();
    IntlDateTimeFormat* dateTimeFormat = IntlDateTimeFormat::create(vm, jsCast<IntlDateTimeFormatConstructor*>(state->callee()));

    // 3. ReturnIfAbrupt(dateTimeFormat).
    ASSERT(dateTimeFormat);

    // 4. Return InitializeDateTimeFormat(dateTimeFormat, locales, options).
    JSValue locales = state->argument(0);
    JSValue options = state->argument(1);
    dateTimeFormat->initializeDateTimeFormat(*state, locales, options);
    return JSValue::encode(dateTimeFormat);
}

ConstructType IntlDateTimeFormatConstructor::getConstructData(JSCell*, ConstructData& constructData)
{
    constructData.native.function = constructIntlDateTimeFormat;
    return ConstructType::Host;
}

CallType IntlDateTimeFormatConstructor::getCallData(JSCell*, CallData& callData)
{
    callData.native.function = callIntlDateTimeFormat;
    return CallType::Host;
}

bool IntlDateTimeFormatConstructor::getOwnPropertySlot(JSObject* object, ExecState* state, PropertyName propertyName, PropertySlot& slot)
{
    return getStaticFunctionSlot<InternalFunction>(state, dateTimeFormatConstructorTable, jsCast<IntlDateTimeFormatConstructor*>(object), propertyName, slot);
}

EncodedJSValue JSC_HOST_CALL IntlDateTimeFormatConstructorFuncSupportedLocalesOf(ExecState* state)
{
    // 12.2.2 Intl.DateTimeFormat.supportedLocalesOf(locales [, options]) (ECMA-402 2.0)

    // 1. Let availableLocales be %DateTimeFormat%.[[availableLocales]].
    JSGlobalObject* globalObject = state->callee()->globalObject();
    const HashSet<String> availableLocales = globalObject->intlDateTimeFormatAvailableLocales();

    // 2. Let requestedLocales be CanonicalizeLocaleList(locales).
    Vector<String> requestedLocales = canonicalizeLocaleList(*state, state->argument(0));
    if (state->hadException())
        return JSValue::encode(jsUndefined());

    // 3. Return SupportedLocales(availableLocales, requestedLocales, options).
    return JSValue::encode(supportedLocales(*state, availableLocales, requestedLocales, state->argument(1)));
}

void IntlDateTimeFormatConstructor::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    IntlDateTimeFormatConstructor* thisObject = jsCast<IntlDateTimeFormatConstructor*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());

    Base::visitChildren(thisObject, visitor);

    visitor.append(&thisObject->m_dateTimeFormatStructure);
}

} // namespace JSC

#endif // ENABLE(INTL)

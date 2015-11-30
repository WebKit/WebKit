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
#include "ObjectConstructor.h"
#include "SlotVisitorInlines.h"
#include "StructureInlines.h"
#include <unicode/ucol.h>

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

static Vector<String> sortLocaleData(const String& locale, const String& key)
{
    // 9.1 Internal slots of Service Constructors & 10.2.3 Internal slots (ECMA-402 2.0)
    Vector<String> keyLocaleData;
    if (key == "co") {
        // 10.2.3 "The first element of [[sortLocaleData]][locale].co and [[searchLocaleData]][locale].co must be null for all locale values."
        keyLocaleData.append(String());

        UErrorCode status = U_ZERO_ERROR;
        UEnumeration* enumeration = ucol_getKeywordValuesForLocale("collation", locale.utf8().data(), TRUE, &status);
        if (U_SUCCESS(status)) {
            const char* keywordValue;
            while ((keywordValue = uenum_next(enumeration, nullptr, &status)) && U_SUCCESS(status)) {
                String collation(keywordValue);

                // 10.2.3 "The values "standard" and "search" must not be used as elements in any [[sortLocaleData]][locale].co and [[searchLocaleData]][locale].co array."
                if (collation == "standard" || collation == "search")
                    continue;

                // Map keyword values to BCP 47 equivalents.
                if (collation == "dictionary")
                    collation = ASCIILiteral("dict");
                else if (collation == "gb2312han")
                    collation = ASCIILiteral("gb2312");
                else if (collation == "phonebook")
                    collation = ASCIILiteral("phonebk");
                else if (collation == "traditional")
                    collation = ASCIILiteral("trad");

                keyLocaleData.append(collation);
            }
            uenum_close(enumeration);
        }
    } else if (key == "kn") {
        keyLocaleData.append(ASCIILiteral("false"));
        keyLocaleData.append(ASCIILiteral("true"));
    } else
        ASSERT_NOT_REACHED();
    return keyLocaleData;
}

static Vector<String> searchLocaleData(const String&, const String& key)
{
    // 9.1 Internal slots of Service Constructors & 10.2.3 Internal slots (ECMA-402 2.0)
    Vector<String> keyLocaleData;
    if (key == "co") {
        // 10.2.3 "The first element of [[sortLocaleData]][locale].co and [[searchLocaleData]][locale].co must be null for all locale values."
        keyLocaleData.append(String());
    } else if (key == "kn") {
        keyLocaleData.append(ASCIILiteral("false"));
        keyLocaleData.append(ASCIILiteral("true"));
    } else if (key == "sensitivity") {
        // 10.2.3 "[[searchLocaleData]][locale] must have a sensitivity property with a String value equal to "base", "accent", "case", or "variant" for all locale values."
        keyLocaleData.append("variant");
    } else
        ASSERT_NOT_REACHED();
    return keyLocaleData;
}

static IntlCollator* initializeCollator(ExecState& state, IntlCollator& collator, JSValue locales, JSValue optionsValue)
{
    // 10.1.1 InitializeCollator (collator, locales, options) (ECMA-402 2.0)

    // 1. If collator has an [[initializedIntlObject]] internal slot with value true, throw a TypeError exception.
    // 2. Set collator.[[initializedIntlObject]] to true.

    // 3. Let requestedLocales be CanonicalizeLocaleList(locales).
    Vector<String> requestedLocales = canonicalizeLocaleList(state, locales);
    // 4. ReturnIfAbrupt(requestedLocales).
    if (state.hadException())
        return nullptr;

    // 5. If options is undefined, then
    JSObject* options;
    if (optionsValue.isUndefined()) {
        // a. Let options be ObjectCreate(%ObjectPrototype%).
        options = constructEmptyObject(&state);
    } else { // 6. Else
        // a. Let options be ToObject(options).
        options = optionsValue.toObject(&state);
        // b. ReturnIfAbrupt(options).
        if (state.hadException())
            return nullptr;
    }

    // 7. Let u be GetOption(options, "usage", "string", «"sort", "search"», "sort").
    const HashSet<String> usages({ ASCIILiteral("sort"), ASCIILiteral("search") });
    String usage = intlStringOption(state, options, state.vm().propertyNames->usage, usages, "usage must be either \"sort\" or \"search\"", ASCIILiteral("sort"));
    // 8. ReturnIfAbrupt(u).
    if (state.hadException())
        return nullptr;
    // 9. Set collator.[[usage]] to u.
    collator.setUsage(usage);

    // 10. If u is "sort", then
    // a. Let localeData be the value of %Collator%.[[sortLocaleData]];
    // 11. Else
    // a. Let localeData be the value of %Collator%.[[searchLocaleData]].
    Vector<String> (*localeData)(const String&, const String&);
    if (usage == "sort")
        localeData = sortLocaleData;
    else
        localeData = searchLocaleData;

    // 12. Let opt be a new Record.
    HashMap<String, String> opt;

    // 13. Let matcher be GetOption(options, "localeMatcher", "string", «"lookup", "best fit"», "best fit").
    const HashSet<String> matchers({ ASCIILiteral("lookup"), ASCIILiteral("best fit") });
    String matcher = intlStringOption(state, options, state.vm().propertyNames->localeMatcher, matchers, "localeMatcher must be either \"lookup\" or \"best fit\"", ASCIILiteral("best fit"));
    // 14. ReturnIfAbrupt(matcher).
    if (state.hadException())
        return nullptr;
    // 15. Set opt.[[localeMatcher]] to matcher.
    opt.set(ASCIILiteral("localeMatcher"), matcher);

    // 16. For each row in Table 1, except the header row, do:
    // a. Let key be the name given in the Key column of the row.
    // b. Let prop be the name given in the Property column of the row.
    // c. Let type be the string given in the Type column of the row.
    // d. Let list be a List containing the Strings given in the Values column of the row, or undefined if no strings are given.
    // e. Let value be GetOption(options, prop, type, list, undefined).
    // f. ReturnIfAbrupt(value).
    // g. If the string given in the Type column of the row is "boolean" and value is not undefined, then
    //    i. Let value be ToString(value).
    //    ii. ReturnIfAbrupt(value).
    // h. Set opt.[[<key>]] to value.
    {
        String numericString;
        bool usesFallback;
        bool numeric = intlBooleanOption(state, options, state.vm().propertyNames->numeric, usesFallback);
        if (state.hadException())
            return nullptr;
        if (!usesFallback)
            numericString = ASCIILiteral(numeric ? "true" : "false");
        opt.set(ASCIILiteral("kn"), numericString);
    }
    {
        const HashSet<String> caseFirsts({ ASCIILiteral("upper"), ASCIILiteral("lower"), ASCIILiteral("false") });
        String caseFirst = intlStringOption(state, options, state.vm().propertyNames->caseFirst, caseFirsts, "caseFirst must be either \"upper\", \"lower\", or \"false\"", String());
        if (state.hadException())
            return nullptr;
        opt.set(ASCIILiteral("kf"), caseFirst);
    }

    // 17. Let relevantExtensionKeys be the value of %Collator%.[[relevantExtensionKeys]].
    // FIXME: Implement kf (caseFirst).
    const Vector<String> relevantExtensionKeys { ASCIILiteral("co"), ASCIILiteral("kn") };

    // 18. Let r be ResolveLocale(%Collator%.[[availableLocales]], requestedLocales, opt, relevantExtensionKeys, localeData).
    const HashSet<String>& availableLocales = state.callee()->globalObject()->intlCollatorAvailableLocales();
    HashMap<String, String> result = resolveLocale(availableLocales, requestedLocales, opt, relevantExtensionKeys, localeData);

    // 19. Set collator.[[locale]] to the value of r.[[locale]].
    collator.setLocale(result.get(ASCIILiteral("locale")));

    // 20. Let k be 0.
    // 21. Let lenValue be Get(relevantExtensionKeys, "length").
    // 22. Let len be ToLength(lenValue).
    // 23. Repeat while k < len:
    // a. Let Pk be ToString(k).
    // b. Let key be Get(relevantExtensionKeys, Pk).
    // c. ReturnIfAbrupt(key).
    // d. If key is "co", then
    //    i. Let property be "collation".
    //    ii. Let value be the value of r.[[co]].
    //    iii. If value is null, let value be "default".
    // e. Else use the row of Table 1 that contains the value of key in the Key column:
    //    i. Let property be the name given in the Property column of the row.
    //    ii. Let value be the value of r.[[<key>]].
    //    iii. If the name given in the Type column of the row is "boolean", let value be the result of comparing value with "true".
    // f. Set collator.[[<property>]] to value.
    // g. Increase k by 1.
    ASSERT(relevantExtensionKeys.size() == 2);
    {
        ASSERT(relevantExtensionKeys[0] == "co");
        const String& value = result.get(ASCIILiteral("co"));
        collator.setCollation(value.isNull() ? ASCIILiteral("default") : value);
    }
    {
        ASSERT(relevantExtensionKeys[1] == "kn");
        const String& value = result.get(ASCIILiteral("kn"));
        collator.setNumeric(value == "true");
    }

    // 24. Let s be GetOption(options, "sensitivity", "string", «"base", "accent", "case", "variant"», undefined).
    const HashSet<String> sensitivities({ ASCIILiteral("base"), ASCIILiteral("accent"), ASCIILiteral("case"), ASCIILiteral("variant") });
    String sensitivity = intlStringOption(state, options, state.vm().propertyNames->sensitivity, sensitivities, "sensitivity must be either \"base\", \"accent\", \"case\", or \"variant\"", String());
    // 25. ReturnIfAbrupt(s).
    if (state.hadException())
        return nullptr;
    // 26. If s is undefined, then
    if (sensitivity.isNull()) {
        // a. If u is "sort", then let s be "variant".
        if (usage == "sort")
            sensitivity = ASCIILiteral("variant");
        else {
            // b. Else
            //    i. Let dataLocale be the value of r.[[dataLocale]].
            //    ii. Let dataLocaleData be Get(localeData, dataLocale).
            //    iii. Let s be Get(dataLocaleData, "sensitivity").
            const String& dataLocale = result.get(ASCIILiteral("dataLocale"));
            sensitivity = localeData(dataLocale, ASCIILiteral("sensitivity"))[0];
        }
    }
    // 27. Set collator.[[sensitivity]] to s.
    collator.setSensitivity(sensitivity);

    // 28. Let ip be GetOption(options, "ignorePunctuation", "boolean", undefined, false).
    bool usesFallback;
    bool ignorePunctuation = intlBooleanOption(state, options, state.vm().propertyNames->ignorePunctuation, usesFallback);
    if (usesFallback)
        ignorePunctuation = false;
    // 29. ReturnIfAbrupt(ip).
    if (state.hadException())
        return nullptr;
    // 30. Set collator.[[ignorePunctuation]] to ip.
    collator.setIgnorePunctuation(ignorePunctuation);

    // 31. Set collator.[[boundCompare]] to undefined.
    // 32. Set collator.[[initializedCollator]] to true.
    // 33. Return collator.
    return &collator;
}

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
        asObject(collator)->setPrototypeWithCycleCheck(state, proto);
    }

    // 3. ReturnIfAbrupt(collator).
    ASSERT(collator);

    // 4. Return InitializeCollator(collator, locales, options).
    JSValue locales = state->argument(0);
    JSValue options = state->argument(1);
    return JSValue::encode(initializeCollator(*state, *collator, locales, options));
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
    return JSValue::encode(initializeCollator(*state, *collator, locales, options));
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

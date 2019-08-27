/*
 * Copyright (C) 2015 Andy VanWagoner (andy@vanwagoner.family)
 * Copyright (C) 2015 Sukolsak Sakshuwong (sukolsak@gmail.com)
 * Copyright (C) 2016-2019 Apple Inc. All Rights Reserved.
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
#include "IntlCollator.h"

#if ENABLE(INTL)

#include "CatchScope.h"
#include "Error.h"
#include "IntlCollatorConstructor.h"
#include "IntlObject.h"
#include "JSBoundFunction.h"
#include "JSCInlines.h"
#include "ObjectConstructor.h"
#include "SlotVisitorInlines.h"
#include "StructureInlines.h"
#include <unicode/ucol.h>
#include <wtf/unicode/Collator.h>

namespace JSC {

const ClassInfo IntlCollator::s_info = { "Object", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(IntlCollator) };

static const char* const relevantCollatorExtensionKeys[3] = { "co", "kn", "kf" };
static const size_t indexOfExtensionKeyCo = 0;
static const size_t indexOfExtensionKeyKn = 1;
static const size_t indexOfExtensionKeyKf = 2;

void IntlCollator::UCollatorDeleter::operator()(UCollator* collator) const
{
    if (collator)
        ucol_close(collator);
}

IntlCollator* IntlCollator::create(VM& vm, Structure* structure)
{
    IntlCollator* format = new (NotNull, allocateCell<IntlCollator>(vm.heap)) IntlCollator(vm, structure);
    format->finishCreation(vm);
    return format;
}

Structure* IntlCollator::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

IntlCollator::IntlCollator(VM& vm, Structure* structure)
    : JSDestructibleObject(vm, structure)
{
}

void IntlCollator::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    ASSERT(inherits(vm, info()));
}

void IntlCollator::destroy(JSCell* cell)
{
    static_cast<IntlCollator*>(cell)->IntlCollator::~IntlCollator();
}

void IntlCollator::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    IntlCollator* thisObject = jsCast<IntlCollator*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());

    Base::visitChildren(thisObject, visitor);

    visitor.append(thisObject->m_boundCompare);
}

static Vector<String> sortLocaleData(const String& locale, size_t keyIndex)
{
    // 9.1 Internal slots of Service Constructors & 10.2.3 Internal slots (ECMA-402 2.0)
    Vector<String> keyLocaleData;
    switch (keyIndex) {
    case indexOfExtensionKeyCo: {
        // 10.2.3 "The first element of [[sortLocaleData]][locale].co and [[searchLocaleData]][locale].co must be null for all locale values."
        keyLocaleData.append({ });

        UErrorCode status = U_ZERO_ERROR;
        UEnumeration* enumeration = ucol_getKeywordValuesForLocale("collation", locale.utf8().data(), false, &status);
        if (U_SUCCESS(status)) {
            const char* collation;
            while ((collation = uenum_next(enumeration, nullptr, &status)) && U_SUCCESS(status)) {
                // 10.2.3 "The values "standard" and "search" must not be used as elements in any [[sortLocaleData]][locale].co and [[searchLocaleData]][locale].co array."
                if (!strcmp(collation, "standard") || !strcmp(collation, "search"))
                    continue;

                // Map keyword values to BCP 47 equivalents.
                if (!strcmp(collation, "dictionary"))
                    collation = "dict";
                else if (!strcmp(collation, "gb2312han"))
                    collation = "gb2312";
                else if (!strcmp(collation, "phonebook"))
                    collation = "phonebk";
                else if (!strcmp(collation, "traditional"))
                    collation = "trad";

                keyLocaleData.append(collation);
            }
            uenum_close(enumeration);
        }
        break;
    }
    case indexOfExtensionKeyKn:
        keyLocaleData.reserveInitialCapacity(2);
        keyLocaleData.uncheckedAppend("false"_s);
        keyLocaleData.uncheckedAppend("true"_s);
        break;
    case indexOfExtensionKeyKf:
        keyLocaleData.reserveInitialCapacity(3);
        keyLocaleData.uncheckedAppend("false"_s);
        keyLocaleData.uncheckedAppend("lower"_s);
        keyLocaleData.uncheckedAppend("upper"_s);
        break;
    default:
        ASSERT_NOT_REACHED();
    }
    return keyLocaleData;
}

static Vector<String> searchLocaleData(const String&, size_t keyIndex)
{
    // 9.1 Internal slots of Service Constructors & 10.2.3 Internal slots (ECMA-402 2.0)
    Vector<String> keyLocaleData;
    switch (keyIndex) {
    case indexOfExtensionKeyCo:
        // 10.2.3 "The first element of [[sortLocaleData]][locale].co and [[searchLocaleData]][locale].co must be null for all locale values."
        keyLocaleData.reserveInitialCapacity(1);
        keyLocaleData.append({ });
        break;
    case indexOfExtensionKeyKn:
        keyLocaleData.reserveInitialCapacity(2);
        keyLocaleData.uncheckedAppend("false"_s);
        keyLocaleData.uncheckedAppend("true"_s);
        break;
    case indexOfExtensionKeyKf:
        keyLocaleData.reserveInitialCapacity(3);
        keyLocaleData.uncheckedAppend("false"_s);
        keyLocaleData.uncheckedAppend("lower"_s);
        keyLocaleData.uncheckedAppend("upper"_s);
        break;
    default:
        ASSERT_NOT_REACHED();
    }
    return keyLocaleData;
}

void IntlCollator::initializeCollator(ExecState& state, JSValue locales, JSValue optionsValue)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // 10.1.1 InitializeCollator (collator, locales, options) (ECMA-402)
    // https://tc39.github.io/ecma402/#sec-initializecollator

    auto requestedLocales = canonicalizeLocaleList(state, locales);
    RETURN_IF_EXCEPTION(scope, void());

    JSObject* options;
    if (optionsValue.isUndefined())
        options = constructEmptyObject(&state, state.lexicalGlobalObject()->nullPrototypeObjectStructure());
    else {
        options = optionsValue.toObject(&state);
        RETURN_IF_EXCEPTION(scope, void());
    }

    String usageString = intlStringOption(state, options, vm.propertyNames->usage, { "sort", "search" }, "usage must be either \"sort\" or \"search\"", "sort");
    RETURN_IF_EXCEPTION(scope, void());
    if (usageString == "sort")
        m_usage = Usage::Sort;
    else if (usageString == "search")
        m_usage = Usage::Search;
    else
        ASSERT_NOT_REACHED();

    auto localeData = (m_usage == Usage::Sort) ? sortLocaleData : searchLocaleData;

    HashMap<String, String> opt;

    String matcher = intlStringOption(state, options, vm.propertyNames->localeMatcher, { "lookup", "best fit" }, "localeMatcher must be either \"lookup\" or \"best fit\"", "best fit");
    RETURN_IF_EXCEPTION(scope, void());
    opt.add("localeMatcher"_s, matcher);

    {
        String numericString;
        bool usesFallback;
        bool numeric = intlBooleanOption(state, options, vm.propertyNames->numeric, usesFallback);
        RETURN_IF_EXCEPTION(scope, void());
        if (!usesFallback)
            numericString = numeric ? "true"_s : "false"_s;
        if (!numericString.isNull())
            opt.add("kn"_s, numericString);
    }
    {
        String caseFirst = intlStringOption(state, options, vm.propertyNames->caseFirst, { "upper", "lower", "false" }, "caseFirst must be either \"upper\", \"lower\", or \"false\"", nullptr);
        RETURN_IF_EXCEPTION(scope, void());
        if (!caseFirst.isNull())
            opt.add("kf"_s, caseFirst);
    }

    auto& availableLocales = state.jsCallee()->globalObject(vm)->intlCollatorAvailableLocales();
    auto result = resolveLocale(state, availableLocales, requestedLocales, opt, relevantCollatorExtensionKeys, WTF_ARRAY_LENGTH(relevantCollatorExtensionKeys), localeData);

    m_locale = result.get("locale"_s);
    if (m_locale.isEmpty()) {
        throwTypeError(&state, scope, "failed to initialize Collator due to invalid locale"_s);
        return;
    }

    const String& collation = result.get("co"_s);
    m_collation = collation.isNull() ? "default"_s : collation;
    m_numeric = result.get("kn"_s) == "true";

    const String& caseFirst = result.get("kf"_s);
    if (caseFirst == "lower")
        m_caseFirst = CaseFirst::Lower;
    else if (caseFirst == "upper")
        m_caseFirst = CaseFirst::Upper;
    else
        m_caseFirst = CaseFirst::False;

    String sensitivityString = intlStringOption(state, options, vm.propertyNames->sensitivity, { "base", "accent", "case", "variant" }, "sensitivity must be either \"base\", \"accent\", \"case\", or \"variant\"", nullptr);
    RETURN_IF_EXCEPTION(scope, void());
    if (sensitivityString == "base")
        m_sensitivity = Sensitivity::Base;
    else if (sensitivityString == "accent")
        m_sensitivity = Sensitivity::Accent;
    else if (sensitivityString == "case")
        m_sensitivity = Sensitivity::Case;
    else
        m_sensitivity = Sensitivity::Variant;

    bool usesFallback;
    bool ignorePunctuation = intlBooleanOption(state, options, vm.propertyNames->ignorePunctuation, usesFallback);
    if (usesFallback)
        ignorePunctuation = false;
    RETURN_IF_EXCEPTION(scope, void());
    m_ignorePunctuation = ignorePunctuation;

    m_initializedCollator = true;
}

void IntlCollator::createCollator(ExecState& state)
{
    VM& vm = state.vm();
    auto scope = DECLARE_CATCH_SCOPE(vm);
    ASSERT(!m_collator);

    if (!m_initializedCollator) {
        initializeCollator(state, jsUndefined(), jsUndefined());
        scope.assertNoException();
    }

    UErrorCode status = U_ZERO_ERROR;
    auto collator = std::unique_ptr<UCollator, UCollatorDeleter>(ucol_open(m_locale.utf8().data(), &status));
    if (U_FAILURE(status))
        return;

    UColAttributeValue strength = UCOL_PRIMARY;
    UColAttributeValue caseLevel = UCOL_OFF;
    UColAttributeValue caseFirst = UCOL_OFF;
    switch (m_sensitivity) {
    case Sensitivity::Base:
        break;
    case Sensitivity::Accent:
        strength = UCOL_SECONDARY;
        break;
    case Sensitivity::Case:
        caseLevel = UCOL_ON;
        break;
    case Sensitivity::Variant:
        strength = UCOL_TERTIARY;
        break;
    }
    switch (m_caseFirst) {
    case CaseFirst::False:
        break;
    case CaseFirst::Lower:
        caseFirst = UCOL_LOWER_FIRST;
        break;
    case CaseFirst::Upper:
        caseFirst = UCOL_UPPER_FIRST;
        break;
    }

    ucol_setAttribute(collator.get(), UCOL_STRENGTH, strength, &status);
    ucol_setAttribute(collator.get(), UCOL_CASE_LEVEL, caseLevel, &status);
    ucol_setAttribute(collator.get(), UCOL_CASE_FIRST, caseFirst, &status);
    ucol_setAttribute(collator.get(), UCOL_NUMERIC_COLLATION, m_numeric ? UCOL_ON : UCOL_OFF, &status);

    // FIXME: Setting UCOL_ALTERNATE_HANDLING to UCOL_SHIFTED causes punctuation and whitespace to be
    // ignored. There is currently no way to ignore only punctuation.
    ucol_setAttribute(collator.get(), UCOL_ALTERNATE_HANDLING, m_ignorePunctuation ? UCOL_SHIFTED : UCOL_DEFAULT, &status);

    // "The method is required to return 0 when comparing Strings that are considered canonically
    // equivalent by the Unicode standard."
    ucol_setAttribute(collator.get(), UCOL_NORMALIZATION_MODE, UCOL_ON, &status);
    if (U_FAILURE(status))
        return;

    m_collator = WTFMove(collator);
}

JSValue IntlCollator::compareStrings(ExecState& state, StringView x, StringView y)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // 10.3.4 CompareStrings abstract operation (ECMA-402 2.0)
    if (!m_collator) {
        createCollator(state);
        if (!m_collator)
            return throwException(&state, scope, createError(&state, "Failed to compare strings."_s));
    }

    UErrorCode status = U_ZERO_ERROR;
    UCharIterator iteratorX = createIterator(x);
    UCharIterator iteratorY = createIterator(y);
    auto result = ucol_strcollIter(m_collator.get(), &iteratorX, &iteratorY, &status);
    if (U_FAILURE(status))
        return throwException(&state, scope, createError(&state, "Failed to compare strings."_s));
    return jsNumber(result);
}

ASCIILiteral IntlCollator::usageString(Usage usage)
{
    switch (usage) {
    case Usage::Sort:
        return "sort"_s;
    case Usage::Search:
        return "search"_s;
    }
    ASSERT_NOT_REACHED();
    return ASCIILiteral::null();
}

ASCIILiteral IntlCollator::sensitivityString(Sensitivity sensitivity)
{
    switch (sensitivity) {
    case Sensitivity::Base:
        return "base"_s;
    case Sensitivity::Accent:
        return "accent"_s;
    case Sensitivity::Case:
        return "case"_s;
    case Sensitivity::Variant:
        return "variant"_s;
    }
    ASSERT_NOT_REACHED();
    return ASCIILiteral::null();
}

ASCIILiteral IntlCollator::caseFirstString(CaseFirst caseFirst)
{
    switch (caseFirst) {
    case CaseFirst::False:
        return "false"_s;
    case CaseFirst::Lower:
        return "lower"_s;
    case CaseFirst::Upper:
        return "upper"_s;
    }
    ASSERT_NOT_REACHED();
    return ASCIILiteral::null();
}

JSObject* IntlCollator::resolvedOptions(ExecState& state)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // 10.3.5 Intl.Collator.prototype.resolvedOptions() (ECMA-402 2.0)
    // The function returns a new object whose properties and attributes are set as if
    // constructed by an object literal assigning to each of the following properties the
    // value of the corresponding internal slot of this Collator object (see 10.4): locale,
    // usage, sensitivity, ignorePunctuation, collation, as well as those properties shown
    // in Table 1 whose keys are included in the %Collator%[[relevantExtensionKeys]]
    // internal slot of the standard built-in object that is the initial value of
    // Intl.Collator.

    if (!m_initializedCollator) {
        initializeCollator(state, jsUndefined(), jsUndefined());
        scope.assertNoException();
    }

    JSObject* options = constructEmptyObject(&state);
    options->putDirect(vm, vm.propertyNames->locale, jsString(vm, m_locale));
    options->putDirect(vm, vm.propertyNames->usage, jsNontrivialString(vm, usageString(m_usage)));
    options->putDirect(vm, vm.propertyNames->sensitivity, jsNontrivialString(vm, sensitivityString(m_sensitivity)));
    options->putDirect(vm, vm.propertyNames->ignorePunctuation, jsBoolean(m_ignorePunctuation));
    options->putDirect(vm, vm.propertyNames->collation, jsString(vm, m_collation));
    options->putDirect(vm, vm.propertyNames->numeric, jsBoolean(m_numeric));
    options->putDirect(vm, vm.propertyNames->caseFirst, jsNontrivialString(vm, caseFirstString(m_caseFirst)));
    return options;
}

void IntlCollator::setBoundCompare(VM& vm, JSBoundFunction* format)
{
    m_boundCompare.set(vm, this, format);
}

} // namespace JSC

#endif // ENABLE(INTL)

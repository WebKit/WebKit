/*
 * Copyright (C) 2018 Andy VanWagoner (andy@vanwagoner.family)
 * Copyright (C) 2019-2021 Apple Inc. All rights reserved.
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
#include "IntlPluralRules.h"

#include "IntlNumberFormatInlines.h"
#include "IntlObjectInlines.h"
#include "JSCInlines.h"
#include "ObjectConstructor.h"

namespace JSC {

const ClassInfo IntlPluralRules::s_info = { "Object", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(IntlPluralRules) };

using UEnumerationDeleter = ICUDeleter<uenum_close>;

IntlPluralRules* IntlPluralRules::create(VM& vm, Structure* structure)
{
    IntlPluralRules* format = new (NotNull, allocateCell<IntlPluralRules>(vm.heap)) IntlPluralRules(vm, structure);
    format->finishCreation(vm);
    return format;
}

Structure* IntlPluralRules::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

IntlPluralRules::IntlPluralRules(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

void IntlPluralRules::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    ASSERT(inherits(vm, info()));
}

template<typename Visitor>
void IntlPluralRules::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    IntlPluralRules* thisObject = jsCast<IntlPluralRules*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());

    Base::visitChildren(thisObject, visitor);
}

DEFINE_VISIT_CHILDREN(IntlPluralRules);

Vector<String> IntlPluralRules::localeData(const String&, RelevantExtensionKey)
{
    return { };
}

// https://tc39.github.io/ecma402/#sec-initializepluralrules
void IntlPluralRules::initializePluralRules(JSGlobalObject* globalObject, JSValue locales, JSValue optionsValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    Vector<String> requestedLocales = canonicalizeLocaleList(globalObject, locales);
    RETURN_IF_EXCEPTION(scope, void());

    JSObject* options = intlCoerceOptionsToObject(globalObject, optionsValue);
    RETURN_IF_EXCEPTION(scope, void());

    ResolveLocaleOptions localeOptions;
    LocaleMatcher localeMatcher = intlOption<LocaleMatcher>(globalObject, options, vm.propertyNames->localeMatcher, { { "lookup"_s, LocaleMatcher::Lookup }, { "best fit"_s, LocaleMatcher::BestFit } }, "localeMatcher must be either \"lookup\" or \"best fit\""_s, LocaleMatcher::BestFit);
    RETURN_IF_EXCEPTION(scope, void());

    const auto& availableLocales = intlPluralRulesAvailableLocales();
    auto resolved = resolveLocale(globalObject, availableLocales, requestedLocales, localeMatcher, localeOptions, { }, localeData);
    m_locale = resolved.locale;
    if (m_locale.isEmpty()) {
        throwTypeError(globalObject, scope, "failed to initialize PluralRules due to invalid locale"_s);
        return;
    }

    m_type = intlOption<Type>(globalObject, options, vm.propertyNames->type, { { "cardinal"_s, Type::Cardinal }, { "ordinal"_s, Type::Ordinal } }, "type must be \"cardinal\" or \"ordinal\""_s, Type::Cardinal);
    RETURN_IF_EXCEPTION(scope, void());

    setNumberFormatDigitOptions(globalObject, this, options, 0, 3, IntlNotation::Standard);
    RETURN_IF_EXCEPTION(scope, void());

    UErrorCode status = U_ZERO_ERROR;
    m_numberFormat = std::unique_ptr<UNumberFormat, UNumberFormatDeleter>(unum_open(UNUM_DECIMAL, nullptr, 0, m_locale.utf8().data(), nullptr, &status));
    if (U_FAILURE(status)) {
        throwTypeError(globalObject, scope, "failed to initialize PluralRules"_s);
        return;
    }

    switch (m_roundingType) {
    case IntlRoundingType::FractionDigits:
        unum_setAttribute(m_numberFormat.get(), UNUM_MIN_INTEGER_DIGITS, m_minimumIntegerDigits);
        unum_setAttribute(m_numberFormat.get(), UNUM_MIN_FRACTION_DIGITS, m_minimumFractionDigits);
        unum_setAttribute(m_numberFormat.get(), UNUM_MAX_FRACTION_DIGITS, m_maximumFractionDigits);
        break;
    case IntlRoundingType::SignificantDigits:
        unum_setAttribute(m_numberFormat.get(), UNUM_SIGNIFICANT_DIGITS_USED, true);
        unum_setAttribute(m_numberFormat.get(), UNUM_MIN_SIGNIFICANT_DIGITS, m_minimumSignificantDigits);
        unum_setAttribute(m_numberFormat.get(), UNUM_MAX_SIGNIFICANT_DIGITS, m_maximumSignificantDigits);
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }

    status = U_ZERO_ERROR;
    m_pluralRules = std::unique_ptr<UPluralRules, UPluralRulesDeleter>(uplrules_openForType(m_locale.utf8().data(), m_type == Type::Ordinal ? UPLURAL_TYPE_ORDINAL : UPLURAL_TYPE_CARDINAL, &status));
    if (U_FAILURE(status)) {
        throwTypeError(globalObject, scope, "failed to initialize PluralRules"_s);
        return;
    }
}

// https://tc39.es/ecma402/#sec-intl.pluralrules.prototype.resolvedoptions
JSObject* IntlPluralRules::resolvedOptions(JSGlobalObject* globalObject) const
{
    ASSERT(m_pluralRules);

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* options = constructEmptyObject(globalObject);
    options->putDirect(vm, vm.propertyNames->locale, jsNontrivialString(vm, m_locale));
    options->putDirect(vm, vm.propertyNames->type, jsNontrivialString(vm, m_type == Type::Ordinal ? "ordinal"_s : "cardinal"_s));
    options->putDirect(vm, vm.propertyNames->minimumIntegerDigits, jsNumber(m_minimumIntegerDigits));
    switch (m_roundingType) {
    case IntlRoundingType::FractionDigits:
        options->putDirect(vm, vm.propertyNames->minimumFractionDigits, jsNumber(m_minimumFractionDigits));
        options->putDirect(vm, vm.propertyNames->maximumFractionDigits, jsNumber(m_maximumFractionDigits));
        break;
    case IntlRoundingType::SignificantDigits:
        options->putDirect(vm, vm.propertyNames->minimumSignificantDigits, jsNumber(m_minimumSignificantDigits));
        options->putDirect(vm, vm.propertyNames->maximumSignificantDigits, jsNumber(m_maximumSignificantDigits));
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }

    JSArray* categories = JSArray::tryCreate(vm, globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithContiguous), 0);
    if (UNLIKELY(!categories)) {
        throwOutOfMemoryError(globalObject, scope);
        return nullptr;
    }

    UErrorCode status = U_ZERO_ERROR;
    auto keywords = std::unique_ptr<UEnumeration, UEnumerationDeleter>(uplrules_getKeywords(m_pluralRules.get(), &status));
    ASSERT(U_SUCCESS(status));
    int32_t resultLength;

    // Category names are always ASCII, so use char[].
    unsigned index = 0;
    while (const char* result = uenum_next(keywords.get(), &resultLength, &status)) {
        ASSERT(U_SUCCESS(status));
        categories->putDirectIndex(globalObject, index++, jsNontrivialString(vm, String(result, resultLength)));
        RETURN_IF_EXCEPTION(scope, { });
    }
    options->putDirect(vm, Identifier::fromString(vm, "pluralCategories"), categories);

    RELEASE_AND_RETURN(scope, options);
}

// https://tc39.es/ecma402/#sec-resolveplural
JSValue IntlPluralRules::select(JSGlobalObject* globalObject, double value) const
{
    ASSERT(m_pluralRules);

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!std::isfinite(value))
        return jsNontrivialString(vm, "other"_s);

    UErrorCode status = U_ZERO_ERROR;
    Vector<UChar, 8> result(8);
    auto length = uplrules_selectWithFormat(m_pluralRules.get(), value, m_numberFormat.get(), result.data(), result.size(), &status);
    if (U_FAILURE(status))
        return throwTypeError(globalObject, scope, "failed to select plural value"_s);

    return jsString(vm, String(result.data(), length));
}

} // namespace JSC

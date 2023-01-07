/*
 * Copyright (C) 2020-2021 Apple Inc. All rights reserved.
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
#include "IntlDisplayNames.h"

#include "IntlCache.h"
#include "IntlObjectInlines.h"
#include "JSCInlines.h"
#include "ObjectConstructor.h"
#include <unicode/ucurr.h>
#include <unicode/uloc.h>
#include <wtf/unicode/icu/ICUHelpers.h>

namespace JSC {

const ClassInfo IntlDisplayNames::s_info = { "Object"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(IntlDisplayNames) };

IntlDisplayNames* IntlDisplayNames::create(VM& vm, Structure* structure)
{
    auto* object = new (NotNull, allocateCell<IntlDisplayNames>(vm)) IntlDisplayNames(vm, structure);
    object->finishCreation(vm);
    return object;
}

Structure* IntlDisplayNames::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

IntlDisplayNames::IntlDisplayNames(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

void IntlDisplayNames::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));
}

// https://tc39.es/ecma402/#sec-Intl.DisplayNames
void IntlDisplayNames::initializeDisplayNames(JSGlobalObject* globalObject, JSValue locales, JSValue optionsValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto requestedLocales = canonicalizeLocaleList(globalObject, locales);
    RETURN_IF_EXCEPTION(scope, void());

    JSObject* options = intlGetOptionsObject(globalObject, optionsValue);
    RETURN_IF_EXCEPTION(scope, void());

    // Does not set either of "ca" or "nu".
    // https://tc39.es/proposal-intl-displaynames/#sec-intl-displaynames-constructor
    ResolveLocaleOptions localeOptions;

    LocaleMatcher localeMatcher = intlOption<LocaleMatcher>(globalObject, options, vm.propertyNames->localeMatcher, { { "lookup"_s, LocaleMatcher::Lookup }, { "best fit"_s, LocaleMatcher::BestFit } }, "localeMatcher must be either \"lookup\" or \"best fit\""_s, LocaleMatcher::BestFit);
    RETURN_IF_EXCEPTION(scope, void());

    auto localeData = [](const String&, RelevantExtensionKey) -> Vector<String> {
        return { };
    };

    const auto& availableLocales = intlDisplayNamesAvailableLocales();
    auto resolved = resolveLocale(globalObject, availableLocales, requestedLocales, localeMatcher, localeOptions, { }, localeData);

    m_locale = resolved.locale;
    if (m_locale.isEmpty()) {
        throwTypeError(globalObject, scope, "failed to initialize DisplayNames due to invalid locale"_s);
        return;
    }

    m_style = intlOption<Style>(globalObject, options, vm.propertyNames->style, { { "narrow"_s, Style::Narrow }, { "short"_s, Style::Short }, { "long"_s, Style::Long } }, "style must be either \"narrow\", \"short\", or \"long\""_s, Style::Long);
    RETURN_IF_EXCEPTION(scope, void());

    auto type = intlOption<std::optional<Type>>(globalObject, options, vm.propertyNames->type, { { "language"_s, Type::Language }, { "region"_s, Type::Region }, { "script"_s, Type::Script }, { "currency"_s, Type::Currency }, { "calendar"_s, Type::Calendar }, { "dateTimeField"_s, Type::DateTimeField } }, "type must be either \"language\", \"region\", \"script\", \"currency\", \"calendar\", or \"dateTimeField\""_s, std::nullopt);
    RETURN_IF_EXCEPTION(scope, void());
    if (!type) {
        throwTypeError(globalObject, scope, "type must not be undefined"_s);
        return;
    }
    m_type = type.value();

    m_fallback = intlOption<Fallback>(globalObject, options, vm.propertyNames->fallback, { { "code"_s, Fallback::Code }, { "none"_s, Fallback::None } }, "fallback must be either \"code\" or \"none\""_s, Fallback::Code);
    RETURN_IF_EXCEPTION(scope, void());

    m_languageDisplay = intlOption<LanguageDisplay>(globalObject, options, vm.propertyNames->languageDisplay, { { "dialect"_s, LanguageDisplay::Dialect }, { "standard"_s, LanguageDisplay::Standard } }, "languageDisplay must be either \"dialect\" or \"standard\""_s, LanguageDisplay::Dialect);
    RETURN_IF_EXCEPTION(scope, void());

    UErrorCode status = U_ZERO_ERROR;

    UDisplayContext contexts[] = {
        // en_GB displays as 'English (United Kingdom)' (Standard Names) or 'British English' (Dialect Names).
        // https://github.com/tc39/proposal-intl-displaynames#language-display-names
        (m_type == Type::Language && m_languageDisplay == LanguageDisplay::Standard) ? UDISPCTX_STANDARD_NAMES : UDISPCTX_DIALECT_NAMES,

        // Capitailization mode can be picked from several options. Possibly either UDISPCTX_CAPITALIZATION_NONE or UDISPCTX_CAPITALIZATION_FOR_STANDALONE is
        // preferable in Intl.DisplayNames. We use UDISPCTX_CAPITALIZATION_FOR_STANDALONE because it makes standalone date format better (fr "Juillet 2008" in ICU test suites),
        // and DisplayNames will support date formats too.
        UDISPCTX_CAPITALIZATION_FOR_STANDALONE,

        // Narrow becomes UDISPCTX_LENGTH_SHORT. But in currency case, we handle differently instead of using ULocaleDisplayNames.
        m_style == Style::Long ? UDISPCTX_LENGTH_FULL : UDISPCTX_LENGTH_SHORT,

        // Always disable ICU SUBSTITUTE since it does not match against what the spec defines. ICU has some special substitute rules, for example, language "en-AA"
        // returns "English (AA)" (while AA country code is not defined), but we would like to return either input value or undefined, so we do not want to have ICU substitute rules.
        // Note that this is effective after ICU 65.
        // https://github.com/unicode-org/icu/commit/53dd621e3a5cff3b78b557c405f1b1d6f125b468
        UDISPCTX_NO_SUBSTITUTE,
    };
    m_localeCString = m_locale.utf8();
    m_displayNames = std::unique_ptr<ULocaleDisplayNames, ULocaleDisplayNamesDeleter>(uldn_openForContext(m_localeCString.data(), contexts, std::size(contexts), &status));
    if (U_FAILURE(status)) {
        throwTypeError(globalObject, scope, "failed to initialize DisplayNames"_s);
        return;
    }
}

// https://tc39.es/proposal-intl-displaynames/#sec-Intl.DisplayNames.prototype.of
JSValue IntlDisplayNames::of(JSGlobalObject* globalObject, JSValue codeValue) const
{

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    ASSERT(m_displayNames);
    auto code = codeValue.toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    // https://tc39.es/proposal-intl-displaynames/#sec-canonicalcodefordisplaynames
    auto canonicalizeCodeForDisplayNames = [](Type type, String&& code) -> CString {
        ASSERT(code.isAllASCII());
        switch (type) {
        case Type::Language: {
            return canonicalizeUnicodeLocaleID(code.ascii()).ascii();
        }
        case Type::Region: {
            // Let code be the result of mapping code to upper case as described in 6.1.
            auto result = code.ascii();
            char* mutableData = result.mutableData();
            for (unsigned index = 0; index < result.length(); ++index)
                mutableData[index] = toASCIIUpper(mutableData[index]);
            return result;
        }
        case Type::Script: {
            // Let code be the result of mapping the first character in code to upper case, and mapping the second, third and fourth character in code to lower case, as described in 6.1.
            auto result = code.ascii();
            char* mutableData = result.mutableData();
            if (result.length() >= 1)
                mutableData[0] = toASCIIUpper(mutableData[0]);
            for (unsigned index = 1; index < result.length(); ++index)
                mutableData[index] = toASCIILower(mutableData[index]);
            return result;
        }
        case Type::Currency:
            ASSERT_NOT_REACHED();
            break;
        case Type::Calendar: {
            // Let code be the result of mapping code to lower case as described in 6.1.
            String lowered = code.convertToASCIILowercase();
            if (auto mapped = mapBCP47ToICUCalendarKeyword(lowered))
                lowered = WTFMove(mapped.value());
            return lowered.ascii();
        }
        case Type::DateTimeField: {
            ASSERT_NOT_REACHED();
            break;
        }
        }
        return { };
    };

    Vector<UChar, 32> buffer;
    UErrorCode status = U_ZERO_ERROR;
    CString canonicalCode;
    switch (m_type) {
    case Type::Language: {
        if (!isUnicodeLanguageId(code)) {
            throwRangeError(globalObject, scope, "argument is not a language id"_s);
            return { };
        }
        canonicalCode = canonicalizeCodeForDisplayNames(m_type, WTFMove(code));
        // Do not use uldn_languageDisplayName since it is not expected one for this "language" type. It returns "en-US" for "en-US" code, instead of "American English".
        status = callBufferProducingFunction(uldn_localeDisplayName, m_displayNames.get(), canonicalCode.data(), buffer);
        break;
    }
    case Type::Region: {
        if (!isUnicodeRegionSubtag(code)) {
            throwRangeError(globalObject, scope, "argument is not a region subtag"_s);
            return { };
        }
        canonicalCode = canonicalizeCodeForDisplayNames(m_type, WTFMove(code));
        status = callBufferProducingFunction(uldn_regionDisplayName, m_displayNames.get(), canonicalCode.data(), buffer);
        break;
    }
    case Type::Script: {
        if (!isUnicodeScriptSubtag(code)) {
            throwRangeError(globalObject, scope, "argument is not a script subtag"_s);
            return { };
        }
        canonicalCode = canonicalizeCodeForDisplayNames(m_type, WTFMove(code));
        status = callBufferProducingFunction(uldn_scriptDisplayName, m_displayNames.get(), canonicalCode.data(), buffer);
        break;
    }
    case Type::Currency: {
        // We do not use uldn_keyValueDisplayName + "currency". This is because of the following reasons.
        //     1. ICU does not respect UDISPCTX_LENGTH_FULL / UDISPCTX_LENGTH_SHORT in its implementation.
        //     2. There is no way to set "narrow" style in ULocaleDisplayNames while currency have "narrow" symbol style.

        // CanonicalCodeForDisplayNames
        // https://tc39.es/proposal-intl-displaynames/#sec-canonicalcodefordisplaynames
        // 5. If ! IsWellFormedCurrencyCode(code) is false, throw a RangeError exception.
        if (!isWellFormedCurrencyCode(code)) {
            throwRangeError(globalObject, scope, "argument is not a well-formed currency code"_s);
            return { };
        }
        ASSERT(code.isAllASCII());

        UCurrNameStyle style = UCURR_LONG_NAME;
        switch (m_style) {
        case Style::Long:
            style = UCURR_LONG_NAME;
            break;
        case Style::Short:
            style = UCURR_SYMBOL_NAME;
            break;
        case Style::Narrow:
            style = UCURR_NARROW_SYMBOL_NAME;
            break;
        }

        // 6. Let code be the result of mapping code to upper case as described in 6.1.
        const UChar currency[4] = {
            toASCIIUpper(code[0]),
            toASCIIUpper(code[1]),
            toASCIIUpper(code[2]),
            u'\0'
        };
        // The result of ucurr_getName is static string so that we do not need to free the result.
        int32_t length = 0;
        UBool isChoiceFormat = false; // We need to pass this, otherwise, we will see crash in ICU 64.
        const UChar* result = ucurr_getName(currency, m_localeCString.data(), style, &isChoiceFormat, &length, &status);
        if (U_FAILURE(status))
            return throwTypeError(globalObject, scope, "Failed to query a display name."_s);
        // ucurr_getName returns U_USING_DEFAULT_WARNING if the display-name is not found. But U_USING_DEFAULT_WARNING is returned even if
        // narrow and short results are the same: narrow "USD" is "$" with U_USING_DEFAULT_WARNING since short "USD" is also "$". We need to check
        // result == currency to check whether ICU actually failed to find the corresponding display-name. This pointer comparison is ensured by
        // ICU API document.
        // > Returns pointer to display string of 'len' UChars. If the resource data contains no entry for 'currency', then 'currency' itself is returned.
        if (status == U_USING_DEFAULT_WARNING && result == currency)
            return (m_fallback == Fallback::None) ? jsUndefined() : jsString(vm, String(currency, 3));
        return jsString(vm, String(result, length));
    }
    case Type::Calendar: {
        // a. If code does not match the Unicode Locale Identifier type nonterminal, throw a RangeError exception.
        if (!isUnicodeLocaleIdentifierType(code)) {
            throwRangeError(globalObject, scope, "argument is not a calendar code"_s);
            return { };
        }
        canonicalCode = canonicalizeCodeForDisplayNames(m_type, WTFMove(code));
        status = callBufferProducingFunction(uldn_keyValueDisplayName, m_displayNames.get(), "calendar", canonicalCode.data(), buffer);
        break;
    }
    case Type::DateTimeField: {
        // We do not use uldn_keyValueDisplayName since it cannot handle narrow length.
        // Instead, we use udatpg_getFieldDisplayName.

        // https://tc39.es/intl-displaynames-v2/#sec-isvaliddatetimefieldcode
        auto isValidDateTimeFieldCode = [](const String& code) -> std::optional<UDateTimePatternField> {
            if (code == "era"_s)
                return UDATPG_ERA_FIELD;
            if (code == "year"_s)
                return UDATPG_YEAR_FIELD;
            if (code == "quarter"_s)
                return UDATPG_QUARTER_FIELD;
            if (code == "month"_s)
                return UDATPG_MONTH_FIELD;
            if (code == "weekOfYear"_s)
                return UDATPG_WEEK_OF_YEAR_FIELD;
            if (code == "weekday"_s)
                return UDATPG_WEEKDAY_FIELD;
            if (code == "day"_s)
                return UDATPG_DAY_FIELD;
            if (code == "dayPeriod"_s)
                return UDATPG_DAYPERIOD_FIELD;
            if (code == "hour"_s)
                return UDATPG_HOUR_FIELD;
            if (code == "minute"_s)
                return UDATPG_MINUTE_FIELD;
            if (code == "second"_s)
                return UDATPG_SECOND_FIELD;
            if (code == "timeZoneName"_s)
                return UDATPG_ZONE_FIELD;
            return std::nullopt;
        };

        auto field = isValidDateTimeFieldCode(code);
        if (!field) {
            throwRangeError(globalObject, scope, "argument is not a dateTimeField code"_s);
            return { };
        }

        UDateTimePGDisplayWidth style = UDATPG_WIDE;
        switch (m_style) {
        case Style::Long:
            style = UDATPG_WIDE;
            break;
        case Style::Short:
            style = UDATPG_ABBREVIATED;
            break;
        case Style::Narrow:
            style = UDATPG_NARROW;
            break;
        }

        buffer = vm.intlCache().getFieldDisplayName(m_localeCString.data(), field.value(), style, status);
        if (U_FAILURE(status))
            return (m_fallback == Fallback::None) ? jsUndefined() : jsString(vm, WTFMove(code));
        return jsString(vm, String(WTFMove(buffer)));
    }
    }
    if (U_FAILURE(status)) {
        // uldn_localeDisplayName, uldn_regionDisplayName, and uldn_scriptDisplayName return U_ILLEGAL_ARGUMENT_ERROR if the display-name is not found.
        // We should return undefined if fallback is "none". Otherwise, we should return input value.
        if (status == U_ILLEGAL_ARGUMENT_ERROR)
            return (m_fallback == Fallback::None) ? jsUndefined() : jsString(vm, String(canonicalCode.data(), canonicalCode.length()));
        return throwTypeError(globalObject, scope, "Failed to query a display name."_s);
    }
    return jsString(vm, String(WTFMove(buffer)));
}

// https://tc39.es/proposal-intl-displaynames/#sec-Intl.DisplayNames.prototype.resolvedOptions
JSObject* IntlDisplayNames::resolvedOptions(JSGlobalObject* globalObject) const
{
    VM& vm = globalObject->vm();
    JSObject* options = constructEmptyObject(globalObject);
    options->putDirect(vm, vm.propertyNames->locale, jsString(vm, m_locale));
    options->putDirect(vm, vm.propertyNames->style, jsNontrivialString(vm, styleString(m_style)));
    options->putDirect(vm, vm.propertyNames->type, jsNontrivialString(vm, typeString(m_type)));
    options->putDirect(vm, vm.propertyNames->fallback, jsNontrivialString(vm, fallbackString(m_fallback)));
    if (m_type == Type::Language)
        options->putDirect(vm, vm.propertyNames->languageDisplay, jsNontrivialString(vm, languageDisplayString(m_languageDisplay)));
    return options;
}

ASCIILiteral IntlDisplayNames::styleString(Style style)
{
    switch (style) {
    case Style::Narrow:
        return "narrow"_s;
    case Style::Short:
        return "short"_s;
    case Style::Long:
        return "long"_s;
    }
    ASSERT_NOT_REACHED();
    return { };
}

ASCIILiteral IntlDisplayNames::typeString(Type type)
{
    switch (type) {
    case Type::Language:
        return "language"_s;
    case Type::Region:
        return "region"_s;
    case Type::Script:
        return "script"_s;
    case Type::Currency:
        return "currency"_s;
    case Type::Calendar:
        return "calendar"_s;
    case Type::DateTimeField:
        return "dateTimeField"_s;
    }
    ASSERT_NOT_REACHED();
    return { };
}

ASCIILiteral IntlDisplayNames::fallbackString(Fallback fallback)
{
    switch (fallback) {
    case Fallback::Code:
        return "code"_s;
    case Fallback::None:
        return "none"_s;
    }
    ASSERT_NOT_REACHED();
    return { };
}

ASCIILiteral IntlDisplayNames::languageDisplayString(LanguageDisplay languageDisplay)
{
    switch (languageDisplay) {
    case LanguageDisplay::Dialect:
        return "dialect"_s;
    case LanguageDisplay::Standard:
        return "standard"_s;
    }
    ASSERT_NOT_REACHED();
    return { };
}

} // namespace JSC

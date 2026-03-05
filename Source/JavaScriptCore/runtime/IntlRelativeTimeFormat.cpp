/*
 * Copyright (C) 2020 Sony Interactive Entertainment Inc.
 * Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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
#include "IntlRelativeTimeFormat.h"

#include "IntlNumberFormatInlines.h"
#include "IntlObjectInlines.h"
#include "IntlPartObject.h"
#include "JSCInlines.h"
#include "ObjectConstructor.h"
#include <unicode/uformattedvalue.h>
#include <wtf/text/MakeString.h>
#include <wtf/unicode/icu/ICUHelpers.h>

namespace JSC {

const ClassInfo IntlRelativeTimeFormat::s_info = { "Object"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(IntlRelativeTimeFormat) };

namespace IntlRelativeTimeFormatInternal {
}

IntlRelativeTimeFormat* IntlRelativeTimeFormat::create(VM& vm, Structure* structure)
{
    auto* format = new (NotNull, allocateCell<IntlRelativeTimeFormat>(vm)) IntlRelativeTimeFormat(vm, structure);
    format->finishCreation(vm);
    return format;
}

Structure* IntlRelativeTimeFormat::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

IntlRelativeTimeFormat::IntlRelativeTimeFormat(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

template<typename Visitor>
void IntlRelativeTimeFormat::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    auto* thisObject = jsCast<IntlRelativeTimeFormat*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());

    Base::visitChildren(thisObject, visitor);
}

DEFINE_VISIT_CHILDREN(IntlRelativeTimeFormat);

Vector<String> IntlRelativeTimeFormat::localeData(const String& locale, RelevantExtensionKey key)
{
    ASSERT_UNUSED(key, key == RelevantExtensionKey::Nu);
    return numberingSystemsForLocale(locale);
}

// https://tc39.es/ecma402/#sec-InitializeRelativeTimeFormat
void IntlRelativeTimeFormat::initializeRelativeTimeFormat(JSGlobalObject* globalObject, JSValue locales, JSValue optionsValue)
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

    String numberingSystem = intlStringOption(globalObject, options, vm.propertyNames->numberingSystem, { }, { }, { });
    RETURN_IF_EXCEPTION(scope, void());
    if (!numberingSystem.isNull()) {
        if (!isUnicodeLocaleIdentifierType(numberingSystem)) {
            throwRangeError(globalObject, scope, "numberingSystem is not a well-formed numbering system value"_s);
            return;
        }
        localeOptions[static_cast<unsigned>(RelevantExtensionKey::Nu)] = numberingSystem;
    }

    const auto& availableLocales = intlRelativeTimeFormatAvailableLocales();
    auto resolved = resolveLocale(globalObject, availableLocales, requestedLocales, localeMatcher, localeOptions, { RelevantExtensionKey::Nu }, localeData);
    m_locale = resolved.locale;
    if (m_locale.isEmpty()) {
        throwTypeError(globalObject, scope, "failed to initialize RelativeTimeFormat due to invalid locale"_s);
        return;
    }

    m_numberingSystem = resolved.extensions[static_cast<unsigned>(RelevantExtensionKey::Nu)];
    CString dataLocaleWithExtensions = makeString(resolved.dataLocale, "-u-nu-"_s, m_numberingSystem).utf8();

    m_style = intlOption<Style>(globalObject, options, vm.propertyNames->style, { { "long"_s, Style::Long }, { "short"_s, Style::Short }, { "narrow"_s, Style::Narrow } }, "style must be either \"long\", \"short\", or \"narrow\""_s, Style::Long);
    RETURN_IF_EXCEPTION(scope, void());
    UDateRelativeDateTimeFormatterStyle icuStyle = UDAT_STYLE_LONG;
    switch (m_style) {
    case Style::Long:
        icuStyle = UDAT_STYLE_LONG;
        break;
    case Style::Short:
        icuStyle = UDAT_STYLE_SHORT;
        break;
    case Style::Narrow:
        icuStyle = UDAT_STYLE_NARROW;
        break;
    }

    m_numeric = intlOption<bool>(globalObject, options, vm.propertyNames->numeric, { { "always"_s, true }, { "auto"_s, false } }, "numeric must be either \"always\" or \"auto\""_s, true);
    RETURN_IF_EXCEPTION(scope, void());

    UErrorCode status = U_ZERO_ERROR;
    auto numberFormat = std::unique_ptr<UNumberFormat, ICUDeleter<unum_close>>(unum_open(UNUM_DECIMAL, nullptr, 0, dataLocaleWithExtensions.data(), nullptr, &status));
    if (U_FAILURE(status)) [[unlikely]] {
        throwTypeError(globalObject, scope, "failed to initialize RelativeTimeFormat"_s);
        return;
    }

    // Align to IntlNumberFormat's default.
    unum_setAttribute(numberFormat.get(), UNUM_MIN_INTEGER_DIGITS, 1);
    unum_setAttribute(numberFormat.get(), UNUM_MIN_FRACTION_DIGITS, 0);
    unum_setAttribute(numberFormat.get(), UNUM_MAX_FRACTION_DIGITS, 3);
    unum_setAttribute(numberFormat.get(), UNUM_GROUPING_USED, true);

    // Grouping attributes have hidden -2 option which makes grouping rules locale-sensitive.
    // While this is supported so long, this is not explicitly exposed as APIs.
    // After ICU 68, it is now exposed as UNUM_MINIMUM_GROUPING_DIGITS_AUTO. Before ICU 68, we can directly use -2.
    // https://unicode-org.atlassian.net/browse/ICU-21109
    // https://github.com/unicode-org/icu/commit/e7bd5b1cefa47a043a9714e21eb9946dd54d593f
    //
    // These options are tested in test262/test/intl402/RelativeTimeFormat/prototype/format/pl-pl-style-long.js etc.
    // e.g. https://github.com/tc39/test262/commit/79c1818a6812a2a6c47e3e3c56ba9f2b3eaff4d5
    constexpr int32_t useLocaleDefault = -2;
    unum_setAttribute(numberFormat.get(), UNUM_GROUPING_SIZE, useLocaleDefault);
    unum_setAttribute(numberFormat.get(), UNUM_SECONDARY_GROUPING_SIZE, useLocaleDefault);
    unum_setAttribute(numberFormat.get(), UNUM_MINIMUM_GROUPING_DIGITS, useLocaleDefault);

    // ureldatefmt_open adopts the UNumberFormat, so we release ownership here.
    m_relativeDateTimeFormatter = std::unique_ptr<URelativeDateTimeFormatter, URelativeDateTimeFormatterDeleter>(ureldatefmt_open(dataLocaleWithExtensions.data(), numberFormat.release(), icuStyle, UDISPCTX_CAPITALIZATION_FOR_STANDALONE, &status));
    if (U_FAILURE(status)) [[unlikely]] {
        throwTypeError(globalObject, scope, "failed to initialize RelativeTimeFormat"_s);
        return;
    }

    m_formattedResult = std::unique_ptr<UFormattedRelativeDateTime, ICUDeleter<ureldatefmt_closeResult>>(ureldatefmt_openResult(&status));
    if (U_FAILURE(status)) [[unlikely]] {
        throwTypeError(globalObject, scope, "failed to initialize RelativeTimeFormat"_s);
        return;
    }

    m_cfpos = std::unique_ptr<UConstrainedFieldPosition, ICUDeleter<ucfpos_close>>(ucfpos_open(&status));
    if (U_FAILURE(status)) [[unlikely]] {
        throwTypeError(globalObject, scope, "failed to initialize RelativeTimeFormat"_s);
        return;
    }
}

ASCIILiteral IntlRelativeTimeFormat::styleString(Style style)
{
    switch (style) {
    case Style::Long:
        return "long"_s;
    case Style::Short:
        return "short"_s;
    case Style::Narrow:
        return "narrow"_s;
    }
    ASSERT_NOT_REACHED();
    return { };
}

// https://tc39.es/ecma402/#sec-intl.relativetimeformat.prototype.resolvedoptions
JSObject* IntlRelativeTimeFormat::resolvedOptions(JSGlobalObject* globalObject) const
{
    VM& vm = globalObject->vm();
    JSObject* options = constructEmptyObject(globalObject);
    options->putDirect(vm, vm.propertyNames->locale, jsNontrivialString(vm, m_locale));
    options->putDirect(vm, vm.propertyNames->style, jsNontrivialString(vm, styleString(m_style)));
    options->putDirect(vm, vm.propertyNames->numeric, jsNontrivialString(vm, m_numeric ? "always"_s : "auto"_s));
    options->putDirect(vm, vm.propertyNames->numberingSystem, jsNontrivialString(vm, m_numberingSystem));
    return options;
}

static StringView singularUnit(StringView unit)
{
    // Plurals are allowed, but thankfully they're all just a simple -s.
    return unit.endsWith('s') ? unit.left(unit.length() - 1) : unit;
}

// https://tc39.es/ecma402/#sec-singularrelativetimeunit
static std::optional<URelativeDateTimeUnit> relativeTimeUnitType(StringView unit)
{
    StringView singular = singularUnit(unit);

    if (singular == "second"_s)
        return UDAT_REL_UNIT_SECOND;
    if (singular == "minute"_s)
        return UDAT_REL_UNIT_MINUTE;
    if (singular == "hour"_s)
        return UDAT_REL_UNIT_HOUR;
    if (singular == "day"_s)
        return UDAT_REL_UNIT_DAY;
    if (singular == "week"_s)
        return UDAT_REL_UNIT_WEEK;
    if (singular == "month"_s)
        return UDAT_REL_UNIT_MONTH;
    if (singular == "quarter"_s)
        return UDAT_REL_UNIT_QUARTER;
    if (singular == "year"_s)
        return UDAT_REL_UNIT_YEAR;

    return std::nullopt;
}

String IntlRelativeTimeFormat::formatInternal(JSGlobalObject* globalObject, double value, StringView unit) const
{
    ASSERT(m_relativeDateTimeFormatter);

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!std::isfinite(value)) {
        throwRangeError(globalObject, scope, "number argument must be finite"_s);
        return String();
    }

    auto unitType = relativeTimeUnitType(unit);
    if (!unitType) {
        throwRangeError(globalObject, scope, "unit argument is not a recognized unit type"_s);
        return String();
    }

    auto formatRelativeTime = m_numeric ? ureldatefmt_formatNumeric : ureldatefmt_format;

    Vector<char16_t, 32> result;
    auto status = callBufferProducingFunction(formatRelativeTime, m_relativeDateTimeFormatter.get(), value, unitType.value(), result);
    if (U_FAILURE(status)) [[unlikely]] {
        throwTypeError(globalObject, scope, "failed to format relative time"_s);
        return String();
    }

    return String(WTF::move(result));
}

// https://tc39.es/ecma402/#sec-FormatRelativeTime
JSValue IntlRelativeTimeFormat::format(JSGlobalObject* globalObject, double value, StringView unit) const
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    String result = formatInternal(globalObject, value, unit);
    RETURN_IF_EXCEPTION(scope, { });

    return jsString(vm, WTF::move(result));
}

// https://tc39.es/ecma402/#sec-FormatRelativeTimeToParts
JSValue IntlRelativeTimeFormat::formatToParts(JSGlobalObject* globalObject, double value, StringView unit) const
{
    ASSERT(m_relativeDateTimeFormatter);
    ASSERT(m_formattedResult);
    ASSERT(m_cfpos);

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!std::isfinite(value)) {
        throwRangeError(globalObject, scope, "number argument must be finite"_s);
        return { };
    }

    auto unitType = relativeTimeUnitType(unit);
    if (!unitType) {
        throwRangeError(globalObject, scope, "unit argument is not a recognized unit type"_s);
        return { };
    }

    UErrorCode status = U_ZERO_ERROR;

    // Reuse cached UFormattedRelativeDateTime to avoid per-call heap allocation.
    if (m_numeric)
        ureldatefmt_formatNumericToResult(m_relativeDateTimeFormatter.get(), value, unitType.value(), m_formattedResult.get(), &status);
    else
        ureldatefmt_formatToResult(m_relativeDateTimeFormatter.get(), value, unitType.value(), m_formattedResult.get(), &status);
    if (U_FAILURE(status)) [[unlikely]]
        return throwTypeError(globalObject, scope, "failed to format relative time"_s);

    auto formattedValue = ureldatefmt_resultAsValue(m_formattedResult.get(), &status);
    if (U_FAILURE(status)) [[unlikely]]
        return throwTypeError(globalObject, scope, "failed to format relative time"_s);

    int32_t formattedStringLength = 0;
    const char16_t* formattedStringPointer = ufmtval_getString(formattedValue, &formattedStringLength, &status);
    if (U_FAILURE(status)) [[unlikely]]
        return throwTypeError(globalObject, scope, "failed to format relative time"_s);
    WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
    String formattedRelativeTime(std::span(formattedStringPointer, formattedStringLength));
    WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

    // Iterate all fields from the UFormattedValue in a single pass.
    // UFIELD_CATEGORY_RELATIVE_DATETIME fields delimit literal vs numeric regions.
    // UFIELD_CATEGORY_NUMBER fields provide number sub-part details (integer, fraction, etc.)
    // within the numeric region, eliminating the need for a separate unum_formatDoubleForFields call.
    ucfpos_reset(m_cfpos.get(), &status);
    if (U_FAILURE(status)) [[unlikely]]
        return throwTypeError(globalObject, scope, "failed to format relative time"_s);

    int32_t numberStart = -1;
    int32_t numberEnd = -1;
    Vector<IntlNumberFormatField> numberFields;

    while (ufmtval_nextPosition(formattedValue, m_cfpos.get(), &status)) {
        if (U_FAILURE(status)) [[unlikely]]
            return throwTypeError(globalObject, scope, "failed to format relative time"_s);

        int32_t category = ucfpos_getCategory(m_cfpos.get(), &status);
        int32_t fieldType = ucfpos_getField(m_cfpos.get(), &status);
        int32_t beginIndex = 0;
        int32_t endIndex = 0;
        ucfpos_getIndexes(m_cfpos.get(), &beginIndex, &endIndex, &status);
        if (U_FAILURE(status)) [[unlikely]]
            return throwTypeError(globalObject, scope, "failed to format relative time"_s);

        if (category == UFIELD_CATEGORY_RELATIVE_DATETIME && fieldType == UDAT_REL_NUMERIC_FIELD) {
            numberStart = beginIndex;
            numberEnd = endIndex;
        } else if (category == UFIELD_CATEGORY_NUMBER && fieldType >= 0) {
            // Collect number fields; positions will be adjusted below.
            numberFields.append({ fieldType, { beginIndex, endIndex } });
        }
    }

    JSArray* parts = JSArray::tryCreate(vm, globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithContiguous), 0);
    if (!parts) [[unlikely]]
        return throwOutOfMemoryError(globalObject, scope);

    JSString* literalString = jsNontrivialString(vm, "literal"_s);

    if (numberStart >= 0) {
        if (numberStart > 0) {
            JSObject* part = createIntlPartObject(globalObject, literalString, jsSubstring(vm, formattedRelativeTime, 0, numberStart));
            parts->push(globalObject, part);
            RETURN_IF_EXCEPTION(scope, { });
        }

        // Adjust field positions to be relative to the numeric substring.
        auto numberPartString = formattedRelativeTime.substring(numberStart, numberEnd - numberStart);
        for (auto& field : numberFields)
            field.m_range = { field.m_range.begin() - numberStart, field.m_range.end() - numberStart };

        double absValue = std::abs(value);
        IntlFieldIterator fieldIterator(WTF::move(numberFields));
        IntlNumberFormat::formatToPartsInternal(globalObject, IntlNumberFormat::Style::Decimal, std::signbit(absValue), IntlMathematicalValue::numberTypeFromDouble(absValue), numberPartString, fieldIterator, parts, nullptr, jsString(vm, singularUnit(unit)));
        RETURN_IF_EXCEPTION(scope, { });

        auto stringLength = formattedRelativeTime.length();
        if (static_cast<unsigned>(numberEnd) < stringLength) {
            JSObject* part = createIntlPartObject(globalObject, literalString, jsSubstring(vm, formattedRelativeTime, numberEnd, stringLength - numberEnd));
            parts->push(globalObject, part);
            RETURN_IF_EXCEPTION(scope, { });
        }
    } else {
        // No numeric field (e.g., numeric: "auto" producing "today", "yesterday").
        JSObject* part = createIntlPartObject(globalObject, literalString, jsString(vm, formattedRelativeTime));
        parts->push(globalObject, part);
        RETURN_IF_EXCEPTION(scope, { });
    }

    return parts;
}

} // namespace JSC

/*
 * Copyright (C) 2020-2024 Apple Inc. All rights reserved.
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
#include "IntlDurationFormat.h"

#include "IntlNumberFormatInlines.h"
#include "IntlObjectInlines.h"
#include "IteratorOperations.h"
#include "JSCInlines.h"
#include "ObjectConstructor.h"
#include <wtf/text/MakeString.h>

// While UListFormatter APIs are draft in ICU 67, they are stable in ICU 68 with the same function signatures.
// So we can assume that these signatures of draft APIs are stable.
// If UListFormatter is available, UNumberFormatter is also available.
#ifdef U_HIDE_DRAFT_API
#undef U_HIDE_DRAFT_API
#endif
#include <unicode/ulistformatter.h>
#include <unicode/unumberformatter.h>
#include <unicode/ures.h>
#define U_HIDE_DRAFT_API 1
#include <unicode/uformattedvalue.h>

namespace JSC {
namespace IntlDurationFormatInternal {
static constexpr bool verbose = false;
}

static constexpr unsigned fractionalDigitsUndefinedValue = std::numeric_limits<unsigned>::max();

const ClassInfo IntlDurationFormat::s_info = { "Object"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(IntlDurationFormat) };

IntlDurationFormat* IntlDurationFormat::create(VM& vm, Structure* structure)
{
    auto* object = new (NotNull, allocateCell<IntlDurationFormat>(vm)) IntlDurationFormat(vm, structure);
    object->finishCreation(vm);
    return object;
}

Structure* IntlDurationFormat::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

IntlDurationFormat::IntlDurationFormat(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

enum class StyleListKind : uint8_t { LongShortNarrow, LongShortNarrowNumeric, LongShortNarrowNumericTwoDigit  };
static IntlDurationFormat::UnitData intlDurationUnitOptions(JSGlobalObject* globalObject, JSObject* options, TemporalUnit unit, PropertyName propertyName, PropertyName displayName, IntlDurationFormat::Style baseStyle, StyleListKind styleList, IntlDurationFormat::UnitStyle digitalBase, std::optional<IntlDurationFormat::UnitStyle> prevStyle)
{
    // https://tc39.es/proposal-intl-duration-format/#sec-getdurationunitoptions

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    std::optional<IntlDurationFormat::UnitStyle> styleValue;
    switch (styleList) {
    case StyleListKind::LongShortNarrow: {
        styleValue = intlOption<std::optional<IntlDurationFormat::UnitStyle>>(globalObject, options, propertyName, { { "long"_s, IntlDurationFormat::UnitStyle::Long }, { "short"_s, IntlDurationFormat::UnitStyle::Short }, { "narrow"_s, IntlDurationFormat::UnitStyle::Narrow } }, "style must be either \"long\", \"short\", or \"narrow\""_s, std::nullopt);
        RETURN_IF_EXCEPTION(scope, { });
        break;
    }
    case StyleListKind::LongShortNarrowNumeric: {
        styleValue = intlOption<std::optional<IntlDurationFormat::UnitStyle>>(globalObject, options, propertyName, { { "long"_s, IntlDurationFormat::UnitStyle::Long }, { "short"_s, IntlDurationFormat::UnitStyle::Short }, { "narrow"_s, IntlDurationFormat::UnitStyle::Narrow }, { "numeric"_s, IntlDurationFormat::UnitStyle::Numeric } }, "style must be either \"long\", \"short\", \"narrow\", or \"numeric\""_s, std::nullopt);
        RETURN_IF_EXCEPTION(scope, { });
        break;
    }
    case StyleListKind::LongShortNarrowNumericTwoDigit: {
        styleValue = intlOption<std::optional<IntlDurationFormat::UnitStyle>>(globalObject, options, propertyName, { { "long"_s, IntlDurationFormat::UnitStyle::Long }, { "short"_s, IntlDurationFormat::UnitStyle::Short }, { "narrow"_s, IntlDurationFormat::UnitStyle::Narrow }, { "numeric"_s, IntlDurationFormat::UnitStyle::Numeric }, { "2-digit"_s, IntlDurationFormat::UnitStyle::TwoDigit } }, "style must be either \"long\", \"short\", \"narrow\" or \"numeric\", or \"2-digit\""_s, std::nullopt);
        RETURN_IF_EXCEPTION(scope, { });
        break;
    }
    }

    IntlDurationFormat::Display displayDefault = IntlDurationFormat::Display::Always;
    IntlDurationFormat::UnitStyle style = IntlDurationFormat::UnitStyle::Short;
    if (styleValue)
        style = styleValue.value();
    else {
        if (baseStyle == IntlDurationFormat::Style::Digital) {
            if (unit != TemporalUnit::Hour && unit != TemporalUnit::Minute && unit != TemporalUnit::Second)
                displayDefault = IntlDurationFormat::Display::Auto;
            style = digitalBase;
        } else {
            displayDefault = IntlDurationFormat::Display::Auto;
            if (prevStyle && (prevStyle.value() == IntlDurationFormat::UnitStyle::Numeric || prevStyle.value() == IntlDurationFormat::UnitStyle::TwoDigit))
                style = IntlDurationFormat::UnitStyle::Numeric;
            else
                style = static_cast<IntlDurationFormat::UnitStyle>(baseStyle);
        }
    }

    IntlDurationFormat::Display display = intlOption<IntlDurationFormat::Display>(globalObject, options, displayName, { { "auto"_s, IntlDurationFormat::Display::Auto }, { "always"_s, IntlDurationFormat::Display::Always } }, "display name must be either \"auto\" or \"always\""_s, displayDefault);
    RETURN_IF_EXCEPTION(scope, { });

    if (prevStyle && (prevStyle.value() == IntlDurationFormat::UnitStyle::Numeric || prevStyle.value() == IntlDurationFormat::UnitStyle::TwoDigit)) {
        if (style != IntlDurationFormat::UnitStyle::Numeric && style != IntlDurationFormat::UnitStyle::TwoDigit) {
            throwRangeError(globalObject, scope, "style option is inconsistent"_s);
            return { };
        }

        if (unit == TemporalUnit::Minute || unit == TemporalUnit::Second)
            style = IntlDurationFormat::UnitStyle::TwoDigit;
    }

    return { style, display };
}

static constexpr IntlDurationFormat::UnitStyle digitalDefaults[numberOfTemporalUnits] = {
    IntlDurationFormat::UnitStyle::Short,
    IntlDurationFormat::UnitStyle::Short,
    IntlDurationFormat::UnitStyle::Short,
    IntlDurationFormat::UnitStyle::Short,
    IntlDurationFormat::UnitStyle::Numeric,
    IntlDurationFormat::UnitStyle::Numeric,
    IntlDurationFormat::UnitStyle::Numeric,
    IntlDurationFormat::UnitStyle::Numeric,
    IntlDurationFormat::UnitStyle::Numeric,
    IntlDurationFormat::UnitStyle::Numeric,
};

static constexpr StyleListKind styleLists[numberOfTemporalUnits] = {
    StyleListKind::LongShortNarrow,
    StyleListKind::LongShortNarrow,
    StyleListKind::LongShortNarrow,
    StyleListKind::LongShortNarrow,
    StyleListKind::LongShortNarrowNumericTwoDigit,
    StyleListKind::LongShortNarrowNumericTwoDigit,
    StyleListKind::LongShortNarrowNumericTwoDigit,
    StyleListKind::LongShortNarrowNumeric,
    StyleListKind::LongShortNarrowNumeric,
    StyleListKind::LongShortNarrowNumeric,
};

static PropertyName displayName(VM& vm, TemporalUnit unit)
{
    switch (unit) {
#define JSC_TEMPORAL_UNIT_PLURAL_PROPERTY_NAME(name, capitalizedName) case TemporalUnit::capitalizedName: return vm.propertyNames->name##sDisplay;
        JSC_TEMPORAL_UNITS(JSC_TEMPORAL_UNIT_PLURAL_PROPERTY_NAME)
#undef JSC_TEMPORAL_UNIT_PLURAL_PROPERTY_NAME
    }

    RELEASE_ASSERT_NOT_REACHED();
}

// https://tc39.es/proposal-intl-duration-format/#sec-Intl.DurationFormat
void IntlDurationFormat::initializeDurationFormat(JSGlobalObject* globalObject, JSValue locales, JSValue optionsValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto requestedLocales = canonicalizeLocaleList(globalObject, locales);
    RETURN_IF_EXCEPTION(scope, void());

    JSObject* options = intlGetOptionsObject(globalObject, optionsValue);
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

    auto localeData = [](const String& locale, RelevantExtensionKey key) -> Vector<String> {
        ASSERT_UNUSED(key, key == RelevantExtensionKey::Nu);
        return numberingSystemsForLocale(locale);
    };

    const auto& availableLocales = intlDurationFormatAvailableLocales();
    auto resolved = resolveLocale(globalObject, availableLocales, requestedLocales, localeMatcher, localeOptions, { RelevantExtensionKey::Nu }, localeData);

    m_locale = resolved.locale;
    if (m_locale.isEmpty()) {
        throwTypeError(globalObject, scope, "failed to initialize DurationFormat due to invalid locale"_s);
        return;
    }

    m_numberingSystem = resolved.extensions[static_cast<unsigned>(RelevantExtensionKey::Nu)];
    m_dataLocaleWithExtensions = makeString(resolved.dataLocale, "-u-nu-"_s, m_numberingSystem).utf8();

    m_style = intlOption<Style>(globalObject, options, vm.propertyNames->style, { { "long"_s, Style::Long }, { "short"_s, Style::Short }, { "narrow"_s, Style::Narrow }, { "digital"_s, Style::Digital } }, "style must be either \"long\", \"short\", \"narrow\", or \"digital\""_s, Style::Short);
    RETURN_IF_EXCEPTION(scope, void());

    std::optional<UnitStyle> prevStyle;
    for (unsigned index = 0; index < numberOfTemporalUnits; ++index) {
        TemporalUnit unit = static_cast<TemporalUnit>(index);
        auto unitData = intlDurationUnitOptions(globalObject, options, unit, temporalUnitPluralPropertyName(vm, unit), displayName(vm, unit), m_style, styleLists[index], digitalDefaults[index], prevStyle);
        RETURN_IF_EXCEPTION(scope, void());
        m_units[index] = unitData;
        switch (unit) {
        case TemporalUnit::Hour:
        case TemporalUnit::Minute:
        case TemporalUnit::Second:
        case TemporalUnit::Millisecond:
        case TemporalUnit::Microsecond:
            prevStyle = unitData.style();
            break;
        default:
            break;
        }
    }

    m_fractionalDigits = intlNumberOption(globalObject, options, vm.propertyNames->fractionalDigits, 0, 9, fractionalDigitsUndefinedValue);
    RETURN_IF_EXCEPTION(scope, void());

    {
        auto toUListFormatterWidth = [](Style style) {
            // 6. Let listStyle be durationFormat.[[Style]].
            // 7. If listStyle is "digital", then
            //     a. Set listStyle to "short".
            // 8. Perform ! CreateDataPropertyOrThrow(lfOpts, "style", listStyle).
            switch (style) {
            case Style::Long:
                return ULISTFMT_WIDTH_WIDE;
            case Style::Short:
            case Style::Digital:
                return ULISTFMT_WIDTH_SHORT;
            case Style::Narrow:
                return ULISTFMT_WIDTH_NARROW;
            }
            return ULISTFMT_WIDTH_WIDE;
        };

        // 5. Perform ! CreateDataPropertyOrThrow(lfOpts, "type", "unit").
        UErrorCode status = U_ZERO_ERROR;
        m_listFormat = std::unique_ptr<UListFormatter, UListFormatterDeleter>(ulistfmt_openForType(m_locale.utf8().data(), ULISTFMT_TYPE_UNITS, toUListFormatterWidth(m_style), &status));
        if (U_FAILURE(status)) {
            throwTypeError(globalObject, scope, "failed to initialize DurationFormat"_s);
            return;
        }
    }
}

static String retrieveSeparator(const CString& locale, const String& numberingSystem)
{
    ASCIILiteral fallbackTimeSeparator = ":"_s;
    UErrorCode status = U_ZERO_ERROR;

    auto bundle = std::unique_ptr<UResourceBundle, ICUDeleter<ures_close>>(ures_open(nullptr, locale.data(), &status));
    if (U_FAILURE(status))
        return fallbackTimeSeparator;

    auto numberElementsBundle = std::unique_ptr<UResourceBundle, ICUDeleter<ures_close>>(ures_getByKey(bundle.get(), "NumberElements", nullptr, &status));
    if (U_FAILURE(status))
        return fallbackTimeSeparator;

    auto numberingSystemBundle = std::unique_ptr<UResourceBundle, ICUDeleter<ures_close>>(ures_getByKey(numberElementsBundle.get(), numberingSystem.utf8().data(), nullptr, &status));
    if (U_FAILURE(status))
        return fallbackTimeSeparator;

    auto symbolsBundle = std::unique_ptr<UResourceBundle, ICUDeleter<ures_close>>(ures_getByKey(numberingSystemBundle.get(), "symbols", nullptr, &status));
    if (U_FAILURE(status))
        return fallbackTimeSeparator;

    int32_t length = 0;
    const UChar* data = ures_getStringByKey(symbolsBundle.get(), "timeSeparator", &length, &status);
    if (U_FAILURE(status))
        return fallbackTimeSeparator;

    return String({ data, static_cast<size_t>(length) });
}

enum class ElementType : uint8_t {
    Literal,
    Element,
};

struct Element {
    ElementType m_type;
    TemporalUnit m_unit;
    String m_string;
    double m_value;
    std::unique_ptr<UFormattedNumber, ICUDeleter<unumf_closeResult>> m_formattedNumber;
};

enum class DurationSignType : uint8_t {
    Negative,
    Positive,
    Zero,
};

// https://tc39.es/proposal-intl-duration-format/#sec-durationsign
static DurationSignType getDurationSign(ISO8601::Duration duration)
{
    for (auto value : duration) {
        if (value < 0)
            return DurationSignType::Negative;
        if (value > 0)
            return DurationSignType::Positive;
    }
    return DurationSignType::Zero;
}

static String int128ToString(Int128 value)
{
    Vector<LChar> resultString;
    bool isNegative = value < 0;
    if (isNegative)
        value = -value;

    while (value) {
        Int128 digit = value % 10;
        resultString.append(static_cast<char>('0' + digit));
        value /= 10;
    }

    if (isNegative)
        resultString.append('-');

    std::reverse(resultString.begin(), resultString.end());

    return StringImpl::adopt(WTFMove(resultString));
}

static String buildDecimalFormat(TemporalUnit unit, Int128 ns)
{
    ASSERT(unit == TemporalUnit::Second || unit == TemporalUnit::Millisecond || unit == TemporalUnit::Microsecond);

    int flactionalDigits = 0;
    Int128 exponent = 0;
    if (unit == TemporalUnit::Second) {
        flactionalDigits = 9;
        exponent = Int128(1000000000);
    } else if (unit == TemporalUnit::Millisecond) {
        flactionalDigits = 6;
        exponent = Int128(1000000);
    } else {
        ASSERT(unit == TemporalUnit::Microsecond);
        flactionalDigits = 3;
        exponent = Int128(1000);
    }

    Int128 integerPart = ns / exponent;
    ASSERT(ns % exponent >= std::numeric_limits<int64_t>::min() && ns % exponent <= std::numeric_limits<int64_t>::max());
    int64_t fractionalPart = std::abs(static_cast<int64_t>(ns % exponent));

    StringBuilder builder;

    builder.append(int128ToString(integerPart));
    builder.append("."_s);

    String fractionalString = String::number(fractionalPart);
    int zeroLength = flactionalDigits - fractionalString.length();
    for (int i = 0; i < zeroLength; ++i)
        builder.append("0"_s);
    builder.append(fractionalString);

    return builder.toString();
}

static Vector<Element> collectElements(JSGlobalObject* globalObject, const IntlDurationFormat* durationFormat, ISO8601::Duration duration)
{
    // https://tc39.es/proposal-intl-duration-format/#sec-partitiondurationformatpattern

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    bool done = false;
    bool needsSignDisplay = false;
    String separator;
    Vector<Element> elements;
    std::optional<DurationSignType> durationSign;
    for (unsigned index = 0; index < numberOfTemporalUnits && !done; ++index) {
        TemporalUnit unit = static_cast<TemporalUnit>(index);
        auto unitData = durationFormat->units()[index];
        double value = duration[unit];
        std::optional<Int128> totalNanosecondsValue;

        StringBuilder skeletonBuilder;

        switch (unit) {
        // 3.j. If unit is "seconds", "milliseconds", or "microseconds", then
        case TemporalUnit::Second:
        case TemporalUnit::Millisecond:
        case TemporalUnit::Microsecond: {
            skeletonBuilder.append("rounding-mode-down"_s);
            IntlDurationFormat::UnitStyle nextStyle = IntlDurationFormat::UnitStyle::Long;
            if (unit == TemporalUnit::Second)
                nextStyle = durationFormat->units()[static_cast<unsigned>(TemporalUnit::Millisecond)].style();
            else if (unit == TemporalUnit::Millisecond)
                nextStyle = durationFormat->units()[static_cast<unsigned>(TemporalUnit::Microsecond)].style();
            else {
                ASSERT(unit == TemporalUnit::Microsecond);
                nextStyle = durationFormat->units()[static_cast<unsigned>(TemporalUnit::Nanosecond)].style();
            }
            if (nextStyle == IntlDurationFormat::UnitStyle::Numeric) {
                if (unit == TemporalUnit::Second)
                    totalNanosecondsValue = duration.totalNanoseconds<TemporalUnit::Second>();
                else if (unit == TemporalUnit::Millisecond)
                    totalNanosecondsValue = duration.totalNanoseconds<TemporalUnit::Millisecond>();
                else {
                    ASSERT(unit == TemporalUnit::Microsecond);
                    totalNanosecondsValue = duration.totalNanoseconds<TemporalUnit::Microsecond>();
                }
                ASSERT(totalNanosecondsValue);

                // https://github.com/unicode-org/icu/blob/master/docs/userguide/format_parse/numbers/skeletons.md#fraction-precision
                skeletonBuilder.append(" ."_s);

                unsigned fractionalDigits = durationFormat->fractionalDigits();
                if (fractionalDigits == fractionalDigitsUndefinedValue)
                    skeletonBuilder.append("#########"_s);
                else {
                    for (unsigned i = 0; i < fractionalDigits; ++i)
                        skeletonBuilder.append('0');
                }
                done = true;
            }
            break;
        }
        default: {
            skeletonBuilder.append("rounding-mode-half-up"_s);
            break;
        }
        }

        auto style = unitData.style();

        // 3.k. If style is "2-digit", then
        //     i. Perform ! CreateDataPropertyOrThrow(nfOpts, "minimumIntegerDigits", 2F).
        skeletonBuilder.append(" integer-width/*"_s);
        if (style == IntlDurationFormat::UnitStyle::TwoDigit)
            skeletonBuilder.append("00"_s);
        else
            skeletonBuilder.append('0');

        // 3.l. If value is not 0 or display is not "auto", then
        value = purifyNaN(value);
        if (value || unitData.display() != IntlDurationFormat::Display::Auto || style == IntlDurationFormat::UnitStyle::TwoDigit || style ==  IntlDurationFormat::UnitStyle::Numeric) {
            auto formatToString = [&](UFormattedNumber* formattedNumber) -> String {
                auto scope = DECLARE_THROW_SCOPE(vm);

                UErrorCode status = U_ZERO_ERROR;
                Vector<UChar, 32> buffer;
                status = callBufferProducingFunction(unumf_resultToString, formattedNumber, buffer);
                if (U_FAILURE(status)) {
                    throwTypeError(globalObject, scope, "Failed to format a number."_s);
                    return { };
                }

                return String(WTFMove(buffer));
            };

            // https://github.com/unicode-org/icu/blob/main/docs/userguide/format_parse/numbers/skeletons.md#sign-display
            if (needsSignDisplay)
                skeletonBuilder.append(" +_"_s);
            else if (!value) {
                if (!durationSign)
                    durationSign = getDurationSign(duration);
                if (durationSign == DurationSignType::Negative) {
                    value = -0.0;
                    needsSignDisplay = true;
                }
            }

            auto formatDouble = [&](const String& skeleton) -> std::unique_ptr<UFormattedNumber, ICUDeleter<unumf_closeResult>> {
                auto scope = DECLARE_THROW_SCOPE(vm);

                dataLogLnIf(IntlDurationFormatInternal::verbose, skeleton);
                StringView skeletonView(skeleton);
                auto upconverted = skeletonView.upconvertedCharacters();

                UErrorCode status = U_ZERO_ERROR;
                auto numberFormatter = std::unique_ptr<UNumberFormatter, UNumberFormatterDeleter>(unumf_openForSkeletonAndLocale(upconverted.get(), skeletonView.length(), durationFormat->dataLocaleWithExtensions().data(), &status));
                if (U_FAILURE(status)) {
                    throwTypeError(globalObject, scope, "Failed to initialize NumberFormat"_s);
                    return { };
                }

                auto formattedNumber = std::unique_ptr<UFormattedNumber, ICUDeleter<unumf_closeResult>>(unumf_openResult(&status));
                if (U_FAILURE(status)) {
                    throwTypeError(globalObject, scope, "Failed to format a number."_s);
                    return { };
                }
                unumf_formatDouble(numberFormatter.get(), value, formattedNumber.get(), &status);
                if (U_FAILURE(status)) {
                    throwTypeError(globalObject, scope, "Failed to format a number."_s);
                    return { };
                }

                return formattedNumber;
            };

            auto formatIntl128AsDecimal = [&](const String& skeleton) -> std::unique_ptr<UFormattedNumber, ICUDeleter<unumf_closeResult>> {
                ASSERT(totalNanosecondsValue);
                ASSERT(unit == TemporalUnit::Second || unit == TemporalUnit::Millisecond || unit == TemporalUnit::Microsecond);

                auto scope = DECLARE_THROW_SCOPE(vm);

                dataLogLnIf(IntlDurationFormatInternal::verbose, skeleton);
                StringView skeletonView(skeleton);
                auto upconverted = skeletonView.upconvertedCharacters();

                UErrorCode status = U_ZERO_ERROR;
                auto numberFormatter = std::unique_ptr<UNumberFormatter, UNumberFormatterDeleter>(unumf_openForSkeletonAndLocale(upconverted.get(), skeletonView.length(), durationFormat->dataLocaleWithExtensions().data(), &status));
                if (U_FAILURE(status)) {
                    throwTypeError(globalObject, scope, "Failed to initialize NumberFormat"_s);
                    return { };
                }

                auto formattedNumber = std::unique_ptr<UFormattedNumber, ICUDeleter<unumf_closeResult>>(unumf_openResult(&status));
                if (U_FAILURE(status)) {
                    throwTypeError(globalObject, scope, "Failed to format a number."_s);
                    return { };
                }

                // We need to keep string alive while strSpan is in use.
                auto string = buildDecimalFormat(unit, totalNanosecondsValue.value());
                auto strSpan = string.impl()->span8();
                unumf_formatDecimal(numberFormatter.get(), reinterpret_cast<const char*>(strSpan.data()), strSpan.size(), formattedNumber.get(), &status);
                if (U_FAILURE(status)) {
                    throwTypeError(globalObject, scope, "Failed to format a number."_s);
                    return { };
                }

                return formattedNumber;
            };

            switch (style) {
            // 3.l.i. If style is "2-digit" or "numeric", then
            case IntlDurationFormat::UnitStyle::TwoDigit:
            case IntlDurationFormat::UnitStyle::Numeric: {
                // https://tc39.es/proposal-intl-duration-format/#sec-formatnumericunits
                ASSERT(unit == TemporalUnit::Hour || unit == TemporalUnit::Minute || unit == TemporalUnit::Second);

                double secondsValue = duration[TemporalUnit::Second];
                if (durationFormat->units()[static_cast<unsigned>(TemporalUnit::Millisecond)].style() == IntlDurationFormat::UnitStyle::Numeric)
                    secondsValue = secondsValue + duration[TemporalUnit::Millisecond] / 1000.0 + duration[TemporalUnit::Microsecond] / 1000000.0 + duration[TemporalUnit::Nanosecond] / 1000000000.0;

                bool needsFormatHours = duration[TemporalUnit::Hour] || durationFormat->units()[static_cast<unsigned>(TemporalUnit::Hour)].display() != IntlDurationFormat::Display::Auto;
                bool needsFormatSeconds = secondsValue || durationFormat->units()[static_cast<unsigned>(TemporalUnit::Second)].display() != IntlDurationFormat::Display::Auto;
                bool needsFormatMinutes = (needsFormatHours && needsFormatSeconds) || duration[TemporalUnit::Minute] || durationFormat->units()[static_cast<unsigned>(TemporalUnit::Minute)].display() != IntlDurationFormat::Display::Auto;

                bool needsFormat = (unit == TemporalUnit::Hour && needsFormatHours) || (unit == TemporalUnit::Minute && needsFormatMinutes) || (unit == TemporalUnit::Second && needsFormatSeconds);
                bool needsSeparator = (unit == TemporalUnit::Hour && needsFormatMinutes) || (unit == TemporalUnit::Minute && needsFormatSeconds);

                if (needsFormat) {
                    auto formattedNumber = totalNanosecondsValue ? formatIntl128AsDecimal(skeletonBuilder.toString()) : formatDouble(skeletonBuilder.toString());
                    RETURN_IF_EXCEPTION(scope, { });

                    auto formatted = formatToString(formattedNumber.get());
                    RETURN_IF_EXCEPTION(scope, { });

                    elements.append({ ElementType::Element, unit, WTFMove(formatted), value, WTFMove(formattedNumber) });
                }

                if (needsSeparator) {
                    if (separator.isNull())
                        separator = retrieveSeparator(durationFormat->dataLocaleWithExtensions(), durationFormat->numberingSystem());
                    elements.append({ ElementType::Literal, unit, separator, value, nullptr });
                }

                break;
            }
            // 3.l.ii. Else
            case IntlDurationFormat::UnitStyle::Long:
            case IntlDurationFormat::UnitStyle::Short:
            case IntlDurationFormat::UnitStyle::Narrow: {
                skeletonBuilder.append(" measure-unit/duration-"_s);
                skeletonBuilder.append(String(temporalUnitSingularPropertyName(vm, unit).uid()));
                if (style == IntlDurationFormat::UnitStyle::Long)
                    skeletonBuilder.append(" unit-width-full-name"_s);
                else if (style == IntlDurationFormat::UnitStyle::Short)
                    skeletonBuilder.append(" unit-width-short"_s);
                else {
                    ASSERT(style == IntlDurationFormat::UnitStyle::Narrow);
                    skeletonBuilder.append(" unit-width-narrow"_s);
                }

                auto formattedNumber = totalNanosecondsValue ? formatIntl128AsDecimal(skeletonBuilder.toString()) : formatDouble(skeletonBuilder.toString());
                RETURN_IF_EXCEPTION(scope, { });

                auto formatted = formatToString(formattedNumber.get());
                RETURN_IF_EXCEPTION(scope, { });

                elements.append({ ElementType::Element, unit, WTFMove(formatted), value, WTFMove(formattedNumber) });
                break;
            }
            }
            if (value)
                needsSignDisplay = true;
        }
    }

    return elements;
}

// https://tc39.es/proposal-intl-duration-format/#sec-Intl.DurationFormat.prototype.format
JSValue IntlDurationFormat::format(JSGlobalObject* globalObject, ISO8601::Duration duration) const
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto elements = collectElements(globalObject, this, WTFMove(duration));
    RETURN_IF_EXCEPTION(scope, { });

    Vector<String, 4> stringList;
    for (unsigned index = 0; index < elements.size(); ++index) {
        StringBuilder builder;
        builder.append(elements[index].m_string);
        do {
            unsigned nextIndex = index + 1;
            if (!(nextIndex < elements.size()))
                break;
            if (elements[nextIndex].m_type != ElementType::Literal)
                break;
            builder.append(elements[nextIndex].m_string);
            ++index;
            ++nextIndex;
            if (!(nextIndex < elements.size()))
                break;
            if (elements[nextIndex].m_type != ElementType::Element)
                break;
            builder.append(elements[nextIndex].m_string);
            ++index;
        } while (true);

        stringList.append(builder.toString());
    }

    ListFormatInput input(WTFMove(stringList));

    Vector<UChar, 32> result;
    auto status = callBufferProducingFunction(ulistfmt_format, m_listFormat.get(), input.stringPointers(), input.stringLengths(), input.size(), result);
    if (U_FAILURE(status))
        return throwTypeError(globalObject, scope, "failed to format list of strings"_s);

    return jsString(vm, String(WTFMove(result)));
}

// https://tc39.es/proposal-intl-duration-format/#sec-Intl.DurationFormat.prototype.formatToParts
JSValue IntlDurationFormat::formatToParts(JSGlobalObject* globalObject, ISO8601::Duration duration) const
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto elements = collectElements(globalObject, this, WTFMove(duration));
    RETURN_IF_EXCEPTION(scope, { });

    Vector<String, 4> stringList;
    Vector<Vector<Element, 1>, 4> groupedElements;
    for (unsigned index = 0; index < elements.size(); ++index) {
        Vector<Element, 1> group;
        group.append(WTFMove(elements[index]));
        do {
            unsigned nextIndex = index + 1;
            if (!(nextIndex < elements.size()))
                break;
            if (elements[nextIndex].m_type != ElementType::Literal)
                break;
            group.append(WTFMove(elements[nextIndex]));
            ++index;
            ++nextIndex;
            if (!(nextIndex < elements.size()))
                break;
            if (elements[nextIndex].m_type != ElementType::Element)
                break;
            group.append(WTFMove(elements[nextIndex]));
            ++index;
        } while (true);

        if (group.size() == 1)
            stringList.append(group[0].m_string);
        else {
            StringBuilder builder;
            for (auto& element : group)
                builder.append(element.m_string);
            stringList.append(builder.toString());
        }
        groupedElements.append(WTFMove(group));
    }
    ASSERT(stringList.size() == groupedElements.size());

    ListFormatInput input(WTFMove(stringList));

    UErrorCode status = U_ZERO_ERROR;

    auto result = std::unique_ptr<UFormattedList, ICUDeleter<ulistfmt_closeResult>>(ulistfmt_openResult(&status));
    if (U_FAILURE(status))
        return throwTypeError(globalObject, scope, "failed to format list of strings"_s);

    ulistfmt_formatStringsToResult(m_listFormat.get(), input.stringPointers(), input.stringLengths(), input.size(), result.get(), &status);
    if (U_FAILURE(status))
        return throwTypeError(globalObject, scope, "failed to format list of strings"_s);

    // UFormattedValue is owned by UFormattedList. We do not need to close it.
    auto formattedValue = ulistfmt_resultAsValue(result.get(), &status);
    if (U_FAILURE(status))
        return throwTypeError(globalObject, scope, "failed to format list of strings"_s);

    JSArray* parts = JSArray::tryCreate(vm, globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithContiguous), 0);
    if (!parts)
        return throwOutOfMemoryError(globalObject, scope);

    int32_t formattedStringLength = 0;
    const UChar* formattedStringPointer = ufmtval_getString(formattedValue, &formattedStringLength, &status);
    if (U_FAILURE(status))
        return throwTypeError(globalObject, scope, "failed to format list of strings"_s);
    StringView resultStringView(std::span(formattedStringPointer, formattedStringLength));

    auto iterator = std::unique_ptr<UConstrainedFieldPosition, ICUDeleter<ucfpos_close>>(ucfpos_open(&status));
    if (U_FAILURE(status))
        return throwTypeError(globalObject, scope, "failed to format list of strings"_s);

    ucfpos_constrainField(iterator.get(), UFIELD_CATEGORY_LIST, ULISTFMT_ELEMENT_FIELD, &status);
    if (U_FAILURE(status))
        return throwTypeError(globalObject, scope, "failed to format list of strings"_s);

    auto literalString = jsNontrivialString(vm, "literal"_s);

    auto createPart = [&](JSString* type, JSString* value) {
        JSObject* part = constructEmptyObject(globalObject);
        part->putDirect(vm, vm.propertyNames->type, type);
        part->putDirect(vm, vm.propertyNames->value, value);
        return part;
    };

    auto pushElements = [&](JSArray* parts, unsigned elementIndex) -> void {
        auto scope = DECLARE_THROW_SCOPE(vm);
        if (elementIndex < groupedElements.size()) {
            auto& elements = groupedElements[elementIndex];
            for (auto& element : elements) {
                switch (element.m_type) {
                case ElementType::Element: {
                    UErrorCode status = U_ZERO_ERROR;
                    auto fieldItr = std::unique_ptr<UFieldPositionIterator, UFieldPositionIteratorDeleter>(ufieldpositer_open(&status));
                    if (U_FAILURE(status)) {
                        throwTypeError(globalObject, scope, "failed to open field position iterator"_s);
                        return;
                    }
                    unumf_resultGetAllFieldPositions(element.m_formattedNumber.get(), fieldItr.get(), &status);
                    if (U_FAILURE(status)) {
                        throwTypeError(globalObject, scope, "Failed to format a number."_s);
                        return;
                    }
                    IntlFieldIterator iterator(*fieldItr.get());
                    JSString* type = jsString(vm, String(temporalUnitSingularPropertyName(vm, element.m_unit).uid()));
                    IntlNumberFormat::formatToPartsInternal(globalObject, IntlNumberFormat::Style::Unit, std::signbit(element.m_value), IntlMathematicalValue::numberTypeFromDouble(element.m_value), element.m_string, iterator, parts, nullptr, type);
                    RETURN_IF_EXCEPTION(scope, void());
                    break;
                }
                case ElementType::Literal: {
                    JSString* value = jsString(vm, element.m_string);
                    JSObject* part = createPart(literalString, value);
                    parts->push(globalObject, part);
                    RETURN_IF_EXCEPTION(scope, void());
                    break;
                }
                }
            }
        }
    };

    int32_t resultLength = resultStringView.length();
    int32_t previousEndIndex = 0;
    unsigned elementIndex = 0;
    while (true) {
        bool next = ufmtval_nextPosition(formattedValue, iterator.get(), &status);
        if (U_FAILURE(status))
            return throwTypeError(globalObject, scope, "failed to format list of strings"_s);
        if (!next)
            break;

        int32_t beginIndex = 0;
        int32_t endIndex = 0;
        ucfpos_getIndexes(iterator.get(), &beginIndex, &endIndex, &status);
        if (U_FAILURE(status))
            return throwTypeError(globalObject, scope, "failed to format list of strings"_s);

        if (previousEndIndex < beginIndex) {
            auto value = jsString(vm, resultStringView.substring(previousEndIndex, beginIndex - previousEndIndex));
            JSObject* part = createPart(literalString, value);
            parts->push(globalObject, part);
            RETURN_IF_EXCEPTION(scope, { });
        }
        previousEndIndex = endIndex;
        pushElements(parts, elementIndex++);
        RETURN_IF_EXCEPTION(scope, { });
    }

    if (previousEndIndex < resultLength) {
        auto value = jsString(vm, resultStringView.substring(previousEndIndex, resultLength - previousEndIndex));
        JSObject* part = createPart(literalString, value);
        parts->push(globalObject, part);
        RETURN_IF_EXCEPTION(scope, { });
    }

    return parts;
}

// https://tc39.es/proposal-intl-duration-format/#sec-Intl.DurationFormat.prototype.resolvedOptions
JSObject* IntlDurationFormat::resolvedOptions(JSGlobalObject* globalObject) const
{
    VM& vm = globalObject->vm();
    JSObject* options = constructEmptyObject(globalObject);
    options->putDirect(vm, vm.propertyNames->locale, jsString(vm, m_locale));
    options->putDirect(vm, vm.propertyNames->numberingSystem, jsString(vm, m_numberingSystem));
    options->putDirect(vm, vm.propertyNames->style, jsNontrivialString(vm, styleString(m_style)));

    for (unsigned index = 0; index < numberOfTemporalUnits; ++index) {
        TemporalUnit unit = static_cast<TemporalUnit>(index);
        UnitData unitData = m_units[index];
        options->putDirect(vm, temporalUnitPluralPropertyName(vm, unit), jsNontrivialString(vm, unitStyleString(unitData.style())));
        options->putDirect(vm, displayName(vm, unit), jsNontrivialString(vm, displayString(unitData.display())));
    }

    if (m_fractionalDigits != fractionalDigitsUndefinedValue)
        options->putDirect(vm, vm.propertyNames->fractionalDigits, jsNumber(m_fractionalDigits));
    return options;
}

ASCIILiteral IntlDurationFormat::styleString(Style style)
{
    switch (style) {
    case Style::Long:
        return "long"_s;
    case Style::Short:
        return "short"_s;
    case Style::Narrow:
        return "narrow"_s;
    case Style::Digital:
        return "digital"_s;
    }
    ASSERT_NOT_REACHED();
    return { };
}

ASCIILiteral IntlDurationFormat::unitStyleString(UnitStyle style)
{
    switch (style) {
    case UnitStyle::Long:
        return "long"_s;
    case UnitStyle::Short:
        return "short"_s;
    case UnitStyle::Narrow:
        return "narrow"_s;
    case UnitStyle::Numeric:
        return "numeric"_s;
    case UnitStyle::TwoDigit:
        return "2-digit"_s;
    }
    ASSERT_NOT_REACHED();
    return { };
}

ASCIILiteral IntlDurationFormat::displayString(Display display)
{
    switch (display) {
    case Display::Always:
        return "always"_s;
    case Display::Auto:
        return "auto"_s;
    }
    ASSERT_NOT_REACHED();
    return { };
}

} // namespace JSC

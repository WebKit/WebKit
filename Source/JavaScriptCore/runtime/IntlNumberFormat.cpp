/*
 * Copyright (C) 2015 Andy VanWagoner (andy@vanwagoner.family)
 * Copyright (C) 2016 Sukolsak Sakshuwong (sukolsak@gmail.com)
 * Copyright (C) 2016-2021 Apple Inc. All rights reserved.
 * Copyright (C) 2020 Sony Interactive Entertainment Inc.
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
#include "IntlNumberFormat.h"

#include "Error.h"
#include "IntlNumberFormatInlines.h"
#include "IntlObjectInlines.h"
#include "JSBoundFunction.h"
#include "JSCInlines.h"
#include "ObjectConstructor.h"
#include <wtf/Range.h>
#include <wtf/unicode/icu/ICUHelpers.h>

#ifdef U_HIDE_DRAFT_API
#undef U_HIDE_DRAFT_API
#endif
#if HAVE(ICU_U_NUMBER_FORMATTER)
#include <unicode/unumberformatter.h>
#endif
#if HAVE(ICU_U_NUMBER_RANGE_FORMATTER)
#include <unicode/unumberrangeformatter.h>
#endif
#define U_HIDE_DRAFT_API 1

namespace JSC {

const ClassInfo IntlNumberFormat::s_info = { "Object"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(IntlNumberFormat) };

namespace IntlNumberFormatInternal {
static constexpr bool verbose = false;
}

#if HAVE(ICU_U_NUMBER_FORMATTER)
void UNumberFormatterDeleter::operator()(UNumberFormatter* formatter)
{
    if (formatter)
        unumf_close(formatter);
}
#endif

#if HAVE(ICU_U_NUMBER_RANGE_FORMATTER)
void UNumberRangeFormatterDeleter::operator()(UNumberRangeFormatter* formatter)
{
    if (formatter)
        unumrf_close(formatter);
}
#endif

IntlNumberFormat* IntlNumberFormat::create(VM& vm, Structure* structure)
{
    IntlNumberFormat* format = new (NotNull, allocateCell<IntlNumberFormat>(vm)) IntlNumberFormat(vm, structure);
    format->finishCreation(vm);
    return format;
}

Structure* IntlNumberFormat::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

IntlNumberFormat::IntlNumberFormat(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

void IntlNumberFormat::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));
}

template<typename Visitor>
void IntlNumberFormat::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    IntlNumberFormat* thisObject = jsCast<IntlNumberFormat*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());

    Base::visitChildren(thisObject, visitor);

    visitor.append(thisObject->m_boundFormat);
}

DEFINE_VISIT_CHILDREN(IntlNumberFormat);

Vector<String> IntlNumberFormat::localeData(const String& locale, RelevantExtensionKey key)
{
    // 9.1 Internal slots of Service Constructors & 11.2.3 Internal slots (ECMA-402 2.0)
    ASSERT_UNUSED(key, key == RelevantExtensionKey::Nu);
    return numberingSystemsForLocale(locale);
}

static inline unsigned computeCurrencySortKey(const String& currency)
{
    ASSERT(currency.length() == 3);
    ASSERT(currency.isAllSpecialCharacters<isASCIIUpper>());
    return (currency[0] << 16) + (currency[1] << 8) + currency[2];
}

static inline unsigned computeCurrencySortKey(const char* currency)
{
    ASSERT(strlen(currency) == 3);
    ASSERT(isAllSpecialCharacters<isASCIIUpper>(currency, 3));
    return (currency[0] << 16) + (currency[1] << 8) + currency[2];
}

static unsigned extractCurrencySortKey(std::pair<const char*, unsigned>* currencyMinorUnit)
{
    return computeCurrencySortKey(currencyMinorUnit->first);
}

static unsigned computeCurrencyDigits(const String& currency)
{
    // 11.1.1 The abstract operation CurrencyDigits (currency)
    // "If the ISO 4217 currency and funds code list contains currency as an alphabetic code,
    // then return the minor unit value corresponding to the currency from the list; else return 2.
    static constexpr std::pair<const char*, unsigned> currencyMinorUnits[] = {
        { "BHD", 3 },
        { "BIF", 0 },
        { "BYR", 0 },
        { "CLF", 4 },
        { "CLP", 0 },
        { "DJF", 0 },
        { "GNF", 0 },
        { "IQD", 3 },
        { "ISK", 0 },
        { "JOD", 3 },
        { "JPY", 0 },
        { "KMF", 0 },
        { "KRW", 0 },
        { "KWD", 3 },
        { "LYD", 3 },
        { "OMR", 3 },
        { "PYG", 0 },
        { "RWF", 0 },
        { "TND", 3 },
        { "UGX", 0 },
        { "UYI", 0 },
        { "VND", 0 },
        { "VUV", 0 },
        { "XAF", 0 },
        { "XOF", 0 },
        { "XPF", 0 }
    };
    auto* currencyMinorUnit = tryBinarySearch<std::pair<const char*, unsigned>>(currencyMinorUnits, WTF_ARRAY_LENGTH(currencyMinorUnits), computeCurrencySortKey(currency), extractCurrencySortKey);
    if (currencyMinorUnit)
        return currencyMinorUnit->second;
    return 2;
}

static std::optional<MeasureUnit> sanctionedSimpleUnitIdentifier(StringView unitIdentifier)
{
    ASSERT(
        std::is_sorted(std::begin(simpleUnits), std::end(simpleUnits),
            [](const MeasureUnit& a, const MeasureUnit& b) {
                return WTF::codePointCompare(StringView(a.subType), StringView(b.subType)) < 0;
            }));

    auto iterator = std::lower_bound(std::begin(simpleUnits), std::end(simpleUnits), unitIdentifier,
        [](const MeasureUnit& unit, StringView unitIdentifier) {
            return WTF::codePointCompare(StringView(unit.subType), unitIdentifier) < 0;
        });
    if (iterator != std::end(simpleUnits) && iterator->subType == unitIdentifier)
        return *iterator;
    return std::nullopt;
}

struct WellFormedUnit {
public:
    explicit WellFormedUnit(MeasureUnit numerator)
        : numerator(numerator)
    {
    }

    WellFormedUnit(MeasureUnit numerator, MeasureUnit denominator)
        : numerator(numerator)
        , denominator(denominator)
    {
    }

    MeasureUnit numerator;
    std::optional<MeasureUnit> denominator;
};

static std::optional<WellFormedUnit> wellFormedUnitIdentifier(StringView unitIdentifier)
{
    // https://tc39.es/ecma402/#sec-iswellformedunitidentifier
    if (auto unit = sanctionedSimpleUnitIdentifier(unitIdentifier))
        return WellFormedUnit(unit.value());

    // If the substring "-per-" does not occur exactly once in unitIdentifier, then return false.
    auto per = StringView("-per-"_s);
    auto position = unitIdentifier.find(per);
    if (position == WTF::notFound)
        return std::nullopt;
    if (unitIdentifier.find(per, position + per.length()) != WTF::notFound)
        return std::nullopt;

    // If the result of IsSanctionedSimpleUnitIdentifier(numerator) is false, then return false.
    auto numerator = unitIdentifier.left(position);
    auto numeratorUnit = sanctionedSimpleUnitIdentifier(numerator);
    if (!numeratorUnit)
        return std::nullopt;

    // If the result of IsSanctionedSimpleUnitIdentifier(denominator) is false, then return false.
    auto denominator = unitIdentifier.substring(position + per.length());
    auto denominatorUnit = sanctionedSimpleUnitIdentifier(denominator);
    if (!denominatorUnit)
        return std::nullopt;

    return WellFormedUnit(numeratorUnit.value(), denominatorUnit.value());
}

static ASCIILiteral partTypeString(UNumberFormatFields field, IntlNumberFormat::Style style, bool sign, IntlMathematicalValue::NumberType type)
{
    switch (field) {
    case UNUM_INTEGER_FIELD:
        switch (type) {
        case IntlMathematicalValue::NumberType::NaN:
            return "nan"_s;
        case IntlMathematicalValue::NumberType::Infinity:
            return "infinity"_s;
        case IntlMathematicalValue::NumberType::Integer:
            return "integer"_s;
        }
        ASSERT_NOT_REACHED();
        return "unknown"_s;
    case UNUM_FRACTION_FIELD:
        return "fraction"_s;
    case UNUM_DECIMAL_SEPARATOR_FIELD:
        return "decimal"_s;
    case UNUM_EXPONENT_SYMBOL_FIELD:
        return "exponentSeparator"_s;
    case UNUM_EXPONENT_SIGN_FIELD:
        return "exponentMinusSign"_s;
    case UNUM_EXPONENT_FIELD:
        return "exponentInteger"_s;
    case UNUM_GROUPING_SEPARATOR_FIELD:
        return "group"_s;
    case UNUM_CURRENCY_FIELD:
        return "currency"_s;
    case UNUM_PERCENT_FIELD:
        // If the style is "unit", we should report as unit.
        // JSTests/test262/test/intl402/NumberFormat/prototype/formatToParts/percent-en-US.js
        return (style == IntlNumberFormat::Style::Unit) ? "unit"_s : "percentSign"_s;
    case UNUM_SIGN_FIELD:
        return sign ? "minusSign"_s : "plusSign"_s;
#if HAVE(ICU_U_NUMBER_FORMATTER)
    case UNUM_MEASURE_UNIT_FIELD:
        return "unit"_s;
    case UNUM_COMPACT_FIELD:
        return "compact"_s;
#endif
    // These should not show up because there is no way to specify them in NumberFormat options.
    // If they do, they don't fit well into any of known part types, so consider it an "unknown".
    case UNUM_PERMILL_FIELD:
    // Any newer additions to the UNumberFormatFields enum should just be considered an "unknown" part.
    default:
        return "unknown"_s;
    }
    return "unknown"_s;
}

// https://tc39.github.io/ecma402/#sec-initializenumberformat
void IntlNumberFormat::initializeNumberFormat(JSGlobalObject* globalObject, JSValue locales, JSValue optionsValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto requestedLocales = canonicalizeLocaleList(globalObject, locales);
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

    const auto& availableLocales = intlNumberFormatAvailableLocales();
    auto resolved = resolveLocale(globalObject, availableLocales, requestedLocales, localeMatcher, localeOptions, { RelevantExtensionKey::Nu }, localeData);

    m_locale = resolved.locale;
    if (m_locale.isEmpty()) {
        throwTypeError(globalObject, scope, "failed to initialize NumberFormat due to invalid locale"_s);
        return;
    }

    m_numberingSystem = resolved.extensions[static_cast<unsigned>(RelevantExtensionKey::Nu)];

    m_style = intlOption<Style>(globalObject, options, vm.propertyNames->style, { { "decimal"_s, Style::Decimal }, { "percent"_s, Style::Percent }, { "currency"_s, Style::Currency }, { "unit"_s, Style::Unit } }, "style must be either \"decimal\", \"percent\", \"currency\", or \"unit\""_s, Style::Decimal);
    RETURN_IF_EXCEPTION(scope, void());

    String currency = intlStringOption(globalObject, options, Identifier::fromString(vm, "currency"_s), { }, { }, { });
    RETURN_IF_EXCEPTION(scope, void());
    if (!currency.isNull()) {
        if (!isWellFormedCurrencyCode(currency)) {
            throwException(globalObject, scope, createRangeError(globalObject, "currency is not a well-formed currency code"_s));
            return;
        }
    }

    unsigned currencyDigits = 0;
    if (m_style == Style::Currency) {
        if (currency.isNull()) {
            throwTypeError(globalObject, scope, "currency must be a string"_s);
            return;
        }

        currency = currency.convertToASCIIUppercase();
        m_currency = currency;
        currencyDigits = computeCurrencyDigits(currency);
    }

    m_currencyDisplay = intlOption<CurrencyDisplay>(globalObject, options, Identifier::fromString(vm, "currencyDisplay"_s), { { "code"_s, CurrencyDisplay::Code }, { "symbol"_s, CurrencyDisplay::Symbol }, { "narrowSymbol"_s, CurrencyDisplay::NarrowSymbol }, { "name"_s, CurrencyDisplay::Name } }, "currencyDisplay must be either \"code\", \"symbol\", or \"name\""_s, CurrencyDisplay::Symbol);
    RETURN_IF_EXCEPTION(scope, void());

    m_currencySign = intlOption<CurrencySign>(globalObject, options, Identifier::fromString(vm, "currencySign"_s), { { "standard"_s, CurrencySign::Standard }, { "accounting"_s, CurrencySign::Accounting } }, "currencySign must be either \"standard\" or \"accounting\""_s, CurrencySign::Standard);
    RETURN_IF_EXCEPTION(scope, void());

    String unit = intlStringOption(globalObject, options, Identifier::fromString(vm, "unit"_s), { }, { }, { });
    RETURN_IF_EXCEPTION(scope, void());
    std::optional<WellFormedUnit> wellFormedUnit;
    if (!unit.isNull()) {
        wellFormedUnit = wellFormedUnitIdentifier(unit);
        if (!wellFormedUnit) {
            throwRangeError(globalObject, scope, "unit is not a well-formed unit identifier"_s);
            return;
        }
        m_unit = unit;
    } else if (m_style == Style::Unit) {
        throwTypeError(globalObject, scope, "unit must be a string"_s);
        return;
    }

    m_unitDisplay = intlOption<UnitDisplay>(globalObject, options, Identifier::fromString(vm, "unitDisplay"_s), { { "short"_s, UnitDisplay::Short }, { "narrow"_s, UnitDisplay::Narrow }, { "long"_s, UnitDisplay::Long } }, "unitDisplay must be either \"short\", \"narrow\", or \"long\""_s, UnitDisplay::Short);
    RETURN_IF_EXCEPTION(scope, void());

    unsigned minimumFractionDigitsDefault = (m_style == Style::Currency) ? currencyDigits : 0;
    unsigned maximumFractionDigitsDefault = (m_style == Style::Currency) ? currencyDigits : (m_style == Style::Percent) ? 0 : 3;

    m_notation = intlOption<IntlNotation>(globalObject, options, Identifier::fromString(vm, "notation"_s), { { "standard"_s, IntlNotation::Standard }, { "scientific"_s, IntlNotation::Scientific }, { "engineering"_s, IntlNotation::Engineering }, { "compact"_s, IntlNotation::Compact } }, "notation must be either \"standard\", \"scientific\", \"engineering\", or \"compact\""_s, IntlNotation::Standard);
    RETURN_IF_EXCEPTION(scope, void());

    setNumberFormatDigitOptions(globalObject, this, options, minimumFractionDigitsDefault, maximumFractionDigitsDefault, m_notation);
    RETURN_IF_EXCEPTION(scope, void());

    m_roundingIncrement = intlNumberOption(globalObject, options, vm.propertyNames->roundingIncrement, 1, 5000, 1);
    RETURN_IF_EXCEPTION(scope, void());
    static constexpr const unsigned roundingIncrementCandidates[] = {
        1, 2, 5, 10, 20, 25, 50, 100, 200, 250, 500, 1000, 2000, 2500, 5000
    };
    if (std::none_of(roundingIncrementCandidates, roundingIncrementCandidates + std::size(roundingIncrementCandidates),
        [&](unsigned candidate) {
            return candidate == m_roundingIncrement;
        })) {
        throwRangeError(globalObject, scope, "roundingIncrement must be one of 1, 2, 5, 10, 20, 25, 50, 100, 200, 250, 500, 1000, 2000, 2500, 5000"_s);
        return;
    }
    if (m_roundingIncrement != 1) {
        if (m_roundingType != IntlRoundingType::FractionDigits) {
            throwTypeError(globalObject, scope, "rounding type is not fraction-digits while roundingIncrement is specified"_s);
            return;
        }
        // FIXME: The proposal has m_maximumFractionDigits != m_minimumFractionDigits check here, but it breaks the use case.
        // We intentionally do not follow to that here until the issue is fixed.
        // https://github.com/tc39/proposal-intl-numberformat-v3/issues/97
    }

    m_trailingZeroDisplay = intlOption<TrailingZeroDisplay>(globalObject, options, vm.propertyNames->trailingZeroDisplay, { { "auto"_s, TrailingZeroDisplay::Auto }, { "stripIfInteger"_s, TrailingZeroDisplay::StripIfInteger } }, "trailingZeroDisplay must be either \"auto\" or \"stripIfInteger\""_s, TrailingZeroDisplay::Auto);
    RETURN_IF_EXCEPTION(scope, void());

    m_compactDisplay = intlOption<CompactDisplay>(globalObject, options, Identifier::fromString(vm, "compactDisplay"_s), { { "short"_s, CompactDisplay::Short }, { "long"_s, CompactDisplay::Long } }, "compactDisplay must be either \"short\" or \"long\""_s, CompactDisplay::Short);
    RETURN_IF_EXCEPTION(scope, void());

    UseGrouping defaultUseGrouping = UseGrouping::Auto;
    if (m_notation == IntlNotation::Compact)
        defaultUseGrouping = UseGrouping::Min2;

    m_useGrouping = intlStringOrBooleanOption<UseGrouping>(globalObject, options, Identifier::fromString(vm, "useGrouping"_s), UseGrouping::Always, UseGrouping::False, { { "min2"_s, UseGrouping::Min2 }, { "auto"_s, UseGrouping::Auto }, { "always"_s, UseGrouping::Always } }, "useGrouping must be either true, false, \"min2\", \"auto\", or \"always\""_s, defaultUseGrouping);
    RETURN_IF_EXCEPTION(scope, void());

    m_signDisplay = intlOption<SignDisplay>(globalObject, options, Identifier::fromString(vm, "signDisplay"_s), { { "auto"_s, SignDisplay::Auto }, { "never"_s, SignDisplay::Never }, { "always"_s, SignDisplay::Always }, { "exceptZero"_s, SignDisplay::ExceptZero }, { "negative"_s, SignDisplay::Negative } }, "signDisplay must be either \"auto\", \"never\", \"always\", \"exceptZero\", or \"negative\""_s, SignDisplay::Auto);
    RETURN_IF_EXCEPTION(scope, void());

    m_roundingMode = intlOption<RoundingMode>(globalObject, options, vm.propertyNames->roundingMode, {
            { "ceil"_s, RoundingMode::Ceil },
            { "floor"_s, RoundingMode::Floor },
            { "expand"_s, RoundingMode::Expand },
            { "trunc"_s, RoundingMode::Trunc },
            { "halfCeil"_s, RoundingMode::HalfCeil },
            { "halfFloor"_s, RoundingMode::HalfFloor },
            { "halfExpand"_s, RoundingMode::HalfExpand },
            { "halfTrunc"_s, RoundingMode::HalfTrunc },
            { "halfEven"_s, RoundingMode::HalfEven }
        }, "roundingMode must be either \"ceil\", \"floor\", \"expand\", \"trunc\", \"halfCeil\", \"halfFloor\", \"halfExpand\", \"halfTrunc\", or \"halfEven\""_s, RoundingMode::HalfExpand);
    RETURN_IF_EXCEPTION(scope, void());

    CString dataLocaleWithExtensions = makeString(resolved.dataLocale, "-u-nu-", m_numberingSystem).utf8();
    dataLogLnIf(IntlNumberFormatInternal::verbose, "dataLocaleWithExtensions:(", dataLocaleWithExtensions , ")");

    // Options are obtained. Configure formatter here.

#if HAVE(ICU_U_NUMBER_FORMATTER)
    // Constructing ICU Number Skeletons to configure UNumberFormatter.
    // https://github.com/unicode-org/icu/blob/master/docs/userguide/format_parse/numbers/skeletons.md

    StringBuilder skeletonBuilder;

    switch (m_roundingMode) {
    case RoundingMode::Ceil:
        skeletonBuilder.append("rounding-mode-ceiling");
        break;
    case RoundingMode::Floor:
        skeletonBuilder.append("rounding-mode-floor");
        break;
    case RoundingMode::Expand:
        skeletonBuilder.append("rounding-mode-up");
        break;
    case RoundingMode::Trunc:
        skeletonBuilder.append("rounding-mode-down");
        break;
    case RoundingMode::HalfCeil: {
        // Only ICU69~ supports half-ceiling. Ignore this option if linked ICU does not support it.
        // https://github.com/unicode-org/icu/commit/e8dfea9bb6bb27596731173b352759e44ad06b21
        if (WTF::ICU::majorVersion() >= 69)
            skeletonBuilder.append("rounding-mode-half-ceiling");
        else
            skeletonBuilder.append("rounding-mode-half-up"); // Default option.
        break;
    }
    case RoundingMode::HalfFloor: {
        // Only ICU69~ supports half-ceil. Ignore this option if linked ICU does not support it.
        // https://github.com/unicode-org/icu/commit/e8dfea9bb6bb27596731173b352759e44ad06b21
        if (WTF::ICU::majorVersion() >= 69)
            skeletonBuilder.append("rounding-mode-half-floor");
        else
            skeletonBuilder.append("rounding-mode-half-up"); // Default option.
        break;
    }
    case RoundingMode::HalfExpand:
        skeletonBuilder.append("rounding-mode-half-up");
        break;
    case RoundingMode::HalfTrunc:
        skeletonBuilder.append("rounding-mode-half-down");
        break;
    case RoundingMode::HalfEven:
        skeletonBuilder.append("rounding-mode-half-even");
        break;
    }

    switch (m_style) {
    case Style::Decimal:
        // No skeleton is needed.
        break;
    case Style::Percent:
        skeletonBuilder.append(" percent scale/100");
        break;
    case Style::Currency: {
        skeletonBuilder.append(" currency/", currency);

        // https://github.com/unicode-org/icu/blob/master/docs/userguide/format_parse/numbers/skeletons.md#unit-width
        switch (m_currencyDisplay) {
        case CurrencyDisplay::Code:
            skeletonBuilder.append(" unit-width-iso-code");
            break;
        case CurrencyDisplay::Symbol:
            // Default option. Do not specify unit-width.
            break;
        case CurrencyDisplay::NarrowSymbol:
            skeletonBuilder.append(" unit-width-narrow");
            break;
        case CurrencyDisplay::Name:
            skeletonBuilder.append(" unit-width-full-name");
            break;
        }
        break;
    }
    case Style::Unit: {
        // The measure-unit stem takes one required option: the unit identifier of the unit to be formatted.
        // The full unit identifier is required: both the type and the subtype (for example, length-meter).
        // https://github.com/unicode-org/icu/blob/master/docs/userguide/format_parse/numbers/skeletons.md#unit
        skeletonBuilder.append(" measure-unit/");
        auto numeratorUnit = wellFormedUnit->numerator;
        skeletonBuilder.append(numeratorUnit.type, '-', numeratorUnit.subType);
        if (auto denominatorUnitValue = wellFormedUnit->denominator) {
            auto denominatorUnit = denominatorUnitValue.value();
            skeletonBuilder.append(" per-measure-unit/", denominatorUnit.type, '-', denominatorUnit.subType);
        }

        // https://github.com/unicode-org/icu/blob/master/docs/userguide/format_parse/numbers/skeletons.md#unit-width
        switch (m_unitDisplay) {
        case UnitDisplay::Short:
            skeletonBuilder.append(" unit-width-short");
            break;
        case UnitDisplay::Narrow:
            skeletonBuilder.append(" unit-width-narrow");
            break;
        case UnitDisplay::Long:
            skeletonBuilder.append(" unit-width-full-name");
            break;
        }
        break;
    }
    }

    appendNumberFormatDigitOptionsToSkeleton(this, skeletonBuilder);

    // Configure this just after precision.
    // https://github.com/unicode-org/icu/blob/main/docs/userguide/format_parse/numbers/skeletons.md#trailing-zero-display
    switch (m_trailingZeroDisplay) {
    case TrailingZeroDisplay::Auto:
        break;
    case TrailingZeroDisplay::StripIfInteger:
        // Only ICU69~ supports trailing zero display. Ignore this option if linked ICU does not support it.
        // https://github.com/unicode-org/icu/commit/b79c299f90d4023ac237db3d0335d568bf21cd36
        if (WTF::ICU::majorVersion() >= 69)
            skeletonBuilder.append("/w");
        break;
    }

    // https://github.com/unicode-org/icu/blob/master/docs/userguide/format_parse/numbers/skeletons.md#notation
    switch (m_notation) {
    case IntlNotation::Standard:
        break;
    case IntlNotation::Scientific:
        skeletonBuilder.append(" scientific");
        break;
    case IntlNotation::Engineering:
        skeletonBuilder.append(" engineering");
        break;
    case IntlNotation::Compact:
        switch (m_compactDisplay) {
        case CompactDisplay::Short:
            skeletonBuilder.append(" compact-short");
            break;
        case CompactDisplay::Long:
            skeletonBuilder.append(" compact-long");
            break;
        }
        break;
    }

    // https://github.com/unicode-org/icu/blob/master/docs/userguide/format_parse/numbers/skeletons.md#sign-display
    // CurrencySign's accounting is a part of SignDisplay in ICU.
    bool useAccounting = (m_style == Style::Currency && m_currencySign == CurrencySign::Accounting);
    switch (m_signDisplay) {
    case SignDisplay::Auto:
        if (useAccounting)
            skeletonBuilder.append(" sign-accounting");
        else
            skeletonBuilder.append(" sign-auto");
        break;
    case SignDisplay::Never:
        skeletonBuilder.append(" sign-never");
        break;
    case SignDisplay::Always:
        if (useAccounting)
            skeletonBuilder.append(" sign-accounting-always");
        else
            skeletonBuilder.append(" sign-always");
        break;
    case SignDisplay::ExceptZero:
        if (useAccounting)
            skeletonBuilder.append(" sign-accounting-except-zero");
        else
            skeletonBuilder.append(" sign-except-zero");
        break;
    case SignDisplay::Negative:
        // Only ICU69~ supports negative sign display. Ignore this option if linked ICU does not support it.
        // https://github.com/unicode-org/icu/commit/1aa0dad8e06ecc99bff442dd37f6daa2d39d9a5a
        if (WTF::ICU::majorVersion() >= 69) {
            if (useAccounting)
                skeletonBuilder.append(" sign-accounting-negative");
            else
                skeletonBuilder.append(" sign-negative");
        }
        break;
    }

    // https://github.com/tc39/proposal-intl-numberformat-v3/issues/3
    // https://github.com/unicode-org/icu/blob/main/docs/userguide/format_parse/numbers/skeletons.md#grouping
    switch (m_useGrouping) {
    case UseGrouping::False:
        skeletonBuilder.append(" group-off");
        break;
    case UseGrouping::Min2:
        skeletonBuilder.append(" group-min2");
        break;
    case UseGrouping::Auto:
        skeletonBuilder.append(" group-auto");
        break;
    case UseGrouping::Always:
        skeletonBuilder.append(" group-on-aligned");
        break;
    }

    String skeleton = skeletonBuilder.toString();
    dataLogLnIf(IntlNumberFormatInternal::verbose, skeleton);
    StringView skeletonView(skeleton);
    auto upconverted = skeletonView.upconvertedCharacters();

    UErrorCode status = U_ZERO_ERROR;
    m_numberFormatter = std::unique_ptr<UNumberFormatter, UNumberFormatterDeleter>(unumf_openForSkeletonAndLocale(upconverted.get(), skeletonView.length(), dataLocaleWithExtensions.data(), &status));
    if (U_FAILURE(status)) {
        throwTypeError(globalObject, scope, "Failed to initialize NumberFormat"_s);
        return;
    }

#if HAVE(ICU_U_NUMBER_RANGE_FORMATTER)
    m_numberRangeFormatter = std::unique_ptr<UNumberRangeFormatter, UNumberRangeFormatterDeleter>(unumrf_openForSkeletonWithCollapseAndIdentityFallback(upconverted.get(), skeletonView.length(), UNUM_RANGE_COLLAPSE_AUTO, UNUM_IDENTITY_FALLBACK_APPROXIMATELY, dataLocaleWithExtensions.data(), nullptr, &status));
    if (U_FAILURE(status)) {
        throwTypeError(globalObject, scope, "failed to initialize NumberFormat"_s);
        return;
    }
#endif
#else
    UNumberFormatStyle style = UNUM_DEFAULT;
    switch (m_style) {
    case Style::Decimal:
        style = UNUM_DECIMAL;
        break;
    case Style::Percent:
        style = UNUM_PERCENT;
        break;
    case Style::Currency:
        switch (m_currencyDisplay) {
        case CurrencyDisplay::Code:
            style = UNUM_CURRENCY_ISO;
            break;
        case CurrencyDisplay::Symbol:
            style = UNUM_CURRENCY;
            break;
        case CurrencyDisplay::NarrowSymbol:
            style = UNUM_CURRENCY; // Use the same option to "symbol" since linked-ICU does not support it.
            break;
        case CurrencyDisplay::Name:
            style = UNUM_CURRENCY_PLURAL;
            break;
        }
        switch (m_currencySign) {
        case CurrencySign::Standard:
            break;
        case CurrencySign::Accounting:
            // Ignore this case since linked ICU does not support it.
            break;
        }
        break;
    case Style::Unit:
        // Ignore this case since linked ICU does not support it.
        break;
    }

    switch (m_notation) {
    case IntlNotation::Standard:
        break;
    case IntlNotation::Scientific:
    case IntlNotation::Engineering:
    case IntlNotation::Compact:
        // Ignore this case since linked ICU does not support it.
        break;
    }

    switch (m_signDisplay) {
    case SignDisplay::Auto:
        break;
    case SignDisplay::Never:
    case SignDisplay::Always:
    case SignDisplay::ExceptZero:
    case SignDisplay::Negative:
        // Ignore this case since linked ICU does not support it.
        break;
    }

    UErrorCode status = U_ZERO_ERROR;
    m_numberFormat = std::unique_ptr<UNumberFormat, ICUDeleter<unum_close>>(unum_open(style, nullptr, 0, dataLocaleWithExtensions.data(), nullptr, &status));
    if (U_FAILURE(status)) {
        throwTypeError(globalObject, scope, "failed to initialize NumberFormat"_s);
        return;
    }

    if (m_style == Style::Currency) {
        unum_setTextAttribute(m_numberFormat.get(), UNUM_CURRENCY_CODE, StringView(m_currency).upconvertedCharacters(), m_currency.length(), &status);
        if (U_FAILURE(status)) {
            throwTypeError(globalObject, scope, "failed to initialize NumberFormat"_s);
            return;
        }
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
    case IntlRoundingType::MorePrecision:
        // Ignore this case since linked ICU does not support it.
        break;
    case IntlRoundingType::LessPrecision:
        // Ignore this case since linked ICU does not support it.
        break;
    }

    switch (m_useGrouping) {
    case UseGrouping::False:
        unum_setAttribute(m_numberFormat.get(), UNUM_GROUPING_USED, false);
        break;
    case UseGrouping::Min2:
        // Ignore this case since linked ICU does not support it.
        break;
    case UseGrouping::Auto:
        break;
    case UseGrouping::Always:
        unum_setAttribute(m_numberFormat.get(), UNUM_GROUPING_USED, true);
        break;
    }
    unum_setAttribute(m_numberFormat.get(), UNUM_ROUNDING_MODE, UNUM_ROUND_HALFUP);
#endif
}

// https://tc39.es/ecma402/#sec-formatnumber
JSValue IntlNumberFormat::format(JSGlobalObject* globalObject, double value) const
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    Vector<UChar, 32> buffer;
#if HAVE(ICU_U_NUMBER_FORMATTER)
    ASSERT(m_numberFormatter);
    UErrorCode status = U_ZERO_ERROR;
    auto formattedNumber = std::unique_ptr<UFormattedNumber, ICUDeleter<unumf_closeResult>>(unumf_openResult(&status));
    if (U_FAILURE(status))
        return throwTypeError(globalObject, scope, "Failed to format a number."_s);
    unumf_formatDouble(m_numberFormatter.get(), value, formattedNumber.get(), &status);
    if (U_FAILURE(status))
        return throwTypeError(globalObject, scope, "Failed to format a number."_s);
    status = callBufferProducingFunction(unumf_resultToString, formattedNumber.get(), buffer);
    if (U_FAILURE(status))
        return throwTypeError(globalObject, scope, "Failed to format a number."_s);
#else
    ASSERT(m_numberFormat);
    auto status = callBufferProducingFunction(unum_formatDouble, m_numberFormat.get(), value, buffer, nullptr);
    if (U_FAILURE(status))
        return throwTypeError(globalObject, scope, "Failed to format a number."_s);
#endif
    return jsString(vm, String(WTFMove(buffer)));
}

// https://tc39.es/ecma402/#sec-formatnumber
JSValue IntlNumberFormat::format(JSGlobalObject* globalObject, IntlMathematicalValue&& value) const
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    value.ensureNonDouble();
    const auto& string = value.getString();

    Vector<UChar, 32> buffer;
#if HAVE(ICU_U_NUMBER_FORMATTER)
    ASSERT(m_numberFormatter);
    UErrorCode status = U_ZERO_ERROR;
    auto formattedNumber = std::unique_ptr<UFormattedNumber, ICUDeleter<unumf_closeResult>>(unumf_openResult(&status));
    if (U_FAILURE(status))
        return throwTypeError(globalObject, scope, "Failed to format a BigInt."_s);
    unumf_formatDecimal(m_numberFormatter.get(), string.data(), string.length(), formattedNumber.get(), &status);
    if (U_FAILURE(status))
        return throwTypeError(globalObject, scope, "Failed to format a BigInt."_s);
    status = callBufferProducingFunction(unumf_resultToString, formattedNumber.get(), buffer);
    if (U_FAILURE(status))
        return throwTypeError(globalObject, scope, "Failed to format a BigInt."_s);
#else
    ASSERT(m_numberFormat);
    auto status = callBufferProducingFunction(unum_formatDecimal, m_numberFormat.get(), string.data(), string.length(), buffer, nullptr);
    if (U_FAILURE(status))
        return throwTypeError(globalObject, scope, "Failed to format a BigInt."_s);
#endif
    return jsString(vm, String(WTFMove(buffer)));
}

#if HAVE(ICU_U_NUMBER_RANGE_FORMATTER)
JSValue IntlNumberFormat::formatRange(JSGlobalObject* globalObject, double start, double end) const
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    ASSERT(m_numberRangeFormatter);

    if (std::isnan(start) || std::isnan(end))
        return throwRangeError(globalObject, scope, "Passed numbers are out of range"_s);

    UErrorCode status = U_ZERO_ERROR;
    auto range = std::unique_ptr<UFormattedNumberRange, ICUDeleter<unumrf_closeResult>>(unumrf_openResult(&status));
    if (U_FAILURE(status))
        return throwTypeError(globalObject, scope, "failed to format a range"_s);

    unumrf_formatDoubleRange(m_numberRangeFormatter.get(), start, end, range.get(), &status);
    if (U_FAILURE(status))
        return throwTypeError(globalObject, scope, "failed to format a range"_s);

    auto* formattedValue = unumrf_resultAsValue(range.get(), &status);
    if (U_FAILURE(status))
        return throwTypeError(globalObject, scope, "failed to format a range"_s);

    int32_t length = 0;
    const UChar* string = ufmtval_getString(formattedValue, &length, &status);
    if (U_FAILURE(status))
        return throwTypeError(globalObject, scope, "failed to format a range"_s);

    return jsString(vm, String(string, length));
}

JSValue IntlNumberFormat::formatRange(JSGlobalObject* globalObject, IntlMathematicalValue&& start, IntlMathematicalValue&& end) const
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    ASSERT(m_numberRangeFormatter);

    if (start.numberType() == IntlMathematicalValue::NumberType::NaN || end.numberType() == IntlMathematicalValue::NumberType::NaN)
        return throwRangeError(globalObject, scope, "Passed numbers are out of range"_s);

    start.ensureNonDouble();
    const auto& startString = start.getString();

    end.ensureNonDouble();
    const auto& endString = end.getString();

    UErrorCode status = U_ZERO_ERROR;
    auto range = std::unique_ptr<UFormattedNumberRange, ICUDeleter<unumrf_closeResult>>(unumrf_openResult(&status));
    if (U_FAILURE(status))
        return throwTypeError(globalObject, scope, "failed to format a range"_s);

    unumrf_formatDecimalRange(m_numberRangeFormatter.get(), startString.data(), startString.length(), endString.data(), endString.length(), range.get(), &status);
    if (U_FAILURE(status))
        return throwTypeError(globalObject, scope, "failed to format a range"_s);

    auto* formattedValue = unumrf_resultAsValue(range.get(), &status);
    if (U_FAILURE(status))
        return throwTypeError(globalObject, scope, "failed to format a range"_s);

    int32_t length = 0;
    const UChar* string = ufmtval_getString(formattedValue, &length, &status);
    if (U_FAILURE(status))
        return throwTypeError(globalObject, scope, "failed to format a range"_s);

    return jsString(vm, String(string, length));
}
#endif

static constexpr int32_t literalField = -1;
struct IntlNumberFormatField {
    int32_t m_field;
    WTF::Range<int32_t> m_range;
};

static Vector<IntlNumberFormatField> flattenFields(Vector<IntlNumberFormatField>&& fields, int32_t formattedStringLength)
{
    // ICU generates sequence of nested fields, but ECMA402 requires non-overlapping sequence of parts.
    // This function flattens nested fields into sequence of non-overlapping parts.
    //
    // Formatted string: "100,000–20,000,000"
    //                    |  |  | | |   |  |
    //                    |  B  | | E   F  |
    //                    |     | |        |
    //                    +--C--+ +---G----+
    //                    +--A--+ +---D----+
    //
    // Ranges ICU generates:
    //     A:    (0, 7)   UFIELD_CATEGORY_NUMBER_RANGE_SPAN startRange
    //     B:    (3, 4)   UFIELD_CATEGORY_NUMBER group ","
    //     C:    (0, 7)   UFIELD_CATEGORY_NUMBER integer
    //     D:    (8, 18)  UFIELD_CATEGORY_NUMBER_RANGE_SPAN endRange
    //     E:    (10, 11) UFIELD_CATEGORY_NUMBER group ","
    //     F:    (14, 15) UFIELD_CATEGORY_NUMBER group ","
    //     G:    (8, 18)  UFIELD_CATEGORY_NUMBER integer
    //
    // Then, we need to generate:
    //     A:    (0, 3)   startRange integer
    //     B:    (3, 4)   startRange group ","
    //     C:    (4, 7)   startRange integer
    //     D:    (7, 8)   shared     literal "-"
    //     E:    (8, 10)  endRange   integer
    //     F:    (10, 11) endRange   group ","
    //     G:    (11, 14) endRange   integer
    //     H:    (14, 15) endRange   group ","
    //     I:    (15, 18) endRange   integer

    std::sort(fields.begin(), fields.end(), [](auto& lhs, auto& rhs) {
        if (lhs.m_range.begin() < rhs.m_range.begin())
            return true;
        if (lhs.m_range.begin() > rhs.m_range.begin())
            return false;
        if (lhs.m_range.end() < rhs.m_range.end())
            return false;
        if (lhs.m_range.end() > rhs.m_range.end())
            return true;
        return lhs.m_field < rhs.m_field;
    });

    Vector<IntlNumberFormatField> flatten;
    Vector<IntlNumberFormatField> stack;
    // Top-level field covers entire parts, which makes parts "literal".
    stack.append(IntlNumberFormatField { literalField, { 0, formattedStringLength } });

    unsigned cursor = 0;
    int32_t begin = 0;
    while (cursor < fields.size()) {
        const auto& field = fields[cursor];

        // If the new field is out of the current top-most field, roll up and insert a flatten field.
        // Because the top-level field in the stack covers all index range, this condition always becomes false
        // if stack size is 1.
        while (stack.last().m_range.end() < field.m_range.begin()) {
            if (begin < stack.last().m_range.end()) {
                IntlNumberFormatField flattenField { stack.last().m_field, { begin, stack.last().m_range.end() } };
                flatten.append(flattenField);
                begin = flattenField.m_range.end();
            }
            stack.removeLast();
        }
        ASSERT(!stack.isEmpty()); // At least, top-level field exists.

        // If the new field is starting with the same index, diving into the new field by adding it into stack.
        if (begin == field.m_range.begin()) {
            stack.append(field);
            ++cursor;
            continue;
        }

        // If there is a room between the current top-most field and the new field, insert a flatten field.
        if (begin < field.m_range.begin()) {
            IntlNumberFormatField flattenField { stack.last().m_field, { begin, field.m_range.begin() } };
            flatten.append(flattenField);
            stack.append(field);
            begin = field.m_range.begin();
            ++cursor;
            continue;
        }
    }

    // Roll up the nested field at the end of the formatted string sequence.
    // For example,
    //
    //      <------------A-------------->
    //      <--------B------------>
    //      <---C---->
    //
    // Then, after C finishes, we should insert remaining B and A.
    while (!stack.isEmpty()) {
        if (begin < stack.last().m_range.end()) {
            IntlNumberFormatField flattenField { stack.last().m_field, { begin, stack.last().m_range.end() } };
            flatten.append(flattenField);
            begin = flattenField.m_range.end();
        }
        stack.removeLast();
    }

    return flatten;
}

#if HAVE(ICU_U_NUMBER_RANGE_FORMATTER_FORMAT_RANGE_TO_PARTS)
static bool numberFieldsPracticallyEqual(const UFormattedValue* formattedValue, UErrorCode& status)
{
    auto iterator = std::unique_ptr<UConstrainedFieldPosition, ICUDeleter<ucfpos_close>>(ucfpos_open(&status));
    if (U_FAILURE(status))
        return false;

    // We only care about UFIELD_CATEGORY_NUMBER_RANGE_SPAN category.
    ucfpos_constrainCategory(iterator.get(), UFIELD_CATEGORY_NUMBER_RANGE_SPAN, &status);
    if (U_FAILURE(status))
        return false;

    bool hasSpan = ufmtval_nextPosition(formattedValue, iterator.get(), &status);
    if (U_FAILURE(status))
        return false;

    return !hasSpan;
}

void IntlNumberFormat::formatRangeToPartsInternal(JSGlobalObject* globalObject, Style style, IntlMathematicalValue&& start, IntlMathematicalValue&& end, const UFormattedValue* formattedValue, JSArray* parts)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    UErrorCode status = U_ZERO_ERROR;

    int32_t formattedStringLength = 0;
    const UChar* formattedStringPointer = ufmtval_getString(formattedValue, &formattedStringLength, &status);
    if (U_FAILURE(status)) {
        throwTypeError(globalObject, scope, "Failed to format number range"_s);
        return;
    }
    StringView resultStringView(formattedStringPointer, formattedStringLength);

    // We care multiple categories (UFIELD_CATEGORY_DATE and UFIELD_CATEGORY_DATE_INTERVAL_SPAN).
    // So we do not constraint iterator.
    auto iterator = std::unique_ptr<UConstrainedFieldPosition, ICUDeleter<ucfpos_close>>(ucfpos_open(&status));
    if (U_FAILURE(status)) {
        throwTypeError(globalObject, scope, "Failed to format number range"_s);
        return;
    }

    auto sharedString = jsNontrivialString(vm, "shared"_s);
    auto startRangeString = jsNontrivialString(vm, "startRange"_s);
    auto endRangeString = jsNontrivialString(vm, "endRange"_s);
    auto literalString = jsNontrivialString(vm, "literal"_s);

    WTF::Range<int32_t> startRange { -1, -1 };
    WTF::Range<int32_t> endRange { -1, -1 };
    Vector<IntlNumberFormatField> fields;

    while (true) {
        bool next = ufmtval_nextPosition(formattedValue, iterator.get(), &status);
        if (U_FAILURE(status)) {
            throwTypeError(globalObject, scope, "Failed to format number range"_s);
            return;
        }
        if (!next)
            break;

        int32_t category = ucfpos_getCategory(iterator.get(), &status);
        if (U_FAILURE(status)) {
            throwTypeError(globalObject, scope, "Failed to format number range"_s);
            return;
        }

        int32_t fieldType = ucfpos_getField(iterator.get(), &status);
        if (U_FAILURE(status)) {
            throwTypeError(globalObject, scope, "Failed to format number range"_s);
            return;
        }

        int32_t beginIndex = 0;
        int32_t endIndex = 0;
        ucfpos_getIndexes(iterator.get(), &beginIndex, &endIndex, &status);
        if (U_FAILURE(status)) {
            throwTypeError(globalObject, scope, "Failed to format number interval"_s);
            return;
        }

        dataLogLnIf(IntlNumberFormatInternal::verbose, category, " ", fieldType, " (", beginIndex, ", ", endIndex, ")");

        if (category != UFIELD_CATEGORY_NUMBER && category != UFIELD_CATEGORY_NUMBER_RANGE_SPAN)
            continue;
        if (category == UFIELD_CATEGORY_NUMBER && fieldType < 0)
            continue;

        if (category == UFIELD_CATEGORY_NUMBER_RANGE_SPAN) {
            // > The special field category UFIELD_CATEGORY_NUMBER_RANGE_SPAN is used to indicate which number
            // > primitives came from which arguments: 0 means start, and 1 means end. The span category
            // > will always occur before the corresponding fields in UFIELD_CATEGORY_NUMBER in the nextPosition() iterator.
            // from ICU comment. So, field 0 is startRange, field 1 is endRange.
            if (!fieldType)
                startRange = WTF::Range<int32_t>(beginIndex, endIndex);
            else {
                ASSERT(fieldType == 1);
                endRange = WTF::Range<int32_t>(beginIndex, endIndex);
            }
            continue;
        }

        ASSERT(category == UFIELD_CATEGORY_NUMBER);

        fields.append(IntlNumberFormatField { fieldType, { beginIndex, endIndex } });
    }

    auto flatten = flattenFields(WTFMove(fields), formattedStringLength);

    auto createPart = [&] (JSString* type, int32_t beginIndex, int32_t length) {
        auto sourceType = [&](int32_t index) -> JSString* {
            if (startRange.contains(index))
                return startRangeString;
            if (endRange.contains(index))
                return endRangeString;
            return sharedString;
        };

        auto value = jsString(vm, resultStringView.substring(beginIndex, length));
        JSObject* part = constructEmptyObject(globalObject);
        part->putDirect(vm, vm.propertyNames->type, type);
        part->putDirect(vm, vm.propertyNames->value, value);
        part->putDirect(vm, vm.propertyNames->source, sourceType(beginIndex));
        return part;
    };

    for (auto& field : flatten) {
        bool sign = false;
        IntlMathematicalValue::NumberType numberType = start.numberType();
        if (startRange.contains(field.m_range.begin())) {
            numberType = start.numberType();
            sign = start.sign();
        } else {
            numberType = end.numberType();
            sign = end.sign();
        }
        auto fieldType = field.m_field;
        auto partType = fieldType == literalField ? literalString : jsNontrivialString(vm, partTypeString(UNumberFormatFields(fieldType), style, sign, numberType));
        JSObject* part = createPart(partType, field.m_range.begin(), field.m_range.distance());
        parts->push(globalObject, part);
        RETURN_IF_EXCEPTION(scope, void());
    }
}

JSValue IntlNumberFormat::formatRangeToParts(JSGlobalObject* globalObject, double start, double end) const
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    ASSERT(m_numberRangeFormatter);

    if (std::isnan(start) || std::isnan(end))
        return throwRangeError(globalObject, scope, "Passed numbers are out of range"_s);

    UErrorCode status = U_ZERO_ERROR;
    auto range = std::unique_ptr<UFormattedNumberRange, ICUDeleter<unumrf_closeResult>>(unumrf_openResult(&status));
    if (U_FAILURE(status))
        return throwTypeError(globalObject, scope, "failed to format a range"_s);

    unumrf_formatDoubleRange(m_numberRangeFormatter.get(), start, end, range.get(), &status);
    if (U_FAILURE(status))
        return throwTypeError(globalObject, scope, "failed to format a range"_s);

    auto* formattedValue = unumrf_resultAsValue(range.get(), &status);
    if (U_FAILURE(status))
        return throwTypeError(globalObject, scope, "failed to format a range"_s);

    bool equal = numberFieldsPracticallyEqual(formattedValue, status);
    if (U_FAILURE(status)) {
        throwTypeError(globalObject, scope, "Failed to format number range"_s);
        return { };
    }

    if (equal)
        RELEASE_AND_RETURN(scope, formatToParts(globalObject, start, jsNontrivialString(vm, "shared"_s)));

    JSArray* parts = JSArray::tryCreate(vm, globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithContiguous), 0);
    if (!parts) {
        throwOutOfMemoryError(globalObject, scope);
        return { };
    }

    formatRangeToPartsInternal(globalObject, m_style, IntlMathematicalValue(start), IntlMathematicalValue(end), formattedValue, parts);
    RETURN_IF_EXCEPTION(scope, { });

    return parts;
}

JSValue IntlNumberFormat::formatRangeToParts(JSGlobalObject* globalObject, IntlMathematicalValue&& start, IntlMathematicalValue&& end) const
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    ASSERT(m_numberRangeFormatter);

    if (start.numberType() == IntlMathematicalValue::NumberType::NaN || end.numberType() == IntlMathematicalValue::NumberType::NaN)
        return throwRangeError(globalObject, scope, "Passed numbers are out of range"_s);

    start.ensureNonDouble();
    const auto& startString = start.getString();

    end.ensureNonDouble();
    const auto& endString = end.getString();

    UErrorCode status = U_ZERO_ERROR;
    auto range = std::unique_ptr<UFormattedNumberRange, ICUDeleter<unumrf_closeResult>>(unumrf_openResult(&status));
    if (U_FAILURE(status))
        return throwTypeError(globalObject, scope, "failed to format a range"_s);

    unumrf_formatDecimalRange(m_numberRangeFormatter.get(), startString.data(), startString.length(), endString.data(), endString.length(), range.get(), &status);
    if (U_FAILURE(status))
        return throwTypeError(globalObject, scope, "failed to format a range"_s);

    auto* formattedValue = unumrf_resultAsValue(range.get(), &status);
    if (U_FAILURE(status))
        return throwTypeError(globalObject, scope, "failed to format a range"_s);

    bool equal = numberFieldsPracticallyEqual(formattedValue, status);
    if (U_FAILURE(status)) {
        throwTypeError(globalObject, scope, "Failed to format number range"_s);
        return { };
    }

    if (equal)
        RELEASE_AND_RETURN(scope, formatToParts(globalObject, WTFMove(start), jsNontrivialString(vm, "shared"_s)));

    JSArray* parts = JSArray::tryCreate(vm, globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithContiguous), 0);
    if (!parts) {
        throwOutOfMemoryError(globalObject, scope);
        return { };
    }

    formatRangeToPartsInternal(globalObject, m_style, WTFMove(start), WTFMove(end), formattedValue, parts);
    RETURN_IF_EXCEPTION(scope, { });

    return parts;
}
#endif

ASCIILiteral IntlNumberFormat::styleString(Style style)
{
    switch (style) {
    case Style::Decimal:
        return "decimal"_s;
    case Style::Percent:
        return "percent"_s;
    case Style::Currency:
        return "currency"_s;
    case Style::Unit:
        return "unit"_s;
    }
    ASSERT_NOT_REACHED();
    return { };
}

ASCIILiteral IntlNumberFormat::currencyDisplayString(CurrencyDisplay currencyDisplay)
{
    switch (currencyDisplay) {
    case CurrencyDisplay::Code:
        return "code"_s;
    case CurrencyDisplay::Symbol:
        return "symbol"_s;
    case CurrencyDisplay::NarrowSymbol:
        return "narrowSymbol"_s;
    case CurrencyDisplay::Name:
        return "name"_s;
    }
    ASSERT_NOT_REACHED();
    return { };
}

ASCIILiteral IntlNumberFormat::notationString(IntlNotation notation)
{
    switch (notation) {
    case IntlNotation::Standard:
        return "standard"_s;
    case IntlNotation::Scientific:
        return "scientific"_s;
    case IntlNotation::Engineering:
        return "engineering"_s;
    case IntlNotation::Compact:
        return "compact"_s;
    }
    ASSERT_NOT_REACHED();
    return { };
}

ASCIILiteral IntlNumberFormat::currencySignString(CurrencySign currencySign)
{
    switch (currencySign) {
    case CurrencySign::Standard:
        return "standard"_s;
    case CurrencySign::Accounting:
        return "accounting"_s;
    }
    ASSERT_NOT_REACHED();
    return { };
}

ASCIILiteral IntlNumberFormat::unitDisplayString(UnitDisplay unitDisplay)
{
    switch (unitDisplay) {
    case UnitDisplay::Short:
        return "short"_s;
    case UnitDisplay::Narrow:
        return "narrow"_s;
    case UnitDisplay::Long:
        return "long"_s;
    }
    ASSERT_NOT_REACHED();
    return { };
}

ASCIILiteral IntlNumberFormat::compactDisplayString(CompactDisplay compactDisplay)
{
    switch (compactDisplay) {
    case CompactDisplay::Short:
        return "short"_s;
    case CompactDisplay::Long:
        return "long"_s;
    }
    ASSERT_NOT_REACHED();
    return { };
}

ASCIILiteral IntlNumberFormat::signDisplayString(SignDisplay signDisplay)
{
    switch (signDisplay) {
    case SignDisplay::Auto:
        return "auto"_s;
    case SignDisplay::Never:
        return "never"_s;
    case SignDisplay::Always:
        return "always"_s;
    case SignDisplay::ExceptZero:
        return "exceptZero"_s;
    case SignDisplay::Negative:
        return "negative"_s;
    }
    ASSERT_NOT_REACHED();
    return { };
}

ASCIILiteral IntlNumberFormat::roundingModeString(RoundingMode roundingMode)
{
    switch (roundingMode) {
    case RoundingMode::Ceil:
        return "ceil"_s;
    case RoundingMode::Floor:
        return "floor"_s;
    case RoundingMode::Expand:
        return "expand"_s;
    case RoundingMode::Trunc:
        return "trunc"_s;
    case RoundingMode::HalfCeil:
        return "halfCeil"_s;
    case RoundingMode::HalfFloor:
        return "halfFloor"_s;
    case RoundingMode::HalfExpand:
        return "halfExpand"_s;
    case RoundingMode::HalfTrunc:
        return "halfTrunc"_s;
    case RoundingMode::HalfEven:
        return "halfEven"_s;
    }
    ASSERT_NOT_REACHED();
    return { };
}

ASCIILiteral IntlNumberFormat::trailingZeroDisplayString(TrailingZeroDisplay trailingZeroDisplay)
{
    switch (trailingZeroDisplay) {
    case TrailingZeroDisplay::Auto:
        return "auto"_s;
    case TrailingZeroDisplay::StripIfInteger:
        return "stripIfInteger"_s;
    }
    ASSERT_NOT_REACHED();
    return { };
}

ASCIILiteral IntlNumberFormat::roundingPriorityString(IntlRoundingType roundingType)
{
    switch (roundingType) {
    case IntlRoundingType::FractionDigits:
    case IntlRoundingType::SignificantDigits:
        return "auto"_s;
    case IntlRoundingType::MorePrecision:
        return "morePrecision"_s;
    case IntlRoundingType::LessPrecision:
        return "lessPrecision"_s;
    }
    ASSERT_NOT_REACHED();
    return { };
}

JSValue IntlNumberFormat::useGroupingValue(VM& vm, UseGrouping useGrouping)
{
    switch (useGrouping) {
    case UseGrouping::False:
        return jsBoolean(false);
    case UseGrouping::Min2:
        return jsNontrivialString(vm, "min2"_s);
    case UseGrouping::Auto:
        return jsNontrivialString(vm, "auto"_s);
    case UseGrouping::Always:
        return jsNontrivialString(vm, "always"_s);
    }
    return jsUndefined();
}

// https://tc39.es/ecma402/#sec-intl.numberformat.prototype.resolvedoptions
JSObject* IntlNumberFormat::resolvedOptions(JSGlobalObject* globalObject) const
{
    VM& vm = globalObject->vm();
    JSObject* options = constructEmptyObject(globalObject);
    options->putDirect(vm, vm.propertyNames->locale, jsString(vm, m_locale));
    options->putDirect(vm, vm.propertyNames->numberingSystem, jsString(vm, m_numberingSystem));
    options->putDirect(vm, vm.propertyNames->style, jsNontrivialString(vm, styleString(m_style)));
    switch (m_style) {
    case Style::Decimal:
    case Style::Percent:
        break;
    case Style::Currency:
        options->putDirect(vm, Identifier::fromString(vm, "currency"_s), jsNontrivialString(vm, m_currency));
        options->putDirect(vm, Identifier::fromString(vm, "currencyDisplay"_s), jsNontrivialString(vm, currencyDisplayString(m_currencyDisplay)));
        options->putDirect(vm, Identifier::fromString(vm, "currencySign"_s), jsNontrivialString(vm, currencySignString(m_currencySign)));
        break;
    case Style::Unit:
        options->putDirect(vm, Identifier::fromString(vm, "unit"_s), jsNontrivialString(vm, m_unit));
        options->putDirect(vm, Identifier::fromString(vm, "unitDisplay"_s), jsNontrivialString(vm, unitDisplayString(m_unitDisplay)));
        break;
    }
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
    case IntlRoundingType::MorePrecision:
    case IntlRoundingType::LessPrecision:
        options->putDirect(vm, vm.propertyNames->minimumFractionDigits, jsNumber(m_minimumFractionDigits));
        options->putDirect(vm, vm.propertyNames->maximumFractionDigits, jsNumber(m_maximumFractionDigits));
        options->putDirect(vm, vm.propertyNames->minimumSignificantDigits, jsNumber(m_minimumSignificantDigits));
        options->putDirect(vm, vm.propertyNames->maximumSignificantDigits, jsNumber(m_maximumSignificantDigits));
        break;
    }
    options->putDirect(vm, Identifier::fromString(vm, "useGrouping"_s), useGroupingValue(vm, m_useGrouping));
    options->putDirect(vm, Identifier::fromString(vm, "notation"_s), jsNontrivialString(vm, notationString(m_notation)));
    if (m_notation == IntlNotation::Compact)
        options->putDirect(vm, Identifier::fromString(vm, "compactDisplay"_s), jsNontrivialString(vm, compactDisplayString(m_compactDisplay)));
    options->putDirect(vm, Identifier::fromString(vm, "signDisplay"_s), jsNontrivialString(vm, signDisplayString(m_signDisplay)));
    options->putDirect(vm, vm.propertyNames->roundingMode, jsNontrivialString(vm, roundingModeString(m_roundingMode)));
    options->putDirect(vm, vm.propertyNames->roundingIncrement, jsNumber(m_roundingIncrement));
    options->putDirect(vm, vm.propertyNames->trailingZeroDisplay, jsNontrivialString(vm, trailingZeroDisplayString(m_trailingZeroDisplay)));
    options->putDirect(vm, vm.propertyNames->roundingPriority, jsNontrivialString(vm, roundingPriorityString(m_roundingType)));
    return options;
}

void IntlNumberFormat::setBoundFormat(VM& vm, JSBoundFunction* format)
{
    m_boundFormat.set(vm, this, format);
}

void IntlNumberFormat::formatToPartsInternal(JSGlobalObject* globalObject, Style style, bool sign, IntlMathematicalValue::NumberType numberType, const String& formatted, IntlFieldIterator& iterator, JSArray* parts, JSString* sourceType, JSString* unit)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto stringLength = formatted.length();

    Vector<IntlNumberFormatField> fields;

    while (true) {
        int32_t beginIndex = 0;
        int32_t endIndex = 0;
        UErrorCode status = U_ZERO_ERROR;
        int32_t fieldType = iterator.next(beginIndex, endIndex, status);
        if (U_FAILURE(status)) {
            throwTypeError(globalObject, scope, "Failed to iterate field position iterator"_s);
            return;
        }
        if (fieldType < 0)
            break;

        fields.append(IntlNumberFormatField { fieldType, { beginIndex, endIndex } });
    }

    auto flatten = flattenFields(WTFMove(fields), stringLength);

    auto literalString = jsNontrivialString(vm, "literal"_s);
    Identifier unitName;
    if (unit)
        unitName = Identifier::fromString(vm, "unit"_s);

    for (auto& field : flatten) {
        auto fieldType = field.m_field;
        auto partType = fieldType == literalField ? literalString : jsNontrivialString(vm, partTypeString(UNumberFormatFields(fieldType), style, sign, numberType));
        auto partValue = jsSubstring(vm, formatted, field.m_range.begin(), field.m_range.distance());
        JSObject* part = constructEmptyObject(globalObject);
        part->putDirect(vm, vm.propertyNames->type, partType);
        part->putDirect(vm, vm.propertyNames->value, partValue);
        if (unit)
            part->putDirect(vm, unitName, unit);
        if (sourceType)
            part->putDirect(vm, vm.propertyNames->source, sourceType);
        parts->push(globalObject, part);
        RETURN_IF_EXCEPTION(scope, void());
    }
}

// https://tc39.github.io/ecma402/#sec-formatnumbertoparts
JSValue IntlNumberFormat::formatToParts(JSGlobalObject* globalObject, double value, JSString* sourceType) const
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    UErrorCode status = U_ZERO_ERROR;
    auto fieldItr = std::unique_ptr<UFieldPositionIterator, UFieldPositionIteratorDeleter>(ufieldpositer_open(&status));
    if (U_FAILURE(status))
        return throwTypeError(globalObject, scope, "failed to open field position iterator"_s);

    Vector<UChar, 32> result;
#if HAVE(ICU_U_NUMBER_FORMATTER)
    ASSERT(m_numberFormatter);
    auto formattedNumber = std::unique_ptr<UFormattedNumber, ICUDeleter<unumf_closeResult>>(unumf_openResult(&status));
    if (U_FAILURE(status))
        return throwTypeError(globalObject, scope, "Failed to format a number."_s);
    unumf_formatDouble(m_numberFormatter.get(), value, formattedNumber.get(), &status);
    if (U_FAILURE(status))
        return throwTypeError(globalObject, scope, "Failed to format a number."_s);
    status = callBufferProducingFunction(unumf_resultToString, formattedNumber.get(), result);
    if (U_FAILURE(status))
        return throwTypeError(globalObject, scope, "Failed to format a number."_s);
    unumf_resultGetAllFieldPositions(formattedNumber.get(), fieldItr.get(), &status);
    if (U_FAILURE(status))
        return throwTypeError(globalObject, scope, "Failed to format a number."_s);
    IntlFieldIterator iterator(*fieldItr.get());
#else
    ASSERT(m_numberFormat);
    status = callBufferProducingFunction(unum_formatDoubleForFields, m_numberFormat.get(), value, result, fieldItr.get());
    if (U_FAILURE(status))
        return throwTypeError(globalObject, scope, "failed to format a number."_s);
    IntlFieldIterator iterator(*fieldItr.get());
#endif

    auto resultString = String(WTFMove(result));

    JSArray* parts = JSArray::tryCreate(vm, globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithContiguous), 0);
    if (!parts)
        return throwOutOfMemoryError(globalObject, scope);

    formatToPartsInternal(globalObject, m_style, std::signbit(value), IntlMathematicalValue::numberTypeFromDouble(value), resultString, iterator, parts, sourceType, nullptr);
    RETURN_IF_EXCEPTION(scope, { });

    return parts;
}

#if HAVE(ICU_U_NUMBER_FORMATTER)
JSValue IntlNumberFormat::formatToParts(JSGlobalObject* globalObject, IntlMathematicalValue&& value, JSString* sourceType) const
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    value.ensureNonDouble();
    const auto& string = value.getString();

    UErrorCode status = U_ZERO_ERROR;
    auto fieldItr = std::unique_ptr<UFieldPositionIterator, UFieldPositionIteratorDeleter>(ufieldpositer_open(&status));
    if (U_FAILURE(status))
        return throwTypeError(globalObject, scope, "failed to open field position iterator"_s);

    Vector<UChar, 32> result;
    ASSERT(m_numberFormatter);
    auto formattedNumber = std::unique_ptr<UFormattedNumber, ICUDeleter<unumf_closeResult>>(unumf_openResult(&status));
    if (U_FAILURE(status))
        return throwTypeError(globalObject, scope, "Failed to format a number."_s);

    unumf_formatDecimal(m_numberFormatter.get(), string.data(), string.length(), formattedNumber.get(), &status);
    if (U_FAILURE(status))
        return throwTypeError(globalObject, scope, "Failed to format a number."_s);

    status = callBufferProducingFunction(unumf_resultToString, formattedNumber.get(), result);
    if (U_FAILURE(status))
        return throwTypeError(globalObject, scope, "Failed to format a number."_s);

    unumf_resultGetAllFieldPositions(formattedNumber.get(), fieldItr.get(), &status);
    if (U_FAILURE(status))
        return throwTypeError(globalObject, scope, "Failed to format a number."_s);

    IntlFieldIterator iterator(*fieldItr.get());

    auto resultString = String(WTFMove(result));

    JSArray* parts = JSArray::tryCreate(vm, globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithContiguous), 0);
    if (!parts)
        return throwOutOfMemoryError(globalObject, scope);

    formatToPartsInternal(globalObject, m_style, value.sign(), value.numberType(), resultString, iterator, parts, sourceType, nullptr);
    RETURN_IF_EXCEPTION(scope, { });

    return parts;
}
#endif

} // namespace JSC

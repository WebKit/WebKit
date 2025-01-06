/*
 * Copyright (C) 2015 Andy VanWagoner (andy@vanwagoner.family)
 * Copyright (C) 2016 Sukolsak Sakshuwong (sukolsak@gmail.com)
 * Copyright (C) 2016-2023 Apple Inc. All rights reserved.
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
#include "ParseInt.h"
#include <wtf/Range.h>
#include <wtf/text/MakeString.h>
#include <wtf/unicode/icu/ICUHelpers.h>

#ifdef U_HIDE_DRAFT_API
#undef U_HIDE_DRAFT_API
#endif
#include <unicode/unumberformatter.h>
#include <unicode/unumberrangeformatter.h>
#define U_HIDE_DRAFT_API 1

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC {

const ClassInfo IntlNumberFormat::s_info = { "Object"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(IntlNumberFormat) };

namespace IntlNumberFormatInternal {
static constexpr bool verbose = false;
}

void UNumberFormatterDeleter::operator()(UNumberFormatter* formatter)
{
    if (formatter)
        unumf_close(formatter);
}

void UNumberRangeFormatterDeleter::operator()(UNumberRangeFormatter* formatter)
{
    if (formatter)
        unumrf_close(formatter);
}

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
    ASSERT(currency.containsOnly<isASCIIUpper>());
    return (currency[0] << 16) + (currency[1] << 8) + currency[2];
}

static inline unsigned computeCurrencySortKey(const std::array<const char, 3>& currency)
{
    ASSERT(containsOnly<isASCIIUpper>(std::span { currency }));
    return (currency[0] << 16) + (currency[1] << 8) + currency[2];
}

static unsigned extractCurrencySortKey(std::pair<std::array<const char, 3>, unsigned>* currencyMinorUnit)
{
    return computeCurrencySortKey(currencyMinorUnit->first);
}

static unsigned computeCurrencyDigits(const String& currency)
{
    // 11.1.1 The abstract operation CurrencyDigits (currency)
    // "If the ISO 4217 currency and funds code list contains currency as an alphabetic code,
    // then return the minor unit value corresponding to the currency from the list; else return 2.
    static constexpr std::pair<std::array<const char, 3>, unsigned> currencyMinorUnits[] = {
        { { 'B', 'H', 'D' }, 3 },
        { { 'B', 'I', 'F' }, 0 },
        { { 'B', 'Y', 'R' }, 0 },
        { { 'C', 'L', 'F' }, 4 },
        { { 'C', 'L', 'P' }, 0 },
        { { 'D', 'J', 'F' }, 0 },
        { { 'G', 'N', 'F' }, 0 },
        { { 'I', 'Q', 'D' }, 3 },
        { { 'I', 'S', 'K' }, 0 },
        { { 'J', 'O', 'D' }, 3 },
        { { 'J', 'P', 'Y' }, 0 },
        { { 'K', 'M', 'F' }, 0 },
        { { 'K', 'R', 'W' }, 0 },
        { { 'K', 'W', 'D' }, 3 },
        { { 'L', 'Y', 'D' }, 3 },
        { { 'O', 'M', 'R' }, 3 },
        { { 'P', 'Y', 'G' }, 0 },
        { { 'R', 'W', 'F' }, 0 },
        { { 'T', 'N', 'D' }, 3 },
        { { 'U', 'G', 'X' }, 0 },
        { { 'U', 'Y', 'I' }, 0 },
        { { 'V', 'N', 'D' }, 0 },
        { { 'V', 'U', 'V' }, 0 },
        { { 'X', 'A', 'F' }, 0 },
        { { 'X', 'O', 'F' }, 0 },
        { { 'X', 'P', 'F' }, 0 }
    };
    auto* currencyMinorUnit = tryBinarySearch<std::pair<std::array<const char, 3>, unsigned>>(currencyMinorUnits, std::size(currencyMinorUnits), computeCurrencySortKey(currency), extractCurrencySortKey);
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

// We intentionally avoid using ICU's UNUM_APPROXIMATELY_SIGN_FIELD and define the same value here.
// UNUM_APPROXIMATELY_SIGN_FIELD can be defined in the header after ICU 71. But dylib ICU can be newer while ICU header version is old.
// We can define UNUM_APPROXIMATELY_SIGN_FIELD here so that we can support old ICU header + newer ICU library combination.
static constexpr UNumberFormatFields UNUM_APPROXIMATELY_SIGN_FIELD = static_cast<UNumberFormatFields>(UNUM_COMPACT_FIELD + 1);

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
    case UNUM_MEASURE_UNIT_FIELD:
        return "unit"_s;
    case UNUM_COMPACT_FIELD:
        return "compact"_s;
IGNORE_GCC_WARNINGS_BEGIN("switch")
    case UNUM_APPROXIMATELY_SIGN_FIELD:
        return "approximatelySign"_s;
IGNORE_GCC_WARNINGS_END
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

    if (Options::useMoreCurrencyDisplayChoices())
        m_currencyDisplay = intlOption<CurrencyDisplay>(globalObject, options, Identifier::fromString(vm, "currencyDisplay"_s), { { "code"_s, CurrencyDisplay::Code }, { "symbol"_s, CurrencyDisplay::Symbol }, { "formalSymbol"_s, CurrencyDisplay::FormalSymbol },  { "narrowSymbol"_s, CurrencyDisplay::NarrowSymbol }, { "name"_s, CurrencyDisplay::Name }, { "never"_s, CurrencyDisplay::Never } }, "currencyDisplay must be either \"code\", \"symbol\", \"formalSymbol\", \"narrowSymbol\", \"name\" or \"never\""_s, CurrencyDisplay::Symbol);
    else
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

    m_compactDisplay = intlOption<CompactDisplay>(globalObject, options, Identifier::fromString(vm, "compactDisplay"_s), { { "short"_s, CompactDisplay::Short }, { "long"_s, CompactDisplay::Long } }, "compactDisplay must be either \"short\" or \"long\""_s, CompactDisplay::Short);
    RETURN_IF_EXCEPTION(scope, void());

    UseGrouping defaultUseGrouping = UseGrouping::Auto;
    if (m_notation == IntlNotation::Compact)
        defaultUseGrouping = UseGrouping::Min2;

    m_useGrouping = intlStringOrBooleanOption<UseGrouping>(globalObject, options, Identifier::fromString(vm, "useGrouping"_s), UseGrouping::Always, UseGrouping::False, { { "min2"_s, UseGrouping::Min2 }, { "auto"_s, UseGrouping::Auto }, { "always"_s, UseGrouping::Always } }, "useGrouping must be either true, false, \"min2\", \"auto\", or \"always\""_s, defaultUseGrouping);
    RETURN_IF_EXCEPTION(scope, void());

    m_signDisplay = intlOption<SignDisplay>(globalObject, options, Identifier::fromString(vm, "signDisplay"_s), { { "auto"_s, SignDisplay::Auto }, { "never"_s, SignDisplay::Never }, { "always"_s, SignDisplay::Always }, { "exceptZero"_s, SignDisplay::ExceptZero }, { "negative"_s, SignDisplay::Negative } }, "signDisplay must be either \"auto\", \"never\", \"always\", \"exceptZero\", or \"negative\""_s, SignDisplay::Auto);
    RETURN_IF_EXCEPTION(scope, void());

    CString dataLocaleWithExtensions = makeString(resolved.dataLocale, "-u-nu-"_s, m_numberingSystem).utf8();
    dataLogLnIf(IntlNumberFormatInternal::verbose, "dataLocaleWithExtensions:(", dataLocaleWithExtensions , ")");

    // Options are obtained. Configure formatter here.

    // Constructing ICU Number Skeletons to configure UNumberFormatter.
    // https://github.com/unicode-org/icu/blob/master/docs/userguide/format_parse/numbers/skeletons.md

    StringBuilder skeletonBuilder;

    switch (m_style) {
    case Style::Decimal:
        // No skeleton is needed.
        break;
    case Style::Percent:
        skeletonBuilder.append(" percent scale/100"_s);
        break;
    case Style::Currency: {
        skeletonBuilder.append(" currency/"_s, currency);

        // https://github.com/unicode-org/icu/blob/master/docs/userguide/format_parse/numbers/skeletons.md#unit-width
        switch (m_currencyDisplay) {
        case CurrencyDisplay::Code:
            skeletonBuilder.append(" unit-width-iso-code"_s);
            break;
        case CurrencyDisplay::Symbol:
            // Default option. Do not specify unit-width.
            break;
        case CurrencyDisplay::FormalSymbol:
            ASSERT(Options::useMoreCurrencyDisplayChoices());
            skeletonBuilder.append(" unit-width-formal"_s);
            break;
        case CurrencyDisplay::NarrowSymbol:
            skeletonBuilder.append(" unit-width-narrow"_s);
            break;
        case CurrencyDisplay::Name:
            skeletonBuilder.append(" unit-width-full-name"_s);
            break;
        case CurrencyDisplay::Never:
            ASSERT(Options::useMoreCurrencyDisplayChoices());
            skeletonBuilder.append(" unit-width-hidden"_s);
            break;
        }
        break;
    }
    case Style::Unit: {
        // The measure-unit stem takes one required option: the unit identifier of the unit to be formatted.
        // The full unit identifier is required: both the type and the subtype (for example, length-meter).
        // https://github.com/unicode-org/icu/blob/master/docs/userguide/format_parse/numbers/skeletons.md#unit
        skeletonBuilder.append(" measure-unit/"_s);
        auto numeratorUnit = wellFormedUnit->numerator;
        skeletonBuilder.append(numeratorUnit.type, '-', numeratorUnit.subType);
        if (auto denominatorUnitValue = wellFormedUnit->denominator) {
            auto denominatorUnit = denominatorUnitValue.value();
            skeletonBuilder.append(" per-measure-unit/"_s, denominatorUnit.type, '-', denominatorUnit.subType);
        }

        // https://github.com/unicode-org/icu/blob/master/docs/userguide/format_parse/numbers/skeletons.md#unit-width
        switch (m_unitDisplay) {
        case UnitDisplay::Short:
            skeletonBuilder.append(" unit-width-short"_s);
            break;
        case UnitDisplay::Narrow:
            skeletonBuilder.append(" unit-width-narrow"_s);
            break;
        case UnitDisplay::Long:
            skeletonBuilder.append(" unit-width-full-name"_s);
            break;
        }
        break;
    }
    }

    appendNumberFormatDigitOptionsToSkeleton(this, skeletonBuilder);

    // https://github.com/unicode-org/icu/blob/master/docs/userguide/format_parse/numbers/skeletons.md#notation
    switch (m_notation) {
    case IntlNotation::Standard:
        break;
    case IntlNotation::Scientific:
        skeletonBuilder.append(" scientific"_s);
        break;
    case IntlNotation::Engineering:
        skeletonBuilder.append(" engineering"_s);
        break;
    case IntlNotation::Compact:
        switch (m_compactDisplay) {
        case CompactDisplay::Short:
            skeletonBuilder.append(" compact-short"_s);
            break;
        case CompactDisplay::Long:
            skeletonBuilder.append(" compact-long"_s);
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
            skeletonBuilder.append(" sign-accounting"_s);
        else
            skeletonBuilder.append(" sign-auto"_s);
        break;
    case SignDisplay::Never:
        skeletonBuilder.append(" sign-never"_s);
        break;
    case SignDisplay::Always:
        if (useAccounting)
            skeletonBuilder.append(" sign-accounting-always"_s);
        else
            skeletonBuilder.append(" sign-always"_s);
        break;
    case SignDisplay::ExceptZero:
        if (useAccounting)
            skeletonBuilder.append(" sign-accounting-except-zero"_s);
        else
            skeletonBuilder.append(" sign-except-zero"_s);
        break;
    case SignDisplay::Negative:
        if (useAccounting)
            skeletonBuilder.append(" sign-accounting-negative"_s);
        else
            skeletonBuilder.append(" sign-negative"_s);
        break;
    }

    // https://github.com/tc39/proposal-intl-numberformat-v3/issues/3
    // https://github.com/unicode-org/icu/blob/main/docs/userguide/format_parse/numbers/skeletons.md#grouping
    switch (m_useGrouping) {
    case UseGrouping::False:
        skeletonBuilder.append(" group-off"_s);
        break;
    case UseGrouping::Min2:
        skeletonBuilder.append(" group-min2"_s);
        break;
    case UseGrouping::Auto:
        skeletonBuilder.append(" group-auto"_s);
        break;
    case UseGrouping::Always:
        skeletonBuilder.append(" group-on-aligned"_s);
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

    m_numberRangeFormatter = std::unique_ptr<UNumberRangeFormatter, UNumberRangeFormatterDeleter>(unumrf_openForSkeletonWithCollapseAndIdentityFallback(upconverted.get(), skeletonView.length(), UNUM_RANGE_COLLAPSE_AUTO, UNUM_IDENTITY_FALLBACK_APPROXIMATELY, dataLocaleWithExtensions.data(), nullptr, &status));
    if (U_FAILURE(status)) {
        throwTypeError(globalObject, scope, "failed to initialize NumberFormat"_s);
        return;
    }
}

// https://tc39.es/ecma402/#sec-formatnumber
JSValue IntlNumberFormat::format(JSGlobalObject* globalObject, double value) const
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    value = purifyNaN(value);

    Vector<UChar, 32> buffer;
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
    return jsString(vm, String(WTFMove(buffer)));
}

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

    return jsString(vm, String({ string, static_cast<size_t>(length) }));
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

    return jsString(vm, String({ string, static_cast<size_t>(length) }));
}

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
    StringView resultStringView(std::span(formattedStringPointer, formattedStringLength));

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

    // After ICU 71, approximatelySign is supported. We use the old path only for < 71.
    if (WTF::ICU::majorVersion() < 71) {
        bool equal = numberFieldsPracticallyEqual(formattedValue, status);
        if (U_FAILURE(status)) {
            throwTypeError(globalObject, scope, "Failed to format number range"_s);
            return { };
        }

        if (equal)
            RELEASE_AND_RETURN(scope, formatToParts(globalObject, start, jsNontrivialString(vm, "shared"_s)));
    }

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

    // After ICU 71, approximatelySign is supported. We use the old path only for < 71.
    if (WTF::ICU::majorVersion() < 71) {
        bool equal = numberFieldsPracticallyEqual(formattedValue, status);
        if (U_FAILURE(status)) {
            throwTypeError(globalObject, scope, "Failed to format number range"_s);
            return { };
        }

        if (equal)
            RELEASE_AND_RETURN(scope, formatToParts(globalObject, WTFMove(start), jsNontrivialString(vm, "shared"_s)));
    }

    JSArray* parts = JSArray::tryCreate(vm, globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithContiguous), 0);
    if (!parts) {
        throwOutOfMemoryError(globalObject, scope);
        return { };
    }

    formatRangeToPartsInternal(globalObject, m_style, WTFMove(start), WTFMove(end), formattedValue, parts);
    RETURN_IF_EXCEPTION(scope, { });

    return parts;
}

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
    case CurrencyDisplay::FormalSymbol:
        ASSERT(Options::useMoreCurrencyDisplayChoices());
        return "formalSymbol"_s;
    case CurrencyDisplay::Never:
        ASSERT(Options::useMoreCurrencyDisplayChoices());
        return "never"_s;
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

ASCIILiteral IntlNumberFormat::trailingZeroDisplayString(IntlTrailingZeroDisplay trailingZeroDisplay)
{
    switch (trailingZeroDisplay) {
    case IntlTrailingZeroDisplay::Auto:
        return "auto"_s;
    case IntlTrailingZeroDisplay::StripIfInteger:
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
    options->putDirect(vm, vm.propertyNames->roundingIncrement, jsNumber(m_roundingIncrement));
    options->putDirect(vm, vm.propertyNames->roundingMode, jsNontrivialString(vm, roundingModeString(m_roundingMode)));
    options->putDirect(vm, vm.propertyNames->roundingPriority, jsNontrivialString(vm, roundingPriorityString(m_roundingType)));
    options->putDirect(vm, vm.propertyNames->trailingZeroDisplay, jsNontrivialString(vm, trailingZeroDisplayString(m_trailingZeroDisplay)));
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

    value = purifyNaN(value);

    UErrorCode status = U_ZERO_ERROR;
    auto fieldItr = std::unique_ptr<UFieldPositionIterator, UFieldPositionIteratorDeleter>(ufieldpositer_open(&status));
    if (U_FAILURE(status))
        return throwTypeError(globalObject, scope, "failed to open field position iterator"_s);

    Vector<UChar, 32> result;
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

    auto resultString = String(WTFMove(result));

    JSArray* parts = JSArray::tryCreate(vm, globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithContiguous), 0);
    if (!parts)
        return throwOutOfMemoryError(globalObject, scope);

    formatToPartsInternal(globalObject, m_style, std::signbit(value), IntlMathematicalValue::numberTypeFromDouble(value), resultString, iterator, parts, sourceType, nullptr);
    RETURN_IF_EXCEPTION(scope, { });

    return parts;
}

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

IntlMathematicalValue IntlMathematicalValue::parseString(JSGlobalObject* globalObject, StringView view)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto trimmed = view.trim([](auto character) {
        return isStrWhiteSpace(character);
    });

    if (!trimmed.length())
        return IntlMathematicalValue { 0.0 };

    if (trimmed.length() > 2 && trimmed[0] == '0') {
        auto character = trimmed[1];
        auto remaining = trimmed.substring(2);
        int32_t radix = 0;
        if (character == 'b' || character == 'B') {
            radix = 2;
            if (!remaining.containsOnly<isASCIIBinaryDigit>())
                return IntlMathematicalValue { PNaN };
        } else if (character == 'o' || character == 'O') {
            radix = 8;
            if (!remaining.containsOnly<isASCIIOctalDigit>())
                return IntlMathematicalValue { PNaN };
        } else if (character == 'x' || character == 'X') {
            radix = 16;
            if (!remaining.containsOnly<isASCIIHexDigit>())
                return IntlMathematicalValue { PNaN };
        }

        if (radix) {
            double result = parseInt(remaining, radix);
            if (result <= maxSafeInteger())
                return IntlMathematicalValue { result };

            JSValue bigInt = JSBigInt::parseInt(globalObject, vm, remaining, radix, JSBigInt::ErrorParseMode::IgnoreExceptions, JSBigInt::ParseIntSign::Unsigned);
            if (!bigInt)
                return IntlMathematicalValue { PNaN };

#if USE(BIGINT32)
            if (bigInt.isBigInt32())
                return IntlMathematicalValue { value.bigInt32AsInt32() };
#endif

            auto* heapBigInt = bigInt.asHeapBigInt();
            auto string = heapBigInt->toString(globalObject, 10);
            RETURN_IF_EXCEPTION(scope, { });

            return IntlMathematicalValue {
                IntlMathematicalValue::NumberType::Integer,
                false,
                string.ascii(),
            };
        }
    }

    if (trimmed == "Infinity"_s || trimmed == "+Infinity"_s)
        return IntlMathematicalValue { std::numeric_limits<double>::infinity() };

    if (trimmed == "-Infinity"_s)
        return IntlMathematicalValue { -std::numeric_limits<double>::infinity() };

    size_t parsedLength = 0;
    double result = parseDouble(trimmed, parsedLength);
    if (parsedLength != trimmed.length())
        return IntlMathematicalValue { PNaN };
    if (!std::isfinite(result))
        return IntlMathematicalValue { result };

    return IntlMathematicalValue { IntlMathematicalValue::NumberType::Integer, trimmed[0] == '-', trimmed.utf8() };
}

} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

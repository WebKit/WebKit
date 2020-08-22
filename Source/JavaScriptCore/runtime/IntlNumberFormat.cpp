/*
 * Copyright (C) 2015 Andy VanWagoner (andy@vanwagoner.family)
 * Copyright (C) 2016 Sukolsak Sakshuwong (sukolsak@gmail.com)
 * Copyright (C) 2016-2020 Apple Inc. All rights reserved.
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
#include <wtf/unicode/icu/ICUHelpers.h>

namespace JSC {

const ClassInfo IntlNumberFormat::s_info = { "Object", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(IntlNumberFormat) };

namespace IntlNumberFormatInternal {
static constexpr bool verbose = false;
}

struct IntlNumberFormatField {
    int32_t type;
    size_t size;
};

IntlNumberFormat* IntlNumberFormat::create(VM& vm, Structure* structure)
{
    IntlNumberFormat* format = new (NotNull, allocateCell<IntlNumberFormat>(vm.heap)) IntlNumberFormat(vm, structure);
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
    ASSERT(inherits(vm, info()));
}

void IntlNumberFormat::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    IntlNumberFormat* thisObject = jsCast<IntlNumberFormat*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());

    Base::visitChildren(thisObject, visitor);

    visitor.append(thisObject->m_boundFormat);
}

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

// Create MeasureUnit like ICU4J.
struct MeasureUnit {
    ASCIILiteral type;
    ASCIILiteral subType;
};

static Optional<MeasureUnit> sanctionedSimpleUnitIdentifier(StringView unitIdentifier)
{
    static constexpr MeasureUnit simpleUnits[] = {
        { "area"_s, "acre"_s },
        { "digital"_s, "bit"_s },
        { "digital"_s, "byte"_s },
        { "temperature"_s, "celsius"_s },
        { "length"_s, "centimeter"_s },
        { "duration"_s, "day"_s },
        { "angle"_s, "degree"_s },
        { "temperature"_s, "fahrenheit"_s },
        { "volume"_s, "fluid-ounce"_s },
        { "length"_s, "foot"_s },
        { "volume"_s, "gallon"_s },
        { "digital"_s, "gigabit"_s },
        { "digital"_s, "gigabyte"_s },
        { "mass"_s, "gram"_s },
        { "area"_s, "hectare"_s },
        { "duration"_s, "hour"_s },
        { "length"_s, "inch"_s },
        { "digital"_s, "kilobit"_s },
        { "digital"_s, "kilobyte"_s },
        { "mass"_s, "kilogram"_s },
        { "length"_s, "kilometer"_s },
        { "volume"_s, "liter"_s },
        { "digital"_s, "megabit"_s },
        { "digital"_s, "megabyte"_s },
        { "length"_s, "meter"_s },
        { "length"_s, "mile"_s },
        { "length"_s, "mile-scandinavian"_s },
        { "volume"_s, "milliliter"_s },
        { "length"_s, "millimeter"_s },
        { "duration"_s, "millisecond"_s },
        { "duration"_s, "minute"_s },
        { "duration"_s, "month"_s },
        { "mass"_s, "ounce"_s },
        { "concentr"_s, "percent"_s },
        { "digital"_s, "petabyte"_s },
        { "mass"_s, "pound"_s },
        { "duration"_s, "second"_s },
        { "mass"_s, "stone"_s },
        { "digital"_s, "terabit"_s },
        { "digital"_s, "terabyte"_s },
        { "duration"_s, "week"_s },
        { "length"_s, "yard"_s },
        { "duration"_s, "year"_s },
    };
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
    return WTF::nullopt;
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
    Optional<MeasureUnit> denominator;
};

static Optional<WellFormedUnit> wellFormedUnitIdentifier(StringView unitIdentifier)
{
    // https://tc39.es/ecma402/#sec-iswellformedunitidentifier
    if (auto unit = sanctionedSimpleUnitIdentifier(unitIdentifier))
        return WellFormedUnit(unit.value());

    // If the substring "-per-" does not occur exactly once in unitIdentifier, then return false.
    auto per = StringView("-per-"_s);
    auto position = unitIdentifier.find(per);
    if (position == WTF::notFound)
        return WTF::nullopt;
    if (unitIdentifier.find(per, position + per.length()) != WTF::notFound)
        return WTF::nullopt;

    // If the result of IsSanctionedSimpleUnitIdentifier(numerator) is false, then return false.
    auto numerator = unitIdentifier.substring(0, position);
    auto numeratorUnit = sanctionedSimpleUnitIdentifier(numerator);
    if (!numeratorUnit)
        return WTF::nullopt;

    // If the result of IsSanctionedSimpleUnitIdentifier(denominator) is false, then return false.
    auto denominator = unitIdentifier.substring(position + per.length());
    auto denominatorUnit = sanctionedSimpleUnitIdentifier(denominator);
    if (!denominatorUnit)
        return WTF::nullopt;

    return WellFormedUnit(numeratorUnit.value(), denominatorUnit.value());
}

// https://tc39.github.io/ecma402/#sec-initializenumberformat
void IntlNumberFormat::initializeNumberFormat(JSGlobalObject* globalObject, JSValue locales, JSValue optionsValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto requestedLocales = canonicalizeLocaleList(globalObject, locales);
    RETURN_IF_EXCEPTION(scope, void());

    JSObject* options;
    if (optionsValue.isUndefined())
        options = constructEmptyObject(vm, globalObject->nullPrototypeObjectStructure());
    else {
        options = optionsValue.toObject(globalObject);
        RETURN_IF_EXCEPTION(scope, void());
    }

    ResolveLocaleOptions localeOptions;

    LocaleMatcher localeMatcher = intlOption<LocaleMatcher>(globalObject, options, vm.propertyNames->localeMatcher, { { "lookup"_s, LocaleMatcher::Lookup }, { "best fit"_s, LocaleMatcher::BestFit } }, "localeMatcher must be either \"lookup\" or \"best fit\""_s, LocaleMatcher::BestFit);
    RETURN_IF_EXCEPTION(scope, void());

    String numberingSystem = intlStringOption(globalObject, options, vm.propertyNames->numberingSystem, { }, nullptr, nullptr);
    RETURN_IF_EXCEPTION(scope, void());
    if (!numberingSystem.isNull()) {
        if (!isUnicodeLocaleIdentifierType(numberingSystem)) {
            throwRangeError(globalObject, scope, "numberingSystem is not a well-formed numbering system value"_s);
            return;
        }
        localeOptions[static_cast<unsigned>(RelevantExtensionKey::Nu)] = numberingSystem;
    }

    auto& availableLocales = intlNumberFormatAvailableLocales();
    auto resolved = resolveLocale(globalObject, availableLocales, requestedLocales, localeMatcher, localeOptions, { RelevantExtensionKey::Nu }, localeData);

    m_locale = resolved.locale;
    if (m_locale.isEmpty()) {
        throwTypeError(globalObject, scope, "failed to initialize NumberFormat due to invalid locale"_s);
        return;
    }

    m_numberingSystem = resolved.extensions[static_cast<unsigned>(RelevantExtensionKey::Nu)];

    m_style = intlOption<Style>(globalObject, options, vm.propertyNames->style, { { "decimal"_s, Style::Decimal }, { "percent"_s, Style::Percent }, { "currency"_s, Style::Currency }, { "unit"_s, Style::Unit } }, "style must be either \"decimal\", \"percent\", \"currency\", or \"unit\""_s, Style::Decimal);
    RETURN_IF_EXCEPTION(scope, void());

    String currency = intlStringOption(globalObject, options, Identifier::fromString(vm, "currency"), { }, nullptr, nullptr);
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

    m_currencyDisplay = intlOption<CurrencyDisplay>(globalObject, options, Identifier::fromString(vm, "currencyDisplay"), { { "code"_s, CurrencyDisplay::Code }, { "symbol"_s, CurrencyDisplay::Symbol }, { "narrowSymbol"_s, CurrencyDisplay::NarrowSymbol }, { "name"_s, CurrencyDisplay::Name } }, "currencyDisplay must be either \"code\", \"symbol\", or \"name\""_s, CurrencyDisplay::Symbol);
    RETURN_IF_EXCEPTION(scope, void());

    m_currencySign = intlOption<CurrencySign>(globalObject, options, Identifier::fromString(vm, "currencySign"), { { "standard"_s, CurrencySign::Standard }, { "accounting"_s, CurrencySign::Accounting } }, "currencySign must be either \"standard\" or \"accounting\""_s, CurrencySign::Standard);
    RETURN_IF_EXCEPTION(scope, void());

    String unit = intlStringOption(globalObject, options, Identifier::fromString(vm, "unit"), { }, nullptr, nullptr);
    RETURN_IF_EXCEPTION(scope, void());
    Optional<WellFormedUnit> wellFormedUnit;
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

    m_unitDisplay = intlOption<UnitDisplay>(globalObject, options, Identifier::fromString(vm, "unitDisplay"), { { "short"_s, UnitDisplay::Short }, { "narrow"_s, UnitDisplay::Narrow }, { "long"_s, UnitDisplay::Long } }, "unitDisplay must be either \"short\", \"narrow\", or \"long\""_s, UnitDisplay::Short);
    RETURN_IF_EXCEPTION(scope, void());

    unsigned minimumFractionDigitsDefault = (m_style == Style::Currency) ? currencyDigits : 0;
    unsigned maximumFractionDigitsDefault = (m_style == Style::Currency) ? currencyDigits : (m_style == Style::Percent) ? 0 : 3;

    m_notation = intlOption<IntlNotation>(globalObject, options, Identifier::fromString(vm, "notation"), { { "standard"_s, IntlNotation::Standard }, { "scientific"_s, IntlNotation::Scientific }, { "engineering"_s, IntlNotation::Engineering }, { "compact"_s, IntlNotation::Compact } }, "notation must be either \"standard\", \"scientific\", \"engineering\", or \"compact\""_s, IntlNotation::Standard);
    RETURN_IF_EXCEPTION(scope, void());

    setNumberFormatDigitOptions(globalObject, this, options, minimumFractionDigitsDefault, maximumFractionDigitsDefault, m_notation);
    RETURN_IF_EXCEPTION(scope, void());

    m_compactDisplay = intlOption<CompactDisplay>(globalObject, options, Identifier::fromString(vm, "compactDisplay"), { { "short"_s, CompactDisplay::Short }, { "long"_s, CompactDisplay::Long } }, "compactDisplay must be either \"short\" or \"long\""_s, CompactDisplay::Short);
    RETURN_IF_EXCEPTION(scope, void());

    TriState useGrouping = intlBooleanOption(globalObject, options, Identifier::fromString(vm, "useGrouping"));
    RETURN_IF_EXCEPTION(scope, void());
    m_useGrouping = useGrouping != TriState::False;

    m_signDisplay = intlOption<SignDisplay>(globalObject, options, Identifier::fromString(vm, "signDisplay"), { { "auto"_s, SignDisplay::Auto }, { "never"_s, SignDisplay::Never }, { "always"_s, SignDisplay::Always }, { "exceptZero"_s, SignDisplay::ExceptZero } }, "signDisplay must be either \"auto\", \"never\", \"always\", or \"exceptZero\""_s, SignDisplay::Auto);
    RETURN_IF_EXCEPTION(scope, void());

    // Options are obtained. Configure formatter here.

#if HAVE(ICU_U_NUMBER_FORMATTER)
    // Constructing ICU Number Skeletons to configure UNumberFormatter.
    // https://github.com/unicode-org/icu/blob/master/docs/userguide/format_parse/numbers/skeletons.md

    StringBuilder skeletonBuilder;
    skeletonBuilder.appendLiteral("rounding-mode-half-up");

    switch (m_style) {
    case Style::Decimal:
        // No skeleton is needed.
        break;
    case Style::Percent:
        skeletonBuilder.appendLiteral(" percent scale/100");
        break;
    case Style::Currency: {
        skeletonBuilder.appendLiteral(" currency/");
        skeletonBuilder.append(currency);

        // https://github.com/unicode-org/icu/blob/master/docs/userguide/format_parse/numbers/skeletons.md#unit-width
        switch (m_currencyDisplay) {
        case CurrencyDisplay::Code:
            skeletonBuilder.appendLiteral(" unit-width-iso-code");
            break;
        case CurrencyDisplay::Symbol:
            // Default option. Do not specify unit-width.
            break;
        case CurrencyDisplay::NarrowSymbol:
            skeletonBuilder.appendLiteral(" unit-width-narrow");
            break;
        case CurrencyDisplay::Name:
            skeletonBuilder.appendLiteral(" unit-width-full-name");
            break;
        }
        break;
    }
    case Style::Unit: {
        // The measure-unit stem takes one required option: the unit identifier of the unit to be formatted.
        // The full unit identifier is required: both the type and the subtype (for example, length-meter).
        // https://github.com/unicode-org/icu/blob/master/docs/userguide/format_parse/numbers/skeletons.md#unit
        skeletonBuilder.appendLiteral(" measure-unit/");
        auto numeratorUnit = wellFormedUnit->numerator;
        skeletonBuilder.append(numeratorUnit.type, '-', numeratorUnit.subType);
        if (auto denominatorUnitValue = wellFormedUnit->denominator) {
            auto denominatorUnit = denominatorUnitValue.value();
            skeletonBuilder.appendLiteral(" per-measure-unit/");
            skeletonBuilder.append(denominatorUnit.type, '-', denominatorUnit.subType);
        }

        // https://github.com/unicode-org/icu/blob/master/docs/userguide/format_parse/numbers/skeletons.md#unit-width
        switch (m_unitDisplay) {
        case UnitDisplay::Short:
            skeletonBuilder.appendLiteral(" unit-width-short");
            break;
        case UnitDisplay::Narrow:
            skeletonBuilder.appendLiteral(" unit-width-narrow");
            break;
        case UnitDisplay::Long:
            skeletonBuilder.appendLiteral(" unit-width-full-name");
            break;
        }
        break;
    }
    }

    // https://github.com/unicode-org/icu/blob/master/docs/userguide/format_parse/numbers/skeletons.md#integer-width
    skeletonBuilder.appendLiteral(" integer-width/");
    skeletonBuilder.append((WTF::ICU::majorVersion() >= 67) ? '*' : '+'); // Prior to ICU 67, use the symbol + instead of *.
    for (unsigned i = 0; i < m_minimumIntegerDigits; ++i)
        skeletonBuilder.append('0');

    switch (m_roundingType) {
    case IntlRoundingType::FractionDigits: {
        // https://github.com/unicode-org/icu/blob/master/docs/userguide/format_parse/numbers/skeletons.md#fraction-precision
        skeletonBuilder.append(" .");
        for (unsigned i = 0; i < m_minimumFractionDigits; ++i)
            skeletonBuilder.append('0');
        for (unsigned i = 0; i < m_maximumFractionDigits - m_minimumFractionDigits; ++i)
            skeletonBuilder.append('#');
        break;
    }
    case IntlRoundingType::SignificantDigits: {
        // https://github.com/unicode-org/icu/blob/master/docs/userguide/format_parse/numbers/skeletons.md#significant-digits-precision
        skeletonBuilder.append(' ');
        for (unsigned i = 0; i < m_minimumSignificantDigits; ++i)
            skeletonBuilder.append('@');
        for (unsigned i = 0; i < m_maximumSignificantDigits - m_minimumSignificantDigits; ++i)
            skeletonBuilder.append('#');
        break;
    }
    case IntlRoundingType::CompactRounding:
        // Do not set anything.
        break;
    }

    // https://github.com/unicode-org/icu/blob/master/docs/userguide/format_parse/numbers/skeletons.md#notation
    switch (m_notation) {
    case IntlNotation::Standard:
        break;
    case IntlNotation::Scientific:
        skeletonBuilder.appendLiteral(" scientific");
        break;
    case IntlNotation::Engineering:
        skeletonBuilder.appendLiteral(" engineering");
        break;
    case IntlNotation::Compact:
        switch (m_compactDisplay) {
        case CompactDisplay::Short:
            skeletonBuilder.appendLiteral(" compact-short");
            break;
        case CompactDisplay::Long:
            skeletonBuilder.appendLiteral(" compact-long");
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
            skeletonBuilder.appendLiteral(" sign-accounting");
        else
            skeletonBuilder.appendLiteral(" sign-auto");
        break;
    case SignDisplay::Never:
        skeletonBuilder.appendLiteral(" sign-never");
        break;
    case SignDisplay::Always:
        if (useAccounting)
            skeletonBuilder.appendLiteral(" sign-accounting-always");
        else
            skeletonBuilder.appendLiteral(" sign-always");
        break;
    case SignDisplay::ExceptZero:
        if (useAccounting)
            skeletonBuilder.appendLiteral(" sign-accounting-except-zero");
        else
            skeletonBuilder.appendLiteral(" sign-except-zero");
        break;
    }

    if (!m_useGrouping)
        skeletonBuilder.appendLiteral(" group-off");

    String skeleton = skeletonBuilder.toString();
    dataLogLnIf(IntlNumberFormatInternal::verbose, m_locale);
    dataLogLnIf(IntlNumberFormatInternal::verbose, skeleton);
    StringView skeletonView(skeleton);
    UErrorCode status = U_ZERO_ERROR;
    m_numberFormatter = std::unique_ptr<UNumberFormatter, UNumberFormatterDeleter>(unumf_openForSkeletonAndLocale(skeletonView.upconvertedCharacters().get(), skeletonView.length(), m_locale.utf8().data(), &status));
    if (U_FAILURE(status)) {
        throwTypeError(globalObject, scope, "Failed to initialize NumberFormat"_s);
        return;
    }
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
            throwTypeError(globalObject, scope, "Failed to initialize NumberFormat since used feature is not supported in the linked ICU version"_s);
            return;
        case CurrencyDisplay::Name:
            style = UNUM_CURRENCY_PLURAL;
            break;
        }
        switch (m_currencySign) {
        case CurrencySign::Standard:
            break;
        case CurrencySign::Accounting:
            throwTypeError(globalObject, scope, "Failed to initialize NumberFormat since used feature is not supported in the linked ICU version"_s);
            return;
        }
        break;
    case Style::Unit:
        throwTypeError(globalObject, scope, "Failed to initialize NumberFormat since used feature is not supported in the linked ICU version"_s);
        return;
    }

    switch (m_notation) {
    case IntlNotation::Standard:
        break;
    case IntlNotation::Scientific:
    case IntlNotation::Engineering:
    case IntlNotation::Compact:
        throwTypeError(globalObject, scope, "Failed to initialize NumberFormat since used feature is not supported in the linked ICU version"_s);
        return;
    }

    switch (m_signDisplay) {
    case SignDisplay::Auto:
        break;
    case SignDisplay::Never:
    case SignDisplay::Always:
    case SignDisplay::ExceptZero:
        throwTypeError(globalObject, scope, "Failed to initialize NumberFormat since used feature is not supported in the linked ICU version"_s);
        return;
    }

    CString dataLocaleWithExtensions = makeString(resolved.dataLocale, "-u-nu-", m_numberingSystem).utf8();
    dataLogLnIf(IntlNumberFormatInternal::verbose, "dataLocaleWithExtensions:(", dataLocaleWithExtensions , ")");

    UErrorCode status = U_ZERO_ERROR;
    m_numberFormat = std::unique_ptr<UNumberFormat, UNumberFormatDeleter>(unum_open(style, nullptr, 0, dataLocaleWithExtensions.data(), nullptr, &status));
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
    case IntlRoundingType::CompactRounding:
        throwTypeError(globalObject, scope, "Failed to initialize NumberFormat since used feature is not supported in the linked ICU version"_s);
        return;
    }
    unum_setAttribute(m_numberFormat.get(), UNUM_GROUPING_USED, m_useGrouping);
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
    auto formattedNumber = std::unique_ptr<UFormattedNumber, UFormattedNumberDeleter>(unumf_openResult(&status));
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
    return jsString(vm, String(buffer));
}

// https://tc39.es/ecma402/#sec-formatnumber
JSValue IntlNumberFormat::format(JSGlobalObject* globalObject, JSBigInt* value) const
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto string = value->toString(globalObject, 10);
    RETURN_IF_EXCEPTION(scope, { });

    ASSERT(string.is8Bit() && string.isAllASCII());
    auto* rawString = reinterpret_cast<const char*>(string.characters8());

    Vector<UChar, 32> buffer;
#if HAVE(ICU_U_NUMBER_FORMATTER)
    ASSERT(m_numberFormatter);
    UErrorCode status = U_ZERO_ERROR;
    auto formattedNumber = std::unique_ptr<UFormattedNumber, UFormattedNumberDeleter>(unumf_openResult(&status));
    if (U_FAILURE(status))
        return throwTypeError(globalObject, scope, "Failed to format a BigInt."_s);
    unumf_formatDecimal(m_numberFormatter.get(), rawString, string.length(), formattedNumber.get(), &status);
    if (U_FAILURE(status))
        return throwTypeError(globalObject, scope, "Failed to format a BigInt."_s);
    status = callBufferProducingFunction(unumf_resultToString, formattedNumber.get(), buffer);
    if (U_FAILURE(status))
        return throwTypeError(globalObject, scope, "Failed to format a BigInt."_s);
#else
    ASSERT(m_numberFormat);
    auto status = callBufferProducingFunction(unum_formatDecimal, m_numberFormat.get(), rawString, string.length(), buffer, nullptr);
    if (U_FAILURE(status))
        return throwTypeError(globalObject, scope, "Failed to format a BigInt."_s);
#endif
    return jsString(vm, String(buffer));
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
    return ASCIILiteral::null();
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
    return ASCIILiteral::null();
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
    return ASCIILiteral::null();
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
    return ASCIILiteral::null();
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
    return ASCIILiteral::null();
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
    return ASCIILiteral::null();
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
    }
    ASSERT_NOT_REACHED();
    return ASCIILiteral::null();
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
        options->putDirect(vm, Identifier::fromString(vm, "currency"), jsNontrivialString(vm, m_currency));
        options->putDirect(vm, Identifier::fromString(vm, "currencyDisplay"), jsNontrivialString(vm, currencyDisplayString(m_currencyDisplay)));
        options->putDirect(vm, Identifier::fromString(vm, "currencySign"), jsNontrivialString(vm, currencySignString(m_currencySign)));
        break;
    case Style::Unit:
        options->putDirect(vm, Identifier::fromString(vm, "unit"), jsNontrivialString(vm, m_unit));
        options->putDirect(vm, Identifier::fromString(vm, "unitDisplay"), jsNontrivialString(vm, unitDisplayString(m_unitDisplay)));
        break;
    }
    options->putDirect(vm, Identifier::fromString(vm, "minimumIntegerDigits"), jsNumber(m_minimumIntegerDigits));
    switch (m_roundingType) {
    case IntlRoundingType::FractionDigits:
        options->putDirect(vm, Identifier::fromString(vm, "minimumFractionDigits"), jsNumber(m_minimumFractionDigits));
        options->putDirect(vm, Identifier::fromString(vm, "maximumFractionDigits"), jsNumber(m_maximumFractionDigits));
        break;
    case IntlRoundingType::SignificantDigits:
        options->putDirect(vm, Identifier::fromString(vm, "minimumSignificantDigits"), jsNumber(m_minimumSignificantDigits));
        options->putDirect(vm, Identifier::fromString(vm, "maximumSignificantDigits"), jsNumber(m_maximumSignificantDigits));
        break;
    case IntlRoundingType::CompactRounding:
        break;
    }
    options->putDirect(vm, Identifier::fromString(vm, "useGrouping"), jsBoolean(m_useGrouping));
    options->putDirect(vm, Identifier::fromString(vm, "notation"), jsNontrivialString(vm, notationString(m_notation)));
    if (m_notation == IntlNotation::Compact)
        options->putDirect(vm, Identifier::fromString(vm, "compactDisplay"), jsNontrivialString(vm, compactDisplayString(m_compactDisplay)));
    options->putDirect(vm, Identifier::fromString(vm, "signDisplay"), jsNontrivialString(vm, signDisplayString(m_signDisplay)));
    return options;
}

void IntlNumberFormat::setBoundFormat(VM& vm, JSBoundFunction* format)
{
    m_boundFormat.set(vm, this, format);
}

static ASCIILiteral partTypeString(UNumberFormatFields field, IntlNumberFormat::Style style, double value)
{
    switch (field) {
    case UNUM_INTEGER_FIELD:
        if (std::isnan(value))
            return "nan"_s;
        if (!std::isfinite(value))
            return "infinity"_s;
        return "integer"_s;
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
        return std::signbit(value) ? "minusSign"_s : "plusSign"_s;
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

void IntlNumberFormat::formatToPartsInternal(JSGlobalObject* globalObject, Style style, double value, const String& formatted, IntlFieldIterator& iterator, JSArray* parts, JSString* unit)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto stringLength = formatted.length();

    int32_t literalFieldType = -1;
    IntlNumberFormatField literalField { literalFieldType, stringLength };
    Vector<IntlNumberFormatField, 32> fields(stringLength, literalField);
    int32_t beginIndex = 0;
    int32_t endIndex = 0;
    UErrorCode status = U_ZERO_ERROR;
    auto fieldType = iterator.next(beginIndex, endIndex, status);
    if (U_FAILURE(status)) {
        throwTypeError(globalObject, scope, "Failed to iterate field position iterator"_s);
        return;
    }
    while (fieldType >= 0) {
        size_t size = endIndex - beginIndex;
        for (auto i = beginIndex; i < endIndex; ++i) {
            // Only override previous value if new value is more specific.
            if (fields[i].size >= size)
                fields[i] = IntlNumberFormatField { fieldType, size };
        }
        fieldType = iterator.next(beginIndex, endIndex, status);
        if (U_FAILURE(status)) {
            throwTypeError(globalObject, scope, "Failed to iterate field position iterator"_s);
            return;
        }
    }

    auto literalString = jsNontrivialString(vm, "literal"_s);
    Identifier unitName;
    if (unit)
        unitName = Identifier::fromString(vm, "unit");

    size_t currentIndex = 0;
    while (currentIndex < stringLength) {
        auto startIndex = currentIndex;
        auto fieldType = fields[currentIndex].type;
        while (currentIndex < stringLength && fields[currentIndex].type == fieldType)
            ++currentIndex;
        auto partType = fieldType == literalFieldType ? literalString : jsString(vm, partTypeString(UNumberFormatFields(fieldType), style, value));
        auto partValue = jsSubstring(vm, formatted, startIndex, currentIndex - startIndex);
        JSObject* part = constructEmptyObject(globalObject);
        part->putDirect(vm, vm.propertyNames->type, partType);
        part->putDirect(vm, vm.propertyNames->value, partValue);
        if (unit)
            part->putDirect(vm, unitName, unit);
        parts->push(globalObject, part);
        RETURN_IF_EXCEPTION(scope, void());
    }
}

// https://tc39.github.io/ecma402/#sec-formatnumbertoparts
JSValue IntlNumberFormat::formatToParts(JSGlobalObject* globalObject, double value) const
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
    auto formattedNumber = std::unique_ptr<UFormattedNumber, UFormattedNumberDeleter>(unumf_openResult(&status));
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

    auto resultString = String(result);

    JSArray* parts = JSArray::tryCreate(vm, globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithContiguous), 0);
    if (!parts)
        return throwOutOfMemoryError(globalObject, scope);

    formatToPartsInternal(globalObject, m_style, value, resultString, iterator, parts);
    RETURN_IF_EXCEPTION(scope, { });

    return parts;
}

} // namespace JSC

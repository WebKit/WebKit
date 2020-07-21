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
#include "IntlObjectInlines.h"
#include "JSBoundFunction.h"
#include "JSCInlines.h"
#include "ObjectConstructor.h"
#include <wtf/unicode/icu/ICUHelpers.h>

namespace JSC {

const ClassInfo IntlNumberFormat::s_info = { "Object", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(IntlNumberFormat) };

namespace IntlNumberFormatInternal {
constexpr bool verbose = false;
}

struct IntlNumberFormatField {
    int32_t type;
    size_t size;
};

void IntlNumberFormat::UNumberFormatDeleter::operator()(UNumberFormat* numberFormat) const
{
    if (numberFormat)
        unum_close(numberFormat);
}

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
    std::pair<const char*, unsigned> currencyMinorUnits[] = {
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

    m_style = intlOption<Style>(globalObject, options, vm.propertyNames->style, { { "decimal"_s, Style::Decimal }, { "percent"_s, Style::Percent }, { "currency"_s, Style::Currency } }, "style must be either \"decimal\", \"percent\", or \"currency\""_s, Style::Decimal);
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

    m_currencyDisplay = intlOption<CurrencyDisplay>(globalObject, options, Identifier::fromString(vm, "currencyDisplay"), { { "code"_s, CurrencyDisplay::Code }, { "symbol"_s, CurrencyDisplay::Symbol }, { "name"_s, CurrencyDisplay::Name } }, "currencyDisplay must be either \"code\", \"symbol\", or \"name\""_s, CurrencyDisplay::Symbol);
    RETURN_IF_EXCEPTION(scope, void());

    unsigned minimumIntegerDigits = intlNumberOption(globalObject, options, Identifier::fromString(vm, "minimumIntegerDigits"), 1, 21, 1);
    RETURN_IF_EXCEPTION(scope, void());
    m_minimumIntegerDigits = minimumIntegerDigits;

    unsigned minimumFractionDigitsDefault = (m_style == Style::Currency) ? currencyDigits : 0;

    unsigned minimumFractionDigits = intlNumberOption(globalObject, options, Identifier::fromString(vm, "minimumFractionDigits"), 0, 20, minimumFractionDigitsDefault);
    RETURN_IF_EXCEPTION(scope, void());
    m_minimumFractionDigits = minimumFractionDigits;

    unsigned maximumFractionDigitsDefault;
    if (m_style == Style::Currency)
        maximumFractionDigitsDefault = std::max(minimumFractionDigits, currencyDigits);
    else if (m_style == Style::Percent)
        maximumFractionDigitsDefault = minimumFractionDigits;
    else
        maximumFractionDigitsDefault = std::max(minimumFractionDigits, 3u);

    unsigned maximumFractionDigits = intlNumberOption(globalObject, options, Identifier::fromString(vm, "maximumFractionDigits"), minimumFractionDigits, 20, maximumFractionDigitsDefault);
    RETURN_IF_EXCEPTION(scope, void());
    m_maximumFractionDigits = maximumFractionDigits;

    JSValue minimumSignificantDigitsValue = options->get(globalObject, Identifier::fromString(vm, "minimumSignificantDigits"));
    RETURN_IF_EXCEPTION(scope, void());

    JSValue maximumSignificantDigitsValue = options->get(globalObject, Identifier::fromString(vm, "maximumSignificantDigits"));
    RETURN_IF_EXCEPTION(scope, void());

    if (!minimumSignificantDigitsValue.isUndefined() || !maximumSignificantDigitsValue.isUndefined()) {
        unsigned minimumSignificantDigits = intlDefaultNumberOption(globalObject, minimumSignificantDigitsValue, Identifier::fromString(vm, "minimumSignificantDigits"), 1, 21, 1);
        RETURN_IF_EXCEPTION(scope, void());
        unsigned maximumSignificantDigits = intlDefaultNumberOption(globalObject, maximumSignificantDigitsValue, Identifier::fromString(vm, "maximumSignificantDigits"), minimumSignificantDigits, 21, 21);
        RETURN_IF_EXCEPTION(scope, void());
        m_minimumSignificantDigits = minimumSignificantDigits;
        m_maximumSignificantDigits = maximumSignificantDigits;
    }

    TriState useGrouping = intlBooleanOption(globalObject, options, Identifier::fromString(vm, "useGrouping"));
    RETURN_IF_EXCEPTION(scope, void());
    m_useGrouping = useGrouping != TriState::False;

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
        case CurrencyDisplay::Name:
            style = UNUM_CURRENCY_PLURAL;
            break;
        default:
            ASSERT_NOT_REACHED();
        }
        break;
    default:
        ASSERT_NOT_REACHED();
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
    if (!m_minimumSignificantDigits) {
        unum_setAttribute(m_numberFormat.get(), UNUM_MIN_INTEGER_DIGITS, m_minimumIntegerDigits);
        unum_setAttribute(m_numberFormat.get(), UNUM_MIN_FRACTION_DIGITS, m_minimumFractionDigits);
        unum_setAttribute(m_numberFormat.get(), UNUM_MAX_FRACTION_DIGITS, m_maximumFractionDigits);
    } else {
        unum_setAttribute(m_numberFormat.get(), UNUM_SIGNIFICANT_DIGITS_USED, true);
        unum_setAttribute(m_numberFormat.get(), UNUM_MIN_SIGNIFICANT_DIGITS, m_minimumSignificantDigits);
        unum_setAttribute(m_numberFormat.get(), UNUM_MAX_SIGNIFICANT_DIGITS, m_maximumSignificantDigits);
    }
    unum_setAttribute(m_numberFormat.get(), UNUM_GROUPING_USED, m_useGrouping);
    unum_setAttribute(m_numberFormat.get(), UNUM_ROUNDING_MODE, UNUM_ROUND_HALFUP);
}

// https://tc39.es/ecma402/#sec-formatnumber
JSValue IntlNumberFormat::format(JSGlobalObject* globalObject, double value) const
{
    ASSERT(m_numberFormat);

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    Vector<UChar, 32> buffer;
    auto status = callBufferProducingFunction(unum_formatDouble, m_numberFormat.get(), value, buffer, nullptr);
    if (U_FAILURE(status))
        return throwTypeError(globalObject, scope, "Failed to format a number."_s);

    return jsString(vm, String(buffer));
}

// https://tc39.es/ecma402/#sec-formatnumber
JSValue IntlNumberFormat::format(JSGlobalObject* globalObject, JSBigInt* value) const
{
    ASSERT(m_numberFormat);

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto string = value->toString(globalObject, 10);
    RETURN_IF_EXCEPTION(scope, { });

    ASSERT(string.is8Bit() && string.isAllASCII());
    auto* rawString = reinterpret_cast<const char*>(string.characters8());

    Vector<UChar, 32> buffer;
    auto status = callBufferProducingFunction(unum_formatDecimal, m_numberFormat.get(), rawString, string.length(), buffer, nullptr);
    if (U_FAILURE(status))
        return throwTypeError(globalObject, scope, "Failed to format a BigInt."_s);

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
    case CurrencyDisplay::Name:
        return "name"_s;
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
    if (m_style == Style::Currency) {
        options->putDirect(vm, Identifier::fromString(vm, "currency"), jsNontrivialString(vm, m_currency));
        options->putDirect(vm, Identifier::fromString(vm, "currencyDisplay"), jsNontrivialString(vm, currencyDisplayString(m_currencyDisplay)));
    }
    options->putDirect(vm, Identifier::fromString(vm, "minimumIntegerDigits"), jsNumber(m_minimumIntegerDigits));
    options->putDirect(vm, Identifier::fromString(vm, "minimumFractionDigits"), jsNumber(m_minimumFractionDigits));
    options->putDirect(vm, Identifier::fromString(vm, "maximumFractionDigits"), jsNumber(m_maximumFractionDigits));
    if (m_minimumSignificantDigits) {
        ASSERT(m_maximumSignificantDigits);
        options->putDirect(vm, Identifier::fromString(vm, "minimumSignificantDigits"), jsNumber(m_minimumSignificantDigits));
        options->putDirect(vm, Identifier::fromString(vm, "maximumSignificantDigits"), jsNumber(m_maximumSignificantDigits));
    }
    options->putDirect(vm, Identifier::fromString(vm, "useGrouping"), jsBoolean(m_useGrouping));
    return options;
}

void IntlNumberFormat::setBoundFormat(VM& vm, JSBoundFunction* format)
{
    m_boundFormat.set(vm, this, format);
}

static ASCIILiteral partTypeString(UNumberFormatFields field, double value)
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
    case UNUM_GROUPING_SEPARATOR_FIELD:
        return "group"_s;
    case UNUM_CURRENCY_FIELD:
        return "currency"_s;
    case UNUM_PERCENT_FIELD:
        return "percentSign"_s;
    case UNUM_SIGN_FIELD:
        return value < 0 ? "minusSign"_s : "plusSign"_s;
    // These should not show up because there is no way to specify them in NumberFormat options.
    // If they do, they don't fit well into any of known part types, so consider it an "unknown".
    case UNUM_PERMILL_FIELD:
    case UNUM_EXPONENT_SYMBOL_FIELD:
    case UNUM_EXPONENT_SIGN_FIELD:
    case UNUM_EXPONENT_FIELD:
    // Any newer additions to the UNumberFormatFields enum should just be considered an "unknown" part.
    default:
        return "unknown"_s;
    }
    return "unknown"_s;
}

void IntlNumberFormat::formatToPartsInternal(JSGlobalObject* globalObject, double value, const String& formatted, UFieldPositionIterator* iterator, JSArray* parts, JSString* unit)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto stringLength = formatted.length();

    int32_t literalFieldType = -1;
    IntlNumberFormatField literalField { literalFieldType, stringLength };
    Vector<IntlNumberFormatField, 32> fields(stringLength, literalField);
    int32_t beginIndex = 0;
    int32_t endIndex = 0;
    auto fieldType = ufieldpositer_next(iterator, &beginIndex, &endIndex);
    while (fieldType >= 0) {
        size_t size = endIndex - beginIndex;
        for (auto i = beginIndex; i < endIndex; ++i) {
            // Only override previous value if new value is more specific.
            if (fields[i].size >= size)
                fields[i] = IntlNumberFormatField { fieldType, size };
        }
        fieldType = ufieldpositer_next(iterator, &beginIndex, &endIndex);
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
        auto partType = fieldType == literalFieldType ? literalString : jsString(vm, partTypeString(UNumberFormatFields(fieldType), value));
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
    ASSERT(m_numberFormat);

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    UErrorCode status = U_ZERO_ERROR;
    auto fieldItr = std::unique_ptr<UFieldPositionIterator, UFieldPositionIteratorDeleter>(ufieldpositer_open(&status));
    if (U_FAILURE(status))
        return throwTypeError(globalObject, scope, "failed to open field position iterator"_s);

    Vector<UChar, 32> result;
    status = callBufferProducingFunction(unum_formatDoubleForFields, m_numberFormat.get(), value, result, fieldItr.get());
    if (U_FAILURE(status))
        return throwTypeError(globalObject, scope, "failed to format a number."_s);

    auto resultString = String(result);

    JSArray* parts = JSArray::tryCreate(vm, globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithContiguous), 0);
    if (!parts)
        return throwOutOfMemoryError(globalObject, scope);

    formatToPartsInternal(globalObject, value, resultString, fieldItr.get(), parts);
    RETURN_IF_EXCEPTION(scope, { });

    return parts;
}

} // namespace JSC

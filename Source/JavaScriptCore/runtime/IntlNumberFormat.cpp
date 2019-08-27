/*
 * Copyright (C) 2015 Andy VanWagoner (andy@vanwagoner.family)
 * Copyright (C) 2016 Sukolsak Sakshuwong (sukolsak@gmail.com)
 * Copyright (C) 2016-2019 Apple Inc. All rights reserved.
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

#if ENABLE(INTL)

#include "CatchScope.h"
#include "Error.h"
#include "IntlNumberFormatConstructor.h"
#include "IntlObject.h"
#include "JSBoundFunction.h"
#include "JSCInlines.h"
#include "ObjectConstructor.h"

#if HAVE(ICU_FORMAT_DOUBLE_FOR_FIELDS)
#include <unicode/ufieldpositer.h>
#endif

namespace JSC {

const ClassInfo IntlNumberFormat::s_info = { "Object", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(IntlNumberFormat) };

static const char* const relevantNumberExtensionKeys[1] = { "nu" };

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
    : JSDestructibleObject(vm, structure)
{
}

void IntlNumberFormat::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    ASSERT(inherits(vm, info()));
}

void IntlNumberFormat::destroy(JSCell* cell)
{
    static_cast<IntlNumberFormat*>(cell)->IntlNumberFormat::~IntlNumberFormat();
}

void IntlNumberFormat::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    IntlNumberFormat* thisObject = jsCast<IntlNumberFormat*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());

    Base::visitChildren(thisObject, visitor);

    visitor.append(thisObject->m_boundFormat);
}

namespace IntlNFInternal {
static Vector<String> localeData(const String& locale, size_t keyIndex)
{
    // 9.1 Internal slots of Service Constructors & 11.2.3 Internal slots (ECMA-402 2.0)
    ASSERT_UNUSED(keyIndex, !keyIndex); // The index of the extension key "nu" in relevantExtensionKeys is 0.
    return numberingSystemsForLocale(locale);
}
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

void IntlNumberFormat::initializeNumberFormat(ExecState& state, JSValue locales, JSValue optionsValue)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // 11.1.2 InitializeNumberFormat (numberFormat, locales, options) (ECMA-402)
    // https://tc39.github.io/ecma402/#sec-initializenumberformat

    auto requestedLocales = canonicalizeLocaleList(state, locales);
    RETURN_IF_EXCEPTION(scope, void());

    JSObject* options;
    if (optionsValue.isUndefined())
        options = constructEmptyObject(&state, state.lexicalGlobalObject()->nullPrototypeObjectStructure());
    else {
        options = optionsValue.toObject(&state);
        RETURN_IF_EXCEPTION(scope, void());
    }

    HashMap<String, String> opt;

    String matcher = intlStringOption(state, options, vm.propertyNames->localeMatcher, { "lookup", "best fit" }, "localeMatcher must be either \"lookup\" or \"best fit\"", "best fit");
    RETURN_IF_EXCEPTION(scope, void());
    opt.add("localeMatcher"_s, matcher);

    auto& availableLocales = state.jsCallee()->globalObject(vm)->intlNumberFormatAvailableLocales();
    auto result = resolveLocale(state, availableLocales, requestedLocales, opt, relevantNumberExtensionKeys, WTF_ARRAY_LENGTH(relevantNumberExtensionKeys), IntlNFInternal::localeData);

    m_locale = result.get("locale"_s);
    if (m_locale.isEmpty()) {
        throwTypeError(&state, scope, "failed to initialize NumberFormat due to invalid locale"_s);
        return;
    }

    m_numberingSystem = result.get("nu"_s);

    String styleString = intlStringOption(state, options, Identifier::fromString(vm, "style"), { "decimal", "percent", "currency" }, "style must be either \"decimal\", \"percent\", or \"currency\"", "decimal");
    RETURN_IF_EXCEPTION(scope, void());
    if (styleString == "decimal")
        m_style = Style::Decimal;
    else if (styleString == "percent")
        m_style = Style::Percent;
    else if (styleString == "currency")
        m_style = Style::Currency;
    else
        ASSERT_NOT_REACHED();

    String currency = intlStringOption(state, options, Identifier::fromString(vm, "currency"), { }, nullptr, nullptr);
    RETURN_IF_EXCEPTION(scope, void());
    if (!currency.isNull()) {
        if (currency.length() != 3 || !currency.isAllSpecialCharacters<isASCIIAlpha>()) {
            throwException(&state, scope, createRangeError(&state, "currency is not a well-formed currency code"_s));
            return;
        }
    }

    unsigned currencyDigits = 0;
    if (m_style == Style::Currency) {
        if (currency.isNull()) {
            throwTypeError(&state, scope, "currency must be a string"_s);
            return;
        }

        currency = currency.convertToASCIIUppercase();
        m_currency = currency;
        currencyDigits = computeCurrencyDigits(currency);
    }

    String currencyDisplayString = intlStringOption(state, options, Identifier::fromString(vm, "currencyDisplay"), { "code", "symbol", "name" }, "currencyDisplay must be either \"code\", \"symbol\", or \"name\"", "symbol");
    RETURN_IF_EXCEPTION(scope, void());
    if (m_style == Style::Currency) {
        if (currencyDisplayString == "code")
            m_currencyDisplay = CurrencyDisplay::Code;
        else if (currencyDisplayString == "symbol")
            m_currencyDisplay = CurrencyDisplay::Symbol;
        else if (currencyDisplayString == "name")
            m_currencyDisplay = CurrencyDisplay::Name;
        else
            ASSERT_NOT_REACHED();
    }

    unsigned minimumIntegerDigits = intlNumberOption(state, options, Identifier::fromString(vm, "minimumIntegerDigits"), 1, 21, 1);
    RETURN_IF_EXCEPTION(scope, void());
    m_minimumIntegerDigits = minimumIntegerDigits;

    unsigned minimumFractionDigitsDefault = (m_style == Style::Currency) ? currencyDigits : 0;

    unsigned minimumFractionDigits = intlNumberOption(state, options, Identifier::fromString(vm, "minimumFractionDigits"), 0, 20, minimumFractionDigitsDefault);
    RETURN_IF_EXCEPTION(scope, void());
    m_minimumFractionDigits = minimumFractionDigits;

    unsigned maximumFractionDigitsDefault;
    if (m_style == Style::Currency)
        maximumFractionDigitsDefault = std::max(minimumFractionDigits, currencyDigits);
    else if (m_style == Style::Percent)
        maximumFractionDigitsDefault = minimumFractionDigits;
    else
        maximumFractionDigitsDefault = std::max(minimumFractionDigits, 3u);

    unsigned maximumFractionDigits = intlNumberOption(state, options, Identifier::fromString(vm, "maximumFractionDigits"), minimumFractionDigits, 20, maximumFractionDigitsDefault);
    RETURN_IF_EXCEPTION(scope, void());
    m_maximumFractionDigits = maximumFractionDigits;

    JSValue minimumSignificantDigitsValue = options->get(&state, Identifier::fromString(vm, "minimumSignificantDigits"));
    RETURN_IF_EXCEPTION(scope, void());

    JSValue maximumSignificantDigitsValue = options->get(&state, Identifier::fromString(vm, "maximumSignificantDigits"));
    RETURN_IF_EXCEPTION(scope, void());

    if (!minimumSignificantDigitsValue.isUndefined() || !maximumSignificantDigitsValue.isUndefined()) {
        unsigned minimumSignificantDigits = intlDefaultNumberOption(state, minimumSignificantDigitsValue, Identifier::fromString(vm, "minimumSignificantDigits"), 1, 21, 1);
        RETURN_IF_EXCEPTION(scope, void());
        unsigned maximumSignificantDigits = intlDefaultNumberOption(state, maximumSignificantDigitsValue, Identifier::fromString(vm, "maximumSignificantDigits"), minimumSignificantDigits, 21, 21);
        RETURN_IF_EXCEPTION(scope, void());
        m_minimumSignificantDigits = minimumSignificantDigits;
        m_maximumSignificantDigits = maximumSignificantDigits;
    }

    bool usesFallback;
    bool useGrouping = intlBooleanOption(state, options, Identifier::fromString(vm, "useGrouping"), usesFallback);
    if (usesFallback)
        useGrouping = true;
    RETURN_IF_EXCEPTION(scope, void());
    m_useGrouping = useGrouping;

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

    UErrorCode status = U_ZERO_ERROR;
    m_numberFormat = std::unique_ptr<UNumberFormat, UNumberFormatDeleter>(unum_open(style, nullptr, 0, m_locale.utf8().data(), nullptr, &status));
    if (U_FAILURE(status)) {
        throwTypeError(&state, scope, "failed to initialize NumberFormat"_s);
        return;
    }

    if (m_style == Style::Currency) {
        unum_setTextAttribute(m_numberFormat.get(), UNUM_CURRENCY_CODE, StringView(m_currency).upconvertedCharacters(), m_currency.length(), &status);
        if (U_FAILURE(status)) {
            throwTypeError(&state, scope, "failed to initialize NumberFormat"_s);
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

    m_initializedNumberFormat = true;
}

JSValue IntlNumberFormat::formatNumber(ExecState& state, double number)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // 11.3.4 FormatNumber abstract operation (ECMA-402 2.0)
    if (!m_initializedNumberFormat)
        return throwTypeError(&state, scope, "Intl.NumberFormat.prototype.format called on value that's not an object initialized as a NumberFormat"_s);

    // Map negative zero to positive zero.
    if (!number)
        number = 0.0;

    UErrorCode status = U_ZERO_ERROR;
    Vector<UChar, 32> buffer(32);
    auto length = unum_formatDouble(m_numberFormat.get(), number, buffer.data(), buffer.size(), nullptr, &status);
    if (status == U_BUFFER_OVERFLOW_ERROR) {
        buffer.grow(length);
        status = U_ZERO_ERROR;
        unum_formatDouble(m_numberFormat.get(), number, buffer.data(), length, nullptr, &status);
    }
    if (U_FAILURE(status))
        return throwException(&state, scope, createError(&state, "Failed to format a number."_s));

    return jsString(vm, String(buffer.data(), length));
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

JSObject* IntlNumberFormat::resolvedOptions(ExecState& state)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // 11.3.5 Intl.NumberFormat.prototype.resolvedOptions() (ECMA-402 2.0)
    // The function returns a new object whose properties and attributes are set as if
    // constructed by an object literal assigning to each of the following properties the
    // value of the corresponding internal slot of this NumberFormat object (see 11.4):
    // locale, numberingSystem, style, currency, currencyDisplay, minimumIntegerDigits,
    // minimumFractionDigits, maximumFractionDigits, minimumSignificantDigits,
    // maximumSignificantDigits, and useGrouping. Properties whose corresponding internal
    // slots are not present are not assigned.

    if (!m_initializedNumberFormat) {
        initializeNumberFormat(state, jsUndefined(), jsUndefined());
        scope.assertNoException();
    }

    JSObject* options = constructEmptyObject(&state);
    options->putDirect(vm, vm.propertyNames->locale, jsString(vm, m_locale));
    options->putDirect(vm, Identifier::fromString(vm, "numberingSystem"), jsString(vm, m_numberingSystem));
    options->putDirect(vm, Identifier::fromString(vm, "style"), jsNontrivialString(vm, styleString(m_style)));
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

#if HAVE(ICU_FORMAT_DOUBLE_FOR_FIELDS)
void IntlNumberFormat::UFieldPositionIteratorDeleter::operator()(UFieldPositionIterator* iterator) const
{
    if (iterator)
        ufieldpositer_close(iterator);
}

ASCIILiteral IntlNumberFormat::partTypeString(UNumberFormatFields field, double value)
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
#if !defined(U_HIDE_DEPRECATED_API)
    case UNUM_FIELD_COUNT:
#endif
    // Any newer additions to the UNumberFormatFields enum should just be considered an "unknown" part.
    default:
        return "unknown"_s;
    }
    return "unknown"_s;
}

JSValue IntlNumberFormat::formatToParts(ExecState& exec, double value)
{
    VM& vm = exec.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // FormatNumberToParts (ECMA-402)
    // https://tc39.github.io/ecma402/#sec-formatnumbertoparts
    // https://tc39.github.io/ecma402/#sec-partitionnumberpattern

    if (!m_initializedNumberFormat)
        return throwTypeError(&exec, scope, "Intl.NumberFormat.prototype.formatToParts called on value that's not an object initialized as a NumberFormat"_s);

    UErrorCode status = U_ZERO_ERROR;
    auto fieldItr = std::unique_ptr<UFieldPositionIterator, UFieldPositionIteratorDeleter>(ufieldpositer_open(&status));
    if (U_FAILURE(status))
        return throwTypeError(&exec, scope, "failed to open field position iterator"_s);

    status = U_ZERO_ERROR;
    Vector<UChar, 32> result(32);
    auto resultLength = unum_formatDoubleForFields(m_numberFormat.get(), value, result.data(), result.size(), fieldItr.get(), &status);
    if (status == U_BUFFER_OVERFLOW_ERROR) {
        status = U_ZERO_ERROR;
        result.grow(resultLength);
        unum_formatDoubleForFields(m_numberFormat.get(), value, result.data(), resultLength, fieldItr.get(), &status);
    }
    if (U_FAILURE(status))
        return throwTypeError(&exec, scope, "failed to format a number."_s);

    int32_t literalFieldType = -1;
    auto literalField = IntlNumberFormatField(literalFieldType, resultLength);
    Vector<IntlNumberFormatField> fields(resultLength, literalField);
    int32_t beginIndex = 0;
    int32_t endIndex = 0;
    auto fieldType = ufieldpositer_next(fieldItr.get(), &beginIndex, &endIndex);
    while (fieldType >= 0) {
        auto size = endIndex - beginIndex;
        for (auto i = beginIndex; i < endIndex; ++i) {
            // Only override previous value if new value is more specific.
            if (fields[i].size >= size)
                fields[i] = IntlNumberFormatField(fieldType, size);
        }
        fieldType = ufieldpositer_next(fieldItr.get(), &beginIndex, &endIndex);
    }

    JSGlobalObject* globalObject = exec.jsCallee()->globalObject(vm);
    JSArray* parts = JSArray::tryCreate(vm, globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithContiguous), 0);
    if (!parts)
        return throwOutOfMemoryError(&exec, scope);
    unsigned index = 0;

    auto resultString = String(result.data(), resultLength);
    auto typePropertyName = Identifier::fromString(vm, "type");
    auto literalString = jsString(vm, "literal"_s);

    int32_t currentIndex = 0;
    while (currentIndex < resultLength) {
        auto startIndex = currentIndex;
        auto fieldType = fields[currentIndex].type;
        while (currentIndex < resultLength && fields[currentIndex].type == fieldType)
            ++currentIndex;
        auto partType = fieldType == literalFieldType ? literalString : jsString(vm, partTypeString(UNumberFormatFields(fieldType), value));
        auto partValue = jsSubstring(vm, resultString, startIndex, currentIndex - startIndex);
        JSObject* part = constructEmptyObject(&exec);
        part->putDirect(vm, typePropertyName, partType);
        part->putDirect(vm, vm.propertyNames->value, partValue);
        parts->putDirectIndex(&exec, index++, part);
        RETURN_IF_EXCEPTION(scope, { });
    }

    return parts;
}
#endif

} // namespace JSC

#endif // ENABLE(INTL)

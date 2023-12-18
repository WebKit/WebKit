/*
 * Copyright (C) 2015 Andy VanWagoner (andy@vanwagoner.family)
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

#pragma once

#include "JSObject.h"
#include "MathCommon.h"
#include "TemporalObject.h"
#include <unicode/unum.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/unicode/icu/ICUHelpers.h>

#if !defined(HAVE_ICU_U_NUMBER_FORMATTER)
// UNUM_COMPACT_FIELD and UNUM_MEASURE_UNIT_FIELD are available after ICU 64.
#if U_ICU_VERSION_MAJOR_NUM >= 64
#define HAVE_ICU_U_NUMBER_FORMATTER 1
#endif
#endif

#if !defined(HAVE_ICU_U_NUMBER_RANGE_FORMATTER)
#if U_ICU_VERSION_MAJOR_NUM >= 68
#define HAVE_ICU_U_NUMBER_RANGE_FORMATTER 1
#endif
#endif

#if !defined(HAVE_ICU_U_NUMBER_RANGE_FORMATTER_FORMAT_RANGE_TO_PARTS)
#if U_ICU_VERSION_MAJOR_NUM >= 69
#define HAVE_ICU_U_NUMBER_RANGE_FORMATTER_FORMAT_RANGE_TO_PARTS 1
#endif
#endif

struct UFormattedValue;
struct UNumberFormatter;
struct UNumberRangeFormatter;

namespace JSC {

class IntlFieldIterator;
class JSBoundFunction;
enum class RelevantExtensionKey : uint8_t;

enum class IntlRoundingType : uint8_t { FractionDigits, SignificantDigits, MorePrecision, LessPrecision };
enum class IntlRoundingPriority : uint8_t { Auto, MorePrecision, LessPrecision };
enum class IntlTrailingZeroDisplay : uint8_t { Auto, StripIfInteger };
enum class IntlNotation : uint8_t { Standard, Scientific, Engineering, Compact };
template<typename IntlType> void setNumberFormatDigitOptions(JSGlobalObject*, IntlType*, JSObject*, unsigned minimumFractionDigitsDefault, unsigned maximumFractionDigitsDefault, IntlNotation);
template<typename IntlType> void appendNumberFormatDigitOptionsToSkeleton(IntlType*, StringBuilder&);

#if HAVE(ICU_U_NUMBER_FORMATTER)
struct UNumberFormatterDeleter {
    JS_EXPORT_PRIVATE void operator()(UNumberFormatter*);
};
#endif

#if HAVE(ICU_U_NUMBER_RANGE_FORMATTER)
struct UNumberRangeFormatterDeleter {
    JS_EXPORT_PRIVATE void operator()(UNumberRangeFormatter*);
};
#endif

class IntlMathematicalValue {
    WTF_MAKE_TZONE_ALLOCATED(IntlMathematicalValue);
public:
    enum class NumberType { Integer, Infinity, NaN, };
    using Value = std::variant<double, CString>;

    IntlMathematicalValue() = default;

    explicit IntlMathematicalValue(double value)
        : m_value(purifyNaN(value))
        , m_numberType(numberTypeFromDouble(value))
        , m_sign(!std::isnan(value) && std::signbit(value))
    { }

    explicit IntlMathematicalValue(NumberType numberType, bool sign, CString value)
        : m_value(value)
        , m_numberType(numberType)
        , m_sign(sign)
    {
    }

    static IntlMathematicalValue parseString(JSGlobalObject*, StringView);

    void ensureNonDouble()
    {
        if (std::holds_alternative<double>(m_value)) {
            switch (m_numberType) {
            case NumberType::Integer: {
                double value = std::get<double>(m_value);
                if (isNegativeZero(value))
                    m_value = CString("-0");
                else
                    m_value = String::number(value).ascii();
                break;
            }
            case NumberType::NaN:
                m_value = CString("nan");
                break;
            case NumberType::Infinity:
                m_value = CString(m_sign ? "-infinity" : "infinity");
                break;
            }
        }
    }

    NumberType numberType() const { return m_numberType; }
    bool sign() const { return m_sign; }
    std::optional<double> tryGetDouble() const
    {
        if (std::holds_alternative<double>(m_value))
            return std::get<double>(m_value);
        return std::nullopt;
    }
    const CString& getString() const
    {
        ASSERT(std::holds_alternative<CString>(m_value));
        return std::get<CString>(m_value);
    }

    static NumberType numberTypeFromDouble(double value)
    {
        if (std::isnan(value))
            return NumberType::NaN;
        if (!std::isfinite(value))
            return NumberType::Infinity;
        return NumberType::Integer;
    }

private:
    Value m_value { 0.0 };
    NumberType m_numberType { NumberType::Integer };
    bool m_sign { false };
};

class IntlNumberFormat final : public JSNonFinalObject {
public:
    using Base = JSNonFinalObject;

    static constexpr bool needsDestruction = true;

    static void destroy(JSCell* cell)
    {
        static_cast<IntlNumberFormat*>(cell)->IntlNumberFormat::~IntlNumberFormat();
    }

    template<typename CellType, SubspaceAccess mode>
    static GCClient::IsoSubspace* subspaceFor(VM& vm)
    {
        return vm.intlNumberFormatSpace<mode>();
    }

    static IntlNumberFormat* create(VM&, Structure*);
    static Structure* createStructure(VM&, JSGlobalObject*, JSValue);

    DECLARE_INFO;

    void initializeNumberFormat(JSGlobalObject*, JSValue locales, JSValue optionsValue);
    JSValue format(JSGlobalObject*, double) const;
    JSValue format(JSGlobalObject*, IntlMathematicalValue&&) const;
    JSValue formatToParts(JSGlobalObject*, double, JSString* sourceType = nullptr) const;
#if HAVE(ICU_U_NUMBER_FORMATTER)
    JSValue formatToParts(JSGlobalObject*, IntlMathematicalValue&&, JSString* sourceType = nullptr) const;
#endif
    JSObject* resolvedOptions(JSGlobalObject*) const;

#if HAVE(ICU_U_NUMBER_RANGE_FORMATTER)
    JSValue formatRange(JSGlobalObject*, double, double) const;
    JSValue formatRange(JSGlobalObject*, IntlMathematicalValue&&, IntlMathematicalValue&&) const;
#endif

#if HAVE(ICU_U_NUMBER_RANGE_FORMATTER_FORMAT_RANGE_TO_PARTS)
    JSValue formatRangeToParts(JSGlobalObject*, double, double) const;
    JSValue formatRangeToParts(JSGlobalObject*, IntlMathematicalValue&&, IntlMathematicalValue&&) const;
#endif

    JSBoundFunction* boundFormat() const { return m_boundFormat.get(); }
    void setBoundFormat(VM&, JSBoundFunction*);

    enum class Style : uint8_t { Decimal, Percent, Currency, Unit };

    static void formatToPartsInternal(JSGlobalObject*, Style, bool sign, IntlMathematicalValue::NumberType, const String& formatted, IntlFieldIterator&, JSArray*, JSString* sourceType, JSString* unit);
    static void formatRangeToPartsInternal(JSGlobalObject*, Style, IntlMathematicalValue&&, IntlMathematicalValue&&, const UFormattedValue*, JSArray*);

    template<typename IntlType>
    friend void setNumberFormatDigitOptions(JSGlobalObject*, IntlType*, JSObject*, unsigned minimumFractionDigitsDefault, unsigned maximumFractionDigitsDefault, IntlNotation);
    template<typename IntlType>
    friend void appendNumberFormatDigitOptionsToSkeleton(IntlType*, StringBuilder&);

    static ASCIILiteral notationString(IntlNotation);

    static IntlNumberFormat* unwrapForOldFunctions(JSGlobalObject*, JSValue);

    static ASCIILiteral roundingModeString(RoundingMode);
    static ASCIILiteral roundingPriorityString(IntlRoundingType);

private:
    IntlNumberFormat(VM&, Structure*);
    DECLARE_DEFAULT_FINISH_CREATION;
    DECLARE_VISIT_CHILDREN;

    static Vector<String> localeData(const String&, RelevantExtensionKey);

    enum class CurrencyDisplay : uint8_t { Code, Symbol, NarrowSymbol, Name };
    enum class CurrencySign : uint8_t { Standard, Accounting };
    enum class UnitDisplay : uint8_t { Short, Narrow, Long };
    enum class CompactDisplay : uint8_t { Short, Long };
    enum class SignDisplay : uint8_t { Auto, Never, Always, ExceptZero, Negative };
    enum class UseGrouping : uint8_t { False, Min2, Auto, Always };

    static ASCIILiteral styleString(Style);
    static ASCIILiteral currencyDisplayString(CurrencyDisplay);
    static ASCIILiteral currencySignString(CurrencySign);
    static ASCIILiteral unitDisplayString(UnitDisplay);
    static ASCIILiteral compactDisplayString(CompactDisplay);
    static ASCIILiteral signDisplayString(SignDisplay);
    static ASCIILiteral trailingZeroDisplayString(IntlTrailingZeroDisplay);
    static JSValue useGroupingValue(VM&, UseGrouping);

    WriteBarrier<JSBoundFunction> m_boundFormat;
#if HAVE(ICU_U_NUMBER_FORMATTER)
    std::unique_ptr<UNumberFormatter, UNumberFormatterDeleter> m_numberFormatter;
#if HAVE(ICU_U_NUMBER_RANGE_FORMATTER)
    std::unique_ptr<UNumberRangeFormatter, UNumberRangeFormatterDeleter> m_numberRangeFormatter;
#endif
#else
    std::unique_ptr<UNumberFormat, ICUDeleter<unum_close>> m_numberFormat;
#endif

    String m_locale;
    String m_numberingSystem;
    String m_currency;
    String m_unit;
    unsigned m_minimumIntegerDigits { 1 };
    unsigned m_minimumFractionDigits { 0 };
    unsigned m_maximumFractionDigits { 3 };
    unsigned m_minimumSignificantDigits { 0 };
    unsigned m_maximumSignificantDigits { 0 };
    unsigned m_roundingIncrement { 1 };
    Style m_style { Style::Decimal };
    CurrencyDisplay m_currencyDisplay;
    CurrencySign m_currencySign;
    UnitDisplay m_unitDisplay;
    CompactDisplay m_compactDisplay;
    IntlNotation m_notation { IntlNotation::Standard };
    SignDisplay m_signDisplay;
    IntlTrailingZeroDisplay m_trailingZeroDisplay { IntlTrailingZeroDisplay::Auto };
    UseGrouping m_useGrouping { UseGrouping::Always };
    RoundingMode m_roundingMode { RoundingMode::HalfExpand };
    IntlRoundingType m_roundingType { IntlRoundingType::FractionDigits };
};

} // namespace JSC

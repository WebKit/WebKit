/*
 * Copyright (C) 2015 Andy VanWagoner (andy@vanwagoner.family)
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

#pragma once

#include "JSObject.h"
#include <unicode/unum.h>
#include <wtf/unicode/icu/ICUHelpers.h>

#if !defined(HAVE_ICU_U_NUMBER_FORMATTER)
// UNUM_COMPACT_FIELD and UNUM_MEASURE_UNIT_FIELD are available after ICU 64.
#if U_ICU_VERSION_MAJOR_NUM >= 64
#define HAVE_ICU_U_NUMBER_FORMATTER 1
#endif
#endif

#if HAVE(ICU_U_NUMBER_FORMATTER)
#include <unicode/unumberformatter.h>
#endif

namespace JSC {

class IntlFieldIterator;
class JSBoundFunction;
enum class RelevantExtensionKey : uint8_t;

enum class IntlRoundingType : uint8_t { FractionDigits, SignificantDigits, CompactRounding };
enum class IntlNotation : uint8_t { Standard, Scientific, Engineering, Compact };
template<typename IntlType> void setNumberFormatDigitOptions(JSGlobalObject*, IntlType*, JSObject*, unsigned minimumFractionDigitsDefault, unsigned maximumFractionDigitsDefault, IntlNotation);

class IntlNumberFormat final : public JSNonFinalObject {
public:
    using Base = JSNonFinalObject;

    static constexpr bool needsDestruction = true;

    static void destroy(JSCell* cell)
    {
        static_cast<IntlNumberFormat*>(cell)->IntlNumberFormat::~IntlNumberFormat();
    }

    template<typename CellType, SubspaceAccess mode>
    static IsoSubspace* subspaceFor(VM& vm)
    {
        return vm.intlNumberFormatSpace<mode>();
    }

    static IntlNumberFormat* create(VM&, Structure*);
    static Structure* createStructure(VM&, JSGlobalObject*, JSValue);

    DECLARE_INFO;

    void initializeNumberFormat(JSGlobalObject*, JSValue locales, JSValue optionsValue);
    JSValue format(JSGlobalObject*, double) const;
    JSValue format(JSGlobalObject*, JSBigInt*) const;
    JSValue formatToParts(JSGlobalObject*, double) const;
    JSObject* resolvedOptions(JSGlobalObject*) const;

    JSBoundFunction* boundFormat() const { return m_boundFormat.get(); }
    void setBoundFormat(VM&, JSBoundFunction*);

    enum class Style : uint8_t { Decimal, Percent, Currency, Unit };

    static void formatToPartsInternal(JSGlobalObject*, Style, double, const String& formatted, IntlFieldIterator&, JSArray*, JSString* unit = nullptr);

    template<typename IntlType>
    friend void setNumberFormatDigitOptions(JSGlobalObject*, IntlType*, JSObject*, unsigned minimumFractionDigitsDefault, unsigned maximumFractionDigitsDefault, IntlNotation);

    static ASCIILiteral notationString(IntlNotation);

private:
    IntlNumberFormat(VM&, Structure*);
    void finishCreation(VM&);
    static void visitChildren(JSCell*, SlotVisitor&);

    static Vector<String> localeData(const String&, RelevantExtensionKey);

    enum class CurrencyDisplay : uint8_t { Code, Symbol, NarrowSymbol, Name };
    enum class CurrencySign : uint8_t { Standard, Accounting };
    enum class UnitDisplay : uint8_t { Short, Narrow, Long };
    enum class CompactDisplay : uint8_t { Short, Long };
    enum class SignDisplay : uint8_t { Auto, Never, Always, ExceptZero };

    static ASCIILiteral styleString(Style);
    static ASCIILiteral currencyDisplayString(CurrencyDisplay);
    static ASCIILiteral currencySignString(CurrencySign);
    static ASCIILiteral unitDisplayString(UnitDisplay);
    static ASCIILiteral compactDisplayString(CompactDisplay);
    static ASCIILiteral signDisplayString(SignDisplay);

    WriteBarrier<JSBoundFunction> m_boundFormat;
#if HAVE(ICU_U_NUMBER_FORMATTER)
    using UNumberFormatterDeleter = ICUDeleter<unumf_close>;
    using UFormattedNumberDeleter = ICUDeleter<unumf_closeResult>;
    std::unique_ptr<UNumberFormatter, UNumberFormatterDeleter> m_numberFormatter;
#else
    using UNumberFormatDeleter = ICUDeleter<unum_close>;
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
    Style m_style { Style::Decimal };
    CurrencyDisplay m_currencyDisplay;
    CurrencySign m_currencySign;
    UnitDisplay m_unitDisplay;
    CompactDisplay m_compactDisplay;
    IntlNotation m_notation { IntlNotation::Standard };
    SignDisplay m_signDisplay;
    bool m_useGrouping { true };
    IntlRoundingType m_roundingType { IntlRoundingType::FractionDigits };
};

} // namespace JSC

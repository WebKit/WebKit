/*
 * Copyright (C) 2018 Andy VanWagoner (andy@vanwagoner.family)
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

#include "IntlNumberFormat.h"
#include <unicode/unum.h>
#include <wtf/unicode/icu/ICUHelpers.h>

struct UPluralRules;

namespace JSC {

struct UPluralRulesDeleter {
    JS_EXPORT_PRIVATE void operator()(UPluralRules*);
};

enum class RelevantExtensionKey : uint8_t;

class IntlPluralRules final : public JSNonFinalObject {
public:
    using Base = JSNonFinalObject;

    static constexpr bool needsDestruction = true;

    static void destroy(JSCell* cell)
    {
        static_cast<IntlPluralRules*>(cell)->IntlPluralRules::~IntlPluralRules();
    }

    template<typename CellType, SubspaceAccess mode>
    static GCClient::IsoSubspace* subspaceFor(VM& vm)
    {
        return vm.intlPluralRulesSpace<mode>();
    }

    static IntlPluralRules* create(VM&, Structure*);
    static Structure* createStructure(VM&, JSGlobalObject*, JSValue);

    DECLARE_INFO;

    template<typename IntlType>
    friend void setNumberFormatDigitOptions(JSGlobalObject*, IntlType*, JSObject*, unsigned minimumFractionDigitsDefault, unsigned maximumFractionDigitsDefault, IntlNotation);
    template<typename IntlType>
    friend void appendNumberFormatDigitOptionsToSkeleton(IntlType*, StringBuilder&);

    void initializePluralRules(JSGlobalObject*, JSValue locales, JSValue options);
    JSValue select(JSGlobalObject*, double value) const;
    JSObject* resolvedOptions(JSGlobalObject*) const;

#if HAVE(ICU_U_NUMBER_RANGE_FORMATTER)
    JSValue selectRange(JSGlobalObject*, double start, double end) const;
#endif

private:
    IntlPluralRules(VM&, Structure*);
    DECLARE_DEFAULT_FINISH_CREATION;
    DECLARE_VISIT_CHILDREN;

    static Vector<String> localeData(const String&, RelevantExtensionKey);

    enum class Type : bool { Cardinal, Ordinal };

    std::unique_ptr<UPluralRules, UPluralRulesDeleter> m_pluralRules;
#if HAVE(ICU_U_NUMBER_FORMATTER)
    std::unique_ptr<UNumberFormatter, UNumberFormatterDeleter> m_numberFormatter;
#if HAVE(ICU_U_NUMBER_RANGE_FORMATTER)
    std::unique_ptr<UNumberRangeFormatter, UNumberRangeFormatterDeleter> m_numberRangeFormatter;
#endif
#else
    using UNumberFormatDeleter = ICUDeleter<unum_close>;
    std::unique_ptr<UNumberFormat, UNumberFormatDeleter> m_numberFormat;
#endif

    String m_locale;
    unsigned m_minimumIntegerDigits { 1 };
    unsigned m_minimumFractionDigits { 0 };
    unsigned m_maximumFractionDigits { 3 };
    unsigned m_minimumSignificantDigits { 0 };
    unsigned m_maximumSignificantDigits { 0 };
    unsigned m_roundingIncrement { 1 };
    IntlTrailingZeroDisplay m_trailingZeroDisplay { IntlTrailingZeroDisplay::Auto };
    RoundingMode m_roundingMode { RoundingMode::HalfExpand };
    IntlRoundingType m_roundingType { IntlRoundingType::FractionDigits };
    Type m_type { Type::Cardinal };
};

} // namespace JSC

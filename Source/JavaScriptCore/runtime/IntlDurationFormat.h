/*
 * Copyright (C) 2020-2022 Apple Inc. All rights reserved.
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

#include "ISO8601.h"
#include "IntlListFormat.h"
#include "IntlNumberFormat.h"
#include "JSObject.h"
#include <wtf/unicode/icu/ICUHelpers.h>

namespace JSC {

enum class RelevantExtensionKey : uint8_t;

class IntlDurationFormat final : public JSNonFinalObject {
public:
    using Base = JSNonFinalObject;

    static constexpr bool needsDestruction = true;

    static void destroy(JSCell* cell)
    {
        static_cast<IntlDurationFormat*>(cell)->IntlDurationFormat::~IntlDurationFormat();
    }

    template<typename CellType, SubspaceAccess mode>
    static GCClient::IsoSubspace* subspaceFor(VM& vm)
    {
        return vm.intlDurationFormatSpace<mode>();
    }

    static IntlDurationFormat* create(VM&, Structure*);
    static Structure* createStructure(VM&, JSGlobalObject*, JSValue);

    DECLARE_INFO;

    void initializeDurationFormat(JSGlobalObject*, JSValue localesValue, JSValue optionsValue);

    JSValue format(JSGlobalObject*, ISO8601::Duration) const;
    JSValue formatToParts(JSGlobalObject*, ISO8601::Duration) const;
    JSObject* resolvedOptions(JSGlobalObject*) const;

    enum class Display : uint8_t { Always, Auto };
    enum class Style : uint8_t { Long, Short, Narrow, Digital };
    enum class UnitStyle : uint8_t { Long, Short, Narrow, Numeric, TwoDigit };
    static constexpr unsigned numberOfUnitStyles = 5;
    static_assert(static_cast<unsigned>(Style::Long) == static_cast<unsigned>(UnitStyle::Long));
    static_assert(static_cast<unsigned>(Style::Short) == static_cast<unsigned>(UnitStyle::Short));
    static_assert(static_cast<unsigned>(Style::Narrow) == static_cast<unsigned>(UnitStyle::Narrow));

    class UnitData {
    public:
        UnitData() = default;
        UnitData(UnitStyle style, Display display)
            : m_style(style)
            , m_display(display)
        {
        }

        UnitStyle style() const { return m_style; }
        Display display() const { return m_display; }

    private:
        UnitStyle m_style : 7 { UnitStyle::Long };
        Display m_display : 1 { Display::Always };
    };

    const UnitData* units() const { return m_units; }
    unsigned fractionalDigits() const { return m_fractionalDigits; }
    const String& numberingSystem() const { return m_numberingSystem; }
    const CString& dataLocaleWithExtensions() const { return m_dataLocaleWithExtensions; }

private:
    IntlDurationFormat(VM&, Structure*);
    void finishCreation(VM&);

    static ASCIILiteral styleString(Style);
    static ASCIILiteral unitStyleString(UnitStyle);
    static ASCIILiteral displayString(Display);

#if HAVE(ICU_U_LIST_FORMATTER)
    std::unique_ptr<UListFormatter, UListFormatterDeleter> m_listFormat;
#endif
    String m_locale;
    String m_numberingSystem;
    CString m_dataLocaleWithExtensions;
    unsigned m_fractionalDigits { 0 };
    Style m_style { Style::Long };
    UnitData m_units[numberOfTemporalUnits] { };
};

} // namespace JSC

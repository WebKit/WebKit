/*
 * Copyright (C) 2020 Sony Interactive Entertainment Inc.
 * Copyright (C) 2020-2021 Apple Inc. All rights reserved.
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
#include <unicode/ureldatefmt.h>
#include <wtf/unicode/icu/ICUHelpers.h>

namespace JSC {

enum class RelevantExtensionKey : uint8_t;

class IntlRelativeTimeFormat final : public JSNonFinalObject {
public:
    using Base = JSNonFinalObject;

    static constexpr bool needsDestruction = true;

    static void destroy(JSCell* cell)
    {
        static_cast<IntlRelativeTimeFormat*>(cell)->IntlRelativeTimeFormat::~IntlRelativeTimeFormat();
    }

    template<typename CellType, SubspaceAccess mode>
    static IsoSubspace* subspaceFor(VM& vm)
    {
        return vm.intlRelativeTimeFormatSpace<mode>();
    }

    static IntlRelativeTimeFormat* create(VM&, Structure*);
    static Structure* createStructure(VM&, JSGlobalObject*, JSValue);

    DECLARE_INFO;

    void initializeRelativeTimeFormat(JSGlobalObject*, JSValue locales, JSValue options);
    JSValue format(JSGlobalObject*, double, StringView unitString) const;
    JSValue formatToParts(JSGlobalObject*, double, StringView unitString) const;
    JSObject* resolvedOptions(JSGlobalObject*) const;

private:
    IntlRelativeTimeFormat(VM&, Structure*);
    void finishCreation(VM&);
    DECLARE_VISIT_CHILDREN;

    static Vector<String> localeData(const String&, RelevantExtensionKey);

    String formatInternal(JSGlobalObject*, double, StringView unit) const;

    enum class Style : uint8_t { Long, Short, Narrow };

    using URelativeDateTimeFormatterDeleter = ICUDeleter<ureldatefmt_close>;
    using UNumberFormatDeleter = ICUDeleter<unum_close>;

    static ASCIILiteral styleString(Style);

    std::unique_ptr<URelativeDateTimeFormatter, URelativeDateTimeFormatterDeleter> m_relativeDateTimeFormatter;
    std::unique_ptr<UNumberFormat, UNumberFormatDeleter> m_numberFormat;

    String m_locale;
    String m_numberingSystem;
    Style m_style { Style::Long };
    bool m_numeric { true };
};

} // namespace JSC

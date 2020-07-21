/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include <unicode/uldnames.h>
#include <wtf/unicode/icu/ICUHelpers.h>

namespace JSC {

#if !defined(HAVE_ICU_U_LOCALE_DISPLAY_NAMES)
// We need 61 or later since part of implementation uses UCURR_NARROW_SYMBOL_NAME.
#if U_ICU_VERSION_MAJOR_NUM >= 61
#define HAVE_ICU_U_LOCALE_DISPLAY_NAMES 1
#endif
#endif

enum class RelevantExtensionKey : uint8_t;

class IntlDisplayNames final : public JSNonFinalObject {
public:
    using Base = JSNonFinalObject;

    static constexpr bool needsDestruction = true;

    static void destroy(JSCell* cell)
    {
        static_cast<IntlDisplayNames*>(cell)->IntlDisplayNames::~IntlDisplayNames();
    }

    template<typename CellType, SubspaceAccess mode>
    static IsoSubspace* subspaceFor(VM& vm)
    {
        return vm.intlDisplayNamesSpace<mode>();
    }

    static IntlDisplayNames* create(VM&, Structure*);
    static Structure* createStructure(VM&, JSGlobalObject*, JSValue);

    DECLARE_INFO;

    void initializeDisplayNames(JSGlobalObject*, JSValue localesValue, JSValue optionsValue);

    JSValue of(JSGlobalObject*, JSValue) const;
    JSObject* resolvedOptions(JSGlobalObject*) const;

private:
    IntlDisplayNames(VM&, Structure*);
    void finishCreation(VM&);

    enum class Style : uint8_t { Narrow, Short, Long };
    enum class Type : uint8_t { Language, Region, Script, Currency };
    enum class Fallback : uint8_t { Code, None };

    static ASCIILiteral styleString(Style);
    static ASCIILiteral typeString(Type);
    static ASCIILiteral fallbackString(Fallback);

    using ULocaleDisplayNamesDeleter = ICUDeleter<uldn_close>;
    std::unique_ptr<ULocaleDisplayNames, ULocaleDisplayNamesDeleter> m_displayNames;
    String m_locale;
    // FIXME: We should store it only when m_type is Currency.
    // https://bugs.webkit.org/show_bug.cgi?id=213773
    CString m_localeCString;
    Style m_style { Style::Long };
    Type m_type { Type::Language };
    Fallback m_fallback { Fallback::Code };
};

} // namespace JSC

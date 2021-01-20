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
#include <wtf/unicode/icu/ICUHelpers.h>

#if !defined(HAVE_ICU_U_LIST_FORMATTER)
#if U_ICU_VERSION_MAJOR_NUM >= 67 || (U_ICU_VERSION_MAJOR_NUM >= 66 && USE(APPLE_INTERNAL_SDK))
#define HAVE_ICU_U_LIST_FORMATTER 1
#endif
#endif

struct UListFormatter;

namespace JSC {

enum class RelevantExtensionKey : uint8_t;

struct UListFormatterDeleter {
    JS_EXPORT_PRIVATE void operator()(UListFormatter*);
};

class IntlListFormat final : public JSNonFinalObject {
public:
    using Base = JSNonFinalObject;

    static constexpr bool needsDestruction = true;

    static void destroy(JSCell* cell)
    {
        static_cast<IntlListFormat*>(cell)->IntlListFormat::~IntlListFormat();
    }

    template<typename CellType, SubspaceAccess mode>
    static IsoSubspace* subspaceFor(VM& vm)
    {
        return vm.intlListFormatSpace<mode>();
    }

    static IntlListFormat* create(VM&, Structure*);
    static Structure* createStructure(VM&, JSGlobalObject*, JSValue);

    DECLARE_INFO;

    void initializeListFormat(JSGlobalObject*, JSValue localesValue, JSValue optionsValue);

    JSValue format(JSGlobalObject*, JSValue) const;
    JSValue formatToParts(JSGlobalObject*, JSValue) const;
    JSObject* resolvedOptions(JSGlobalObject*) const;

private:
    IntlListFormat(VM&, Structure*);
    void finishCreation(VM&);

    enum class Type : uint8_t { Conjunction, Disjunction, Unit };
    enum class Style : uint8_t { Short, Long, Narrow };

    static ASCIILiteral typeString(Type);
    static ASCIILiteral styleString(Style);

    std::unique_ptr<UListFormatter, UListFormatterDeleter> m_listFormat;
    String m_locale;
    Type m_type { Type::Conjunction };
    Style m_style { Style::Long };
};

} // namespace JSC

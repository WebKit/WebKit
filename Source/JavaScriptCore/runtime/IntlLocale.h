/*
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

namespace JSC {

class IntlLocale final : public JSNonFinalObject {
public:
    using Base = JSNonFinalObject;

    static constexpr bool needsDestruction = true;

    static void destroy(JSCell* cell)
    {
        static_cast<IntlLocale*>(cell)->IntlLocale::~IntlLocale();
    }

    template<typename CellType, SubspaceAccess mode>
    static GCClient::IsoSubspace* subspaceFor(VM& vm)
    {
        return vm.intlLocaleSpace<mode>();
    }

    static IntlLocale* create(VM&, Structure*);
    static Structure* createStructure(VM&, JSGlobalObject*, JSValue);

    DECLARE_INFO;

    void initializeLocale(JSGlobalObject*, const String& tag, JSValue optionsValue);
    void initializeLocale(JSGlobalObject*, JSValue tagValue, JSValue optionsValue);
    const String& maximal();
    const String& minimal();
    const String& toString();
    const String& baseName();
    const String& language();
    const String& script();
    const String& region();

    const String& calendar();
    const String& caseFirst();
    const String& collation();
    const String& firstDayOfWeek();
    const String& hourCycle();
    const String& numberingSystem();
    TriState numeric();

    JSArray* calendars(JSGlobalObject*);
    JSArray* collations(JSGlobalObject*);
    JSArray* hourCycles(JSGlobalObject*);
    JSArray* numberingSystems(JSGlobalObject*);
    JSValue timeZones(JSGlobalObject*);
    JSObject* textInfo(JSGlobalObject*);
    JSObject* weekInfo(JSGlobalObject*);

private:
    IntlLocale(VM&, Structure*);
    DECLARE_DEFAULT_FINISH_CREATION;
    DECLARE_VISIT_CHILDREN;

    String keywordValue(ASCIILiteral, bool isBoolean = false) const;

    CString m_localeID;

    String m_maximal;
    String m_minimal;
    String m_fullString;
    String m_baseName;
    String m_language;
    String m_script;
    String m_region;
    std::optional<String> m_calendar;
    std::optional<String> m_caseFirst;
    std::optional<String> m_collation;
    std::optional<String> m_firstDayOfWeek;
    std::optional<String> m_hourCycle;
    std::optional<String> m_numberingSystem;
    TriState m_numeric { TriState::Indeterminate };
};

} // namespace JSC

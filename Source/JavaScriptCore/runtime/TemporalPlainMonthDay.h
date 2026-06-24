/*
 * Copyright (C) 2025 Igalia, S.L. All rights reserved.
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

#include <JavaScriptCore/CalendarICUBridge.h>
#include <JavaScriptCore/ISO8601.h>

namespace JSC {

class TemporalPlainMonthDay final : public JSNonFinalObject {
public:
    using Base = JSNonFinalObject;

    template<typename CellType, SubspaceAccess mode>
    static GCClient::IsoSubspace* subspaceFor(VM& vm)
    {
        return vm.temporalPlainDateSpace<mode>();
    }

    static TemporalPlainMonthDay* create(VM&, Structure*, ISO8601::PlainMonthDay&&);
    static TemporalPlainMonthDay* tryCreateIfValid(JSGlobalObject*, Structure*, ISO8601::PlainDate&&);
    static Structure* createStructure(VM&, JSGlobalObject*, JSValue);

    DECLARE_INFO;

    static TemporalPlainMonthDay* from(JSGlobalObject*, JSValue, JSValue);

    ISO8601::PlainMonthDay plainMonthDay() const { return m_plainMonthDay; }
    CalendarID calendarID() const { return m_calendarID; }
    void setCalendarId(WTF::StringView id) { m_calendarID = TemporalCore::calendarIDFromString(id); }
    void setCalendarID(CalendarID id) { m_calendarID = id; }
    String calendarIDAsString() const { return TemporalCore::calendarIDToString(m_calendarID).toString(); }

#define JSC_DEFINE_TEMPORAL_PLAIN_MONTH_DAY_FIELD(name, capitalizedName) \
    decltype(auto) name() const { return m_plainMonthDay.name(); }
    JSC_TEMPORAL_PLAIN_MONTH_DAY_UNITS(JSC_DEFINE_TEMPORAL_PLAIN_MONTH_DAY_FIELD);
#undef JSC_DEFINE_TEMPORAL_PLAIN_MONTH_DAY_FIELD

    ISO8601::PlainDate with(JSGlobalObject*, JSObject*, JSValue);

    String monthCode() const { return ISO8601::monthCode(m_plainMonthDay.month()); }

    String toString(JSGlobalObject*, JSValue options) const;
    String toString() const
    {
        return ISO8601::temporalMonthDayToString(m_plainMonthDay, "auto"_s, m_calendarID);
    }

private:
    TemporalPlainMonthDay(VM&, Structure*, ISO8601::PlainMonthDay&&);
    DECLARE_DEFAULT_FINISH_CREATION;

    template<typename CharacterType>
    static std::optional<ISO8601::PlainMonthDay> parse(StringParsingBuffer<CharacterType>&);
    static ISO8601::PlainMonthDay fromObject(JSGlobalObject*, JSObject*);

    ISO8601::PlainMonthDay m_plainMonthDay;
    CalendarID m_calendarID { 0 };
};

} // namespace JSC

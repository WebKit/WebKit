/*
 * Copyright (C) 2022 Sony Interactive Entertainment Inc.
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
#include <JavaScriptCore/IntlObject.h>

namespace JSC {

class TemporalPlainDateTime final : public JSNonFinalObject {
public:
    using Base = JSNonFinalObject;

    template<typename CellType, SubspaceAccess mode>
    static GCClient::IsoSubspace* subspaceFor(VM& vm)
    {
        return vm.temporalPlainDateTimeSpace<mode>();
    }

    static TemporalPlainDateTime* create(VM&, Structure*, ISO8601::PlainDate&&, ISO8601::PlainTime&&);
    static TemporalPlainDateTime* create(VM&, Structure*, ISO8601::PlainDate&&, ISO8601::PlainTime&&, CalendarID);
    static TemporalPlainDateTime* tryCreateIfValid(JSGlobalObject*, Structure*, ISO8601::PlainDate&&, ISO8601::PlainTime&&);
    static Structure* createStructure(VM&, JSGlobalObject*, JSValue);

    DECLARE_INFO;

    static TemporalPlainDateTime* from(JSGlobalObject*, JSValue item, JSValue options);

    ISO8601::PlainDate plainDate() const { return m_plainDate; }
    ISO8601::PlainTime plainTime() const { return m_plainTime; }
    CalendarID calendarID() const { return m_calendarID; }
    void setCalendarId(WTF::StringView id) { m_calendarID = TemporalCore::calendarIDFromString(id); }
    void setCalendarID(CalendarID id) { m_calendarID = id; }
    String calendarIDAsString() const { return TemporalCore::calendarIDToString(m_calendarID).toString(); }

#define JSC_DEFINE_TEMPORAL_PLAIN_DATE_FIELD(name, capitalizedName) \
    decltype(auto) name() const { return m_plainDate.name(); }
    JSC_TEMPORAL_PLAIN_DATE_UNITS(JSC_DEFINE_TEMPORAL_PLAIN_DATE_FIELD);
#undef JSC_DEFINE_TEMPORAL_PLAIN_DATE_FIELD

#define JSC_DEFINE_TEMPORAL_PLAIN_TIME_FIELD(name, capitalizedName) \
    unsigned name() const { return m_plainTime.name(); }
    JSC_TEMPORAL_PLAIN_TIME_UNITS(JSC_DEFINE_TEMPORAL_PLAIN_TIME_FIELD);
#undef JSC_DEFINE_TEMPORAL_PLAIN_TIME_FIELD

    TemporalPlainDateTime* with(JSGlobalObject*, JSObject* temporalDateLike, JSValue options);
    TemporalPlainDateTime* round(JSGlobalObject*, JSValue options);

    String monthCode() const { return ISO8601::monthCode(m_plainDate.month()); }
    uint8_t dayOfWeek() const { return ISO8601::dayOfWeek(m_plainDate); }
    uint16_t dayOfYear() const { return ISO8601::dayOfYear(m_plainDate); }
    uint8_t weekOfYear() const { return ISO8601::weekOfYear(m_plainDate); }
    int32_t yearOfWeek() const { return ISO8601::yearOfWeek(m_plainDate); }

    ISO8601::Duration differenceTemporalPlainDateTime(JSGlobalObject*, DifferenceOperation, TemporalPlainDateTime*, TemporalUnit, TemporalUnit, RoundingMode, double);

    String toString(JSGlobalObject*, JSValue options) const;
    String toString(std::tuple<Precision, unsigned> precision = { Precision::Auto, 0 }) const
    {
        return ISO8601::temporalDateTimeToString(m_plainDate, m_plainTime, precision);
    }

private:
    TemporalPlainDateTime(VM&, Structure*, ISO8601::PlainDate&&, ISO8601::PlainTime&&);
    TemporalPlainDateTime(VM&, Structure*, ISO8601::PlainDate&&, ISO8601::PlainTime&&, String&&);
    DECLARE_DEFAULT_FINISH_CREATION;

    ISO8601::PlainDate m_plainDate;
    ISO8601::PlainTime m_plainTime;
    CalendarID m_calendarID { 0 };
};

} // namespace JSC

/*
 * Copyright (C) 2022 Apple Inc.
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

#include <JavaScriptCore/CalendarFields.h>
#include <JavaScriptCore/CalendarICUBridge.h>
#include <JavaScriptCore/Error.h>
#include <JavaScriptCore/ISO8601.h>
#include <JavaScriptCore/JSGlobalObject.h>
#include <JavaScriptCore/LazyProperty.h>
#include <JavaScriptCore/TemporalCalendar.h>

namespace JSC {

class TemporalPlainYearMonth final : public JSNonFinalObject {
public:
    using Base = JSNonFinalObject;

    template<typename CellType, SubspaceAccess mode>
    static GCClient::IsoSubspace* subspaceFor(VM& vm)
    {
        return vm.temporalPlainDateSpace<mode>();
    }

    static TemporalPlainYearMonth* create(VM&, Structure*, ISO8601::PlainYearMonth&&);
    static TemporalPlainYearMonth* tryCreateIfValid(JSGlobalObject*, Structure*, ISO8601::PlainDate&&);
    static Structure* createStructure(VM&, JSGlobalObject*, JSValue);

    DECLARE_INFO;

    template<AddOrSubtract>
    static ISO8601::PlainYearMonth addDurationToYearMonth(JSGlobalObject*, ISO8601::PlainYearMonth, ISO8601::Duration, TemporalOverflow, CalendarID calendarID);

    static TemporalPlainYearMonth* from(JSGlobalObject*, JSValue, JSValue);
    static TemporalPlainYearMonth* from(JSGlobalObject*, StringView);

    ISO8601::PlainYearMonth plainYearMonth() const { return m_plainYearMonth; }
    String calendarId() const { return TemporalCore::calendarIDToString(m_calendarID).toString(); }
    CalendarID calendarID() const { return m_calendarID; }
    void setCalendarId(WTF::StringView id) { m_calendarID = TemporalCore::calendarIDFromString(id); }

#define JSC_DEFINE_TEMPORAL_PLAIN_YEAR_MONTH_FIELD(name, capitalizedName) \
    decltype(auto) name() const { return m_plainYearMonth.name(); }
    JSC_TEMPORAL_PLAIN_YEAR_MONTH_UNITS(JSC_DEFINE_TEMPORAL_PLAIN_YEAR_MONTH_FIELD);
#undef JSC_DEFINE_TEMPORAL_PLAIN_YEAR_MONTH_FIELD

    ISO8601::PlainDate with(JSGlobalObject*, JSObject*, JSValue);

    String monthCode() const;

    String toString(JSGlobalObject*, JSValue options) const;
    String toString() const;

    ISO8601::Duration until(JSGlobalObject*, TemporalPlainYearMonth*, JSValue options);
    ISO8601::Duration since(JSGlobalObject*, TemporalPlainYearMonth*, JSValue options);

private:
    TemporalPlainYearMonth(VM&, Structure*, ISO8601::PlainYearMonth&&);
    void finishCreation(VM&);

    template<DifferenceOperation>
    ISO8601::Duration sinceOrUntil(JSGlobalObject*, TemporalPlainYearMonth*, JSValue);

    ISO8601::PlainYearMonth m_plainYearMonth;
    CalendarID m_calendarID { 0 };
};

// https://tc39.es/proposal-temporal/#sec-temporal-adddurationtoyearmonth
template<AddOrSubtract op>
ISO8601::PlainYearMonth TemporalPlainYearMonth::addDurationToYearMonth(JSGlobalObject* globalObject, ISO8601::PlainYearMonth yearMonth, ISO8601::Duration duration, TemporalOverflow overflow, CalendarID calendarID)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if constexpr (op == AddOrSubtract::Subtract)
        duration = -duration;

    auto durationToAdd = TemporalDuration::toDateDurationRecordWithoutTime(globalObject, duration);
    RETURN_IF_EXCEPTION(scope, { });

    auto result = TemporalCore::plainYearMonthAdd(calendarID, yearMonth.isoPlainDate(), durationToAdd, overflow);
    if (!result) {
        if (result.error().kind == TemporalErrorKind::TypeError)
            throwTypeError(globalObject, scope, String(result.error().message));
        else
            throwRangeError(globalObject, scope, String(result.error().message));
        return { };
    }
    return ISO8601::PlainYearMonth(WTF::move(result->isoDate));
}

} // namespace JSC

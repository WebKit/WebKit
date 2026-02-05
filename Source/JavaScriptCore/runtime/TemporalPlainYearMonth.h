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

#include "ISO8601.h"
#include "LazyProperty.h"
#include "TemporalCalendar.h"

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
    static ISO8601::PlainYearMonth addDurationToYearMonth(JSGlobalObject*, ISO8601::PlainYearMonth, ISO8601::Duration, TemporalOverflow);

    static TemporalPlainYearMonth* from(JSGlobalObject*, JSValue, JSValue);
    static TemporalPlainYearMonth* from(JSGlobalObject*, StringView);

    TemporalCalendar* calendar() { return m_calendar.get(this); }
    ISO8601::PlainYearMonth plainYearMonth() const { return m_plainYearMonth; }

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

    DECLARE_VISIT_CHILDREN;

private:
    TemporalPlainYearMonth(VM&, Structure*, ISO8601::PlainYearMonth&&);
    void finishCreation(VM&);

    template<DifferenceOperation>
    ISO8601::Duration sinceOrUntil(JSGlobalObject*, TemporalPlainYearMonth*, JSValue);

    ISO8601::PlainYearMonth m_plainYearMonth;
    LazyProperty<TemporalPlainYearMonth, TemporalCalendar> m_calendar;
};

// https://tc39.es/proposal-temporal/#sec-temporal-adddurationtoyearmonth
template<AddOrSubtract op>
ISO8601::PlainYearMonth TemporalPlainYearMonth::addDurationToYearMonth(JSGlobalObject* globalObject, ISO8601::PlainYearMonth yearMonth, ISO8601::Duration duration, TemporalOverflow overflow)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if constexpr (op == AddOrSubtract::Subtract)
        duration = -duration;
    auto sign = TemporalDuration::sign(duration);
    auto year = yearMonth.year();
    auto month = yearMonth.month();
    auto constexpr day = 1;
    auto intermediateDate = ISO8601::PlainDate(year, month, day);
    if (!ISO8601::isDateTimeWithinLimits(year, month, day, 0, 0, 0, 0, 0, 0)) [[unlikely]] {
        throwRangeError(globalObject, scope, "date out of range in add or subtract"_s);
        return { };
    }
    ISO8601::PlainDate date;
    if (sign < 0) {
        auto oneMonthDuration = ISO8601::Duration { 0, 1, 0, 0, 0, 0, 0, 0, 0, 0 };
        auto nextMonth = TemporalCalendar::isoDateAdd(globalObject,
            intermediateDate, oneMonthDuration, TemporalOverflow::Constrain);
        RETURN_IF_EXCEPTION(scope, { });
        int32_t y = nextMonth.year();
        uint8_t m = nextMonth.month();
        uint8_t d = nextMonth.day() - 1;
        date = TemporalCalendar::balanceISODate(globalObject, y, m, d);
    } else
        date = intermediateDate;
    auto durationToAdd = TemporalDuration::toDateDurationRecordWithoutTime(globalObject, duration);
    RETURN_IF_EXCEPTION(scope, { });
    auto addedDate = TemporalCalendar::isoDateAdd(globalObject, date, durationToAdd, overflow);
    RETURN_IF_EXCEPTION(scope, { });
    return ISO8601::PlainYearMonth(addedDate.year(), addedDate.month());
}

} // namespace JSC

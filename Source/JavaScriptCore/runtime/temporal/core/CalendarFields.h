/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

// JSC Temporal Core — Calendar field resolution (dateFromFields, yearMonthFromFields, monthDayFromFields)
// temporal_rs reference: src/builtins/core/calendar.rs, types.rs, fields.rs

#include <JavaScriptCore/ISO8601.h>
#include <JavaScriptCore/IntlObject.h>
#include <JavaScriptCore/JSExportMacros.h>
#include <JavaScriptCore/TemporalCoreTypes.h>
#include <JavaScriptCore/TemporalEnums.h>
#include <optional>
#include <wtf/text/WTFString.h>

namespace JSC {

struct ParsedMonthCode;

namespace TemporalCore {

// Input fields for calendar date resolution.
// Maps to temporal_rs CalendarFields.
struct CalendarFieldsIn {
    std::optional<int32_t> year;
    std::optional<uint32_t> month;
    std::optional<ParsedMonthCode> monthCode;
    std::optional<uint8_t> day;
    std::optional<String> era;
    std::optional<int32_t> eraYear;
};

struct TimeFieldsIn {
    std::optional<double> hour;
    std::optional<double> minute;
    std::optional<double> second;
    std::optional<double> millisecond;
    std::optional<double> microsecond;
    std::optional<double> nanosecond;
};

// Result of calendar field resolution: ISO date + calendar ID.
struct ResolvedCalendarDate {
    ISO8601::PlainDate isoDate;
    CalendarID calendarId { 0 };
};

enum class ResolveType : uint8_t { Date, YearMonth, MonthDay };
JS_EXPORT_PRIVATE TemporalResult<void> nonISOResolveFields(CalendarID, CalendarFieldsIn&, ResolveType);

JS_EXPORT_PRIVATE TemporalResult<ResolvedCalendarDate> dateFromFields(CalendarID, const CalendarFieldsIn&, TemporalOverflow);

JS_EXPORT_PRIVATE TemporalResult<ResolvedCalendarDate> yearMonthFromFields(CalendarID, const CalendarFieldsIn&, TemporalOverflow);

JS_EXPORT_PRIVATE TemporalResult<ResolvedCalendarDate> monthDayFromFields(CalendarID, const CalendarFieldsIn&, TemporalOverflow);

JS_EXPORT_PRIVATE TemporalResult<ResolvedCalendarDate> plainYearMonthWith(CalendarID, const ISO8601::PlainDate& currentISODate, const CalendarFieldsIn& partialFields, TemporalOverflow);

JS_EXPORT_PRIVATE TemporalResult<ResolvedCalendarDate> plainDateWith(CalendarID, const ISO8601::PlainDate& currentISODate, const CalendarFieldsIn& partialFields, TemporalOverflow);

JS_EXPORT_PRIVATE TemporalResult<ISO8601::Duration> differenceYearMonth(CalendarID, const ISO8601::PlainDate& thisISODate, const ISO8601::PlainDate& otherISODate, TemporalUnit largestUnit);

JS_EXPORT_PRIVATE TemporalResult<ResolvedCalendarDate> plainYearMonthAdd(CalendarID, const ISO8601::PlainDate& currentISODate, const ISO8601::Duration&, TemporalOverflow);

JS_EXPORT_PRIVATE TemporalResult<ResolvedCalendarDate> plainYearMonthToPlainDate(CalendarID, const ISO8601::PlainDate& pymISODate, uint8_t day);

JS_EXPORT_PRIVATE TemporalResult<ResolvedCalendarDate> plainYearMonthFromISODate(CalendarID, const ISO8601::PlainDate& fullISODate);

JS_EXPORT_PRIVATE TemporalResult<ResolvedCalendarDate> plainMonthDayFromISODate(CalendarID, const ISO8601::PlainDate& fullISODate, TemporalOverflow);

} // namespace TemporalCore
} // namespace JSC

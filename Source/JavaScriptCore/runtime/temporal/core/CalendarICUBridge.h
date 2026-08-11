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

// JSC Temporal Core — ICU calendar bridge
// temporal_rs reference: src/builtins/core/calendar.rs
// Last synced: v0.2.3

#include <JavaScriptCore/ISO8601.h>
#include <JavaScriptCore/IntlObject.h>
#include <JavaScriptCore/JSExportMacros.h>
#include <JavaScriptCore/TemporalCoreTypes.h>
#include <JavaScriptCore/TemporalEnums.h>
#include <optional>
#include <wtf/text/WTFString.h>

namespace JSC {
namespace TemporalCore {

// CalendarFields — extracted calendar representation of an ISO date.
// temporal_rs: icu4x AnyCalendarDate fields (year, month, day, era, monthCode)
struct CalendarFields {
    int32_t year { 0 };
    uint8_t month { 1 };
    uint8_t day { 1 };
    bool isLeapMonth { false };
    std::optional<String> era;
    std::optional<int32_t> eraYear;
    String monthCode;
};

JS_EXPORT_PRIVATE CalendarID calendarIDFromString(StringView);

inline StringView calendarIDToString(CalendarID id) { return intlAvailableCalendars()[id]; }

inline bool calendarIsISO(CalendarID id) { return id == iso8601CalendarID(); }

// calendarHasEras — true for calendars that expose era/eraYear fields in Temporal.
// Covers the 13 spec-defined era-bearing calendars; unknown future calendars default to false.
inline bool calendarHasEras(CalendarID id)
{
    return id == buddhistCalendarID() || id == copticCalendarID() || id == ethioaaCalendarID()
        || id == ethiopicCalendarID() || id == gregoryCalendarID() || id == hebrewCalendarID()
        || id == indianCalendarID() || id == islamicCivilCalendarID()
        || id == islamicTblaCalendarID() || id == islamicUmalquraCalendarID()
        || id == japaneseCalendarID() || id == persianCalendarID() || id == rocCalendarID();
}

// https://tc39.es/proposal-intl-era-monthcode/#sec-temporal-calendarhasmidyeareras
inline bool calendarHasMidYearEras(CalendarID id) { return id == japaneseCalendarID(); }

// calendarIsLunisolar — true for calendars with leap months (Chinese, Dangi, Hebrew).
// NOTE: temporal_rs Calendar::is_iso() returns true for ISO8601 (opposite semantic).
inline bool calendarIsLunisolar(CalendarID id)
{
    return id == chineseCalendarID() || id == dangiCalendarID() || id == hebrewCalendarID();
}

TemporalResult<CalendarFields> isoToCalendarFields(CalendarID, const ISO8601::PlainDate& isoDate);

JS_EXPORT_PRIVATE TemporalResult<int32_t> calendarYear(CalendarID, const ISO8601::PlainDate& isoDate);

JS_EXPORT_PRIVATE TemporalResult<uint8_t> calendarMonth(CalendarID, const ISO8601::PlainDate& isoDate);

JS_EXPORT_PRIVATE TemporalResult<String> calendarMonthCode(CalendarID, const ISO8601::PlainDate& isoDate);

JS_EXPORT_PRIVATE TemporalResult<uint8_t> calendarDay(CalendarID, const ISO8601::PlainDate& isoDate);

JS_EXPORT_PRIVATE TemporalResult<int32_t> calendarDayOfYear(CalendarID, const ISO8601::PlainDate& isoDate);

JS_EXPORT_PRIVATE uint8_t calendarDayOfWeek(CalendarID, const ISO8601::PlainDate& isoDate);

JS_EXPORT_PRIVATE std::optional<uint8_t> calendarWeekOfYear(CalendarID, const ISO8601::PlainDate& isoDate);

JS_EXPORT_PRIVATE std::optional<int32_t> calendarYearOfWeek(CalendarID, const ISO8601::PlainDate& isoDate);

JS_EXPORT_PRIVATE bool isValidMonthCodeForCalendar(CalendarID, ParsedMonthCode);

JS_EXPORT_PRIVATE TemporalResult<bool> yearContainsMonthCode(CalendarID, int32_t year, ParsedMonthCode);

JS_EXPORT_PRIVATE TemporalResult<ParsedMonthCode> constrainMonthCode(CalendarID, int32_t year, ParsedMonthCode, TemporalOverflow);

JS_EXPORT_PRIVATE TemporalResult<int32_t> monthCodeOrdinalInYear(CalendarID, ParsedMonthCode, int32_t year);

JS_EXPORT_PRIVATE TemporalResult<std::optional<String>> calendarEra(CalendarID, const ISO8601::PlainDate& isoDate);

JS_EXPORT_PRIVATE TemporalResult<std::optional<int32_t>> calendarEraYear(CalendarID, const ISO8601::PlainDate& isoDate);

JS_EXPORT_PRIVATE TemporalResult<int32_t> calendarDaysInMonth(CalendarID, const ISO8601::PlainDate& isoDate);

JS_EXPORT_PRIVATE TemporalResult<int32_t> calendarDaysInYear(CalendarID, const ISO8601::PlainDate& isoDate);

JS_EXPORT_PRIVATE TemporalResult<int32_t> calendarMonthsInYear(CalendarID, const ISO8601::PlainDate& isoDate);

JS_EXPORT_PRIVATE TemporalResult<bool> calendarInLeapYear(CalendarID, const ISO8601::PlainDate& isoDate);

JS_EXPORT_PRIVATE TemporalResult<ISO8601::PlainDate> calendarDateAdd(CalendarID, const ISO8601::PlainDate& isoDate, const ISO8601::Duration&, TemporalOverflow);

TemporalResult<ISO8601::Duration> calendarDateUntil(CalendarID, const ISO8601::PlainDate& one, const ISO8601::PlainDate& two, TemporalUnit largestUnit);

// EcmaReferenceYear — extended calendar year whose ISO date falls nearest 1972 for (monthNumber, day).
// Used by PlainMonthDay to choose a stable reference ISO year for non-ISO calendars.
// icu4x: EcmaReferenceYearError (components/calendar/src/error.rs).
enum class EcmaReferenceYearError : uint8_t {
    // (calendar, monthCode) has no representation — e.g. Hebrew M01L, or a leap monthCode on a solar calendar.
    MonthNotInCalendar,
    // Leap month exists but not near 1972. Constrain: retry non-leap variant and drop the leap flag.
    // Reject: throw RangeError.
    UseRegularIfConstrain,
};

Expected<int32_t, EcmaReferenceYearError> ecmaReferenceYear(CalendarID, uint8_t monthNumber, bool isLeapMonth, uint8_t day);

JS_EXPORT_PRIVATE std::optional<ASCIILiteral> canonicalizeEraInCalendar(CalendarID, StringView era);

JS_EXPORT_PRIVATE std::optional<std::pair<ASCIILiteral, int32_t>> remapNonPositiveEraYear(CalendarID, StringView era, int32_t eraYear);

JS_EXPORT_PRIVATE std::optional<int32_t> calendarDateArithmeticYearForEraYear(CalendarID, StringView era, int32_t eraYear);

JS_EXPORT_PRIVATE TemporalResult<ISO8601::PlainDate> nonISOCalendarDateToISO(CalendarID, std::optional<int32_t> year, uint8_t month, uint8_t day, std::optional<ParsedMonthCode>, TemporalOverflow);

} // namespace TemporalCore
} // namespace JSC

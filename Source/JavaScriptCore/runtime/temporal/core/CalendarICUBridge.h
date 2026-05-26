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

CalendarID JS_EXPORT_PRIVATE calendarIDFromString(StringView);

inline StringView calendarIDToString(CalendarID id) { return intlAvailableCalendars()[id]; }

inline bool calendarIsISO(CalendarID id) { return id == iso8601CalendarID(); }

// calendarHasEras — true for calendars that expose era/eraYear fields in Temporal.
// Covers the 15 spec-defined era-bearing calendars; unknown future calendars default to false.
inline bool calendarHasEras(CalendarID id)
{
    return id == buddhistCalendarID() || id == copticCalendarID() || id == ethioaaCalendarID()
        || id == ethiopicCalendarID() || id == gregoryCalendarID() || id == hebrewCalendarID()
        || id == indianCalendarID() || id == islamicCalendarID() || id == islamicCivilCalendarID()
        || id == islamicRgsaCalendarID() || id == islamicTblaCalendarID() || id == islamicUmalquraCalendarID()
        || id == japaneseCalendarID() || id == persianCalendarID() || id == rocCalendarID();
}

// calendarIsLunisolar — true for calendars with leap months (Chinese, Dangi, Hebrew).
// NOTE: temporal_rs Calendar::is_iso() returns true for ISO8601 (opposite semantic).
inline bool calendarIsLunisolar(CalendarID id)
{
    return id == chineseCalendarID() || id == dangiCalendarID() || id == hebrewCalendarID();
}

// Returns true for any Islamic calendar variant.
inline bool calendarIsIslamic(CalendarID id)
{
    return id == islamicCalendarID() || id == islamicCivilCalendarID() || id == islamicRgsaCalendarID()
        || id == islamicTblaCalendarID() || id == islamicUmalquraCalendarID();
}

TemporalResult<CalendarFields> isoToCalendarFields(CalendarID, const ISO8601::PlainDate& isoDate);

TemporalResult<int32_t> JS_EXPORT_PRIVATE calendarYear(CalendarID, const ISO8601::PlainDate& isoDate);

TemporalResult<uint8_t> JS_EXPORT_PRIVATE calendarMonth(CalendarID, const ISO8601::PlainDate& isoDate);

TemporalResult<String> JS_EXPORT_PRIVATE calendarMonthCode(CalendarID, const ISO8601::PlainDate& isoDate);

TemporalResult<uint8_t> JS_EXPORT_PRIVATE calendarDay(CalendarID, const ISO8601::PlainDate& isoDate);

TemporalResult<std::optional<String>> JS_EXPORT_PRIVATE calendarEra(CalendarID, const ISO8601::PlainDate& isoDate);

TemporalResult<std::optional<int32_t>> JS_EXPORT_PRIVATE calendarEraYear(CalendarID, const ISO8601::PlainDate& isoDate);

TemporalResult<int32_t> JS_EXPORT_PRIVATE calendarDaysInMonth(CalendarID, const ISO8601::PlainDate& isoDate);

TemporalResult<int32_t> JS_EXPORT_PRIVATE calendarDaysInYear(CalendarID, const ISO8601::PlainDate& isoDate);

TemporalResult<int32_t> JS_EXPORT_PRIVATE calendarMonthsInYear(CalendarID, const ISO8601::PlainDate& isoDate);

TemporalResult<bool> JS_EXPORT_PRIVATE calendarInLeapYear(CalendarID, const ISO8601::PlainDate& isoDate);

TemporalResult<ISO8601::PlainDate> JS_EXPORT_PRIVATE calendarDateAdd(CalendarID, const ISO8601::PlainDate& isoDate, const ISO8601::Duration&, TemporalOverflow);

TemporalResult<ISO8601::Duration> calendarDateUntil(CalendarID, const ISO8601::PlainDate& one, const ISO8601::PlainDate& two, TemporalUnit largestUnit);

// EcmaReferenceYear — returns the extended calendar year whose ISO date falls nearest 1972 for (monthNumber, day).
// Used by PlainMonthDay to choose a stable reference ISO year for non-ISO calendars.
// Sentinel: ecmaReferenceYear returns this when the leap month doesn't exist near 1972
// and the caller should use the non-leap month reference year if overflow=Constrain,
// or throw RangeError if overflow=Reject.
// icu4x: EcmaReferenceYearError::UseRegularIfConstrain
static constexpr int32_t ecmaRefYearUseRegular = INT32_MIN;

// Sentinel: ecmaReferenceYear returns this when the month code is invalid for this calendar
// (the month simply doesn't exist, regardless of overflow mode).
// icu4x: EcmaReferenceYearError::MonthNotInCalendar
static constexpr int32_t ecmaRefYearNotInCalendar = INT32_MIN + 1;

int32_t ecmaReferenceYear(CalendarID, uint8_t monthNumber, bool isLeapMonth, uint8_t day);

int32_t JS_EXPORT_PRIVATE lunarCalendarExtendedYearFor1972(CalendarID);

TemporalResult<ISO8601::PlainDate> JS_EXPORT_PRIVATE calendarDateFromFields(CalendarID, int32_t year, uint8_t month, uint8_t day, std::optional<StringView> era, std::optional<int32_t> eraYear, std::optional<ParsedMonthCode>, TemporalOverflow);

} // namespace TemporalCore
} // namespace JSC

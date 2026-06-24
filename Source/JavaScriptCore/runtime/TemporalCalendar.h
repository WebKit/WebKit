/*
 * Copyright (C) 2021-2022 Apple Inc. All rights reserved.
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

#include <JavaScriptCore/CalendarArithmetic.h>
#include <JavaScriptCore/CalendarFields.h>
#include <JavaScriptCore/ISO8601.h>
#include <JavaScriptCore/ISOArithmetic.h>
#include <JavaScriptCore/IntlObject.h>
#include <JavaScriptCore/JSObject.h>
#include <JavaScriptCore/TemporalDuration.h>
#include <JavaScriptCore/TemporalObject.h>

namespace JSC {

// Free helper functions (previously static methods of the removed TemporalCalendar JSObject class).

std::optional<CalendarID> JS_EXPORT_PRIVATE isBuiltinCalendar(StringView);

std::optional<ParsedMonthCode> parseMonthCode(JSGlobalObject*, JSValue argument);

ISO8601::PlainDate isoDateFromFields(JSGlobalObject*, TemporalDateFormat, int32_t, uint32_t, uint32_t, std::optional<ParsedMonthCode>, TemporalOverflow, CalendarID = iso8601CalendarID());

template<DifferenceOperation>
ISO8601::Duration differenceTemporalPlainYearMonth(JSGlobalObject*, const ISO8601::PlainYearMonth&, const ISO8601::PlainYearMonth&, unsigned, TemporalUnit, TemporalUnit, RoundingMode, CalendarID = iso8601CalendarID());

ISO8601::PlainDate addDurationToDate(JSGlobalObject*, const ISO8601::PlainDate&, const ISO8601::Duration&, TemporalOverflow);
ISO8601::PlainDate isoDateAdd(JSGlobalObject*, const ISO8601::PlainDate&, const ISO8601::Duration&, TemporalOverflow);
ISO8601::PlainDate calendarDateAdd(JSGlobalObject*, CalendarID, const ISO8601::PlainDate&, const ISO8601::Duration&, TemporalOverflow);
ISO8601::Duration calendarDateUntil(CalendarID, const ISO8601::PlainDate&, const ISO8601::PlainDate&, TemporalUnit);

enum class FieldSetType { Date, YearMonth, MonthDay };
enum class CalendarRead { Read, Skip };
template<FieldSetType type = FieldSetType::Date, CalendarRead calendarRead = CalendarRead::Read>
TemporalCore::CalendarFieldsIn readCalendarFieldsFromObject(JSGlobalObject*, JSObject* bag, CalendarID& outCalendarId);

// Fields read from a ZonedDateTime property bag (from() or with()).
struct ZonedDateTimeFields {
    // Calendar date fields (from PrepareCalendarFields).
    TemporalCore::CalendarFieldsIn dateFields;
    // Time fields — ZDT-specific, read in alphabetical order interleaved.
    // Stored as optional so with() can distinguish "not provided" from "provided as 0".
    std::optional<double> hour;
    std::optional<double> minute;
    std::optional<double> second;
    std::optional<double> millisecond;
    std::optional<double> microsecond;
    std::optional<double> nanosecond;
    // ZDT-specific fields — resolved in readZonedDateTimeFieldsFromObject per spec.
    std::optional<int64_t> offsetNs; // parsed from the "offset" string property
    TimeZone timeZone; // resolved TimeZone handle (Full mode only)
    // Presence flags (needed for with() partial validation).
    bool dayPresent { false };
    bool monthPresent { false };
    bool monthCodePresent { false };
    bool yearPresent { false };
    bool anyFieldSet { false }; // true if at least one field was present (for with())
};

enum class ZonedDateTimeFieldMode {
    Full, // from(): timeZone is the only required field (spec requiredFieldNames = «time-zone»)
    Partial, // with(): all fields optional, anyFieldSet is tracked (~partial~ mode)
};

template<ZonedDateTimeFieldMode mode = ZonedDateTimeFieldMode::Full, CalendarRead calendarRead = CalendarRead::Read>
ZonedDateTimeFields readZonedDateTimeFieldsFromObject(JSGlobalObject*, JSObject* bag, CalendarID& outCalendarId);

} // namespace JSC

/*
 * Copyright (C) 2021 Sony Interactive Entertainment Inc.
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

#include "IntlObject.h"
#include "TemporalObject.h"
#include <wtf/Int128.h>
#include <wtf/TZoneMalloc.h>

namespace JSC {
namespace ISO8601 {

static constexpr int32_t maxYear = 275760;
static constexpr int32_t minYear = -271821;
static constexpr int32_t outOfRangeYear = minYear - 1;

class Duration {
    WTF_MAKE_TZONE_ALLOCATED(Duration);
public:
    using const_iterator = std::array<double, numberOfTemporalUnits>::const_iterator;

    Duration() = default;
    Duration(double years, double months, double weeks, double days, double hours, double minutes, double seconds, double milliseconds, double microseconds, double nanoseconds)
        : m_data {
            years,
            months,
            weeks,
            days,
            hours,
            minutes,
            seconds,
            milliseconds,
            microseconds,
            nanoseconds,
        }
    { }

#define JSC_DEFINE_ISO8601_DURATION_FIELD(name, capitalizedName) \
    double name##s() const { return m_data[static_cast<uint8_t>(TemporalUnit::capitalizedName)]; } \
    void set##capitalizedName##s(double value) { m_data[static_cast<uint8_t>(TemporalUnit::capitalizedName)] = !value ? 0 : value; }
    JSC_TEMPORAL_UNITS(JSC_DEFINE_ISO8601_DURATION_FIELD);
#undef JSC_DEFINE_ISO8601_DURATION_FIELD

    double& operator[](size_t i) { return m_data[i]; }
    const double& operator[](size_t i) const { return m_data[i]; }
    double& operator[](TemporalUnit u) { return m_data[static_cast<uint8_t>(u)]; }
    const double& operator[](TemporalUnit u) const { return m_data[static_cast<uint8_t>(u)]; }
    const_iterator begin() const { return m_data.begin(); }
    const_iterator end() const { return m_data.end(); }
    void clear() { m_data.fill(0); }

    template<TemporalUnit unit>
    std::optional<Int128> totalNanoseconds() const;

    Duration operator-() const
    {
        Duration result(*this);
        for (auto& value : result.m_data) {
            if (value)
                value = -value;
        }
        return result;
    }

private:
    std::array<double, numberOfTemporalUnits> m_data { };
};

class InternalDuration;

class ExactTime {
    WTF_MAKE_TZONE_ALLOCATED(ExactTime);
public:
    static constexpr Int128 dayRangeSeconds { 86400'00000000 }; // 1e8 days
    static constexpr Int128 nsPerMicrosecond { 1000 };
    static constexpr Int128 nsPerMillisecond { 1'000'000 };
    static constexpr Int128 nsPerSecond { 1'000'000'000 };
    static constexpr Int128 nsPerMinute = nsPerSecond * 60;
    static constexpr Int128 nsPerHour = nsPerMinute * 60;
    static constexpr Int128 nsPerDay = nsPerHour * 24;
    static constexpr Int128 minValue = -dayRangeSeconds * nsPerSecond;
    static constexpr Int128 maxValue = dayRangeSeconds * nsPerSecond;

    constexpr ExactTime() = default;
    constexpr ExactTime(const ExactTime&) = default;
    constexpr explicit ExactTime(Int128 epochNanoseconds) : m_epochNanoseconds(epochNanoseconds) { }

    static constexpr ExactTime fromEpochMilliseconds(int64_t epochMilliseconds)
    {
        return ExactTime(Int128 { epochMilliseconds } * ExactTime::nsPerMillisecond);
    }
    static ExactTime fromISOPartsAndOffset(int32_t y, uint8_t mon, uint8_t d, unsigned h, unsigned min, unsigned s, unsigned ms, unsigned micros, unsigned ns, int64_t offset);

    int64_t epochMilliseconds() const
    {
        return static_cast<int64_t>(m_epochNanoseconds / ExactTime::nsPerMillisecond);
    }
    int64_t floorEpochMilliseconds() const
    {
        auto div = m_epochNanoseconds / ExactTime::nsPerMillisecond;
        auto rem = m_epochNanoseconds % ExactTime::nsPerMillisecond;
        if (rem && m_epochNanoseconds < 0)
            div -= 1;
        return static_cast<int64_t>(div);
    }
    constexpr Int128 epochNanoseconds() const
    {
        return m_epochNanoseconds;
    }

    int nanosecondsFraction() const
    {
        return static_cast<int>(m_epochNanoseconds % ExactTime::nsPerSecond);
    }

    String asString() const
    {
        StringBuilder builder;
        if (m_epochNanoseconds < 0) {
            builder.append('-');
            asStringImpl(builder, -m_epochNanoseconds);
        } else
            asStringImpl(builder, m_epochNanoseconds);
        return builder.toString();
    }

    // IsValidEpochNanoseconds ( epochNanoseconds )
    // https://tc39.es/proposal-temporal/#sec-temporal-isvalidepochnanoseconds
    constexpr bool isValid() const
    {
        return m_epochNanoseconds >= ExactTime::minValue && m_epochNanoseconds <= ExactTime::maxValue;
    }

    friend constexpr auto operator<=>(const ExactTime&, const ExactTime&) = default;

    std::optional<ExactTime> add(Duration) const;
    InternalDuration difference(JSGlobalObject*, ExactTime, unsigned, TemporalUnit, RoundingMode) const;
    ExactTime round(JSGlobalObject*, unsigned, TemporalUnit, RoundingMode) const;

    static ExactTime now();

private:
    static void asStringImpl(StringBuilder& builder, Int128 value)
    {
        if (value > 9)
            asStringImpl(builder, value / 10);
        builder.append(static_cast<Latin1Character>(static_cast<unsigned>(value % 10) + '0'));
    }

    Int128 m_epochNanoseconds { };
};

// https://tc39.es/proposal-temporal/#sec-temporal-internal-duration-records
// Represents a duration as an ISO8601::Duration (in which all time fields
// are ignored) along with an Int128 time duration that represents the sum
// of all time fields. Used to avoid losing precision in intermediate calculations.
class InternalDuration final {
public:
    InternalDuration(Duration d, Int128 t)
        : m_dateDuration(d), m_time(t) { }
    InternalDuration()
        : m_dateDuration(Duration()), m_time(0) { }
    static constexpr Int128 maxTimeDuration = 9'007'199'254'740'992 * ExactTime::nsPerSecond - 1;

    int32_t sign() const;

    int32_t timeDurationSign() const
    {
        return m_time < 0 ? -1 : m_time > 0 ? 1 : 0;
    }

    Int128 time() const { return m_time; }

    Duration dateDuration() const { return m_dateDuration; }

    static InternalDuration combineDateAndTimeDuration(Duration, Int128);
private:

    // Time fields are ignored
    Duration m_dateDuration;

    // A time duration is an integer in the inclusive interval from -maxTimeDuration
    // to maxTimeDuration, where
    // maxTimeDuration = 2**53 × 10**9 - 1 = 9,007,199,254,740,991,999,999,999.
    // It represents the portion of a Temporal.Duration object that deals with time
    // units, but as a combined value of total nanoseconds.
    Int128 m_time;
};

class PlainTime {
    WTF_MAKE_TZONE_ALLOCATED(PlainTime);
public:
    constexpr PlainTime()
        : m_millisecond(0)
        , m_microsecond(0)
        , m_nanosecond(0)
    {
    }

    constexpr PlainTime(unsigned hour, unsigned minute, unsigned second, unsigned millisecond, unsigned microsecond, unsigned nanosecond)
        : m_hour(hour)
        , m_minute(minute)
        , m_second(second)
        , m_millisecond(millisecond)
        , m_microsecond(microsecond)
        , m_nanosecond(nanosecond)
    { }

#define JSC_DEFINE_ISO8601_PLAIN_TIME_FIELD(name, capitalizedName) \
    unsigned name() const { return m_##name; }
    JSC_TEMPORAL_PLAIN_TIME_UNITS(JSC_DEFINE_ISO8601_PLAIN_TIME_FIELD);
#undef JSC_DEFINE_ISO8601_DURATION_FIELD

    friend bool operator==(const PlainTime&, const PlainTime&) = default;

private:
    uint8_t m_hour { 0 };
    uint8_t m_minute { 0 };
    uint8_t m_second { 0 };
    uint32_t m_millisecond : 10;
    uint32_t m_microsecond : 10;
    uint32_t m_nanosecond : 10;
};
static_assert(sizeof(PlainTime) <= sizeof(uint64_t));

// More effective for our purposes than isInBounds<int32_t>.
constexpr bool isYearWithinLimits(double year)
{
    return year >= minYear && year <= maxYear;
}

constexpr bool isYearWithinLimits(int32_t year)
{
    return year >= minYear && year <= maxYear;
}

// https://tc39.es/proposal-temporal/#sec-temporal-isoyearmonthwithinlimits
constexpr bool isYearMonthWithinLimits(int32_t year, int32_t month)
{
    if (!isYearWithinLimits(year))
        return false;
    if (year == minYear && month < 4)
        return false;
    if (year == maxYear && month > 9)
        return false;
    return true;
}

// Note that PlainDate does not include week unit.
// year can be negative. And month and day starts with 1.
class PlainDate {
    WTF_MAKE_TZONE_ALLOCATED(PlainDate);
public:
    constexpr PlainDate()
        : m_year(0)
        , m_month(1)
        , m_day(1)
    {
    }

    constexpr PlainDate(int32_t year, unsigned month, unsigned day)
        : m_year(year)
        , m_month(month)
        , m_day(day)
    {
        ASSERT(isYearWithinLimits(year) || year == outOfRangeYear);
    }

    friend bool operator==(const PlainDate&, const PlainDate&) = default;

    int32_t year() const { return m_year; }
    uint8_t month() const { return m_month; }
    uint8_t day() const { return m_day; }

private:
    // ECMAScript max / min date's year can be represented <= 20 bits.
    // However, PlainDate must be able to represent out-of-range years,
    // since the validity checking is separate from date parsing.
    // For example, see the test262 test
    // Temporal/PlainDate/prototype/until/throws-if-rounded-date-outside-valid-iso-range.js
    // The solution to this is to use a sentinel value (outOfRangeYear) to represent
    // all out-of-range years. The PlainDate constructor checks the invariant
    // that either the year is within limits, or it's equal to this sentinel value.
    int32_t m_year : 21;
    int32_t m_month : 5; // Starts with 1.
    int32_t m_day : 6; // Starts with 1.
};
static_assert(sizeof(PlainDate) == sizeof(int32_t));

using TimeZone = Variant<TimeZoneID, int64_t>;

class PlainYearMonth final {
    WTF_MAKE_TZONE_ALLOCATED(PlainYearMonth);
public:
    constexpr PlainYearMonth()
        : m_isoPlainDate(0, 1, 1)
    {
    }

    constexpr PlainYearMonth(int32_t year, unsigned month)
        : m_isoPlainDate(year, month, 1)
    {
    }

    constexpr PlainYearMonth(PlainDate&& d)
        : m_isoPlainDate(d)
    {
    }

    friend bool operator==(const PlainYearMonth&, const PlainYearMonth&) = default;

    int32_t year() const { return m_isoPlainDate.year(); }
    uint8_t month() const { return m_isoPlainDate.month(); }

    const PlainDate& isoPlainDate() const { return m_isoPlainDate; }
private:
    PlainDate m_isoPlainDate;
};
static_assert(sizeof(PlainYearMonth) == sizeof(PlainDate));

class PlainMonthDay {
    WTF_MAKE_TZONE_ALLOCATED(PlainMonthDay);
public:
    constexpr PlainMonthDay()
        : m_isoPlainDate(0, 1, 1)
    {
    }

    constexpr PlainMonthDay(unsigned month, int32_t day)
        : m_isoPlainDate(2, month, day)
    {
    }

    constexpr PlainMonthDay(PlainDate&& d)
        : m_isoPlainDate(d)
    {
    }

    friend bool operator==(const PlainMonthDay&, const PlainMonthDay&) = default;

    uint8_t month() const { return m_isoPlainDate.month(); }
    uint32_t day() const { return m_isoPlainDate.day(); }

    const PlainDate& isoPlainDate() const { return m_isoPlainDate; }
private:
    PlainDate m_isoPlainDate;
};
static_assert(sizeof(PlainYearMonth) == sizeof(PlainDate));

// https://tc39.es/proposal-temporal/#sec-temporal-parsetemporaltimezonestring
// Record { [[Z]], [[OffsetString]], [[Name]] }
struct TimeZoneRecord {
    bool m_z { false };
    std::optional<int64_t> m_offset;
    Variant<Vector<Latin1Character>, int64_t> m_nameOrOffset;
};

static constexpr unsigned minCalendarLength = 3;
static constexpr unsigned maxCalendarLength = 8;
enum class RFC9557Flag : bool { None, Critical }; // "Critical" = "!" flag
enum class RFC9557Key : bool { Calendar, Other };
using RFC9557Value = Vector<Latin1Character, maxCalendarLength>;
struct RFC9557Annotation {
    RFC9557Flag m_flag;
    RFC9557Key m_key;
    RFC9557Value m_value;
};

// https://tc39.es/proposal-temporal/#sup-isvalidtimezonename
std::optional<TimeZoneID> parseTimeZoneName(StringView);
std::optional<Duration> parseDuration(StringView);
std::optional<int64_t> parseUTCOffset(StringView, bool parseSubMinutePrecision = true);
std::optional<int64_t> parseUTCOffsetInMinutes(StringView);
enum class ValidateTimeZoneID : bool { No, Yes };
using CalendarID = RFC9557Value;
std::optional<std::tuple<PlainTime, std::optional<TimeZoneRecord>>> parseTime(StringView);
std::optional<std::tuple<PlainTime, std::optional<TimeZoneRecord>, std::optional<CalendarID>>> parseCalendarTime(StringView);
std::optional<std::tuple<PlainDate, std::optional<PlainTime>, std::optional<TimeZoneRecord>>> parseDateTime(StringView, TemporalDateFormat);
std::optional<std::tuple<PlainDate, std::optional<PlainTime>, std::optional<TimeZoneRecord>, std::optional<CalendarID>>> parseCalendarDateTime(StringView, TemporalDateFormat);
uint8_t dayOfWeek(PlainDate);
uint16_t dayOfYear(PlainDate);
uint8_t weeksInYear(int32_t year);
uint8_t weekOfYear(PlainDate);
uint8_t daysInMonth(int32_t year, uint8_t month);
uint8_t daysInMonth(uint8_t month);
String formatTimeZoneOffsetString(int64_t);
String temporalTimeToString(PlainTime, std::tuple<Precision, unsigned>);
String temporalDateToString(PlainDate);
String temporalDateTimeToString(PlainDate, PlainTime, std::tuple<Precision, unsigned>);
String temporalMonthDayToString(PlainMonthDay, StringView);
String monthCode(uint32_t);

bool isValidDuration(const Duration&);
bool isValidISODate(double, double, double);
PlainDate createISODateRecord(double, double, double);

std::optional<ExactTime> parseInstant(StringView);
std::optional<ParsedMonthCode> parseMonthCode(StringView);

bool isDateTimeWithinLimits(int32_t year, uint8_t month, uint8_t day, unsigned hour, unsigned minute, unsigned second, unsigned millisecond, unsigned microsecond, unsigned nanosecond);

Int128 roundTimeDuration(JSGlobalObject*, Int128, unsigned, TemporalUnit, RoundingMode);

} // namespace ISO8601

using CheckedInt128 = Checked<Int128, RecordOverflow>;

CheckedInt128 checkedCastDoubleToInt128(double n);

} // namespace JSC

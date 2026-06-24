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

#include <JavaScriptCore/IntlObject.h>
#include <JavaScriptCore/TemporalObject.h>
#include <wtf/Int128.h>
#include <wtf/OptionSet.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/text/StringBuilder.h>

namespace JSC {
namespace ISO8601 {

static constexpr int32_t maxYear = 275760;
static constexpr int32_t minYear = -271821;
static constexpr int32_t outOfRangeYear = minYear - 1;

static constexpr int32_t monthsPerYear = 12;
static constexpr int32_t daysPerWeek = 7;

class Duration {
    WTF_MAKE_TZONE_ALLOCATED(Duration);
public:
    Duration() = default;

    // Converts double to int64_t without C++ UB. Values outside int64_t range
    // saturate to INT64_MIN/MAX — those sentinel values always fail isValidDuration
    // (INT64_MAX >> 2^32 limit for date fields; saturated time fields overflow
    // totalNanoseconds). For in-range inputs the conversion is exact.
    static int64_t doubleToInt64Saturating(double v)
    {
        // INT64_MIN (-2^63) is exactly representable as double; INT64_MAX (2^63-1) is not —
        // it rounds up to 2^63 = -(double)INT64_MIN. So both bounds derive from INT64_MIN.
        static constexpr double int64AsDoubleMin = static_cast<double>(std::numeric_limits<int64_t>::min()); // -2^63, exact
        static constexpr double int64AsDoubleMax = -static_cast<double>(std::numeric_limits<int64_t>::min()); // +2^63, > INT64_MAX
        if (!(v < int64AsDoubleMax))
            return std::numeric_limits<int64_t>::max();
        if (!(v > int64AsDoubleMin))
            return std::numeric_limits<int64_t>::min();
        return truncateDoubleToInt64(v);
    }

    Duration(int64_t years, int64_t months, int64_t weeks, int64_t days, int64_t hours, int64_t minutes, int64_t seconds, int64_t milliseconds, Int128 microseconds, Int128 nanoseconds)
        : m_years(years)
        , m_months(months)
        , m_weeks(weeks)
        , m_days(days)
        , m_hours(hours)
        , m_minutes(minutes)
        , m_seconds(seconds)
        , m_milliseconds(milliseconds)
        , m_microseconds(microseconds)
        , m_nanoseconds(nanoseconds)
    {
    }

    int64_t years() const { return m_years; }
    int64_t months() const { return m_months; }
    int64_t weeks() const { return m_weeks; }
    int64_t days() const { return m_days; }
    int64_t hours() const { return m_hours; }
    int64_t minutes() const { return m_minutes; }
    int64_t seconds() const { return m_seconds; }
    int64_t milliseconds() const { return m_milliseconds; }
    Int128 microseconds() const { return m_microseconds; }
    Int128 nanoseconds() const { return m_nanoseconds; }

    // Typed setters for internal arithmetic with already-validated integer values.
    // double overloads are deleted: use setField(TemporalUnit, double) for JS inputs,
    // which routes through doubleToInt64Saturating to avoid UB on ARM32.
    void setYears(int64_t v) { m_years = v; }
    void setYears(double) = delete;
    void setMonths(int64_t v) { m_months = v; }
    void setMonths(double) = delete;
    void setWeeks(int64_t v) { m_weeks = v; }
    void setWeeks(double) = delete;
    void setDays(int64_t v) { m_days = v; }
    void setDays(double) = delete;
    void setHours(int64_t v) { m_hours = v; }
    void setHours(double) = delete;
    void setMinutes(int64_t v) { m_minutes = v; }
    void setMinutes(double) = delete;
    void setSeconds(int64_t v) { m_seconds = v; }
    void setSeconds(double) = delete;
    void setMilliseconds(int64_t v) { m_milliseconds = v; }
    void setMilliseconds(double) = delete;
    void setMicroseconds(Int128 v) { m_microseconds = v; }
    void setNanoseconds(Int128 v) { m_nanoseconds = v; }

    // Read-only field access by index or TemporalUnit — returns double for JS-layer compatibility.
    double operator[](size_t i) const { return (*this)[static_cast<TemporalUnit>(i)]; }
    double operator[](TemporalUnit u) const {
        switch (u) {
        case TemporalUnit::Year:
            return static_cast<double>(m_years);
        case TemporalUnit::Month:
            return static_cast<double>(m_months);
        case TemporalUnit::Week:
            return static_cast<double>(m_weeks);
        case TemporalUnit::Day:
            return static_cast<double>(m_days);
        case TemporalUnit::Hour:
            return static_cast<double>(m_hours);
        case TemporalUnit::Minute:
            return static_cast<double>(m_minutes);
        case TemporalUnit::Second:
            return static_cast<double>(m_seconds);
        case TemporalUnit::Millisecond:
            return static_cast<double>(m_milliseconds);
        case TemporalUnit::Microsecond:
            return static_cast<double>(m_microseconds);
        case TemporalUnit::Nanosecond:
            return static_cast<double>(m_nanoseconds);
        }
        ASSERT_NOT_REACHED();
        return 0;
    }

    void setField(size_t i, double v) { setField(static_cast<TemporalUnit>(i), v); }
    void setField(TemporalUnit, double);

    void clear() { *this = Duration(); }

    template<TemporalUnit unit>
    std::optional<Int128> NODELETE totalNanoseconds() const;

    Duration operator-() const
    {
        return Duration(-m_years, -m_months, -m_weeks, -m_days,
            -m_hours, -m_minutes, -m_seconds, -m_milliseconds,
            -m_microseconds, -m_nanoseconds);
    }

private:
    int64_t m_years { 0 };
    int64_t m_months { 0 };
    int64_t m_weeks { 0 };
    int64_t m_days { 0 };
    int64_t m_hours { 0 };
    int64_t m_minutes { 0 };
    int64_t m_seconds { 0 };
    int64_t m_milliseconds { 0 };
    Int128 m_microseconds { 0 };
    Int128 m_nanoseconds { 0 };
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
    constexpr ExactTime& operator=(const ExactTime&) = default;
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

    std::optional<ExactTime> NODELETE add(Duration) const;
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

    int32_t NODELETE sign() const;

    int32_t timeDurationSign() const
    {
        return m_time < 0 ? -1 : m_time > 0 ? 1 : 0;
    }

    Int128 time() const { return m_time; }

    Duration dateDuration() const { return m_dateDuration; }

    static InternalDuration NODELETE JS_EXPORT_PRIVATE combineDateAndTimeDuration(Duration, Int128);
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

    const PlainDate& isoPlainDate() const LIFETIME_BOUND { return m_isoPlainDate; }
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

    const PlainDate& isoPlainDate() const LIFETIME_BOUND { return m_isoPlainDate; }
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
    bool m_offsetHasSubMinutePrecision { false };
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

enum class TemporalProduction : uint8_t {
    Instant = 1 << 0, // TemporalInstantString
    DateTimeZoned = 1 << 1, // TemporalDateTimeString[+Zoned]
    DateTimeUnzoned = 1 << 2, // TemporalDateTimeString[~Zoned]
    YearMonth = 1 << 3, // TemporalYearMonthString
    MonthDay = 1 << 4, // TemporalMonthDayString
    Time = 1 << 5, // TemporalTimeString
};
using TemporalProductionSet = OptionSet<TemporalProduction>;

struct ParsedISODateTime {
    std::optional<PlainDate> date;
    std::optional<PlainTime> time;
    std::optional<TimeZoneRecord> timeZone;
    std::optional<CalendarID> calendar;
    TemporalProduction matched { };
    // True when the matched goal was the SHORT FORM:
    //   AnnotatedYearMonth (DateDay absent) or AnnotatedMonthDay (DateYear absent).
    // parseISODateTime already enforces Step 4.a.ii.(3)/(4) (short-form goals require
    // iso8601 calendar — returns std::nullopt otherwise). This flag remains for
    // consumers that branch on full-date vs short-form (e.g. PlainMonthDay).
    bool isShortForm { false };
};

JS_EXPORT_PRIVATE std::optional<ParsedISODateTime> parseISODateTime(StringView, TemporalProductionSet);

std::optional<TimeZone> JS_EXPORT_PRIVATE parseTemporalTimeZoneIdentifier(StringView);
// Strict variant: accepts only a bare UTC offset or bare IANA name — no embedded datetime strings.
std::optional<TimeZone> parseTimeZoneIdentifierStrict(StringView);
uint8_t dayOfWeek(PlainDate);
uint16_t NODELETE dayOfYear(PlainDate);
uint8_t weeksInYear(int32_t year);
uint8_t weekOfYear(PlainDate);
int32_t yearOfWeek(PlainDate);
uint8_t NODELETE daysInMonth(int32_t year, uint8_t month);
uint8_t daysInMonth(uint8_t month);
String formatTimeZoneOffsetString(int64_t);
String temporalTimeToString(PlainTime, std::tuple<Precision, unsigned>);
String temporalDateToString(PlainDate);
JS_EXPORT_PRIVATE String temporalDateTimeToString(PlainDate, PlainTime, std::tuple<Precision, unsigned>);
String temporalYearMonthToString(PlainYearMonth, StringView, unsigned calendarId);
String temporalMonthDayToString(PlainMonthDay, StringView, unsigned calendarId);
String monthCode(uint32_t);

bool NODELETE isValidDuration(const Duration&);
bool NODELETE isValidISODate(double, double, double);
PlainDate NODELETE createISODateRecord(double, double, double);

std::optional<ParsedMonthCode> NODELETE parseMonthCode(StringView);
std::optional<TimeZone> JS_EXPORT_PRIVATE parseTemporalTimeZoneIdentifier(StringView);

bool isDateTimeWithinLimits(int32_t year, uint8_t month, uint8_t day, unsigned hour, unsigned minute, unsigned second, unsigned millisecond, unsigned microsecond, unsigned nanosecond);

Int128 roundTimeDuration(JSGlobalObject*, Int128, unsigned, TemporalUnit, RoundingMode);

} // namespace ISO8601

using CheckedInt128 = Checked<Int128, RecordOverflow>;

CheckedInt128 NODELETE checkedCastDoubleToInt128(double n);

} // namespace JSC

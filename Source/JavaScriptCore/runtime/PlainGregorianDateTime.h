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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <wtf/Assertions.h>
#include <wtf/StdLibExtras.h>

namespace JSC {

// The broken-down form of an ECMAScript Date, in either local time or UTC. Milliseconds and
// smaller units are not kept: they are recovered from the Date's time value directly.
//
//     int21_t  year              21  [-271821, 275760]
//     uint4_t  month              4  [0, 11]
//     uint5_t  monthDay           5  [1, 31]
//     uint3_t  weekDay            3  [0, 6]
//     uint5_t  hour               5  [0, 23]
//     uint6_t  minute             6  [0, 59]
//     uint6_t  second             6  [0, 59]
//     int13_t  utcOffsetInMinute 13  [-1440, 1440]
//     uint1_t  isDST              1  [0, 1]
//
//                                64 in total
//
// The fields are laid out by hand rather than with bitfields because the JITs load them out of a
// DateInstance directly. An all-zero payload is reserved to mean "not computed yet": monthDay is
// never 0 in a real date, so no valid value collides with it.
class PlainGregorianDateTime {
public:
    static constexpr int32_t minYear = -271821;
    static constexpr int32_t maxYear = 275760;

    constexpr PlainGregorianDateTime() = default;

    static constexpr unsigned yearWidth = 21;
    static constexpr unsigned monthWidth = 4;
    static constexpr unsigned monthDayWidth = 5;
    static constexpr unsigned weekDayWidth = 3;
    static constexpr unsigned hourWidth = 5;
    static constexpr unsigned minuteWidth = 6;
    static constexpr unsigned secondWidth = 6;
    static constexpr unsigned utcOffsetInMinuteWidth = 13;
    static constexpr unsigned isDSTWidth = 1;

    static constexpr uint64_t yearMask = (1ULL << yearWidth) - 1;
    static constexpr uint64_t monthMask = (1ULL << monthWidth) - 1;
    static constexpr uint64_t monthDayMask = (1ULL << monthDayWidth) - 1;
    static constexpr uint64_t weekDayMask = (1ULL << weekDayWidth) - 1;
    static constexpr uint64_t hourMask = (1ULL << hourWidth) - 1;
    static constexpr uint64_t minuteMask = (1ULL << minuteWidth) - 1;
    static constexpr uint64_t secondMask = (1ULL << secondWidth) - 1;
    static constexpr uint64_t utcOffsetInMinuteMask = (1ULL << utcOffsetInMinuteWidth) - 1;
    static constexpr uint64_t isDSTMask = (1ULL << isDSTWidth) - 1;

    static constexpr unsigned yearOffset = 64 - yearWidth;
    static constexpr unsigned monthOffset = yearOffset - monthWidth;
    static constexpr unsigned monthDayOffset = monthOffset - monthDayWidth;
    static constexpr unsigned weekDayOffset = monthDayOffset - weekDayWidth;
    static constexpr unsigned hourOffset = weekDayOffset - hourWidth;
    static constexpr unsigned minuteOffset = hourOffset - minuteWidth;
    static constexpr unsigned secondOffset = minuteOffset - secondWidth;
    static constexpr unsigned utcOffsetInMinuteOffset = secondOffset - utcOffsetInMinuteWidth;
    static constexpr unsigned isDSTOffset = utcOffsetInMinuteOffset - isDSTWidth;
    static_assert(!isDSTOffset);

    PlainGregorianDateTime(int32_t year, int32_t month, int32_t monthDay, int32_t weekDay, int32_t hour, int32_t minute, int32_t second, int32_t utcOffsetInMinute, bool isDST)
    {
        ASSERT(year >= minYear && year <= maxYear);
        ASSERT(month >= 0 && month <= 11);
        ASSERT(monthDay >= 1 && monthDay <= 31);
        ASSERT(weekDay >= 0 && weekDay <= 6);
        ASSERT(hour >= 0 && hour <= 23);
        ASSERT(minute >= 0 && minute <= 59);
        ASSERT(second >= 0 && second <= 59);
        ASSERT(utcOffsetInMinute >= -(60 * 24) && utcOffsetInMinute <= (60 * 24));

        m_payload = (static_cast<uint64_t>(static_cast<uint32_t>(year) & yearMask) << yearOffset)
            | (static_cast<uint64_t>(month) << monthOffset)
            | (static_cast<uint64_t>(monthDay) << monthDayOffset)
            | (static_cast<uint64_t>(weekDay) << weekDayOffset)
            | (static_cast<uint64_t>(hour) << hourOffset)
            | (static_cast<uint64_t>(minute) << minuteOffset)
            | (static_cast<uint64_t>(second) << secondOffset)
            | (static_cast<uint64_t>(static_cast<uint32_t>(utcOffsetInMinute) & utcOffsetInMinuteMask) << utcOffsetInMinuteOffset)
            | (static_cast<uint64_t>(isDST) << isDSTOffset);

        ASSERT(year == this->year());
        ASSERT(month == this->month());
        ASSERT(monthDay == this->monthDay());
        ASSERT(weekDay == this->weekDay());
        ASSERT(hour == this->hour());
        ASSERT(minute == this->minute());
        ASSERT(second == this->second());
        ASSERT(utcOffsetInMinute == this->utcOffsetInMinute());
        ASSERT(isDST == this->isDST());
    }

    // Signed, and stored in the topmost bits, so an arithmetic shift sign-extends it.
    int32_t year() const { return static_cast<int32_t>(std::bit_cast<int64_t>(m_payload) >> yearOffset); }

    int32_t month() const { return static_cast<int32_t>((m_payload >> monthOffset) & monthMask); }
    int32_t monthDay() const { return static_cast<int32_t>((m_payload >> monthDayOffset) & monthDayMask); }
    int32_t weekDay() const { return static_cast<int32_t>((m_payload >> weekDayOffset) & weekDayMask); }
    int32_t hour() const { return static_cast<int32_t>((m_payload >> hourOffset) & hourMask); }
    int32_t minute() const { return static_cast<int32_t>((m_payload >> minuteOffset) & minuteMask); }
    int32_t second() const { return static_cast<int32_t>((m_payload >> secondOffset) & secondMask); }

    int32_t utcOffsetInMinute() const
    {
        int64_t value = std::bit_cast<int64_t>(m_payload << (64 - utcOffsetInMinuteWidth - utcOffsetInMinuteOffset));
        return static_cast<int32_t>(value >> (64 - utcOffsetInMinuteWidth));
    }

    bool isDST() const { return m_payload & isDSTMask; }

    // monthDay is never zero in a real date, so those bits alone say whether this holds one. That
    // leaves the other bits free to distinguish "never computed" (an all-zero payload) from
    // "computed for a time value that has since changed" (the stale marker).
    static constexpr uint64_t validityMask = monthDayMask << monthDayOffset;
    explicit operator bool() const { return m_payload & validityMask; }

    static constexpr PlainGregorianDateTime staleMarker() { return PlainGregorianDateTime(1); }
    bool hasNeverBeenComputed() const { return !m_payload; }

    uint64_t payload() const { return m_payload; }

private:
    explicit constexpr PlainGregorianDateTime(uint64_t payload)
        : m_payload(payload)
    {
    }

    uint64_t m_payload { 0 };
};
static_assert(sizeof(PlainGregorianDateTime) == sizeof(uint64_t));

} // namespace JSC

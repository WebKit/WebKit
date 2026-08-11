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

#include "config.h"
#include "CalendarArithmetic.h"

namespace JSC {
namespace TemporalCore {

// CalendarDateUntil (ISO8601 path) — temporal_rs: Calendar::date_until
// https://tc39.es/proposal-temporal/#sec-temporal-calendardateuntil (ISO8601 path)
ISO8601::Duration calendarDateUntil(const ISO8601::PlainDate& one, const ISO8601::PlainDate& two, TemporalUnit largestUnit)
{
    // 3. If calendar is "iso8601", then
    //    3.k. Return ! CreateDateDurationRecord(years, months, weeks, days).
    // (full ISO8601 path implemented in diffISODate)
    return diffISODate(one, two, largestUnit);
}

// CalendarDateAdd (ISO8601 path) — temporal_rs: Calendar::date_add
// https://tc39.es/proposal-temporal/#sec-temporal-calendardateadd (ISO8601 path)
TemporalResult<ISO8601::PlainDate> calendarDateAdd(const ISO8601::PlainDate& date, const ISO8601::Duration& duration, TemporalOverflow overflow)
{
    // 1. If calendar is "iso8601", then
    //    1.a-1.d. BalanceISOYearMonth + RegulateISODate + AddDaysToISODate.
    //    3. If ISODateWithinLimits(result) is false, throw a RangeError.
    //    4. Return result.
    // (full ISO8601 path implemented in isoDateAdd)
    return isoDateAdd(date, duration, overflow);
}

} // namespace TemporalCore
} // namespace JSC

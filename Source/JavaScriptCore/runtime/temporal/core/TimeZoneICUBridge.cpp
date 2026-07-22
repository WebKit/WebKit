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
#include "TimeZoneICUBridge.h"

#include "CalendarArithmetic.h"
#include "CalendarICUBridge.h"
#include "DateConstructor.h"
#include "IntlObject.h"
#include "JSCTimeZone.h"
#include <unicode/ucal.h>
#include <wtf/DateMath.h>
#include <wtf/Lock.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/TinyLRUCache.h>
#include <wtf/unicode/icu/ICUHelpers.h>

namespace JSC {
namespace TemporalCore {

class TimeZoneCacheEntry final : public ThreadSafeRefCounted<TimeZoneCacheEntry> {
    WTF_MAKE_TZONE_ALLOCATED(TimeZoneCacheEntry);
public:
    Lock useLock;
    std::unique_ptr<UCalendar, ICUDeleter<ucal_close>> cal;
};
WTF_MAKE_TZONE_ALLOCATED_IMPL(TimeZoneCacheEntry);

struct TimeZoneLRUCachePolicy {
    static bool isKeyNull(const TimeZone&) { return false; }
    static RefPtr<TimeZoneCacheEntry> createValueForNullKey() { return nullptr; }
    static RefPtr<TimeZoneCacheEntry> createValueForKey(const TimeZone&) { return adoptRef(*new TimeZoneCacheEntry); }
    static TimeZone createKeyForStorage(const TimeZone& tz) { return tz; }
};

static RefPtr<TimeZoneCacheEntry> timeZoneCacheEntry(const TimeZone& timeZone)
{
    static Lock cacheLock;
    static LazyNeverDestroyed<TinyLRUCache<TimeZone, RefPtr<TimeZoneCacheEntry>, 8, TimeZoneLRUCachePolicy>> cache;
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        cache.construct();
    });
    Locker locker { cacheLock };
    return cache.get().get(timeZone);
}

template<typename F>
static auto withTimeZone(const TimeZone& timeZone, F&& fn) -> decltype(fn(static_cast<UCalendar*>(nullptr)))
{
    ASSERT(timeZone.isID());
    auto entry = timeZoneCacheEntry(timeZone);
    ASSERT(entry);
    Locker locker { entry->useLock };
    if (!entry->cal) {
        String tzStr = timeZone.toICUString();
        StringView view(tzStr);
        auto upconverted = view.upconvertedCharacters();
        UErrorCode status = U_ZERO_ERROR;
        entry->cal = std::unique_ptr<UCalendar, ICUDeleter<ucal_close>>(ucal_open(upconverted, view.length(), "", UCAL_DEFAULT, &status));
    }
    if (entry->cal)
        ucal_clear(entry->cal.get());
    return fn(entry->cal.get());
}

// getOffsetMsAtEpoch — internal: queries ICU for UTC+DST offset in ms at a given epoch
static std::optional<int32_t> getOffsetMsAtEpoch(UCalendar* cal, double epochMs)
{
    UErrorCode status = U_ZERO_ERROR;
    ucal_setMillis(cal, epochMs, &status);
    if (U_FAILURE(status)) [[unlikely]]
        return std::nullopt;
    int32_t rawOffset = ucal_get(cal, UCAL_ZONE_OFFSET, &status);
    if (U_FAILURE(status)) [[unlikely]]
        return std::nullopt;
    int32_t dstOffset = ucal_get(cal, UCAL_DST_OFFSET, &status);
    if (U_FAILURE(status)) [[unlikely]]
        return std::nullopt;
    return rawOffset + dstOffset;
}

// isoDateTimeToLocalMs — internal: converts ISO date+time to epoch ms treating local time as UTC (no offset applied)
static double isoDateTimeToLocalMs(const ISO8601::PlainDate& date, const ISO8601::PlainTime& time)
{
    // makeDay takes month as 0-indexed; PlainDate month is 1-indexed.
    double days = makeDay(date.year(), date.month() - 1, date.day());
    double timeMs = makeTime(time.hour(), time.minute(), time.second(), time.millisecond());
    return makeDate(days, timeMs);
}

static constexpr std::pair<int64_t, int64_t> floorDivMod(int64_t num, int64_t denom)
{
    int64_t q = num / denom;
    int64_t r = num % denom;
    if (r < 0) {
        r += denom;
        q -= 1;
    }
    return { q, r };
}

// temporal_rs: IsoTime::balance (src/iso.rs:664). Cascades ns→μs→ms→s→m→h; returns overflow days.
static std::pair<int64_t, ISO8601::PlainTime> balanceIsoTime(int64_t hour, int64_t minute, int64_t second, int64_t millisecond, int64_t microsecond, int64_t nanosecond)
{
    auto [nq, ns] = floorDivMod(nanosecond, 1000);
    microsecond += nq;
    auto [uq, us] = floorDivMod(microsecond, 1000);
    millisecond += uq;
    auto [mq, ms] = floorDivMod(millisecond, 1000);
    second += mq;
    auto [sq, s] = floorDivMod(second, 60);
    minute += sq;
    auto [minq, min] = floorDivMod(minute, 60);
    hour += minq;
    auto [days, h] = floorDivMod(hour, 24);
    return { days, ISO8601::PlainTime(static_cast<int>(h), static_cast<int>(min), static_cast<int>(s), static_cast<int>(ms), static_cast<int>(us), static_cast<int>(ns)) };
}

// temporal_rs: IsoDate::balance (src/iso.rs:343). Normalizes (y, m, d) via epoch-days round-trip.
static ISO8601::PlainDate balanceIsoDate(int32_t year, int32_t month, int64_t day)
{
    double epochDays = makeDay(year, month - 1, static_cast<int>(day));
    auto [y, m0, d] = WTF::yearMonthDayFromDays(static_cast<int32_t>(epochDays));
    return ISO8601::PlainDate(y, static_cast<uint8_t>(m0 + 1), static_cast<uint8_t>(d));
}

// temporal_rs: IsoDateTime::from_epoch_nanos (src/iso.rs:87).
// Steps 2-3 of GetISODateTimeFor with caller-supplied offset.
ISO8601::PlainDateTime exactTimeToLocalDateAndTime(ISO8601::ExactTime exactTime, int64_t offsetNs)
{
    // Floor-split epochNs into (epochMs, remainderNs).
    Int128 epochNs = exactTime.epochNanoseconds();
    Int128 nsPerMs = ISO8601::ExactTime::nsPerMillisecond;
    Int128 remainderNs128 = epochNs % nsPerMs;
    Int128 epochMs128 = epochNs / nsPerMs;
    if (remainderNs128 < 0) {
        remainderNs128 += nsPerMs;
        epochMs128 -= 1;
    }
    int64_t epochMs = static_cast<int64_t>(epochMs128);
    int64_t remainderNs = static_cast<int64_t>(remainderNs128);

    // Raw y/m/d/h/m/s/ms from epochMs; μs/ns from remainderNs. Offset is applied by balance below.
    WTF::Int64Milliseconds epochMsWT(epochMs);
    int32_t days = WTF::msToDays(epochMsWT);
    int32_t timeInDayMs = WTF::timeInDay(epochMsWT, days);
    auto [year, month0, day] = WTF::yearMonthDayFromDays(days);

    int64_t hour = timeInDayMs / 3'600'000;
    int64_t minute = (timeInDayMs / 60'000) % 60;
    int64_t second = (timeInDayMs / 1'000) % 60;
    int64_t millisecond = timeInDayMs % 1'000;
    int64_t microsecond = remainderNs / 1'000;
    int64_t nanosecond = remainderNs % 1'000;

    auto [overflowDays, time] = balanceIsoTime(hour, minute, second, millisecond, microsecond, nanosecond + offsetNs);
    return ISO8601::PlainDateTime(balanceIsoDate(year, month0 + 1, static_cast<int64_t>(day) + overflowDays), time);
}

// getOffsetNanosecondsFor — temporal_rs: TimeZone::get_offset_nanos_for (src/builtins/core/time_zone.rs)
// https://tc39.es/proposal-temporal/#sec-temporal-getoffsetnanosecondsfor
TemporalResult<int64_t> getOffsetNanosecondsFor(const TimeZone& timeZone, ISO8601::ExactTime exactTime)
{
    // 1. Let parseResult be ! ParseTimeZoneIdentifier(timeZone).
    // 2. If parseResult.[[OffsetMinutes]] is not ~empty~, return parseResult.[[OffsetMinutes]] × (60 × 10^9).
    if (timeZone.isUTCOffset())
        return timeZone.utcOffsetNanoseconds();

    // 3. Return GetNamedTimeZoneOffsetNanoseconds(parseResult.[[Name]], epochNs).
    // NOTE: Implemented via ICU4C: UCAL_ZONE_OFFSET + UCAL_DST_OFFSET.
    return withTimeZone(timeZone, [&](UCalendar* cal) -> TemporalResult<int64_t> {
        if (!cal) [[unlikely]]
            return makeUnexpected(rangeError(icuOpenTimeZoneFailed));
        double epochMs = static_cast<double>(exactTime.floorEpochMilliseconds());
        auto offsetMs = getOffsetMsAtEpoch(cal, epochMs);
        if (!offsetMs)
            return makeUnexpected(rangeError(icuTimeZoneOffsetFailed));
        constexpr int64_t nsPerMs = 1'000'000;
        return static_cast<int64_t>(*offsetMs) * nsPerMs;
    });
}

// https://tc39.es/proposal-temporal/#sec-temporal-getisodatetimefor
TemporalResult<ISO8601::PlainDateTime> getISODateTimeFor(const TimeZone& timeZone, ISO8601::ExactTime epochNs)
{
    // Step 1: Let offsetNs be GetOffsetNanosecondsFor(tz, epochNs).
    auto offsetResult = getOffsetNanosecondsFor(timeZone, epochNs);
    if (!offsetResult) [[unlikely]]
        return makeUnexpected(offsetResult.error());
    // Steps 2-3: GetISOPartsFromEpoch(ℝ(epochNs) + offsetNs) + CombineISODateAndTimeRecord.
    return exactTimeToLocalDateAndTime(epochNs, *offsetResult);
}

// getNamedTimeZoneEpochNanoseconds — ICU4C implementation of the implementation-defined AO
// https://tc39.es/proposal-temporal/#sec-getnamedtimezoneepochnanoseconds
// NOTE: Uses ucal_getTimeZoneOffsetFromLocal (ICU 69+), which resolves a local wall-clock time
// to its UTC offset(s) directly, with explicit control over the gap (nonExistingTimeOpt) and
// fold (duplicatedTimeOpt) cases. Querying the FORMER (pre-transition) and LATTER (post-transition)
// interpretations yields the two bracket offsets, from which normal/gap/fold follow arithmetically.
// This replaces the previous ucal_setDateTime + transition-probing + pre-LMT-fallback approach.
static TemporalResult<PossibleEpochNanoseconds> getNamedTimeZoneEpochNanoseconds(const TimeZone& timeZone, const ISO8601::PlainDate& date, const ISO8601::PlainTime& time)
{
    // NOTE: Sub-ms fields are added back after ms-level computation; offset changes occur at ≥second granularity.
    Int128 subMs = static_cast<Int128>(time.microsecond()) * 1000 + static_cast<Int128>(time.nanosecond());

    // Compute the "naive" local epoch — the local datetime treated as UTC, used for gap/fold detection.
    double localMs = isoDateTimeToLocalMs(date, time);

    return withTimeZone(timeZone, [&](UCalendar* cal) -> TemporalResult<PossibleEpochNanoseconds> {
        if (!cal) [[unlikely]]
            return makeUnexpected(rangeError(icuOpenTimeZoneFailed));

        auto makeExactTime = [&](double epochMs) -> ISO8601::ExactTime {
            auto base = ISO8601::ExactTime::fromEpochMilliseconds(static_cast<int64_t>(epochMs));
            return ISO8601::ExactTime(base.epochNanoseconds() + subMs);
        };

        // Store the naive local epoch as the calendar's time value. ucal_getTimeZoneOffsetFromLocal
        // reinterprets that stored value as wall-clock (local) time. IMPORTANT: do NOT use
        // ucal_setDateTime here — it would apply ICU's own disambiguation and store a resolved UTC
        // instant, which is not what getTimeZoneOffsetFromLocal expects to read.
        UErrorCode status = U_ZERO_ERROR;
        ucal_setMillis(cal, localMs, &status);
        if (U_FAILURE(status)) [[unlikely]]
            return makeUnexpected(rangeError(icuSetCalendarFailed));

        // Query both interpretations of the local time. Away from any transition they are equal.
        //   FORMER = offset in effect before a nearby transition, LATTER = offset after it.
        auto offsetMsFor = [&](UTimeZoneLocalOption opt) -> std::optional<int64_t> {
            UErrorCode s = U_ZERO_ERROR;
            int32_t rawOffset = 0;
            int32_t dstOffset = 0;
            ucal_getTimeZoneOffsetFromLocal(cal, opt, opt, &rawOffset, &dstOffset, &s);
            if (U_FAILURE(s)) [[unlikely]]
                return std::nullopt;
            return static_cast<int64_t>(rawOffset) + static_cast<int64_t>(dstOffset);
        };

        auto before = offsetMsFor(UCAL_TZ_LOCAL_FORMER);
        auto after = offsetMsFor(UCAL_TZ_LOCAL_LATTER);
        if (!before || !after) [[unlikely]]
            return makeUnexpected(rangeError(icuTimeZoneOffsetFailed));

        constexpr int64_t nsPerMs = static_cast<int64_t>(ISO8601::ExactTime::nsPerMillisecond);

        // Normal: a single offset is in force → exactly one instant.
        if (*before == *after)
            return PossibleEpochNanoseconds { makeExactTime(localMs - static_cast<double>(*before)) };

        // Offset jumped up (spring-forward) → the local time is skipped (gap): no instant maps here.
        // Hand the bracket offsets to the disambiguator, preserving the prior GapOffsets contract
        // (disambiguate: earlier = naiveNs − afterNs; later/compatible = naiveNs − beforeNs).
        if (*after > *before)
            return PossibleEpochNanoseconds { GapOffsets { *before * nsPerMs, *after * nsPerMs } };

        // Offset jumped down (fall-back) → the local time occurs twice (fold).
        // earlier uses the larger (pre-transition) offset; later uses the smaller (post-transition).
        ISO8601::ExactTime earlier = makeExactTime(localMs - static_cast<double>(*before));
        ISO8601::ExactTime later = makeExactTime(localMs - static_cast<double>(*after));
        ASSERT(earlier.epochNanoseconds() <= later.epochNanoseconds());
        return PossibleEpochNanoseconds { std::array<ISO8601::ExactTime, 2> { earlier, later } };
    }); // withTimeZone
}

// getPossibleEpochNanosecondsFor — temporal_rs: TimeZone::get_possible_epoch_ns_for (src/builtins/core/time_zone.rs)
// https://tc39.es/proposal-temporal/#sec-temporal-getpossibleepochnanoseconds
TemporalResult<PossibleEpochNanoseconds> getPossibleEpochNanosecondsFor(const TimeZone& timeZone, const ISO8601::PlainDate& date, const ISO8601::PlainTime& time)
{
    // 1. Let parseResult be ! ParseTimeZoneIdentifier(timeZone).
    // 2. If parseResult.[[OffsetMinutes]] is not ~empty~, then
    //    a-d. Balance, check range, compute UTC epoch ns, return « epochNs ».
    // NOTE: UTC-offset timezones always yield exactly 1 candidate; no ICU needed.
    if (timeZone.isUTCOffset()) {
        Int128 subMs = static_cast<Int128>(time.microsecond()) * 1000 + static_cast<Int128>(time.nanosecond());
        int64_t offsetMs = timeZone.utcOffsetNanoseconds() / static_cast<int64_t>(ISO8601::ExactTime::nsPerMillisecond);
        double localMs = isoDateTimeToLocalMs(date, time);
        double epochMs = localMs - static_cast<double>(offsetMs);
        auto base = ISO8601::ExactTime::fromEpochMilliseconds(static_cast<int64_t>(epochMs));
        ISO8601::ExactTime exactTime(base.epochNanoseconds() + subMs);
        if (!exactTime.isValid())
            return makeUnexpected(rangeError(epochNanosecondsOutOfRange));
        return PossibleEpochNanoseconds { exactTime };
    }

    // 3. Else, let possibleEpochNanoseconds be GetNamedTimeZoneEpochNanoseconds(parseResult.[[Name]], isoDateTime).
    auto possible = getNamedTimeZoneEpochNanoseconds(timeZone, date, time);
    if (!possible)
        return makeUnexpected(possible.error());

    // 4. For each value epochNanoseconds in possibleEpochNanoseconds:
    //    a. If IsValidEpochNanoseconds(epochNanoseconds) is false, throw a RangeError.
    for (auto& candidate : epochCandidates(*possible)) {
        if (!candidate.isValid())
            return makeUnexpected(rangeError(epochNanosecondsOutOfRange));
    }

    // 5. Return possibleEpochNanoseconds.
    return *possible;
}

// getTimeZoneTransition — ICU4C implementation of GetNamedTimeZoneNextTransition / GetNamedTimeZonePreviousTransition
// https://tc39.es/proposal-temporal/#sec-temporal-getnamedtimezonenexttransition (Next)
// https://tc39.es/proposal-temporal/#sec-temporal-getnamedtimezoneprevioustransition (Previous)
TemporalResult<std::optional<ISO8601::ExactTime>> getTimeZoneTransition(const TimeZone& timeZone, ISO8601::ExactTime exactTime, TransitionDirection direction)
{
    // 1. If timeZone is a UTC-offset timezone, return null (no transitions).
    if (timeZone.isUTCOffset())
        return std::optional<ISO8601::ExactTime> { std::nullopt };

    // 2. Compute epochMs.
    double epochMs;
    if (direction == TransitionDirection::Previous) {
        Int128 epochNs = exactTime.epochNanoseconds();
        int64_t ms = static_cast<int64_t>(epochNs / static_cast<int64_t>(ISO8601::ExactTime::nsPerMillisecond));
        if (epochNs % static_cast<int64_t>(ISO8601::ExactTime::nsPerMillisecond) > 0)
            ms++;
        epochMs = static_cast<double>(ms);
    } else
        epochMs = static_cast<double>(exactTime.floorEpochMilliseconds());

    return withTimeZone(timeZone, [&](UCalendar* cal) -> TemporalResult<std::optional<ISO8601::ExactTime>> {
        if (!cal) [[unlikely]]
            return makeUnexpected(rangeError(icuOpenTimeZoneFailed));

        UErrorCode status = U_ZERO_ERROR;
        ucal_setMillis(cal, epochMs, &status);
        if (U_FAILURE(status)) [[unlikely]]
            return makeUnexpected(rangeError(icuSetCalendarFailed));

        UTimeZoneTransitionType transType = (direction == TransitionDirection::Next) ? UCAL_TZ_TRANSITION_NEXT : UCAL_TZ_TRANSITION_PREVIOUS;

        // 3. Repeat (up to 20 times to skip rule-only changes).
        for (int safetyLimit = 0; safetyLimit < 20; ++safetyLimit) {
            UDate transitionMs = 0;
            bool hasTransition = ucal_getTimeZoneTransitionDate(cal, transType, &transitionMs, &status);
            if (U_FAILURE(status)) [[unlikely]]
                return makeUnexpected(rangeError(icuTransitionFailed));

            if (!hasTransition)
                return std::optional<ISO8601::ExactTime> { std::nullopt };

            auto transition = ISO8601::ExactTime::fromEpochMilliseconds(static_cast<int64_t>(transitionMs));
            if (!transition.isValid())
                return std::optional<ISO8601::ExactTime> { std::nullopt };

            // Check if offset actually changed at this transition by querying before/after on cal directly.
            double beforeMs = transitionMs - 1.0;
            double afterMs = transitionMs;
            ucal_setMillis(cal, beforeMs, &status);
            int32_t offsetBefore = ucal_get(cal, UCAL_ZONE_OFFSET, &status) + ucal_get(cal, UCAL_DST_OFFSET, &status);
            ucal_setMillis(cal, afterMs, &status);
            int32_t offsetAfter = ucal_get(cal, UCAL_ZONE_OFFSET, &status) + ucal_get(cal, UCAL_DST_OFFSET, &status);
            if (U_FAILURE(status)) [[unlikely]]
                return std::optional<ISO8601::ExactTime> { transition };
            if (offsetBefore != offsetAfter)
                return std::optional<ISO8601::ExactTime> { transition };

            ucal_setMillis(cal, direction == TransitionDirection::Previous ? beforeMs : transitionMs + 1.0, &status);
            if (U_FAILURE(status)) [[unlikely]]
                return makeUnexpected(rangeError(icuTransitionFailed));
        }
        return std::optional<ISO8601::ExactTime> { std::nullopt };
    }); // withTimeZone
}

// disambiguatePossibleEpochNanoseconds — temporal_rs: TimeZone::disambiguate_possible_epoch_nanos (src/builtins/core/time_zone.rs)
// https://tc39.es/proposal-temporal/#sec-temporal-disambiguatepossibleepochnanoseconds
// NOTE: For the gap case (n=0), bracket offsets are precomputed in GapOffsets { beforeNs, afterNs } to avoid extra ICU calls.
static TemporalResult<ISO8601::ExactTime> disambiguatePossibleEpochNanoseconds(const PossibleEpochNanoseconds& possible, const ISO8601::PlainDate& date, const ISO8601::PlainTime& time, TemporalDisambiguation disambiguation)
{
    double localMs = isoDateTimeToLocalMs(date, time);
    Int128 subMs = static_cast<Int128>(time.microsecond()) * 1000 + static_cast<Int128>(time.nanosecond());
    Int128 naiveNs = static_cast<Int128>(localMs) * ISO8601::ExactTime::nsPerMillisecond + subMs;

    return WTF::switchOn(possible,
        // 1. n = elements in possibleEpochNs (encoded in Variant type).
        // 2. If n = 1, return the sole element.
        [](const ISO8601::ExactTime& t) -> TemporalResult<ISO8601::ExactTime> {
            return t;
        },
        // 3. If n ≠ 0 (DST fold: 2 candidates [earlier, later]):
        //    a. If disambiguation is ~earlier~ or ~compatible~, return possibleEpochNs[0].
        //    b. If disambiguation is ~later~, return possibleEpochNs[n-1].
        //    c. Assert: disambiguation is ~reject~. Throw a RangeError.
        [&](const std::array<ISO8601::ExactTime, 2>& fold) -> TemporalResult<ISO8601::ExactTime> {
            if (disambiguation == TemporalDisambiguation::Reject)
                return makeUnexpected(rangeError("ambiguous instant: use a 'disambiguation' option to resolve"_s));
            if (disambiguation == TemporalDisambiguation::Earlier || disambiguation == TemporalDisambiguation::Compatible)
                return fold[0];
            ASSERT(disambiguation == TemporalDisambiguation::Later);
            return fold[1];
        },
        // 4. Assert: n = 0 (DST gap — local time does not exist).
        // 5. If disambiguation is ~reject~, throw a RangeError.
        // 6-20. Bracket offsets from GapOffsets; step 16 (~earlier~): naiveNs - offsetAfter; step 17-20 (~later~/~compatible~): naiveNs - offsetBefore.
        [&](const GapOffsets& gap) -> TemporalResult<ISO8601::ExactTime> {
            if (disambiguation == TemporalDisambiguation::Reject)
                return makeUnexpected(rangeError("nonexistent instant: local time does not exist in this time zone (DST gap)"_s));
            if (disambiguation == TemporalDisambiguation::Earlier)
                return ISO8601::ExactTime(naiveNs - Int128(gap.afterNs));
            return ISO8601::ExactTime(naiveNs - Int128(gap.beforeNs));
        });
}

// getEpochNanosecondsFor — temporal_rs: TimeZone::get_epoch_nanoseconds_for (src/builtins/core/time_zone.rs)
// https://tc39.es/proposal-temporal/#sec-temporal-getepochnanosecondsfor
TemporalResult<ISO8601::ExactTime> getEpochNanosecondsFor(const TimeZone& timeZone, const ISO8601::PlainDate& date, const ISO8601::PlainTime& time, TemporalDisambiguation disambiguation)
{
    // 1. Let possibleEpochNs be ? GetPossibleEpochNanosecondsFor(timeZone, date, time).
    auto possible = getPossibleEpochNanosecondsFor(timeZone, date, time);
    if (!possible)
        return makeUnexpected(possible.error());
    // 2. Return ? DisambiguatePossibleInstants(possibleEpochNs, timeZone, date, time, disambiguation).
    return disambiguatePossibleEpochNanoseconds(*possible, date, time, disambiguation);
}

// addZonedDateTime — temporal_rs: ZonedDateTime::add_zoned_date_time (src/builtins/core/zoned_date_time.rs)
// https://tc39.es/proposal-temporal/#sec-temporal-addzoneddatetime
TemporalResult<ISO8601::ExactTime> addZonedDateTime(ISO8601::ExactTime startEpochNs, const TimeZone& timeZone, const ISO8601::Duration& duration, TemporalOverflow overflow, CalendarID calendarId)
{
    // NOTE: Pre-compute the time duration (norm) as nanoseconds; used by AddInstant in steps 1 and 7.
    CheckedInt128 m = checkedCastDoubleToInt128(duration.minutes()) + checkedCastDoubleToInt128(duration.hours()) * Int128(60);
    CheckedInt128 s = checkedCastDoubleToInt128(duration.seconds()) + m * Int128(60);
    CheckedInt128 ms = checkedCastDoubleToInt128(duration.milliseconds()) + s * Int128(1000);
    CheckedInt128 us = CheckedInt128(duration.microseconds()) + ms * Int128(1000);
    CheckedInt128 ns = CheckedInt128(duration.nanoseconds()) + us * Int128(1000);
    ASSERT(!ns.hasOverflowed());
    Int128 norm = ns;

    // 1. If DateDurationSign(duration.[[Date]]) = 0, return ? AddInstant(epochNanoseconds, duration.[[Time]]).
    bool noDateComponents = !duration.years() && !duration.months()
        && !duration.weeks() && !duration.days();
    if (noDateComponents) {
        if (!norm)
            return startEpochNs;
        ISO8601::ExactTime result(startEpochNs.epochNanoseconds() + norm);
        if (!result.isValid())
            return makeUnexpected(rangeError("Duration addition results in an out-of-range ZonedDateTime"_s));
        return result;
    }

    // 2. Let isoDateTime be GetISODateTimeFor(timeZone, epochNanoseconds).
    auto isoDateTimeResult = getISODateTimeFor(timeZone, startEpochNs);
    if (!isoDateTimeResult) [[unlikely]]
        return makeUnexpected(isoDateTimeResult.error());
    auto [date, time] = *isoDateTimeResult;

    // 3. Let addedDate be ? CalendarDateAdd(calendar, isoDateTime.[[ISODate]], duration.[[Date]], overflow).
    ISO8601::Duration dateDuration(duration.years(), duration.months(), duration.weeks(), duration.days(), 0, 0, 0, 0, 0, 0);
    TemporalResult<ISO8601::PlainDate> addedDateResult;
    if (calendarIsISO(calendarId))
        addedDateResult = calendarDateAdd(date, dateDuration, overflow);
    else
        addedDateResult = TemporalCore::calendarDateAdd(calendarId, date, dateDuration, overflow);
    if (!addedDateResult)
        return makeUnexpected(addedDateResult.error());

    // 4. Let intermediateDateTime be CombineISODateAndTimeRecord(addedDate, isoDateTime.[[Time]]).
    // 5. If ISODateTimeWithinLimits(intermediateDateTime) is false, throw a RangeError exception.
    if (!ISO8601::isDateTimeWithinLimits(addedDateResult->year(), addedDateResult->month(), addedDateResult->day(), time.hour(), time.minute(), time.second(), time.millisecond(), time.microsecond(), time.nanosecond()))
        return makeUnexpected(rangeError("intermediate datetime out of range"_s));

    // 6. Let intermediateNs be ! GetEpochNanosecondsFor(timeZone, intermediateDateTime, ~compatible~).
    auto intermediateResult = getEpochNanosecondsFor(timeZone, *addedDateResult, time, TemporalDisambiguation::Compatible);
    if (!intermediateResult)
        return makeUnexpected(intermediateResult.error());

    // 7. Return ? AddInstant(intermediateNs, duration.[[Time]]).
    ISO8601::ExactTime result(intermediateResult->epochNanoseconds() + norm);
    if (!result.isValid())
        return makeUnexpected(rangeError("Duration addition results in an out-of-range ZonedDateTime"_s));
    return result;
}

// timeZoneEquals — temporal_rs: TimeZone::time_zone_equals_with_provider (src/builtins/core/time_zone.rs)
// https://tc39.es/proposal-canonical-tz/#sec-temporal-timezoneequals
// Spec operates on Time Zone Identifier records. Step 1 (Object identity) and steps 2-4
// (canonicalization + SameValue on the canonical strings) are subsumed by JSC's TimeZone
// representation: identical TimeZone values mean both sides resolved to the same record.
// Step 7 (both named) reduces to primary-identifier comparison via intlPrimaryTimeZoneID.
// Step 8 (both offset) reduces to numeric offset comparison — already covered by
// TimeZone::operator== since offset records share m_id == offsetTimeZoneID.
// Mixed kinds fall through to step 9 (false).
bool timeZoneEquals(const TimeZone& a, const TimeZone& b)
{
    if (a == b)
        return true;
    if (a.isUTCOffset() || b.isUTCOffset())
        return false;
    return intlPrimaryTimeZoneID(a.id()) == intlPrimaryTimeZoneID(b.id());
}

bool timeZoneEquals(StringView id1, StringView id2)
{
    if (id1 == id2)
        return true;
    auto a = ISO8601::parseTemporalTimeZoneIdentifier(id1);
    auto b = ISO8601::parseTemporalTimeZoneIdentifier(id2);
    if (!a || !b)
        return false;
    return timeZoneEquals(*a, *b);
}

} // namespace TemporalCore
} // namespace JSC

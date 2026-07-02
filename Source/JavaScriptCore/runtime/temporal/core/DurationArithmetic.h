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

// JSC Temporal Core — Duration arithmetic algorithms
// temporal_rs reference: src/builtins/core/duration.rs
// Last synced: v0.2.3

#include <JavaScriptCore/CalendarICUBridge.h>
#include <JavaScriptCore/ISO8601.h>
#include <JavaScriptCore/JSCTimeZone.h>
#include <JavaScriptCore/JSExportMacros.h>
#include <JavaScriptCore/TemporalCoreTypes.h>
#include <JavaScriptCore/TemporalObject.h>
#include <optional>
#include <utility>
#include <wtf/Int128.h>

namespace JSC {

// NudgeResult — output record from a single nudge step: rounded duration, epoch ns, and calendar-expand flag.
// temporal_rs: NudgeResultRecord (src/builtins/core/duration.rs)
struct NudgeResult {
    ISO8601::InternalDuration duration;
    Int128 nudgedEpochNs;
    bool didExpandCalendarUnit;
    NudgeResult() { }
    NudgeResult(ISO8601::InternalDuration d, Int128 ns, bool expanded)
        : duration(d)
        , nudgedEpochNs(ns)
        , didExpandCalendarUnit(expanded)
    {
    }
};

// Nudged — combines a NudgeResult with a fractional total used by RoundRelativeDuration.
// temporal_rs: NudgedRecord (src/builtins/core/duration.rs)
struct Nudged {
    NudgeResult nudgeResult;
    double total;
    Nudged() { }
    Nudged(NudgeResult n, double t)
        : nudgeResult(n)
        , total(t)
    {
    }
};

} // namespace JSC

namespace JSC {
namespace TemporalCore {

int JS_EXPORT_PRIVATE durationSign(const ISO8601::Duration&);

ISO8601::Duration JS_EXPORT_PRIVATE negateDuration(const ISO8601::Duration&);

ISO8601::Duration JS_EXPORT_PRIVATE absDuration(const ISO8601::Duration&);

TemporalUnit NODELETE JS_EXPORT_PRIVATE largestSubduration(const ISO8601::Duration&);

int64_t JS_EXPORT_PRIVATE totalSeconds(const ISO8601::Duration&);

Int128 JS_EXPORT_PRIVATE totalSubseconds(const ISO8601::Duration&);

std::optional<double> JS_EXPORT_PRIVATE balanceDuration(ISO8601::Duration&, TemporalUnit largestUnit);

Int128 JS_EXPORT_PRIVATE timeDurationFromComponents(double hours, double minutes, double seconds, double milliseconds, double microseconds, double nanoseconds);

std::pair<int64_t, Int128> JS_EXPORT_PRIVATE splitTimeDuration(Int128 timeDuration);

ISO8601::PlainTime JS_EXPORT_PRIVATE plainTimeFromSubdayNs(Int128 ns);

double JS_EXPORT_PRIVATE totalTimeDuration(Int128, TemporalUnit);

TemporalResult<ISO8601::Duration> JS_EXPORT_PRIVATE temporalDurationFromInternal(ISO8601::InternalDuration, TemporalUnit largestUnit);

ISO8601::InternalDuration JS_EXPORT_PRIVATE toInternalDuration(ISO8601::Duration);

ISO8601::InternalDuration JS_EXPORT_PRIVATE toInternalDurationRecord(ISO8601::Duration);

TemporalResult<Int128> JS_EXPORT_PRIVATE add24HourDaysToTimeDuration(Int128 timeDuration, double days);

TemporalResult<ISO8601::InternalDuration> JS_EXPORT_PRIVATE toInternalDurationRecordWith24HourDays(ISO8601::Duration);

TemporalResult<ISO8601::Duration> JS_EXPORT_PRIVATE toDateDurationRecordWithoutTime(ISO8601::Duration);

Int128 JS_EXPORT_PRIVATE getUTCEpochNanoseconds(ISO8601::PlainDate, ISO8601::PlainTime);

constexpr int32_t unitIndexInTable(TemporalUnit);

constexpr TemporalUnit unitInTable(int32_t);

TemporalResult<ISO8601::Duration> JS_EXPORT_PRIVATE adjustDateDurationRecord(const ISO8601::Duration& dateDuration, int64_t days, std::optional<int64_t> weeks, std::optional<int64_t> months);

TemporalResult<Nudged> JS_EXPORT_PRIVATE nudgeToCalendarUnit(int32_t sign,
    const ISO8601::InternalDuration&, Int128 originEpochNs, Int128 destEpochNs,
    ISO8601::PlainDate, ISO8601::PlainTime, double increment, TemporalUnit,
    RoundingMode, const TimeZone* = nullptr, CalendarID = iso8601CalendarID());

TemporalResult<NudgeResult> JS_EXPORT_PRIVATE nudgeToZonedTime(int32_t sign,
    const ISO8601::InternalDuration&, ISO8601::PlainDate, ISO8601::PlainTime,
    const TimeZone&, double increment, TemporalUnit, RoundingMode,
    CalendarID = iso8601CalendarID());

TemporalResult<NudgeResult> JS_EXPORT_PRIVATE nudgeToDayOrTime(ISO8601::InternalDuration,
    Int128 destEpochNs, TemporalUnit largestUnit, double increment,
    TemporalUnit smallestUnit, RoundingMode);

TemporalResult<ISO8601::InternalDuration> JS_EXPORT_PRIVATE bubbleRelativeDuration(int32_t sign,
    ISO8601::InternalDuration, Int128 nudgedEpochNs, ISO8601::PlainDate, ISO8601::PlainTime,
    TemporalUnit largestUnit, TemporalUnit smallestUnit,
    const TimeZone*, CalendarID);

TemporalResult<void> JS_EXPORT_PRIVATE roundRelativeDuration(ISO8601::InternalDuration&,
    Int128 originEpochNs, Int128 destEpochNs, ISO8601::PlainDate, ISO8601::PlainTime,
    TemporalUnit largestUnit, double increment, TemporalUnit smallestUnit,
    RoundingMode, const TimeZone*, CalendarID);

} // namespace TemporalCore
} // namespace JSC

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

JS_EXPORT_PRIVATE int durationSign(const ISO8601::Duration&);

JS_EXPORT_PRIVATE ISO8601::Duration negateDuration(const ISO8601::Duration&);

JS_EXPORT_PRIVATE ISO8601::Duration absDuration(const ISO8601::Duration&);

JS_EXPORT_PRIVATE TemporalUnit NODELETE largestSubduration(const ISO8601::Duration&);

JS_EXPORT_PRIVATE int64_t totalSeconds(const ISO8601::Duration&);

JS_EXPORT_PRIVATE Int128 totalSubseconds(const ISO8601::Duration&);

JS_EXPORT_PRIVATE std::optional<double> balanceDuration(ISO8601::Duration&, TemporalUnit largestUnit);

JS_EXPORT_PRIVATE Int128 timeDurationFromComponents(double hours, double minutes, double seconds, double milliseconds, double microseconds, double nanoseconds);

JS_EXPORT_PRIVATE std::pair<int64_t, Int128> splitTimeDuration(Int128 timeDuration);

JS_EXPORT_PRIVATE ISO8601::PlainTime plainTimeFromSubdayNs(Int128 ns);

JS_EXPORT_PRIVATE double totalTimeDuration(Int128, TemporalUnit);

JS_EXPORT_PRIVATE TemporalResult<ISO8601::Duration> temporalDurationFromInternal(ISO8601::InternalDuration, TemporalUnit largestUnit);

JS_EXPORT_PRIVATE ISO8601::InternalDuration toInternalDuration(ISO8601::Duration);

JS_EXPORT_PRIVATE ISO8601::InternalDuration toInternalDurationRecord(ISO8601::Duration);

JS_EXPORT_PRIVATE TemporalResult<Int128> add24HourDaysToTimeDuration(Int128 timeDuration, double days);

JS_EXPORT_PRIVATE TemporalResult<ISO8601::InternalDuration> toInternalDurationRecordWith24HourDays(ISO8601::Duration);

JS_EXPORT_PRIVATE TemporalResult<ISO8601::Duration> toDateDurationRecordWithoutTime(ISO8601::Duration);

JS_EXPORT_PRIVATE Int128 getUTCEpochNanoseconds(ISO8601::PlainDate, ISO8601::PlainTime);

constexpr int32_t unitIndexInTable(TemporalUnit);

constexpr TemporalUnit unitInTable(int32_t);

JS_EXPORT_PRIVATE TemporalResult<ISO8601::Duration> adjustDateDurationRecord(const ISO8601::Duration& dateDuration, int64_t days, std::optional<int64_t> weeks, std::optional<int64_t> months);

JS_EXPORT_PRIVATE TemporalResult<Nudged> nudgeToCalendarUnit(int32_t sign,
    const ISO8601::InternalDuration&, Int128 originEpochNs, Int128 destEpochNs,
    ISO8601::PlainDate, ISO8601::PlainTime, double increment, TemporalUnit,
    RoundingMode, const TimeZone* = nullptr, CalendarID = iso8601CalendarID());

JS_EXPORT_PRIVATE TemporalResult<NudgeResult> nudgeToZonedTime(int32_t sign,
    const ISO8601::InternalDuration&, ISO8601::PlainDate, ISO8601::PlainTime,
    const TimeZone&, double increment, TemporalUnit, RoundingMode,
    CalendarID = iso8601CalendarID());

JS_EXPORT_PRIVATE TemporalResult<NudgeResult> nudgeToDayOrTime(ISO8601::InternalDuration,
    Int128 destEpochNs, TemporalUnit largestUnit, double increment,
    TemporalUnit smallestUnit, RoundingMode);

JS_EXPORT_PRIVATE TemporalResult<ISO8601::InternalDuration> bubbleRelativeDuration(int32_t sign,
    ISO8601::InternalDuration, Int128 nudgedEpochNs, ISO8601::PlainDate, ISO8601::PlainTime,
    TemporalUnit largestUnit, TemporalUnit smallestUnit,
    const TimeZone*, CalendarID);

JS_EXPORT_PRIVATE TemporalResult<void> roundRelativeDuration(ISO8601::InternalDuration&,
    Int128 originEpochNs, Int128 destEpochNs, ISO8601::PlainDate, ISO8601::PlainTime,
    TemporalUnit largestUnit, double increment, TemporalUnit smallestUnit,
    RoundingMode, const TimeZone*, CalendarID);

} // namespace TemporalCore
} // namespace JSC

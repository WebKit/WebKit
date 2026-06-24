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

// JSC Temporal Core — ICU timezone bridge
// temporal_rs reference: src/tz.rs
// Last synced: v0.2.3

#include <JavaScriptCore/ISO8601.h>
#include <JavaScriptCore/JSCTimeZone.h>
#include <JavaScriptCore/JSExportMacros.h>
#include <JavaScriptCore/TemporalCoreTypes.h>
#include <JavaScriptCore/TemporalEnums.h>
#include <array>
#include <span>

namespace JSC {
namespace TemporalCore {

// GapOffsets carries the timezone offsets bracketing a DST spring-forward gap.
// beforeNs: UTC offset (ns) just before the transition — used for Later/Compatible disambiguation.
// afterNs: UTC offset (ns) just after the transition — used for Earlier disambiguation.
struct GapOffsets {
    int64_t beforeNs { 0 };
    int64_t afterNs  { 0 };
};

// PossibleEpochNanoseconds — the 0/1/2 candidates for a local date+time in a timezone.
// Matches temporal_rs CandidateEpochNanoseconds (provider/src/provider.rs).
//   GapOffsets                          — DST gap: local time does not exist; carries gap bracket offsets
//   ISO8601::ExactTime                  — unambiguous: exactly one candidate
//   std::array<ISO8601::ExactTime, 2>   — DST fold: two candidates [earlier, later]
using PossibleEpochNanoseconds = Variant<GapOffsets, ISO8601::ExactTime, std::array<ISO8601::ExactTime, 2>>;

// Helper: returns true when the local time falls in a DST gap (no valid instant).
inline bool isGap(const PossibleEpochNanoseconds& p) { return std::holds_alternative<GapOffsets>(p); }

// Helper: returns a span over the ExactTime candidates (empty for gap, 1 for single, 2 for fold).
WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
inline std::span<const ISO8601::ExactTime> epochCandidates(const PossibleEpochNanoseconds& p)
{
    if (auto* a = std::get_if<std::array<ISO8601::ExactTime, 2>>(&p))
        return *a;
    if (auto* t = std::get_if<ISO8601::ExactTime>(&p))
        return std::span<const ISO8601::ExactTime>(t, t + 1);
    return { }; // gap — no candidates
}
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

void JS_EXPORT_PRIVATE exactTimeToLocalDateAndTime(ISO8601::ExactTime, int64_t offsetNs, ISO8601::PlainDate&, ISO8601::PlainTime&);

// https://tc39.es/proposal-canonical-tz/#sec-temporal-timezoneequals
// Fast path: both operands are already Time Zone Identifier Records. No parsing,
// no string comparison — at most one array lookup per side to resolve aliases.
bool JS_EXPORT_PRIVATE timeZoneEquals(const TimeZone&, const TimeZone&);
// String form: parses each side to a TimeZone, then delegates. Returns false if
// either input is not a syntactically valid identifier.
bool JS_EXPORT_PRIVATE timeZoneEquals(StringView id1, StringView id2);

TemporalResult<int64_t> JS_EXPORT_PRIVATE getOffsetNanosecondsFor(const TimeZone&, ISO8601::ExactTime);

TemporalResult<PossibleEpochNanoseconds> JS_EXPORT_PRIVATE getPossibleEpochNanosecondsFor(const TimeZone&, const ISO8601::PlainDate&, const ISO8601::PlainTime&);

TemporalResult<std::optional<ISO8601::ExactTime>> JS_EXPORT_PRIVATE getTimeZoneTransition(const TimeZone&, ISO8601::ExactTime, TransitionDirection);

TemporalResult<ISO8601::ExactTime> JS_EXPORT_PRIVATE getEpochNanosecondsFor(const TimeZone&, const ISO8601::PlainDate&, const ISO8601::PlainTime&, TemporalDisambiguation);

TemporalResult<ISO8601::ExactTime> JS_EXPORT_PRIVATE addZonedDateTime(ISO8601::ExactTime startEpochNs, const TimeZone&, const ISO8601::Duration&, TemporalOverflow, CalendarID calendarKind = iso8601CalendarID());

} // namespace TemporalCore
} // namespace JSC

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

// JSC Temporal Core — PlainDateTime algorithms
// temporal_rs reference: src/builtins/core/plain_date_time.rs
// Last synced: v0.2.3

#include <JavaScriptCore/CalendarICUBridge.h>
#include <JavaScriptCore/ISO8601.h>
#include <JavaScriptCore/JSExportMacros.h>
#include <JavaScriptCore/TemporalCoreTypes.h>
#include <JavaScriptCore/TemporalObject.h>

namespace JSC {
namespace TemporalCore {

int32_t NODELETE JS_EXPORT_PRIVATE compareISODateTime(ISO8601::PlainDate, ISO8601::PlainTime, ISO8601::PlainDate, ISO8601::PlainTime);

TemporalResult<ISO8601::InternalDuration> JS_EXPORT_PRIVATE differencePlainDateTimeWithRounding(
    ISO8601::PlainDate thisDate, ISO8601::PlainTime thisTime,
    ISO8601::PlainDate otherDate, ISO8601::PlainTime otherTime,
    CalendarID,
    TemporalUnit largestUnit, TemporalUnit smallestUnit,
    RoundingMode, double increment);

TemporalResult<ISO8601::Duration> JS_EXPORT_PRIVATE differenceTemporalPlainDateTime(
    DifferenceOperation,
    ISO8601::PlainDate thisDate, ISO8601::PlainTime thisTime,
    ISO8601::PlainDate otherDate, ISO8601::PlainTime otherTime,
    CalendarID,
    TemporalUnit smallestUnit, TemporalUnit largestUnit,
    RoundingMode, double increment);

} // namespace TemporalCore
} // namespace JSC

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

// JSC Temporal Core — ISO 8601 date arithmetic
// temporal_rs reference: src/iso.rs
// Last synced: v0.2.3

#include <JavaScriptCore/ISO8601.h>
#include <JavaScriptCore/JSExportMacros.h>
#include <JavaScriptCore/TemporalCoreTypes.h>
#include <JavaScriptCore/TemporalObject.h>

namespace JSC {
namespace TemporalCore {

JS_EXPORT_PRIVATE ISO8601::PlainYearMonth balanceISOYearMonth(int64_t year, int64_t month);

JS_EXPORT_PRIVATE ISO8601::PlainDate addDaysToISODate(const ISO8601::PlainDate&, int64_t days);

JS_EXPORT_PRIVATE TemporalResult<ISO8601::PlainDate> regulateISODate(int32_t year, int32_t month, int64_t day, TemporalOverflow);

JS_EXPORT_PRIVATE TemporalResult<ISO8601::PlainDate> isoDateAdd(const ISO8601::PlainDate&, const ISO8601::Duration&, TemporalOverflow);

JS_EXPORT_PRIVATE int32_t NODELETE isoDateCompare(const ISO8601::PlainDate&, const ISO8601::PlainDate&);

JS_EXPORT_PRIVATE int32_t NODELETE isoTimeCompare(const ISO8601::PlainTime&, const ISO8601::PlainTime&);

JS_EXPORT_PRIVATE ISO8601::Duration diffISODate(const ISO8601::PlainDate& one, const ISO8601::PlainDate& two, TemporalUnit largestUnit);

JS_EXPORT_PRIVATE ISO8601::InternalDuration diffISODateTime(const ISO8601::PlainDate& d1, const ISO8601::PlainTime& t1, const ISO8601::PlainDate& d2, const ISO8601::PlainTime& t2, TemporalUnit largestUnit);

struct RoundedISODateTime {
    ISO8601::PlainDate date;
    ISO8601::PlainTime time;
};
JS_EXPORT_PRIVATE RoundedISODateTime roundISODateTime(ISO8601::PlainDate, ISO8601::PlainTime, Int128 incrementNs, TemporalUnit, RoundingMode);

} // namespace TemporalCore
} // namespace JSC

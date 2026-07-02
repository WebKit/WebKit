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

// JSC Temporal Core — Calendar arithmetic (ISO8601 path)
// temporal_rs reference: src/builtins/core/calendar.rs
// Last synced: v0.2.3

#include <JavaScriptCore/ISO8601.h>
#include <JavaScriptCore/ISOArithmetic.h>
#include <JavaScriptCore/JSExportMacros.h>
#include <JavaScriptCore/TemporalCoreTypes.h>
#include <JavaScriptCore/TemporalObject.h>

namespace JSC {
namespace TemporalCore {

ISO8601::Duration JS_EXPORT_PRIVATE calendarDateUntil(const ISO8601::PlainDate& one, const ISO8601::PlainDate& two, TemporalUnit largestUnit);

TemporalResult<ISO8601::PlainDate> JS_EXPORT_PRIVATE calendarDateAdd(const ISO8601::PlainDate&, const ISO8601::Duration&, TemporalOverflow);

} // namespace TemporalCore
} // namespace JSC

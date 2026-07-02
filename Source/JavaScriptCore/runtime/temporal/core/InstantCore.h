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

// JSC Temporal Core — Instant algorithms
// temporal_rs reference: src/builtins/core/instant.rs
// Last synced: v0.2.3

#include <JavaScriptCore/ISO8601.h>
#include <JavaScriptCore/JSExportMacros.h>
#include <JavaScriptCore/TemporalObject.h>
#include <optional>
#include <wtf/text/WTFString.h>

namespace JSC {
namespace TemporalCore {

// MaximumTemporalInstantRoundingIncrement — temporal_rs: internal (no single fn; values are hardcoded per spec table)
// Values from Temporal.Instant.prototype.round steps 15-20 (one per unit).
// https://tc39.es/proposal-temporal/#sec-temporal.instant.prototype.round
constexpr double NODELETE maximumInstantIncrement(TemporalUnit smallestUnit)
{
    // Maximum increment = how many units of smallestUnit fit in one day.
    return static_cast<double>(static_cast<int64_t>(lengthInNanoseconds(TemporalUnit::Day)))
        / static_cast<double>(static_cast<int64_t>(lengthInNanoseconds(smallestUnit)));
}

WTF::String JS_EXPORT_PRIVATE instantToString(ISO8601::ExactTime, std::optional<int64_t> offsetNs, PrecisionData);

} // namespace TemporalCore
} // namespace JSC

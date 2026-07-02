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

// JSC Temporal Core — Rounding algorithms
// temporal_rs reference: src/rounding.rs
//                        src/options.rs
//                        src/options/increment.rs
// Last synced: v0.2.3

#include <JavaScriptCore/JSExportMacros.h>
#include <JavaScriptCore/TemporalCoreTypes.h>
#include <JavaScriptCore/TemporalEnums.h>
#include <optional>

namespace JSC {
namespace TemporalCore {

RoundingMode JS_EXPORT_PRIVATE negateTemporalRoundingMode(RoundingMode);

double JS_EXPORT_PRIVATE applyUnsignedRoundingMode(double x, double r1, double r2, UnsignedRoundingMode);

std::optional<unsigned> JS_EXPORT_PRIVATE maximumRoundingIncrement(TemporalUnit);

double JS_EXPORT_PRIVATE roundNumberToIncrementDouble(double x, double increment, RoundingMode);

Int128 JS_EXPORT_PRIVATE roundNumberToIncrementAsIfPositive(Int128 x, Int128 increment, RoundingMode);

Int128 JS_EXPORT_PRIVATE roundNumberToIncrementInt128(Int128 x, Int128 increment, RoundingMode);

TemporalResult<void> JS_EXPORT_PRIVATE validateTemporalRoundingIncrement(double increment, std::optional<double> dividend, Inclusivity);

} // namespace TemporalCore
} // namespace JSC

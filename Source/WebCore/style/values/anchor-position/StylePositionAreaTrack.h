/*
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <wtf/Forward.h>

namespace WebCore {
namespace Style {

// Specifies which tracks(s) on the axis that the position-area span occupies.
// Represented as 3 bits: start track, center track, end track.
enum class PositionAreaTrack : uint8_t {
    Start     = 0b001, // First track.
    SpanStart = 0b011, // First and center tracks.
    End       = 0b100, // Last track.
    SpanEnd   = 0b110, // Center and last track.
    Center    = 0b010, // Center track.
    SpanAll   = 0b111, // All tracks along the axis.
};

constexpr PositionAreaTrack flipPositionAreaTrack(PositionAreaTrack track)
{
    // We need to cast values out of the enum type restrictions in order to do math.
    auto trackBits = static_cast<uint8_t>(track);
    constexpr uint8_t startBit = static_cast<uint8_t>(PositionAreaTrack::Start);
    constexpr uint8_t endBit = static_cast<uint8_t>(PositionAreaTrack::End);
    constexpr uint8_t sideBits = startBit | endBit;

    bool isSymmetric = !(trackBits & startBit) == !(trackBits & endBit);
    auto invertedValue = isSymmetric ? trackBits
        // Flip side bits and merge with not-side bits.
        : ((trackBits & sideBits) ^ sideBits) | (trackBits & ~sideBits);

    return static_cast<PositionAreaTrack>(invertedValue);
}

// MARK: - Logging

WTF::TextStream& operator<<(WTF::TextStream&, PositionAreaTrack);

} // namespace Style
} // namespace WebCore

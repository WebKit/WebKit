/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <cstdint>
#include <wtf/EnumClassOperatorOverloads.h>
#include <wtf/StdLibExtras.h>

namespace JSC {

enum class IterationMode : uint16_t {
    Generic = 1 << 0,
    FastArray = 1 << 1,
    FastMap = 1 << 2,
    FastSet = 1 << 3,
    FastString = 1 << 4,
    FastArrayValues = 1 << 5,
    FastArrayKeys = 1 << 6,
    FastArrayEntries = 1 << 7,
    FastMapKeys = 1 << 8,
    FastMapValues = 1 << 9,
    FastMapEntries = 1 << 10,
    FastSetValues = 1 << 11,
    FastSetEntries = 1 << 12,
};

constexpr unsigned numberOfIterationModes = 13;

// To keep the amount of code emitted for one iteration site reasonable, we only allow this many
// distinct fast iteration modes per site. Once the limit is reached, newly observed iterable
// types are recorded and handled as Generic instead.
constexpr unsigned maxNumberOfFastIterationModes = 2;

OVERLOAD_BITWISE_OPERATORS_FOR_ENUM_CLASS_WITH_INTERGRALS(IterationMode);

inline bool canUseFastIterationMode(uint16_t seenModes, IterationMode mode)
{
    ASSERT(mode != IterationMode::Generic);
    uint16_t seenFastModes = seenModes & ~static_cast<uint16_t>(IterationMode::Generic);
    if (seenFastModes & static_cast<uint16_t>(mode))
        return true;
    return static_cast<unsigned>(std::popcount(seenFastModes)) < maxNumberOfFastIterationModes;
}

struct IterationModeMetadata {
    uint16_t seenModes { 0 };
    static constexpr ptrdiff_t offsetOfSeenModes() { return OBJECT_OFFSETOF(IterationModeMetadata, seenModes); }
    static_assert(sizeof(decltype(seenModes)) == sizeof(IterationMode));
};

} // namespace JSC

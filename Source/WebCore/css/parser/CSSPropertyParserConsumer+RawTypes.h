/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#include "CSSUnits.h"
#include <variant>

namespace WebCore {

enum CSSValueID : uint16_t;

namespace CSSPropertyParserHelpers {

struct NoneRaw {
    bool operator==(const NoneRaw&) const = default;
};

struct NumberRaw {
    double value;

    bool operator==(const NumberRaw&) const = default;
};

struct PercentRaw {
    double value;

    bool operator==(const PercentRaw&) const = default;
};

struct AngleRaw {
    CSSUnitType type;
    double value;

    bool operator==(const AngleRaw&) const = default;
};

struct LengthRaw {
    CSSUnitType type;
    double value;

    bool operator==(const LengthRaw&) const = default;
};

struct ResolutionRaw {
    CSSUnitType type;
    double value;

    bool operator==(const ResolutionRaw&) const = default;
};

struct TimeRaw {
    CSSUnitType type;
    double value;

    bool operator==(const TimeRaw&) const = default;
};

struct SymbolRaw {
    CSSValueID value;

    bool operator==(const SymbolRaw&) const = default;
};

enum class IntegerValueRange : uint8_t { All, Positive, NonNegative };

constexpr double computeMinimumValue(IntegerValueRange range)
{
    switch (range) {
    case IntegerValueRange::All:
        return -std::numeric_limits<double>::infinity();
    case IntegerValueRange::NonNegative:
        return 0.0;
    case IntegerValueRange::Positive:
        return 1.0;
    }

    RELEASE_ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT();

    return 0.0;
}

template<typename IntType, IntegerValueRange Range>
struct IntegerRaw {
    IntType value;

    bool operator==(const IntegerRaw<IntType, Range>&) const = default;
};

using LengthOrPercentRaw = std::variant<LengthRaw, PercentRaw>;
using PercentOrNumberRaw = std::variant<PercentRaw, NumberRaw>;
using NumberOrNoneRaw = std::variant<NumberRaw, NoneRaw>;
using PercentOrNoneRaw = std::variant<PercentRaw, NoneRaw>;
using PercentOrNumberOrNoneRaw = std::variant<PercentRaw, NumberRaw, NoneRaw>;
using AngleOrNumberRaw = std::variant<AngleRaw, NumberRaw>;
using AngleOrNumberOrNoneRaw = std::variant<AngleRaw, NumberRaw, NoneRaw>;

} // namespace CSSPropertyParserHelpers
} // namespace WebCore

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

class CSSParserTokenRange;

namespace CSSPropertyParserHelpers {

struct NoneRaw { };

struct NumberRaw {
    double value;
};

struct PercentRaw {
    double value;
};

struct AngleRaw {
    CSSUnitType type;
    double value;
};

struct LengthRaw {
    CSSUnitType type;
    double value;
};

using LengthOrPercentRaw = std::variant<LengthRaw, PercentRaw>;
using NumberOrPercentRaw = std::variant<NumberRaw, PercentRaw>;
using NumberOrNoneRaw = std::variant<NumberRaw, NoneRaw>;
using PercentOrNoneRaw = std::variant<PercentRaw, NoneRaw>;
using NumberOrPercentOrNoneRaw = std::variant<NumberRaw, PercentRaw, NoneRaw>;
using AngleOrNumberRaw = std::variant<AngleRaw, NumberRaw>;
using AngleOrNumberOrNoneRaw = std::variant<AngleRaw, NumberRaw, NoneRaw>;

enum class NegativePercentagePolicy : bool { Forbid, Allow };
enum class UnitlessQuirk : bool { Allow, Forbid };
enum class UnitlessZeroQuirk : bool { Allow, Forbid };
enum class IntegerValueRange : uint8_t { All, Positive, NonNegative };

// MARK: - Comma
// FIXME: Rename to `consumeComma`.
bool consumeCommaIncludingWhitespace(CSSParserTokenRange&);

// MARK: - Slash
// FIXME: Rename to `consumeSlash`.
bool consumeSlashIncludingWhitespace(CSSParserTokenRange&);

// MARK: - Function
// NOTE: consumeFunction expects the range starts with a FunctionToken.
CSSParserTokenRange consumeFunction(CSSParserTokenRange&);

} // namespace CSSPropertyParserHelpers
} // namespace WebCore

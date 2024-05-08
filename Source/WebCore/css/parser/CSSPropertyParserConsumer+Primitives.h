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

#include "CSSParserMode.h"
#include "CSSPropertyParserConsumer+RawTypes.h"
#include "Length.h"
#include <wtf/Ref.h>

namespace WebCore {

class CSSParserTokenRange;
class CSSCalcValue;

namespace CSSPropertyParserHelpers {

bool equal(const Ref<CSSCalcValue>&, const Ref<CSSCalcValue>&);

template<typename T> struct UnevaluatedCalc {
    using RawType = T;
    Ref<CSSCalcValue> calc;

    inline bool operator==(const UnevaluatedCalc<T>& other)
    {
        return equal(calc, other.calc);
    }
};

enum class NegativePercentagePolicy : bool { Forbid, Allow };
enum class UnitlessQuirk : bool { Allow, Forbid };
enum class UnitlessZeroQuirk : bool { Allow, Forbid };

struct CSSPropertyParserOptions {
    CSSParserMode parserMode                    { HTMLStandardMode };
    ValueRange valueRange                       { ValueRange::All };
    NegativePercentagePolicy negativePercentage { NegativePercentagePolicy::Forbid };
    UnitlessQuirk unitless                      { UnitlessQuirk::Forbid };
    UnitlessZeroQuirk unitlessZero              { UnitlessZeroQuirk::Forbid };
};

// FIXME: Presentational HTML attributes shouldn't use the CSS parser for lengths.
bool shouldAcceptUnitlessValue(double, CSSPropertyParserOptions);

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

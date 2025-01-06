/*
 * Copyright (C) 2024 Samuel Weinig <sam@webkit.org>
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

#include "CSSCalcSymbolsAllowed.h"
#include "CSSPrimitiveNumericTypes.h"
#include "CSSPropertyParserOptions.h"
#include <optional>

namespace WebCore {

namespace Calculation {
enum class Category : uint8_t;
}

class CSSParserTokenRange;
struct CSSParserContext;

enum CSSValueID : uint16_t;

namespace CSSCalc {

struct SimplificationOptions;
struct Tree;

struct ParserOptions {
    // `category` represents the context in which the parse is taking place.
    Calculation::Category category;

    // `range` represents the allowed numeric range for the calculated result.
    CSS::Range range;

    // `allowedSymbols` contains additional symbols that can be used in the calculation. These will need to be resolved before the calculation can be resolved.
    CSSCalcSymbolsAllowed allowedSymbols;

    // `propertyOptions` contains options about the specific property the calc() is intended to be used with.
    CSSPropertyParserOptions propertyOptions;
};

// Parses and simplifies the provided `CSSParserTokenRange` into a CSSCalc::Tree. Returns `std::nullopt` on failure.
std::optional<Tree> parseAndSimplify(CSSParserTokenRange&, const CSSParserContext&, const ParserOptions&, const SimplificationOptions&);

// Returns whether the provided `CSSValueID` is one of the functions that should be parsed as a `calc()`.
bool isCalcFunction(CSSValueID function, const CSSParserContext&);

} // namespace CSSCalc
} // namespace WebCore

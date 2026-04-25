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

#include "CSSCalcSymbolTable.h"
#include "CSSPrimitiveNumericRange.h"
#include "CSSToLengthConversionData.h"

namespace WebCore {

class RenderStyle;

namespace CSS {
enum class Category : uint8_t;
}

namespace CSSCalc {
struct Tree;
}

namespace Style {
namespace Calculation {

struct Tree;

struct ToCSSOptions {
    // `category` represents the context in which the conversion is taking place.
    CSS::Category category;

    // `range` represents the allowed numeric range for the calculated result.
    CSS::Range range;

    // `style` represents the RenderStyle the Tree is from for zoom calculations.
    const RenderStyle& style;
};

struct ToStyleOptions {
    // `category` represents the context in which the conversion is taking place.
    CSS::Category category;

    // `range` represents the allowed numeric range for the calculated result.
    CSS::Range range;

    // `conversionData` contains information needed to convert length units into their canonical forms.
    std::optional<CSSToLengthConversionData> conversionData;

    // `symbolTable` contains information needed to convert unresolved symbols into Numeric values.
    CSSCalcSymbolTable symbolTable;
};

// Converts from Style::Calculation::Tree to CSSCalc::Tree.
CSSCalc::Tree toCSS(const Tree&, const ToCSSOptions&);

// Converts from CSSCalc::Tree to Style::Calculation::Tree.
Tree toStyle(const CSSCalc::Tree&, const ToStyleOptions&);

} // namespace Calculation
} // namespace Style
} // namespace WebCore

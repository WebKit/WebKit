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
#include "CSSCalcTree.h"
#include "CSSToLengthConversionData.h"

namespace WebCore {

namespace Calculation {
enum class Category : uint8_t;
}

namespace CSSCalc {

// https://drafts.csswg.org/css-values-4/#calc-simplification

struct SimplificationOptions {
    // `category` represents the context in which the simplification is taking place.
    Calculation::Category category;

    // `conversionData` contains information needed to convert length units into their canonical forms.
    std::optional<CSSToLengthConversionData> conversionData;

    // `symbolTable` contains information needed to convert unresolved symbols into Numeric values.
    CSSCalcSymbolTable symbolTable;

    // `allowZeroValueLengthRemovalFromSum` allows removal of 0 value lengths (px, em, etc.) from Sum operations.
    bool allowZeroValueLengthRemovalFromSum = false;

    // `allowUnresolvedUnits` allows math operations to be evaluated even if the unit is not fully canonicalized. Only meaningful if `conversionData` cannot be supplied.
    bool allowUnresolvedUnits = false;

    // `allowNonMatchingUnits` allows math operations to be evaluated even if the units of its arguments don't match. Only meaningful if `conversionData` cannot be supplied.
    bool allowNonMatchingUnits = false;
};

// MARK: Copy & Simplify

Tree copyAndSimplify(const Tree&, const SimplificationOptions&);
Child copyAndSimplify(const Child&, const SimplificationOptions&);

// MARK: In-place Simplify

std::optional<Child> simplify(Number&, const SimplificationOptions&);
std::optional<Child> simplify(Percentage&, const SimplificationOptions&);
std::optional<Child> simplify(NonCanonicalDimension&, const SimplificationOptions&);
std::optional<Child> simplify(CanonicalDimension&, const SimplificationOptions&);
std::optional<Child> simplify(Symbol&, const SimplificationOptions&);
std::optional<Child> simplify(Sum&, const SimplificationOptions&);
std::optional<Child> simplify(Product&, const SimplificationOptions&);
std::optional<Child> simplify(Negate&, const SimplificationOptions&);
std::optional<Child> simplify(Invert&, const SimplificationOptions&);
std::optional<Child> simplify(Min&, const SimplificationOptions&);
std::optional<Child> simplify(Max&, const SimplificationOptions&);
std::optional<Child> simplify(Clamp&, const SimplificationOptions&);
std::optional<Child> simplify(RoundNearest&, const SimplificationOptions&);
std::optional<Child> simplify(RoundUp&, const SimplificationOptions&);
std::optional<Child> simplify(RoundDown&, const SimplificationOptions&);
std::optional<Child> simplify(RoundToZero&, const SimplificationOptions&);
std::optional<Child> simplify(Mod&, const SimplificationOptions&);
std::optional<Child> simplify(Rem&, const SimplificationOptions&);
std::optional<Child> simplify(Sin&, const SimplificationOptions&);
std::optional<Child> simplify(Cos&, const SimplificationOptions&);
std::optional<Child> simplify(Tan&, const SimplificationOptions&);
std::optional<Child> simplify(Asin&, const SimplificationOptions&);
std::optional<Child> simplify(Acos&, const SimplificationOptions&);
std::optional<Child> simplify(Atan&, const SimplificationOptions&);
std::optional<Child> simplify(Atan2&, const SimplificationOptions&);
std::optional<Child> simplify(Pow&, const SimplificationOptions&);
std::optional<Child> simplify(Sqrt&, const SimplificationOptions&);
std::optional<Child> simplify(Hypot&, const SimplificationOptions&);
std::optional<Child> simplify(Log&, const SimplificationOptions&);
std::optional<Child> simplify(Exp&, const SimplificationOptions&);
std::optional<Child> simplify(Abs&, const SimplificationOptions&);
std::optional<Child> simplify(Sign&, const SimplificationOptions&);
std::optional<Child> simplify(Progress&, const SimplificationOptions&);
std::optional<Child> simplify(MediaProgress&, const SimplificationOptions&);
std::optional<Child> simplify(ContainerProgress&, const SimplificationOptions&);
std::optional<Child> simplify(Anchor&, const SimplificationOptions&);
std::optional<Child> simplify(AnchorSize&, const SimplificationOptions&);

// MARK: Unit Canonicalization

std::optional<CanonicalDimension> canonicalize(NonCanonicalDimension, const std::optional<CSSToLengthConversionData>&);

} // namespace CSSCalc
} // namespace WebCore

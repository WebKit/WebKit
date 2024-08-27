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

#include "CSSToLengthConversionData.h"
#include <optional>

namespace WebCore {

class CSSCalcSymbolTable;

namespace CSSCalc {

struct Tree;

struct EvaluationOptions {
    // `conversionData` contains information needed to convert units into their canonical forms.
    std::optional<CSSToLengthConversionData> conversionData;

    // `symbolTable` contains information needed to convert unresolved symbols into numeric values.
    const CSSCalcSymbolTable& symbolTable;

    // `allowUnresolvedUnits` allows math operations to be evaluated even if the unit is not fully canonicalized. Only meaningful if `conversionData` cannot be supplied.
    bool allowUnresolvedUnits = false;

    // `allowNonMatchingUnits` allows math operations to be evaluated even if the units of its arguments don't match. Only meaningful if `conversionData` cannot be supplied.
    bool allowNonMatchingUnits = false;
};

std::optional<double> evaluateDouble(const Tree&, const EvaluationOptions&);

} // namespace CSSCalc
} // namespace WebCore

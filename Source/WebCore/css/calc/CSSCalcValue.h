/*
 * Copyright (C) 2011, 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "CSSValue.h"
#include "CSSValueKeywords.h"
#include "CalculationValue.h"
#include <wtf/Forward.h>
#include <wtf/Ref.h>

namespace WebCore {

class CSSCalcExpressionNode;
class CSSCalcSymbolTable;
class CSSParserTokenRange;
class CSSToLengthConversionData;
class RenderStyle;

enum class CSSUnitType : uint8_t;
enum class CalculationCategory : uint8_t;
enum class ValueRange : uint8_t;

class CSSCalcValue final : public CSSValue {
public:
    static RefPtr<CSSCalcValue> create(CSSValueID function, const CSSParserTokenRange&, CalculationCategory destinationCategory, ValueRange, const CSSCalcSymbolTable&, bool allowsNegativePercentage = false);
    static RefPtr<CSSCalcValue> create(CSSValueID function, const CSSParserTokenRange&, CalculationCategory destinationCategory, ValueRange);
    static RefPtr<CSSCalcValue> create(const CalculationValue&, const RenderStyle&);
    static RefPtr<CSSCalcValue> create(Ref<CSSCalcExpressionNode>&&, bool allowsNegativePercentage = false);
    ~CSSCalcValue();

    CalculationCategory category() const;
    double doubleValue() const;
    double computeLengthPx(const CSSToLengthConversionData&) const;
    CSSUnitType primitiveType() const;

    Ref<CalculationValue> createCalculationValue(const CSSToLengthConversionData&) const;
    void setPermittedValueRange(ValueRange);

    void collectDirectComputationalDependencies(HashSet<CSSPropertyID>&) const;
    void collectDirectRootComputationalDependencies(HashSet<CSSPropertyID>&) const;

    String customCSSText() const;
    bool equals(const CSSCalcValue&) const;
    
    static bool isCalcFunction(CSSValueID);

    void dump(TextStream&) const;

    bool convertingToLengthRequiresNonNullStyle(int lengthConversion) const;

private:
    CSSCalcValue(Ref<CSSCalcExpressionNode>&&, bool shouldClampToNonNegative);

    double clampToPermittedRange(double) const;

    const Ref<CSSCalcExpressionNode> m_expression;
    bool m_shouldClampToNonNegative;
};

TextStream& operator<<(TextStream&, const CSSCalcValue&);

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CSS_VALUE(CSSCalcValue, isCalcValue())

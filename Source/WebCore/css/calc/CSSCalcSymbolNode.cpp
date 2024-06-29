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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CSSCalcSymbolNode.h"

#include "CSSCalcCategoryMapping.h"
#include "CSSCalcSymbolTable.h"
#include "CSSPrimitiveValue.h"
#include "CSSValueKeywords.h"
#include "CalcExpressionNode.h"
#include <wtf/text/TextStream.h>

namespace WebCore {

Ref<CSSCalcSymbolNode> CSSCalcSymbolNode::create(CSSValueID symbol, CSSUnitType unitType)
{
    return adoptRef(*new CSSCalcSymbolNode(symbol, unitType));
}

CSSCalcSymbolNode::CSSCalcSymbolNode(CSSValueID symbol, CSSUnitType unitType)
    : CSSCalcExpressionNode(calcUnitCategory(unitType))
    , m_symbol(symbol)
    , m_unitType(unitType)
{
}

String CSSCalcSymbolNode::customCSSText() const
{
    return nameLiteralForSerialization(m_symbol);
}

bool CSSCalcSymbolNode::isResolvable() const
{
    return false;
}

bool CSSCalcSymbolNode::isZero() const
{
    return false;
}

std::unique_ptr<CalcExpressionNode> CSSCalcSymbolNode::createCalcExpression(const CSSToLengthConversionData&) const
{
    // CSSCalcSymbolNode does not support conversion to CalcExpression and there are currently
    // no specifications that require it.
    ASSERT_NOT_REACHED();
    return nullptr;
}

double CSSCalcSymbolNode::doubleValue(CSSUnitType, const CSSCalcSymbolTable& symbolTable) const
{
    // The symbol table should only lack the value in the case of programmer error.
    auto result = symbolTable.get(m_symbol);
    ASSERT(result);
    return result->value;
}

double CSSCalcSymbolNode::computeLengthPx(const CSSToLengthConversionData&) const
{
    // CSSCalcSymbolNode does not support length computation and there are currently
    // no specifications that require it.
    ASSERT_NOT_REACHED();
    return 0;
}

bool CSSCalcSymbolNode::equals(const CSSCalcExpressionNode& other) const
{
    if (type() != other.type())
        return false;

    return m_symbol == static_cast<const CSSCalcSymbolNode&>(other).m_symbol
        && m_unitType == static_cast<const CSSCalcSymbolNode&>(other).m_unitType;
}

CSSCalcExpressionNode::Type CSSCalcSymbolNode::type() const
{
    return Type::CssCalcSymbol;
}

CSSUnitType CSSCalcSymbolNode::primitiveType() const
{
    return m_unitType;
}

void CSSCalcSymbolNode::collectComputedStyleDependencies(ComputedStyleDependencies&) const
{
}

void CSSCalcSymbolNode::dump(TextStream& ts) const
{
    ts << "symbol['" << nameLiteral(m_symbol) << "'] (category: " << category() << ", type: " << primitiveType() << ")";
}

} // namespace WebCore
